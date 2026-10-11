#include "blitter.h"

#include "llc_stdstring.h"
#include "llc_file.h"
#include "llc_path.h"

#include "llc_json_expression.h"
#include "llc_noise.h"
#include "llc_parse.h"
#include "llc_deflate.h"
#include "llc_aes.h"
#include "llc_chrono.h"
#include "llc_view_bit.h"
#include "llc_bit.h"

LLC_USING_TYPEINT();

::llc::err_t			blt::loadConfig				(::blt::SBlitter & appState, ::llc::vcst_t jsonFileName)	{
	if_fail_fef(::llc::jsonFileRead(appState.Config, jsonFileName), "Failed to load configuration file: %.*s.", LLC_VIEW_ARGS(jsonFileName));
	const ::llc::err_t			indexApp					= ::llc::jsonExpressionResolve(::llc::vcst_t{"application.blitter"	}, appState.Config.Reader, (uint32_t)0, appState.Folder);
	::llc::err_t				indexDB;
	if_fail_fef(indexDB = ::llc::jsonExpressionResolve(::llc::vcst_t{"database"}, appState.Config.Reader, (u2_t)indexApp, appState.Folder), "'database' not found in '%s'.", "application.blitter");
	const ::llc::err_t			indexPath					= ::llc::jsonExpressionResolve(::llc::vcst_t{"path.database"			}, appState.Config.Reader, (uint32_t)indexApp, appState.Folder);
	if(::llc::failed(indexPath)) {
		warning_printf("'path.database' not found in '%s'. Using current path as database root.", "application.blitter");
		appState.Folder									= "./";
	}
	if_fail_fef(::blt::configDatabases(appState.Databases, appState.Config.Reader, indexDB, {}, appState.Folder), "%s", "Failed to load query.");
	return 0;
}

sttc ::llc::err_t		dbFileExtension				(const ::blt::DATABASE_HOST & hostType, bool bEncrypted, ::llc::vcst_t & extension) {
	extension										= bEncrypted
		? ((::blt::DATABASE_HOST_DEFLATE & hostType) ? ::llc::vcst_t{"czon"} : ::llc::vcst_t{"cson"})
		: ((::blt::DATABASE_HOST_DEFLATE & hostType) ? ::llc::vcst_t{"zson"} : ::llc::vcst_t{"sson"})
		;
	return 0;
}

::llc::err_t			blt::blockFileName			(::llc::string & filename, ::llc::vcst_t dbName, bool bEncrypted, const ::blt::DATABASE_HOST hostType, const uint32_t block) {
	if_fail_fef(filename.append(dbName), "%s", "Out of memory?");
	char												temp[64]					= {};
	::llc::vcst_t							extension					= {};
	::dbFileExtension(hostType, bEncrypted, extension);
	sprintf_s(temp, ".%u.%s", block, extension.begin());
	if_fail_fef(filename.append(::llc::vcst_t{temp}), "%s", "Out of memory?");
	return 0;
}

::llc::err_t			blt::tableFolderName		(::llc::string & foldername, ::llc::vcst_t dbName, const uint32_t blockSize) {
	if_fail_fef(foldername.append(dbName), "%s", "Out of memory?");
	char												temp[128]					= {};
	sprintf_s(temp, ".%u.db", blockSize);
	if_fail_fef(foldername.append(::llc::vcst_t{temp}), "%s", "Out of memory?");
	return 0;
}

::llc::err_t			blt::tableFileName			(::llc::string & filename, const ::blt::DATABASE_HOST & hostType, bool bEncrypted, ::llc::vcst_t jsonDBKey) {
	if_fail_fef(filename.append(jsonDBKey), "%s", "Out of memory?");
	char												temp[64]					= {};
	::llc::vcst_t							extension					= {};
	::dbFileExtension(hostType, bEncrypted, extension);
	sprintf_s(temp, ".%s", extension.begin());
	if_fail_fef(filename.append(::llc::vcst_t{temp}), "%s", "Out of memory?");
	return 0;
}

