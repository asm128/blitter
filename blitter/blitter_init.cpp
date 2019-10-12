#include "blitter.h"

#include "gpk_stdstring.h"
#include "gpk_storage.h"

#include "gpk_json_expression.h"
#include "gpk_noise.h"
#include "gpk_parse.h"
#include "gpk_find.h"
#include "gpk_deflate.h"
#include "gpk_aes.h"
#include "gpk_cgi.h"
#include "gpk_chrono.h"
#include "gpk_view_bit.h"
#include "gpk_module.h"

::gpk::error_t									blt::loadConfig				(::blt::SBlitter & appState, const ::gpk::view_const_string & jsonFileName)	{
	gpk_necall(::gpk::jsonFileRead(appState.Config, jsonFileName), "Failed to load configuration file: %s.", ::gpk::toString(jsonFileName).begin());
	const ::gpk::error_t								indexApp					= ::gpk::jsonExpressionResolve(::gpk::view_const_string{"application.blitter"	}, appState.Config.Reader, (uint32_t)0, appState.Folder);
	const ::gpk::error_t								indexDB						= ::gpk::jsonExpressionResolve(::gpk::view_const_string{"database"				}, appState.Config.Reader, (uint32_t)indexApp, appState.Folder);
	gpk_necall(indexDB, "'database' not found in '%s'.", "application.blitter");
	const ::gpk::error_t								indexPath					= ::gpk::jsonExpressionResolve(::gpk::view_const_string{"path.database"			}, appState.Config.Reader, (uint32_t)indexApp, appState.Folder);
	if(errored(indexPath)) {
		warning_printf("'path.database' not found in '%s'. Using current path as database root.", "application.blitter");
		appState.Folder									= "./";
	}
	gpk_necall(::blt::configDatabases(appState.Databases, appState.Config.Reader, indexDB, {}, appState.Folder), "%s", "Failed to load query.");
	return 0;
}

::gpk::error_t									dbFileExtension				(const ::blt::DATABASE_HOST & hostType, bool bEncrypted, ::gpk::view_const_string & extension) {
	extension										= bEncrypted
		? ((::blt::DATABASE_HOST_DEFLATE & hostType) ? "czon" : "cson")
		: ((::blt::DATABASE_HOST_DEFLATE & hostType) ? "zson" : "sson")
		;
	return 0;
}

::gpk::error_t									blt::blockFileName			(::gpk::array_pod<char_t> & filename, const ::gpk::view_const_char & dbName, bool bEncrypted, const ::blt::DATABASE_HOST hostType, const uint32_t block) {
	gpk_necall(filename.append(dbName), "%s", "Out of memory?");
	char												temp[64]					= {};
	::gpk::view_const_string							extension					= {};
	::dbFileExtension(hostType, bEncrypted, extension);
	sprintf_s(temp, ".%u.%s", block, extension.begin());
	gpk_necall(filename.append(::gpk::view_const_string{temp}), "%s", "Out of memory?");
	return 0;
}

::gpk::error_t									blt::tableFolderName		(::gpk::array_pod<char_t> & foldername, const ::gpk::view_const_char & dbName, const uint32_t blockSize) {
	gpk_necall(foldername.append(dbName), "%s", "Out of memory?");
	char												temp[128]					= {};
	sprintf_s(temp, ".%u.db", blockSize);
	gpk_necall(foldername.append(::gpk::view_const_string{temp}), "%s", "Out of memory?");
	return 0;
}

::gpk::error_t									blt::tableFileName			(::gpk::array_pod<char_t> & filename, const ::blt::DATABASE_HOST & hostType, bool bEncrypted, const ::gpk::view_const_char & jsonDBKey) {
	gpk_necall(filename.append(jsonDBKey), "%s", "Out of memory?");
	char												temp[64]					= {};
	::gpk::view_const_string							extension					= {};
	::dbFileExtension(hostType, bEncrypted, extension);
	sprintf_s(temp, ".%s", extension.begin());
	gpk_necall(filename.append(::gpk::view_const_string{temp}), "%s", "Out of memory?");
	return 0;
}

