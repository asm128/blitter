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

::gpk::error_t									blt::loadConfig				(::blt::SBlitter & appState, const ::gpk::view_const_string & jsonFileName)	{
	gpk_necall(::gpk::jsonFileRead(appState.Config, jsonFileName), "Failed to load configuration file: %s.", ::gpk::toString(jsonFileName).begin());
	const ::gpk::error_t								indexApp					= ::gpk::jsonExpressionResolve("application.blitter", appState.Config.Reader, (uint32_t)0, appState.Folder);
	const ::gpk::error_t								indexDB						= ::gpk::jsonExpressionResolve("database", appState.Config.Reader, (uint32_t)indexApp, appState.Folder);
	gpk_necall(indexDB, "'database' not found in '%s'.", "application.blitter");
	const ::gpk::error_t								indexPath					= ::gpk::jsonExpressionResolve("path.database", appState.Config.Reader, (uint32_t)indexApp, appState.Folder);
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

::gpk::error_t									blt::blockFileName			(::gpk::array_pod<char_t> & filename, const ::gpk::view_const_string & dbName, bool bEncrypted, const ::blt::DATABASE_HOST hostType, const uint32_t block) {
	gpk_necall(filename.append(dbName), "%s", "Out of memory?");
	char												temp[64]					= {};
	::gpk::view_const_string							extension					= {};
	::dbFileExtension(hostType, bEncrypted, extension);
	sprintf_s(temp, ".%u.%s", block, extension.begin());
	gpk_necall(filename.append(::gpk::view_const_string{temp}), "%s", "Out of memory?");
	return 0;
}

::gpk::error_t									blt::tableFolderName		(::gpk::array_pod<char_t> & foldername, const ::gpk::view_const_string & dbName, const uint32_t blockSize) {
	gpk_necall(foldername.append(dbName), "%s", "Out of memory?");
	char												temp[128]					= {};
	sprintf_s(temp, ".%u.db", blockSize);
	gpk_necall(foldername.append(::gpk::view_const_string{temp}), "%s", "Out of memory?");
	return 0;
}

::gpk::error_t									blt::tableFileName			(::gpk::array_pod<char_t> & filename, const ::blt::DATABASE_HOST & hostType, bool bEncrypted, const ::gpk::view_const_string & jsonDBKey) {
	gpk_necall(filename.append(jsonDBKey), "%s", "Out of memory?");
	char												temp[64]					= {};
	::gpk::view_const_string							extension					= {};
	::dbFileExtension(hostType, bEncrypted, extension);
	sprintf_s(temp, ".%s", extension.begin());
	gpk_necall(filename.append(::gpk::view_const_string{temp}), "%s", "Out of memory?");
	return 0;
}

::gpk::error_t									crcVerifyAndRemove			(::gpk::array_pod<byte_t> & bytes)	{
	ree_if(bytes.size() < 8, "Invalid input. No CRC can be found in an array of %u bytes.", bytes.size());
	uint64_t											check						= 0;
	const uint32_t										startOfCRC					= bytes.size() - 8;
	const uint64_t										found						= (bytes.size() >= 8) ? *(uint64_t*)&bytes[startOfCRC] : (uint64_t)-1LL;
	for(uint32_t iByte = 0; iByte < startOfCRC; ++iByte)
		check											+= ::gpk::noise1DBase(bytes[iByte], ::blt::CRC_SEED);
	ree_if(check != found, "CRC Check failed: Stored: %llu. Calculated: %llu.", found, check);
	gpk_necall(bytes.resize(bytes.size() - 8), "%s", "Out of memory?");
	return 0;
}

static	::gpk::error_t							dbFileLoad					(::blt::SLoadCache & loadCache, ::blt::TNamedBlitterDB & jsonDB, const ::gpk::view_const_string & fileName, uint32_t idBlock)	{
	const int32_t										idxBlock					= jsonDB.Val.Blocks.push_back({});
	gpk_necall(idxBlock, "%s", "Out of memory?");
	gpk_necall(jsonDB.Val.BlockIndices.push_back(idBlock), "%s", "Out of memory?");
	gpk_necall(jsonDB.Val.Offsets.push_back(idBlock * (int64_t)jsonDB.Val.BlockSize), "%s", "Out of memory?");
	jsonDB.Val.Blocks[idxBlock].create();
	::gpk::SJSONFile									& dbBlock					= *jsonDB.Val.Blocks[idxBlock];
	loadCache										= {};
	if(gbit_false(jsonDB.Val.HostType, ::blt::DATABASE_HOST_DEFLATE)) {
		if(0 == jsonDB.Val.EncryptionKey.size())
			gpk_necall(::gpk::fileToMemory({fileName.begin(), fileName.size()}, dbBlock.Bytes), "Failed to load file: '%s'", fileName.begin());
		else {
			::gpk::array_pod<char_t>							& encrypted					= loadCache.Encrypted;
			gpk_necall(::gpk::fileToMemory({fileName.begin(), fileName.size()}, encrypted), "Failed to load file: '%s'", fileName.begin());
			gpk_necall(::gpk::aesDecode(encrypted, jsonDB.Val.EncryptionKey, ::gpk::AES_LEVEL_256, dbBlock.Bytes), "Failed to decompress file: '%s'", fileName.begin());
		}
	}
	else {
		::gpk::array_pod<char_t>							& deflated					= loadCache.Deflated;
		if(0 == jsonDB.Val.EncryptionKey.size()) {
			gpk_necall(::gpk::fileToMemory({fileName.begin(), fileName.size()}, deflated), "Failed to load file: '%s'", fileName.begin());
			gpk_necall(::gpk::arrayInflate({&deflated[sizeof(uint32_t)], deflated.size() - sizeof(uint32_t)}, dbBlock.Bytes), "Failed to decompress file: '%s'", fileName.begin());
		}
		else {
			::gpk::array_pod<char_t>							& encrypted					= loadCache.Encrypted;
			gpk_necall(::gpk::fileToMemory({fileName.begin(), fileName.size()}, encrypted), "Failed to load file: '%s'", fileName.begin());
			gpk_necall(::gpk::aesDecode(encrypted, jsonDB.Val.EncryptionKey, ::gpk::AES_LEVEL_256, deflated), "Failed to decompress file: '%s'", fileName.begin());
			gpk_necall(::gpk::arrayInflate({&deflated[sizeof(uint32_t)], deflated.size() - sizeof(uint32_t)}, dbBlock.Bytes), "Failed to decompress file: '%s'", fileName.begin());
		}
	}
	gpk_necall(::crcVerifyAndRemove(dbBlock.Bytes), "CRC check failed: %s.", "??");
	gpk_necall(::gpk::jsonParse(dbBlock.Reader, {dbBlock.Bytes.begin(), dbBlock.Bytes.size()}), "Failed to read configuration: %s.", "??");
	return idxBlock;
}

::gpk::error_t									blt::tableFileLoad			(::blt::SLoadCache & loadCache, ::blt::TNamedBlitterDB & jsonDB, const ::gpk::view_const_string & folder)	{
	rni_if(jsonDB.Val.Blocks.size() && jsonDB.Val.Blocks[0]->Bytes.size(), "Table already loaded: '%s'.", ::gpk::toString(jsonDB.Key).begin());
	::gpk::array_pod<char_t>							fileName					= folder;
	gpk_necall(fileName.push_back('/')		, "%s", "Out of memory?");
	gpk_necall(::blt::tableFileName(fileName, jsonDB.Val.HostType, jsonDB.Val.EncryptionKey.size() > 0,jsonDB.Key), "%s", "Out of memory?");
	info_printf("Loading database file: %s.", ::gpk::toString({fileName.begin(), fileName.size()}).begin());
	return ::dbFileLoad(loadCache, jsonDB, {fileName.begin(), fileName.size()}, 0);
}

::gpk::error_t									blt::blockFileLoad			(::blt::SLoadCache & loadCache, ::blt::TNamedBlitterDB & jsonDB, const ::gpk::view_const_string & folder, uint32_t block)	{
	{
		::gpk::error_t										blockIndex					= ::gpk::find(block, {jsonDB.Val.BlockIndices.begin(), jsonDB.Val.BlockIndices.size()});
		rvi_if(blockIndex, 0 <= blockIndex, "Block already loaded for database '%s': %u.", ::gpk::toString(jsonDB.Key).begin(), block);
	}
	gpk_necall(::gpk::find(block, ::gpk::view_const_uint32{jsonDB.Val.BlocksOnDisk}), "Block %u doesn't exist.", block);
	::gpk::array_pod<char_t>							fileName					= folder;
	gpk_necall(fileName.push_back('/'), "%s", "Out of memory?");
	gpk_necall(::blt::tableFolderName(fileName, jsonDB.Key, jsonDB.Val.BlockSize), "%s", "Out of memory?");
	gpk_necall(fileName.push_back('/'), "%s", "Out of memory?");
	gpk_necall(::blt::blockFileName(fileName, jsonDB.Key, jsonDB.Val.EncryptionKey.size() > 0, jsonDB.Val.HostType, block), "%s", "Out of memory?");
	info_printf("Loading block file: %s.", ::gpk::toString({fileName.begin(), fileName.size()}).begin());
	return ::dbFileLoad(loadCache, jsonDB, {fileName.begin(), fileName.size()}, block);
}

::gpk::error_t									blt::configDatabases		(::gpk::array_obj<::blt::TNamedBlitterDB> & databases, const ::gpk::SJSONReader & configReader, const int32_t indexConfigNode, const ::gpk::view_array<const ::gpk::view_const_string> & databasesToLoad, const ::gpk::view_const_string & folder)	{
	::gpk::view_const_string							jsonResult					= {};
	const int32_t										indexObjectDatabases		= (-1 == indexConfigNode)
		? ::gpk::jsonExpressionResolve("application.blitter.database", configReader, 0, jsonResult)
		: indexConfigNode
		;
	gpk_necall(indexObjectDatabases, "%s", "Failed to get database config from JSON file.");
	jsonResult										= "";
	const ::gpk::error_t								databaseArraySize			= ::gpk::jsonArraySize(*configReader[indexObjectDatabases]);
	gpk_necall(databaseArraySize, "%s", "Failed to get database count from config file.");
	char												temp[64]					= {};
	gpk_necall(databases.resize(databaseArraySize), "%s", "Out of memory?");
	::blt::SLoadCache									loadCache;
	for(uint32_t iDatabase = 0, countDatabases = (uint32_t)databaseArraySize; iDatabase < countDatabases; ++iDatabase) {
		sprintf_s(temp, "['%u'].name", iDatabase);
		gpk_necall(::gpk::jsonExpressionResolve(temp, configReader, indexObjectDatabases, jsonResult), "Failed to load config from json! Last contents found: %s.", jsonResult.begin());
		::blt::TNamedBlitterDB								& jsonDB					= databases[iDatabase];
		jsonDB.Key										= jsonResult;
		{	// -- Load database block size
			sprintf_s(temp, "['%u'].block", iDatabase);
			int32_t												indexBlockNode				= ::gpk::jsonExpressionResolve(temp, configReader, indexObjectDatabases, jsonResult);
			gwarn_if(errored(indexBlockNode), "Failed to load config from json! Last contents found: %s.", jsonResult.begin())
			else
				::gpk::parseIntegerDecimal(jsonResult, &(jsonDB.Val.BlockSize = 0));
		}
		::gpk::array_pod<char_t>							dbfilename					= {};
		gpk_necall(::blt::tableFileName(dbfilename, jsonDB.Val.HostType, jsonDB.Val.EncryptionKey.size() > 0, jsonDB.Key), "%s", "??");
		{	// -- Load database modes (remote, deflate)
			info_printf("Loading database info for '%s'.", dbfilename.begin());
			sprintf_s(temp, "['%u'].source", iDatabase);
			jsonResult										= {};
			int32_t												typeFound					= ::gpk::jsonExpressionResolve(temp, configReader, indexObjectDatabases, jsonResult);
			gwarn_if(errored(typeFound), "Failed to load database type for database: %s. Defaulting to local.", dbfilename.begin());
			jsonDB.Val.HostType								= (::gpk::view_const_string{"local"} == jsonResult || errored(typeFound)) ? ::blt::DATABASE_HOST_LOCAL : ::blt::DATABASE_HOST_REMOTE;
			sprintf_s(temp, "['%u'].deflate", iDatabase);
			jsonResult										= {};
			typeFound										= ::gpk::jsonExpressionResolve(temp, configReader, indexObjectDatabases, jsonResult);
			gwarn_if(errored(typeFound), "Failed to load database compression for database: %s. Defaulting to uncompressed.", dbfilename.begin());
			if(::gpk::view_const_string{"true"} == jsonResult)
				jsonDB.Val.HostType								|= ::blt::DATABASE_HOST_DEFLATE;
		}
		{	// -- Load field bindings
			sprintf_s(temp, "['%u'].bind", iDatabase);
			::gpk::error_t										indexBindArray				= ::gpk::jsonExpressionResolve(temp, configReader, indexObjectDatabases, jsonResult);
			w_if(errored(indexBindArray), "No bindings found for database file: %s.", dbfilename.begin())
			else {
				::gpk::error_t										sizeBindArray				= ::gpk::jsonArraySize(*configReader[indexBindArray]);
				gpk_necall(sizeBindArray, "Cannot get size of array: %s.", "??");
				gpk_necall(jsonDB.Val.Bindings.resize(sizeBindArray), "%s", "Out of memory?");;
				for(uint32_t iBind = 0; iBind < jsonDB.Val.Bindings.size(); ++iBind) {
					sprintf_s(temp, "['%u']", iBind);
					gpk_necall(::gpk::jsonExpressionResolve(temp, configReader, indexBindArray, jsonResult), "Failed to load config from json! Last contents found: %s.", jsonResult.begin());
					jsonDB.Val.Bindings[iBind]						= jsonResult;
				}
			}
		}
		if(gbit_true(jsonDB.Val.HostType, ::blt::DATABASE_HOST_REMOTE))
			continue;

		if(jsonDB.Val.BlockSize) {	// block databases get loaded on-demand. However, we need to know which blocks are available on disk in order to fill missing blocks with empty records.
			::gpk::view_const_string								extension;
			::dbFileExtension(jsonDB.Val.HostType, jsonDB.Val.EncryptionKey.size() > 0, extension);
			::gpk::array_pod<char_t>								folderName			= folder;
			gpk_necall(folderName.push_back('/'), "Failed to load database: %s. Out of memory?", dbfilename.begin());
			gpk_necall(::blt::tableFolderName(folderName, jsonDB.Key, jsonDB.Val.BlockSize), "Failed to load database: %s. Out of memory?", dbfilename.begin());
			::gpk::array_obj<::gpk::array_pod<char_t>>				blockFiles;
			gpk_necall(::gpk::pathList({folderName.begin(), folderName.size()}, blockFiles), "Failed to load database: %s. Out of memory?", dbfilename.begin());
			for(uint32_t iFile = 0; iFile < blockFiles.size(); ++iFile) {
				const ::gpk::view_const_char							pathBlock			= blockFiles[iFile];
				if(pathBlock.size() >= extension.size()) {
					if(extension != ::gpk::view_const_string{&pathBlock[pathBlock.size() - extension.size()], extension.size()})
						continue;
				}
				const ::gpk::view_const_string							blockDigits			= {&pathBlock[folderName.size() + jsonDB.Key.size() + 2], pathBlock.size() - folderName.size() - extension.size() - jsonDB.Key.size() - 3};
				uint64_t												blockIndex			= 0;
				gpk_necall(::gpk::parseIntegerDecimal(blockDigits, &blockIndex), "Failed to load database: %s. Out of memory?", dbfilename.begin());
				bool												bAdd				= true;
				for(uint32_t i = 0; i < jsonDB.Val.BlocksOnDisk.size(); ++i)
					if(((uint32_t)blockIndex) < jsonDB.Val.BlocksOnDisk[i]) {
						gpk_necall(jsonDB.Val.BlocksOnDisk.insert(i, (uint32_t)blockIndex), "%s.", "Out of memory?");
						bAdd											= false;
						break;
					}
				if(bAdd)
					gpk_necall(jsonDB.Val.BlocksOnDisk.push_back((uint32_t)blockIndex), "Failed to load database: %s. Out of memory?", dbfilename.begin());
			}
			for(uint32_t i = 0; i < jsonDB.Val.BlocksOnDisk.size(); ++i)
				info_printf("Database block found for '%s': %u.", dbfilename.begin(), jsonDB.Val.BlocksOnDisk[i]);
			continue;
		}
		// -- Load json database file.
		if(0 == databasesToLoad.size())
			gpk_necall(::blt::tableFileLoad(loadCache, jsonDB, folder), "Failed to load database: %s.", dbfilename.begin());
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

static	::gpk::error_t							queryLoad						(::blt::SBlitterQuery& query, const ::gpk::view_array<const ::gpk::TKeyValConstString> keyvals, ::gpk::array_obj<::gpk::view_const_string> & expansionKeyStorage) {
	::gpk::keyvalNumeric("offset", keyvals, query.Range.Offset);
	if(0 > ::gpk::keyvalNumeric("limit", keyvals, query.Range.Count))
		::gpk::keyvalNumeric("count", keyvals, query.Range.Count);

	const ::gpk::error_t								indexExpand						= ::gpk::find("expand", keyvals);
	if(0 <= indexExpand) {
		query.Expand									= keyvals[indexExpand].Val;
		gpk_necall(::gpk::split(query.Expand, '.', expansionKeyStorage), "%s", "Out of memory?");
		query.ExpansionKeys								= expansionKeyStorage;
	}
	return 0;
}

::gpk::error_t									blt::requestProcess				(::gpk::SExpressionReader & expressionReader, ::blt::SBlitterQuery & query, const ::gpk::SHTTPAPIRequest & request, ::gpk::array_obj<::gpk::view_const_string> & expansionKeyStorage)						{
	// --- Generate response
	query.Database									= (request.Path.size() > 1)
		? (('/' == request.Path[0]) ? ::gpk::view_const_string{&request.Path[1], request.Path.size() - 1} : ::gpk::view_const_string{request.Path.begin(), request.Path.size()})
		: ::gpk::view_const_string{}
		;
	{	// --- Retrieve detail part
		uint64_t											detail							= (uint64_t)-1LL;
		::gpk::view_const_string							strDetail						= {};
		const ::gpk::error_t								indexOfLastBar					= ::gpk::rfind('/', query.Database);
		const uint32_t										startOfDetail					= (uint32_t)(indexOfLastBar + 1);
		if(indexOfLastBar > 0 && startOfDetail < query.Database.size()) {
			strDetail										= {&query.Database[startOfDetail], query.Database.size() - startOfDetail};
			query.Database									= {query.Database.begin(), (uint32_t)indexOfLastBar};
			if(strDetail.size()) {
				::gpk::stoull(strDetail, &detail);
				query.Detail									= (int64_t)detail;
			}
		}
	}
	{ // retrieve path and database and read expression if any
		const ::gpk::error_t								indexPath						= ::gpk::find('.', query.Database);
		if(((uint32_t)indexPath + 1) < query.Database.size())
			query.Path										= {&query.Database[indexPath + 1]	, query.Database.size() - (indexPath + 1)};

		if(0 <= indexPath)
			query.Database									= {query.Database.begin()			, (uint32_t)indexPath};

		if(query.Path.size())
			gpk_necall(::gpk::expressionReaderParse(expressionReader, query.Path), "Error: %s", "Invalid path syntax?");
	}

	// --- Retrieve query data from querystring.
	gpk_necall(::queryLoad(query, request.QueryStringKeyVals, expansionKeyStorage), "%s", "Out of memory?");
	return 0;
}