::llc::err_t			blt::tableFolderInit		(::llc::string & finalFolderName, ::llc::vcst_t dbPath, ::llc::vcst_t dbName, const uint32_t blockSize) {
	finalFolderName										= dbPath;
	if(finalFolderName.size()) {
		if(finalFolderName[finalFolderName.size()-1] != '/' && finalFolderName[finalFolderName.size()-1] != '\\')
			if_fail_fef(finalFolderName.push_back('/'), "%s", "Out of memory?");
		if_fail_fef(::blt::tableFolderName(finalFolderName, dbName, blockSize), "%s", "??");
		if_fail_fef(::llc::pathCreate({finalFolderName.begin(), finalFolderName.size()}), "Failed to create database folder: %s.", finalFolderName.begin());
		info_printf("Output folder: %s.", finalFolderName.begin());
		if(finalFolderName[finalFolderName.size()-1] != '/' && finalFolderName[finalFolderName.size()-1] != '\\')
			if_fail_fef(finalFolderName.push_back('/'), "%s", "Out of memory?");
	}
	else if(0 == finalFolderName.size())
		if_fail_fef(finalFolderName.append(::llc::vcst_t{"./"}), "%s", "Out of memory?");
	return 0;
}

static	::llc::err_t		dbFileLoad					(::llc::SLoadCache & loadCache, ::blt::TNamedBlitterDB & jsonDB, ::llc::vcst_t folder, ::llc::vcst_t fileName, uint32_t idBlock)	{
	if(jsonDB.Val.Blocks.size() < ::blt::MAX_TABLE_BLOCKS_IN_MEMORY) {
		const int32_t										idxBlock					= jsonDB.Val.Blocks.push_back({});
		if_fail_fef(idxBlock																					, "%s", "Out of memory?");
		if_fail_fef(jsonDB.Val.BlockIndices.push_back(idBlock)												, "%s", "Out of memory?");
		if_fail_fef(jsonDB.Val.Offsets.push_back(idBlock * (int64_t)jsonDB.Val.BlockSize)					, "%s", "Out of memory?");
		if_fail_fef(jsonDB.Val.BlockDirty.resize(jsonDB.Val.BlockIndices.size() / sizeof(uint32_t) + 1, 0U)	, "%s", "Out of memory?");
		if_fail_fef(jsonDB.Val.BlockTimes.push_back(::llc::timeCurrentInUs())								, "%s", "Out of memory?");
		jsonDB.Val.Blocks[idxBlock].create();
		::llc::SJSONFile									& dbBlock					= *jsonDB.Val.Blocks[idxBlock];
		if_fail_fef(::llc::fileToMemorySecure(loadCache, fileName, jsonDB.Val.EncryptionKey.cu8(), llc::bit_true(jsonDB.Val.HostType, ::blt::DATABASE_HOST_DEFLATE), dbBlock.Bytes), "Failed to open block file: %.*s.", LLC_VIEW_ARGS(fileName));
		if_fail_fef(::llc::jsonParse(dbBlock.Reader, dbBlock.Bytes.cc()), "Failed to parse database block file: %.*s.", LLC_VIEW_ARGS(fileName));
		return idxBlock;
	}
	else {
		const uint64_t										* time						= 0;
		const uint32_t										idxBlock					= ::llc::min(::llc::view_array<const uint64_t>{jsonDB.Val.BlockTimes}, &time);
		if_fail_fef(idxBlock, "%s", "Out of memory?");
		::llc::view_bit<uint32_t>							dirty						= {jsonDB.Val.BlockDirty.begin(), jsonDB.Val.Blocks.size()};
		if(dirty[idxBlock]) {
			::llc::string						folderName						= {};
			::blt::SWriteCache								writeCache						= {loadCache};
			if_fail_fef(::blt::tableFolderInit(folderName, folder, jsonDB.Key, jsonDB.Val.BlockSize), "Failed to initialize folder for table '%.*s'. Not enough permissions?", LLC_VIEW_ARGS(jsonDB.Key));
			if_fail_fef(::blt::blockWrite(writeCache, folderName, jsonDB.Key, jsonDB.Val.EncryptionKey, jsonDB.Val.HostType, jsonDB.Val.Blocks[idxBlock]->Bytes.cu8(), jsonDB.Val.BlockIndices[idxBlock]), "%s", "Unknown error!");
			dirty[idxBlock]									= false;
		}

		jsonDB.Val.BlockIndices	[idxBlock]				= idBlock;
		jsonDB.Val.Offsets		[idxBlock]				= (idBlock * (int64_t)jsonDB.Val.BlockSize);
		jsonDB.Val.BlockTimes	[idxBlock]				= ::llc::timeCurrentInUs();
		::llc::SJSONFile									& dbBlock					= *jsonDB.Val.Blocks[idxBlock];
		dbBlock.Bytes	.clear();
		dbBlock.Reader	.Reset();
		if_fail_fef(::llc::fileToMemorySecure(loadCache, fileName, jsonDB.Val.EncryptionKey.cu8(), llc::bit_true(jsonDB.Val.HostType, ::blt::DATABASE_HOST_DEFLATE), dbBlock.Bytes), "Failed to open block file: %.*s.", LLC_VIEW_ARGS(fileName));
		if_fail_fef(::llc::jsonParse(dbBlock.Reader, dbBlock.Bytes.cc()), "Failed to parse database block file: %.*s.", LLC_VIEW_ARGS(fileName));
		dirty[idxBlock]									= true;
		return idxBlock;
	}
}