::gpk::error_t									blt::tableFolderInit		(::gpk::array_pod<char_t> & finalFolderName, const ::gpk::view_const_char & dbPath, const ::gpk::view_const_char & dbName, const uint32_t blockSize) {
	finalFolderName										= dbPath;
	if(finalFolderName.size()) {
		if(finalFolderName[finalFolderName.size()-1] != '/' && finalFolderName[finalFolderName.size()-1] != '\\')
			gpk_necall(finalFolderName.push_back('/'), "%s", "Out of memory?");
		gpk_necall(::blt::tableFolderName(finalFolderName, dbName, blockSize), "%s", "??");
		gpk_necall(::gpk::pathCreate({finalFolderName.begin(), finalFolderName.size()}), "Failed to create database folder: %s.", finalFolderName.begin());
		info_printf("Output folder: %s.", finalFolderName.begin());
		if(finalFolderName[finalFolderName.size()-1] != '/' && finalFolderName[finalFolderName.size()-1] != '\\')
			gpk_necall(finalFolderName.push_back('/'), "%s", "Out of memory?");
	}
	else if(0 == finalFolderName.size())
		gpk_necall(finalFolderName.append(::gpk::view_const_string{"./"}), "%s", "Out of memory?");
	return 0;
}

static	::gpk::error_t							dbFileLoad					(::gpk::SLoadCache & loadCache, ::blt::TNamedBlitterDB & jsonDB, const ::gpk::view_const_char & folder, const ::gpk::view_const_char & fileName, uint32_t idBlock)	{
	if(jsonDB.Val.Blocks.size() < ::blt::MAX_TABLE_BLOCKS_IN_MEMORY) {
		const int32_t										idxBlock					= jsonDB.Val.Blocks.push_back({});
		gpk_necall(idxBlock																					, "%s", "Out of memory?");
		gpk_necall(jsonDB.Val.BlockIndices.push_back(idBlock)												, "%s", "Out of memory?");
		gpk_necall(jsonDB.Val.Offsets.push_back(idBlock * (int64_t)jsonDB.Val.BlockSize)					, "%s", "Out of memory?");
		gpk_necall(jsonDB.Val.BlockDirty.resize(jsonDB.Val.BlockIndices.size() / sizeof(uint32_t) + 1, 0U)	, "%s", "Out of memory?");
		gpk_necall(jsonDB.Val.BlockTimes.push_back(::gpk::timeCurrentInUs())								, "%s", "Out of memory?");
		jsonDB.Val.Blocks[idxBlock].create();
		::gpk::SJSONFile									& dbBlock					= *jsonDB.Val.Blocks[idxBlock];
		gpk_necall(::gpk::fileToMemorySecure(loadCache, dbBlock.Bytes, fileName, jsonDB.Val.EncryptionKey, gbit_true(jsonDB.Val.HostType, ::blt::DATABASE_HOST_DEFLATE)), "Failed to open block file: %s.", ::gpk::toString(fileName).begin());
		gpk_necall(::gpk::jsonParse(dbBlock.Reader, dbBlock.Bytes), "Failed to parse database block file: %s.", ::gpk::toString(fileName).begin());
		return idxBlock;
	}
	else {
		const uint64_t										* time						= 0;
		const uint32_t										idxBlock					= ::gpk::min(::gpk::view_array<const uint64_t>{jsonDB.Val.BlockTimes}, &time);
		gpk_necall(idxBlock, "%s", "Out of memory?");
		::gpk::view_bit<uint32_t>							dirty						= {jsonDB.Val.BlockDirty.begin(), jsonDB.Val.Blocks.size()};
		if(dirty[idxBlock]) {
			::gpk::array_pod<char_t>						folderName						= {};
			::blt::SWriteCache								writeCache						= {loadCache};
			gpk_necall(::blt::tableFolderInit(folderName, folder, jsonDB.Key, jsonDB.Val.BlockSize), "Failed to initialize folder for table '%s'. Not enough permissions?", ::gpk::toString(jsonDB.Key).begin());
			gpk_necall(::blt::blockWrite(writeCache, folderName, jsonDB.Key, jsonDB.Val.EncryptionKey, jsonDB.Val.HostType, jsonDB.Val.Blocks[idxBlock]->Bytes, jsonDB.Val.BlockIndices[idxBlock]), "%s", "Unknown error!");
			dirty[idxBlock]									= false;
		}

		jsonDB.Val.BlockIndices	[idxBlock]				= idBlock;
		jsonDB.Val.Offsets		[idxBlock]				= (idBlock * (int64_t)jsonDB.Val.BlockSize);
		jsonDB.Val.BlockTimes	[idxBlock]				= ::gpk::timeCurrentInUs();
		::gpk::SJSONFile									& dbBlock					= *jsonDB.Val.Blocks[idxBlock];
		dbBlock.Bytes	.clear();
		dbBlock.Reader	.Reset();
		gpk_necall(::gpk::fileToMemorySecure(loadCache, dbBlock.Bytes, fileName, jsonDB.Val.EncryptionKey, gbit_true(jsonDB.Val.HostType, ::blt::DATABASE_HOST_DEFLATE)), "Failed to open block file: %s.", ::gpk::toString(fileName).begin());
		gpk_necall(::gpk::jsonParse(dbBlock.Reader, dbBlock.Bytes), "Failed to parse database block file: %s.", ::gpk::toString(fileName).begin());
		dirty[idxBlock]									= true;
		return idxBlock;
	}
}

::gpk::error_t									blt::tableFileLoad			(::gpk::SLoadCache & loadCache, ::blt::TNamedBlitterDB & jsonDB, const ::gpk::view_const_char & folder)	{
	rni_if(jsonDB.Val.Blocks.size() && jsonDB.Val.Blocks[0]->Bytes.size(), "Table already loaded: '%s'.", ::gpk::toString(jsonDB.Key).begin());
	::gpk::array_pod<char_t>							fileName					= folder;
	gpk_necall(fileName.push_back('/')		, "%s", "Out of memory?");
	gpk_necall(::blt::tableFileName(fileName, jsonDB.Val.HostType, jsonDB.Val.EncryptionKey.size() > 0,jsonDB.Key), "%s", "Out of memory?");
	info_printf("Loading database file: %s.", ::gpk::toString(fileName).begin());
	gpk_necall(::dbFileLoad(loadCache, jsonDB, folder, fileName, 0), "Failed to load database from file: %s.", ::gpk::toString(fileName).begin());
	jsonDB.Val.BlocksOnDisk							= {};
	jsonDB.Val.MaxBlockOnDisk						= 0;
	return jsonDB.Val.BlocksOnDisk.push_back(0);
}

::gpk::error_t									blt::blockFileLoad			(::gpk::SLoadCache & loadCache, ::blt::TNamedBlitterDB & jsonDB, const ::gpk::view_const_char & folder, uint32_t block)	{
	{
		::gpk::error_t										blockIndex					= ::gpk::rfind(block, {jsonDB.Val.BlockIndices.begin(), jsonDB.Val.BlockIndices.size()});
		if(0 <= blockIndex) {
			jsonDB.Val.BlockTimes[blockIndex]				= ::gpk::timeCurrentInUs();
			info_printf("Block already loaded for database '%s': %u.", ::gpk::toString(jsonDB.Key).begin(), block);
			return blockIndex;
		}
	}
	gpk_necall(::gpk::find(block, ::gpk::view_const_uint32{jsonDB.Val.BlocksOnDisk}), "Block %u doesn't exist.", block);
	::gpk::array_pod<char_t>							fileName					= folder;
	gpk_necall(fileName.push_back('/'), "%s", "Out of memory?");
	gpk_necall(::blt::tableFolderName(fileName, jsonDB.Key, jsonDB.Val.BlockSize), "%s", "Out of memory?");
	gpk_necall(fileName.push_back('/'), "%s", "Out of memory?");
	gpk_necall(::blt::blockFileName(fileName, jsonDB.Key, jsonDB.Val.EncryptionKey.size() > 0, jsonDB.Val.HostType, block), "%s", "Out of memory?");
	info_printf("Loading block file: %s.", ::gpk::toString(fileName).begin());
	return ::dbFileLoad(loadCache, jsonDB, folder, fileName, block);
}
static	::gpk::error_t							table_description_load			(::blt::STableDescription & tableDesc, const ::gpk::SJSONReader & configReader, const uint32_t iNode, const ::gpk::view_const_char & tableName)	{
	int32_t												indexOfTable					= iNode;//::gpk::jsonExpressionResolve({tableName.begin(), tableName.size()}, configReader, iNode);
	gpk_necall(indexOfTable, "API configuration (%s) not found in JSON file.", ::gpk::toString(tableName).begin());
	::gpk::view_const_string							strValue						= {};
	::gpk::jsonExpressionResolve("public", configReader, indexOfTable, strValue);
	tableDesc.Public								= strValue == ::gpk::view_const_string{"true"};

	int32_t												indexOfFields					= ::gpk::jsonExpressionResolve("fields", configReader, indexOfTable);
	rnw_if(errored(indexOfFields), "Fields for table (%s) not found in JSON file.", ::gpk::toString(tableName).begin());
	::gpk::array_obj<::gpk::view_const_char>			namesOfFields;
	gpk_necall(::gpk::jsonObjectKeyList(configReader, indexOfFields, configReader.View, namesOfFields), "Failed to list fields: %s.", "Unknown reason");
	tableDesc.Fields.resize(namesOfFields.size());
	for(uint32_t iKey = 0; iKey < namesOfFields.size(); ++iKey) {
		::gpk::SJSONFieldBinding							& fieldDesc						= tableDesc.Fields[iKey];
		fieldDesc.Field									= namesOfFields[iKey];
		::gpk::view_const_string							fieldName						= {fieldDesc.Field.begin(), fieldDesc.Field.size()};
		const ::gpk::error_t								indexOfValue					= ::gpk::jsonObjectValueGet(configReader, indexOfFields, {fieldDesc.Field.begin(), fieldDesc.Field.size()});
		fieldDesc.Type									= configReader.Token[indexOfValue].Type;
		::gpk::view_const_char								viewOfValue						= configReader.View[indexOfValue];
		if(::gpk::JSON_TYPE_OBJECT == fieldDesc.Type) {
			const ::gpk::error_t								indexOfMode					= ::gpk::jsonObjectValueGet(configReader, indexOfValue, ::gpk::view_const_string{"resolve"});
			const ::gpk::error_t								indexOfDisplay				= ::gpk::jsonObjectValueGet(configReader, indexOfValue, ::gpk::view_const_string{"display"});
			::blt::TKeyValBindDescription						& bindDescription			= tableDesc.FieldMapDescription[tableDesc.FieldMapDescription.push_back({fieldName, {}})];
			//bindDescription.Val.Resolve						= (0 > indexOfMode		) ? ::blt::FIELD_RESOLVE_api	: ::gpk::get_value<::blt::FIELD_RESOLVE>(configReader.View[indexOfMode]		);
			bindDescription.Val.Display						= (0 > indexOfDisplay	) ? ::blt::FIELD_DISPLAY_select	: ::gpk::get_value<::blt::FIELD_DISPLAY>(configReader.View[indexOfDisplay]	);
			const ::gpk::error_t								indexOfBind					= ::gpk::jsonObjectValueGet(configReader, indexOfValue, ::gpk::view_const_string{"table"});
			::gpk::view_const_char								otherTableName				= configReader.View[indexOfBind];
			::gpk::array_pod<char_t>							moduleName						= ::gpk::view_const_string("blt_");
			moduleName.append(otherTableName);
			moduleName										= ::gpk::toString(moduleName);
			bindDescription.Val.HandleModule				= GPK_LOAD_MODULE(moduleName.begin());
			if(bindDescription.Val.HandleModule)
				bindDescription.Val.FuncModule					= GPK_LOAD_MODULE_FUNCTION(bindDescription.Val.HandleModule, "blt_output_generate");;
			tableDesc.FieldMaps.push_back({fieldName, otherTableName});
		}
	}
	gpk_necall(::gpk::jsonMapToFields(tableDesc.IndicesMapToField, tableDesc.Fields, tableDesc.FieldMaps), "Failed to generate field indices for table '%s'.", ::gpk::toString(tableName).begin());
	gpk_necall(::gpk::jsonFieldsToMap(tableDesc.IndicesFieldToMap, tableDesc.Fields, tableDesc.FieldMaps), "Failed to generate field indices for table '%s'.", ::gpk::toString(tableName).begin());
	tableDesc.TableName								= tableName;
	return 0;
}