::llc::err_t			blt::tableFileLoad			(::llc::SLoadCache & loadCache, ::blt::TNamedBlitterDB & jsonDB, ::llc::vcst_t folder)	{
	rni_if(jsonDB.Val.Blocks.size() && jsonDB.Val.Blocks[0]->Bytes.size(), "Table already loaded: '%.*s'.", LLC_VIEW_ARGS(jsonDB.Key));
	::llc::string							fileName					= folder;
	if_fail_fef(fileName.push_back('/')		, "%s", "Out of memory?");
	if_fail_fef(::blt::tableFileName(fileName, jsonDB.Val.HostType, jsonDB.Val.EncryptionKey.size() > 0,jsonDB.Key), "%s", "Out of memory?");
	info_printf("Loading database file: %.*s.", LLC_VIEW_ARGS(fileName));
	if_fail_fef(::dbFileLoad(loadCache, jsonDB, folder, fileName, 0), "Failed to load database from file: %.*s.", LLC_VIEW_ARGS(fileName));
	jsonDB.Val.BlocksOnDisk.clear();
	jsonDB.Val.MaxBlockOnDisk						= 0;
	return jsonDB.Val.BlocksOnDisk.push_back(0);
}

::llc::err_t			blt::blockFileLoad				(::llc::SLoadCache & loadCache, ::blt::TNamedBlitterDB & jsonDB, ::llc::vcst_t folder, uint32_t block)	{
	{
		::llc::err_t			blockIndex						= ::llc::rfind(block, {jsonDB.Val.BlockIndices.begin(), jsonDB.Val.BlockIndices.size()});
		if(0 <= blockIndex) {
			jsonDB.Val.BlockTimes[blockIndex]				= ::llc::timeCurrentInUs();
			info_printf("Block already loaded for database '%.*s': %u.", LLC_VIEW_ARGS(jsonDB.Key), block);
			return blockIndex;
		}
	}
	if_fail_fef(::llc::find(block, {jsonDB.Val.BlocksOnDisk}), "Block %u doesn't exist.", block);
	::llc::string							fileName						= folder;
	if_fail_fef(fileName.push_back('/'), "%s", "Out of memory?");
	if_fail_fef(::blt::tableFolderName(fileName, jsonDB.Key, jsonDB.Val.BlockSize), "%s", "Out of memory?");
	if_fail_fef(fileName.push_back('/'), "%s", "Out of memory?");
	if_fail_fef(::blt::blockFileName(fileName, jsonDB.Key, jsonDB.Val.EncryptionKey.size() > 0, jsonDB.Val.HostType, block), "%s", "Out of memory?");
	info_printf("Loading block file: %.*s.", LLC_VIEW_ARGS(fileName));
	return ::dbFileLoad(loadCache, jsonDB, folder, fileName, block);
}
static	::llc::err_t		table_description_load			(::blt::STableDescription & tableDesc, const ::llc::SJSONReader & configReader, const uint32_t iNode, ::llc::vcst_t tableName)	{
	int32_t												indexOfTable					= iNode;//::llc::jsonExpressionResolve(tableName, configReader, iNode);
	if_fail_fef(indexOfTable, "API configuration (%.*s) not found in JSON file.", LLC_VIEW_ARGS(tableName));
	::llc::vcst_t							strValue						= {};
	::llc::jsonExpressionResolve(::llc::vcst_t{"public"}, configReader, indexOfTable, strValue);
	tableDesc.Public								= strValue == ::llc::vcst_t{"true"};

	int32_t												indexOfFields;
	if_fail_vwf(0, indexOfFields = ::llc::jsonExpressionResolve(::llc::vcst_t{"fields"}, configReader, indexOfTable), "Fields for table (%.*s) not found in JSON file.", LLC_VIEW_ARGS(tableName));
	::llc::aobj<::llc::vcst_t>			namesOfFields;
	if_fail_fef(::llc::jsonObjectKeyList(configReader, indexOfFields, namesOfFields), "Failed to list fields: %s.", "Unknown reason");
	tableDesc.Fields.resize(namesOfFields.size());
	for(uint32_t iKey = 0; iKey < namesOfFields.size(); ++iKey) {
		::llc::SJSONFieldBinding							& fieldDesc						= tableDesc.Fields[iKey];
		fieldDesc.Field									= namesOfFields[iKey];
		::llc::vcst_t							fieldName						= {fieldDesc.Field.begin(), fieldDesc.Field.size()};
		const ::llc::err_t							indexOfValue					= ::llc::jsonObjectValueGet(configReader, indexOfFields, {fieldDesc.Field.begin(), fieldDesc.Field.size()});
		fieldDesc.Type									= configReader.Token[indexOfValue].Type;
		::llc::vcst_t								viewOfValue						= configReader.View[indexOfValue];
		if(::llc::JSON_TYPE_OBJECT == fieldDesc.Type) {
			[[maybe_unused]] const ::llc::err_t							indexOfMode					= ::llc::jsonObjectValueGet(configReader, indexOfValue, ::llc::vcst_t{"resolve"});
			const ::llc::err_t							indexOfDisplay				= ::llc::jsonObjectValueGet(configReader, indexOfValue, ::llc::vcst_t{"display"});
			::blt::TKeyValBindDescription						& bindDescription			= tableDesc.FieldMapDescription[tableDesc.FieldMapDescription.push_back({fieldName, {}})];
			//bindDescription.Val.Resolve						= (0 > indexOfMode		) ? ::blt::FIELD_RESOLVE_api	: ::llc::get_value<::blt::FIELD_RESOLVE>(configReader.View[indexOfMode]		);
			bindDescription.Val.Display						= (0 > indexOfDisplay	) ? ::blt::FIELD_DISPLAY_select	: ::llc::get_value<::blt::FIELD_DISPLAY>(configReader.View[indexOfDisplay]	);
			const ::llc::err_t							indexOfBind					= ::llc::jsonObjectValueGet(configReader, indexOfValue, ::llc::vcst_t{"table"});
			::llc::vcst_t								otherTableName				= configReader.View[indexOfBind];
			tableDesc.FieldMaps.push_back({fieldName, otherTableName});
		}
	}
	if_fail_fef(::llc::jsonMapToFields(tableDesc.IndicesMapToField, tableDesc.Fields, tableDesc.FieldMaps), "Failed to generate field indices for table '%.*s'.", LLC_VIEW_ARGS(tableName));
	if_fail_fef(::llc::jsonFieldsToMap(tableDesc.IndicesFieldToMap, tableDesc.Fields, tableDesc.FieldMaps), "Failed to generate field indices for table '%.*s'.", LLC_VIEW_ARGS(tableName));
	tableDesc.TableName								= tableName;
	return 0;
}