::gpk::error_t									blt::configDatabases		(::gpk::array_obj<::blt::TNamedBlitterDB> & databases, const ::gpk::SJSONReader & configReader, const int32_t indexConfigNode, const ::gpk::view_array<const ::gpk::view_const_string> & databasesToLoad, const ::gpk::view_const_string & folder)	{
	::gpk::view_const_string							jsonResult					= {};
	const int32_t										indexObjectDatabases		= (-1 == indexConfigNode)
		? ::gpk::jsonExpressionResolve(::gpk::view_const_string{"application.blitter.database"}, configReader, 0, jsonResult)
		: indexConfigNode
		;
	gpk_necall(indexObjectDatabases, "%s", "Failed to get database config from JSON file.");
	jsonResult										= "";
	const ::gpk::error_t								databaseArraySize			= ::gpk::jsonArraySize(*configReader[indexObjectDatabases]);
	gpk_necall(databaseArraySize, "%s", "Failed to get database count from config file.");
	char												temp[64]					= {};
	gpk_necall(databases.resize(databaseArraySize), "%s", "Out of memory?");
	::gpk::SLoadCache									loadCache;
	for(uint32_t iDatabase = 0, countDatabases = (uint32_t)databaseArraySize; iDatabase < countDatabases; ++iDatabase) {
		sprintf_s(temp, "['%u']", iDatabase);
		const int32_t										iJSONTable					=::gpk::jsonExpressionResolve(::gpk::view_const_string{temp}, configReader, indexObjectDatabases, jsonResult);
		gpk_necall(iJSONTable, "Failed to load config from json! Last contents found: %s.", jsonResult.begin());
		gpk_necall(::gpk::jsonExpressionResolve("name", configReader, iJSONTable, jsonResult), "Failed to load config from json! Last contents found: %s.", jsonResult.begin());
		::blt::TNamedBlitterDB								& jsonDB					= databases[iDatabase];
		jsonDB.Key										= jsonResult;
		gpk_necall(::table_description_load(jsonDB.Val.Description, configReader, iJSONTable, jsonDB.Key), "%s", "Unknown error");
		{	// -- Load database block size
			int32_t												indexBlockNode				= ::gpk::jsonExpressionResolve("block", configReader, iJSONTable, jsonResult);
			gwarn_if(errored(indexBlockNode), "Failed to load config from json! Last contents found: %s.", ::gpk::toString(jsonResult).begin())
			else
				::gpk::parseIntegerDecimal(jsonResult, &(jsonDB.Val.BlockSize = 0));
		}
		{	// -- Load database block size
			int32_t												indexKeyNode				= ::gpk::jsonExpressionResolve("key", configReader, iJSONTable, jsonResult);
			gwarn_if(errored(indexKeyNode), "Failed to load config from json! Last contents found: %s.", ::gpk::toString(jsonResult).begin())
			else
				jsonDB.Val.EncryptionKey = jsonResult;
		}
		{	// -- Load database modes (remote, deflate)
			info_printf("Loading database info for '%s'.", ::gpk::toString(jsonDB.Key).begin());
			jsonResult										= {};
			int32_t												typeFound					= ::gpk::jsonExpressionResolve("source", configReader, iJSONTable, jsonResult);
			gwarn_if(errored(typeFound), "Failed to load database type for database: %s. Defaulting to local.", ::gpk::toString(jsonDB.Key).begin());
			jsonDB.Val.HostType								= (::gpk::view_const_string{"local"} == jsonResult || errored(typeFound)) ? ::blt::DATABASE_HOST_LOCAL : ::blt::DATABASE_HOST_REMOTE;
			jsonResult										= {};
			typeFound										= ::gpk::jsonExpressionResolve("deflate", configReader, iJSONTable, jsonResult);
			gwarn_if(errored(typeFound), "Failed to load database compression for database: %s. Defaulting to uncompressed.", ::gpk::toString(jsonDB.Key).begin());
			if(::gpk::view_const_string{"true"} == jsonResult)
				jsonDB.Val.HostType								|= ::blt::DATABASE_HOST_DEFLATE;
		}
		{	// -- Load field bindings
			::gpk::error_t										indexBindArray				= ::gpk::jsonExpressionResolve("bind", configReader, iJSONTable, jsonResult);
			w_if(errored(indexBindArray), "No bindings found for database file: %s.", ::gpk::toString(jsonDB.Key).begin())
			else {
				::gpk::error_t										sizeBindArray				= ::gpk::jsonArraySize(*configReader[indexBindArray]);
				gpk_necall(sizeBindArray, "Cannot get size of array: %s.", "??");
				gpk_necall(jsonDB.Val.Bindings.resize(sizeBindArray), "%s", "Out of memory?");;
				for(uint32_t iBind = 0; iBind < jsonDB.Val.Bindings.size(); ++iBind) {
					sprintf_s(temp, "['%u']", iBind);
					gpk_necall(::gpk::jsonExpressionResolve(::gpk::view_const_string{temp}, configReader, indexBindArray, jsonResult), "Failed to load config from json! Last contents found: %s.", jsonResult.begin());
					jsonDB.Val.Bindings[iBind]						= jsonResult;
				}
			}
		}
		::gpk::array_pod<char_t>							dbfilename					= {};
		gpk_necall(::blt::tableFileName(dbfilename, jsonDB.Val.HostType, jsonDB.Val.EncryptionKey.size() > 0, jsonDB.Key), "%s", "??");
		if(gbit_true(jsonDB.Val.HostType, ::blt::DATABASE_HOST_REMOTE))
			continue;

		if(jsonDB.Val.BlockSize) {	// block databases get loaded on-demand. However, we need to know which blocks are available on disk in order to fill missing blocks with empty records.
			::gpk::view_const_string								extension;
			::dbFileExtension(jsonDB.Val.HostType, jsonDB.Val.EncryptionKey.size() > 0, extension);
			::gpk::array_pod<char_t>								folderName			= folder;
			gpk_necall(folderName.push_back('/'), "Failed to load database: %s. Out of memory?", dbfilename.begin());
			gpk_necall(::blt::tableFolderName(folderName, jsonDB.Key, jsonDB.Val.BlockSize), "Failed to load database: %s. Out of memory?", dbfilename.begin());
			::gpk::array_obj<::gpk::array_pod<char_t>>				blockFiles;
			if errored(::gpk::pathList({folderName.begin(), folderName.size()}, blockFiles))
				gpk_necall(::gpk::pathCreate(folderName), "Failed to load database: %s. Out of memory?", dbfilename.begin());

			for(uint32_t iFile = 0; iFile < blockFiles.size(); ++iFile) {
				const ::gpk::view_const_char							pathBlock			= blockFiles[iFile];
				if(pathBlock.size() >= extension.size()) {
					if(extension != ::gpk::view_const_string{&pathBlock[pathBlock.size() - extension.size()], extension.size()})
						continue;
				}
				const ::gpk::view_const_string							blockDigits			= {&pathBlock[folderName.size() + jsonDB.Key.size() + 2], pathBlock.size() - folderName.size() - extension.size() - jsonDB.Key.size() - 3};
				uint64_t												blockIndex			= 0;
				gpk_necall(::gpk::parseIntegerDecimal(blockDigits, &blockIndex), "Failed to load database: %s. Out of memory?", dbfilename.begin());
				bool													bAdd				= true;
				for(uint32_t i = 0; i < jsonDB.Val.BlocksOnDisk.size(); ++i)
					if(((uint32_t)blockIndex) < jsonDB.Val.BlocksOnDisk[i]) {
						gpk_necall(jsonDB.Val.BlocksOnDisk.insert(i, (uint32_t)blockIndex), "%s.", "Out of memory?");
						jsonDB.Val.MaxBlockOnDisk							= ::gpk::max(jsonDB.Val.MaxBlockOnDisk, (int32_t)blockIndex);
						bAdd												= false;
						break;
					}

				if(bAdd) {
					jsonDB.Val.MaxBlockOnDisk							= ::gpk::max(jsonDB.Val.MaxBlockOnDisk, (int32_t)blockIndex);
					gpk_necall(jsonDB.Val.BlocksOnDisk.push_back((uint32_t)blockIndex), "Failed to load database: %s. Out of memory?", dbfilename.begin());
				}
			}
			for(uint32_t i = 0; i < jsonDB.Val.BlocksOnDisk.size(); ++i)
				info_printf("Database block found for '%s': %u.", dbfilename.begin(), jsonDB.Val.BlocksOnDisk[i]);
			continue;
		}
		// -- Load json database file.
		if(0 == databasesToLoad.size()) {
			gpk_necall(::blt::tableFileLoad(loadCache, jsonDB, folder), "Failed to load database: %s.", dbfilename.begin());
		}
		else {
			for(uint32_t iDB = 0; iDB = databasesToLoad.size(); ++iDB)
				if(databasesToLoad[iDB] == jsonDB.Key) {
					gpk_necall(::blt::tableFileLoad(loadCache, jsonDB, folder), "Failed to load database: %s.", dbfilename.begin());
					break;
				}
		}
	}
	return 0;
}

static	::gpk::error_t							queryLoad						(::blt::SBlitterQuery& query, const ::gpk::view_array<const ::gpk::TKeyValConstString> keyvals, ::gpk::array_obj<::gpk::view_const_char> & expansionKeyStorage) {
	::gpk::keyvalNumeric("offset", keyvals, query.Range.Offset);
	if(0 > ::gpk::keyvalNumeric("limit", keyvals, query.Range.Count))
		::gpk::keyvalNumeric("count", keyvals, query.Range.Count);

	const ::gpk::error_t								indexExpand						= ::gpk::find("expand", keyvals);
	if(0 <= indexExpand) {
		gpk_necall(::gpk::split(::gpk::view_const_char{keyvals[indexExpand].Val}, '.', expansionKeyStorage), "%s", "Out of memory?");
		query.ExpansionKeys								= expansionKeyStorage;
	}
	return 0;
}

::gpk::error_t									blt::requestProcess				(::gpk::SExpressionReader & expressionReader, ::blt::SBlitterQuery & query, const ::gpk::SHTTPAPIRequest & request, ::gpk::array_obj<::gpk::view_const_char> & expansionKeyStorage)						{
	// --- Generate response
	{
		::gpk::array_obj<::gpk::view_const_char>			pathSplit						= {};
		::gpk::split(request.Path, '/', pathSplit);
		query.Database									= (0 < pathSplit.size()) ? pathSplit[0] : request.Path;
		query.Command									= (1 < pathSplit.size()) ? pathSplit[1] : ::gpk::view_const_char{};
		if(2 < pathSplit.size()) {
			// --- Retrieve detail part
			uint64_t											detail							= (uint64_t)-1LL;
			::gpk::view_const_char								strDetail						= {};
			strDetail										= pathSplit[pathSplit.size() - 1];
			if(strDetail.size()) {
				::gpk::parseIntegerDecimal(strDetail, &detail);
				query.Detail									= (int64_t)detail;
			}
		}
		info_printf("Parameters: \nDatabase: %s.\nCommand: %s.\nDetail: %llu.", ::gpk::toString(query.Database).begin(), ::gpk::toString(query.Command).begin(), query.Detail);
	}
	{ // retrieve path and database and read expression if any
		const ::gpk::error_t								indexPath						= ::gpk::find('.', query.Database);
		if(((uint32_t)indexPath + 1) < query.Database.size())
			query.Path										= {&query.Database[indexPath + 1]	, query.Database.size() - (indexPath + 1)};

		if(0 <= indexPath)
			query.Database									= {query.Database.begin()			, (uint32_t)indexPath};

		if(query.Path.size())
			gpk_necall(::gpk::expressionReaderParse(expressionReader, {query.Path.begin(), query.Path.size()}), "Error: %s", "Invalid path syntax?");
	}
	query.Record									= request.ContentBody;
	gpk_necall(::gpk::jsonParse(query.RecordReader, query.Record), "Failed to parse JSON record '%s'", ::gpk::toString(query.Record).begin());
	// --- Retrieve query data from querystring.
	gpk_necall(::queryLoad(query, request.QueryStringKeyVals, expansionKeyStorage), "%s", "Out of memory?");
	return 0;
}