::llc::err_t			blt::configDatabases		(::llc::aobj<::blt::TNamedBlitterDB> & databases, const ::llc::SJSONReader & configReader, const int32_t indexConfigNode, const ::llc::view_array<const ::llc::vcst_t> & databasesToLoad, ::llc::vcst_t folder)	{
	::llc::vcst_t							jsonResult					= {};
	const int32_t										indexObjectDatabases		= (-1 == indexConfigNode)
		? ::llc::jsonExpressionResolve(::llc::vcst_t{"application.blitter.database"}, configReader, 0, jsonResult)
		: indexConfigNode
		;
	if_fail_fef(indexObjectDatabases, "%s", "Failed to get database config from JSON file.");
	jsonResult										= "";
	const ::llc::err_t							databaseArraySize			= ::llc::jsonArraySize(*configReader[indexObjectDatabases]);
	if_fail_fef(databaseArraySize, "%s", "Failed to get database count from config file.");
	char												temp[64]					= {};
	if_fail_fef(databases.resize(databaseArraySize), "%s", "Out of memory?");
	::llc::SLoadCache									loadCache;
	for(uint32_t iDatabase = 0, countDatabases = (uint32_t)databaseArraySize; iDatabase < countDatabases; ++iDatabase) {
		sprintf_s(temp, "['%u']", iDatabase);
		const int32_t										iJSONTable					=::llc::jsonExpressionResolve(::llc::vcst_t{temp}, configReader, indexObjectDatabases, jsonResult);
		if_fail_fef(iJSONTable, "Failed to load config from json! Last contents found: %s.", jsonResult.begin());
		if_fail_fef(::llc::jsonExpressionResolve(::llc::vcst_t{"name"}, configReader, iJSONTable, jsonResult), "Failed to load config from json! Last contents found: %s.", jsonResult.begin());
		::blt::TNamedBlitterDB								& jsonDB					= databases[iDatabase];
		jsonDB.Key										= jsonResult;
		if_fail_fef(::table_description_load(jsonDB.Val.Description, configReader, iJSONTable, jsonDB.Key), "%s", "Unknown error");
		{	// -- Load database block size
			int32_t												indexBlockNode;
			if_fail_wf(indexBlockNode = ::llc::jsonExpressionResolve(::llc::vcst_t{"block"}, configReader, iJSONTable, jsonResult), "Failed to load config from json! Last contents found: %.*s.", LLC_VIEW_ARGS(jsonResult))
			else
				::llc::parseIntegerDecimal(jsonResult, (jsonDB.Val.BlockSize = 0));
		}
		{	// -- Load database block size
			int32_t												indexKeyNode;
			if_fail_wf(indexKeyNode = ::llc::jsonExpressionResolve(::llc::vcst_t{"key"}, configReader, iJSONTable, jsonResult), "Failed to load config from json! Last contents found: %.*s.", LLC_VIEW_ARGS(jsonResult))
			else
				jsonDB.Val.EncryptionKey = jsonResult;
		}
		{	// -- Load database modes (remote, deflate)
			info_printf("Loading database info for '%.*s'.", LLC_VIEW_ARGS(jsonDB.Key));
			jsonResult										= {};
			int32_t												typeFound;
			if_fail_wf(typeFound = ::llc::jsonExpressionResolve(::llc::vcst_t{"source"}, configReader, iJSONTable, jsonResult), "Failed to load database type for database: %.*s. Defaulting to local.", LLC_VIEW_ARGS(jsonDB.Key));
			jsonDB.Val.HostType								= (::llc::vcst_t{"local"} == jsonResult || ::llc::failed(typeFound)) ? ::blt::DATABASE_HOST_LOCAL : ::blt::DATABASE_HOST_REMOTE;
			jsonResult										= {};
			typeFound										= ::llc::jsonExpressionResolve(::llc::vcst_t{"deflate"}, configReader, iJSONTable, jsonResult);
			if_fail_wf(typeFound, "Failed to load database compression for database: %.*s. Defaulting to uncompressed.", LLC_VIEW_ARGS(jsonDB.Key));
			if(::llc::vcst_t{"true"} == jsonResult)
				jsonDB.Val.HostType								|= ::blt::DATABASE_HOST_DEFLATE;
		}
		{	// -- Load field bindings
			::llc::err_t			indexBindArray;
			if_fail_wf(indexBindArray = ::llc::jsonExpressionResolve(::llc::vcst_t{"bind"}, configReader, iJSONTable, jsonResult), "No bindings found for database file: %.*s.", LLC_VIEW_ARGS(jsonDB.Key))
			else {
				::llc::err_t			sizeBindArray				= ::llc::jsonArraySize(*configReader[indexBindArray]);
				if_fail_fef(sizeBindArray, "Cannot get size of array: %s.", "??");
				if_fail_fef(jsonDB.Val.Bindings.resize(sizeBindArray), "%s", "Out of memory?");;
				for(uint32_t iBind = 0; iBind < jsonDB.Val.Bindings.size(); ++iBind) {
					sprintf_s(temp, "['%u']", iBind);
					if_fail_fef(::llc::jsonExpressionResolve(::llc::vcst_t{temp}, configReader, indexBindArray, jsonResult), "Failed to load config from json! Last contents found: %s.", jsonResult.begin());
					jsonDB.Val.Bindings[iBind]						= jsonResult;
				}
			}
		}
		::llc::string							dbfilename					= {};
		if_fail_fef(::blt::tableFileName(dbfilename, jsonDB.Val.HostType, jsonDB.Val.EncryptionKey.size() > 0, jsonDB.Key), "%s", "??");
		if(llc::bit_true(jsonDB.Val.HostType, ::blt::DATABASE_HOST_REMOTE))
			continue;

		if(jsonDB.Val.BlockSize) {	// block databases get loaded on-demand. However, we need to know which blocks are available on disk in order to fill missing blocks with empty records.
			::llc::vcst_t								extension;
			::dbFileExtension(jsonDB.Val.HostType, jsonDB.Val.EncryptionKey.size() > 0, extension);
			::llc::string								folderName			= folder;
			if_fail_fef(folderName.push_back('/'), "Failed to load database: %s. Out of memory?", dbfilename.begin());
			if_fail_fef(::blt::tableFolderName(folderName, jsonDB.Key, jsonDB.Val.BlockSize), "Failed to load database: %s. Out of memory?", dbfilename.begin());
			::llc::aobj<::llc::string>				blockFiles;
			if_fail(::llc::pathList(folderName, blockFiles))
				if_fail_fef(::llc::pathCreate(folderName), "Failed to load database: %s. Out of memory?", dbfilename.begin());

			for(uint32_t iFile = 0; iFile < blockFiles.size(); ++iFile) {
				const ::llc::vcst_t							pathBlock			= blockFiles[iFile];
				if(pathBlock.size() >= extension.size()) {
					if(extension != ::llc::vcst_t{&pathBlock[pathBlock.size() - extension.size()], extension.size()})
						continue;
				}
				const ::llc::vcst_t							blockDigits			= {&pathBlock[folderName.size() + jsonDB.Key.size() + 2], pathBlock.size() - folderName.size() - extension.size() - jsonDB.Key.size() - 3};
				uint64_t												blockIndex			= 0;
				if_fail_fef(::llc::parseIntegerDecimal(blockDigits, blockIndex), "Failed to load database: %s. Out of memory?", dbfilename.begin());
				bool													bAdd				= true;
				for(uint32_t i = 0; i < jsonDB.Val.BlocksOnDisk.size(); ++i)
					if(((uint32_t)blockIndex) < jsonDB.Val.BlocksOnDisk[i]) {
						if_fail_fef(jsonDB.Val.BlocksOnDisk.insert(i, (uint32_t)blockIndex), "%s.", "Out of memory?");
						jsonDB.Val.MaxBlockOnDisk							= ::llc::max(jsonDB.Val.MaxBlockOnDisk, (int32_t)blockIndex);
						bAdd												= false;
						break;
					}

				if(bAdd) {
					jsonDB.Val.MaxBlockOnDisk							= ::llc::max(jsonDB.Val.MaxBlockOnDisk, (int32_t)blockIndex);
					if_fail_fef(jsonDB.Val.BlocksOnDisk.push_back((uint32_t)blockIndex), "Failed to load database: %s. Out of memory?", dbfilename.begin());
				}
			}
			for(uint32_t i = 0; i < jsonDB.Val.BlocksOnDisk.size(); ++i)
				info_printf("Database block found for '%s': %u.", dbfilename.begin(), jsonDB.Val.BlocksOnDisk[i]);
			continue;
		}
		// -- Load json database file.
		if(0 == databasesToLoad.size()) {
			if_fail_fef(::blt::tableFileLoad(loadCache, jsonDB, folder), "Failed to load database: %s.", dbfilename.begin());
		}
		else {
			for(uint32_t iDB = 0; iDB < databasesToLoad.size(); ++iDB)
				if(databasesToLoad[iDB] == jsonDB.Key) {
					if_fail_fef(::blt::tableFileLoad(loadCache, jsonDB, folder), "Failed to load database: %s.", dbfilename.begin());
					break;
				}
		}
	}
	return 0;
}

static	::llc::err_t		queryLoad						(::blt::SBlitterQuery& query, const ::llc::view_array<const ::llc::TKeyValConstString> keyvals, ::llc::aobj<::llc::vcsc_t> & expansionKeyStorage) {
	if(0 <= ::llc::find(::llc::vcst_t{"offset"}, keyvals))
		::llc::keyvalNumeric("offset", keyvals, query.Range.Offset);
	if(0 <= ::llc::find(::llc::vcst_t{"limit"}, keyvals))
		::llc::keyvalNumeric("limit", keyvals, query.Range.Count);
	else if(0 <= ::llc::find(::llc::vcst_t{"count"}, keyvals))
		::llc::keyvalNumeric("count", keyvals, query.Range.Count);

	const ::llc::err_t							indexExpand						= ::llc::find(::llc::vcst_t{"expand"}, keyvals);
	if(0 <= indexExpand) {
		if_fail_fef(::llc::split(::llc::vcst_t{keyvals[indexExpand].Val}, '.', expansionKeyStorage), "%s", "Out of memory?");
		query.ExpansionKeys								= expansionKeyStorage;
	}
	return 0;
}

::llc::err_t			blt::requestProcess				(::llc::SExpressionReader & expressionReader, ::blt::SBlitterQuery & query, const ::blt::SBlitterRequest & request, ::llc::aobj<::llc::vcsc_t> & expansionKeyStorage)						{
	// --- Generate response
	{
		::llc::aobj<::llc::vcsc_t>			pathSplit						= {};
		::llc::split(request.Path, '/', pathSplit);
		query.Database									= (0 < pathSplit.size()) ? ::llc::vcst_t{pathSplit[0].begin(), pathSplit[0].size()} : request.Path;
		query.Command									= (1 < pathSplit.size()) ? ::llc::vcst_t{pathSplit[1].begin(), pathSplit[1].size()} : ::llc::vcst_t{};
		if(2 < pathSplit.size()) {
			// --- Retrieve detail part
			uint64_t											detail							= (uint64_t)-1LL;
			::llc::vcst_t								strDetail						= {};
			strDetail										= pathSplit[pathSplit.size() - 1];
			if(strDetail.size()) {
				::llc::parseIntegerDecimal(strDetail, detail);
				query.Detail									= (int64_t)detail;
			}
		}
		info_printf("Parameters: \nDatabase: %.*s.\nCommand: %.*s.\nDetail: %" LLC_FMT_S3 ".", LLC_VIEW_ARGS(query.Database), LLC_VIEW_ARGS(query.Command), query.Detail);
	}
	{ // retrieve path and database and read expression if any
		const ::llc::err_t							indexPath						= ::llc::find('.', query.Database);
		query.Path										= {};
		if(0 <= indexPath) {
			if(((::llc::u2_t)indexPath + 1) < query.Database.size())
				query.Path										= {&query.Database[indexPath + 1]	, query.Database.size() - (indexPath + 1)};
			query.Database									= {query.Database.begin()			, (uint32_t)indexPath};
		}

		if(query.Path.size())
			if_fail_fef(::llc::expressionReaderParse(expressionReader, {query.Path.begin(), query.Path.size()}), "Error: %s", "Invalid path syntax?");
	}
	query.Record									= request.ContentBody;
	if_fail_fef(::llc::jsonParse(query.RecordReader, query.Record), "Failed to parse JSON record '%.*s'", LLC_VIEW_ARGS(query.Record));
	// --- Retrieve query data from querystring.
	if_fail_fef(::queryLoad(query, request.QueryStringKeyVals, expansionKeyStorage), "%s", "Out of memory?");
	return 0;
}
