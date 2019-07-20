#include "blitter.h"

#include "gpk_stdstring.h"
#include "gpk_process.h"
#include "gpk_storage.h"

#include "gpk_json_expression.h"
#include "gpk_noise.h"
#include "gpk_parse.h"
#include "gpk_find.h"
#include "gpk_deflate.h"
#include "gpk_aes.h"


::gpk::error_t									blt::queryLoad				(::blt::SBlitterQuery& query, const ::gpk::view_array<const ::gpk::TKeyValConstString> keyvals)	{
	::gpk::keyvalNumeric("offset", keyvals, query.Range.Offset);
	if(0 > ::gpk::keyvalNumeric("limit", keyvals, query.Range.Count))
		::gpk::keyvalNumeric("count", keyvals, query.Range.Count);

	::gpk::error_t										indexExpand					= ::gpk::find("expand", keyvals);
	if(0 <= indexExpand) 
		query.Expand									= keyvals[indexExpand].Val;
	return 0;
}

::gpk::error_t									blt::blockFileName			(::gpk::array_pod<char_t> & filename, const ::gpk::view_const_string & dbName, bool bEncrypted, const ::blt::DATABASE_HOST hostType, const uint32_t block) {
	filename.append(dbName);
	char												temp[64]					= {};
	const ::gpk::view_const_string						extension					= bEncrypted
		? ((::blt::DATABASE_HOST_DEFLATE & hostType) ? "czon" : "cson")
		: ((::blt::DATABASE_HOST_DEFLATE & hostType) ? "zson" : "sson")
		;
	sprintf_s(temp, ".%u.%s", block, extension.begin());
	gpk_necall(filename.append(::gpk::view_const_string{temp}), "%s", "Out of memory?");
	return 0;
}

::gpk::error_t									blt::tableFolderName		(::gpk::array_pod<char_t> & foldername, const ::gpk::view_const_string & dbName, const uint32_t blockSize) {
	foldername.append(dbName);
	char												temp[64]					= {};
	sprintf_s(temp, ".%u.db", blockSize);
	gpk_necall(foldername.append(::gpk::view_const_string{temp}), "%s", "Out of memory?");
	return 0;
}

::gpk::error_t									blt::tableFileName			(::gpk::array_pod<char_t> & filename, const ::blt::DATABASE_HOST & hostType, bool bEncrypted, const ::gpk::view_const_string & jsonDBKey) {
	filename.append(jsonDBKey);
	char												temp[64]					= {};
	const ::gpk::view_const_string						extension					= bEncrypted
		? ((::blt::DATABASE_HOST_DEFLATE & hostType) ? "czon" : "cson")
		: ((::blt::DATABASE_HOST_DEFLATE & hostType) ? "zson" : "json")
		;
	sprintf_s(temp, ".%s", extension.begin());
	gpk_necall(filename.append(::gpk::view_const_string{temp}), "%s", "Out of memory?");
	return 0;
}

::gpk::error_t									blt::tableFileLoad			(::blt::TKeyValBlitterDB & jsonDB, const ::gpk::view_const_string & folder)	{
	::gpk::array_pod<char_t>							fileName					= folder;
	gpk_necall(fileName.push_back('/')		, "%s", "Out of memory?");
	gpk_necall(::blt::tableFileName(fileName, jsonDB.Val.HostType, jsonDB.Val.EncryptionKey.size() > 0,jsonDB.Key), "%s", "Out of memory?");
	info_printf("Loading json file: %s.", fileName.begin());
	const int32_t										idxBlock					= jsonDB.Val.Blocks.push_back({});
	gpk_necall(idxBlock, "%s", "Out of memory?");
	gpk_necall(jsonDB.Val.Offsets.push_back(0), "%s", "Out of memory?");
	if(0 == jsonDB.Val.EncryptionKey.size() && gbit_false(jsonDB.Val.HostType, ::blt::DATABASE_HOST_DEFLATE)) {
		gpk_necall(::gpk::fileToMemory({fileName.begin(), fileName.size()}, jsonDB.Val.Blocks[idxBlock].Bytes), "Failed to load file: '%s'", fileName.begin());
		return ::gpk::jsonParse(jsonDB.Val.Blocks[idxBlock].Reader, {jsonDB.Val.Blocks[idxBlock].Bytes.begin(), jsonDB.Val.Blocks[idxBlock].Bytes.size()});
	}
	else if(0 == jsonDB.Val.EncryptionKey.size() && gbit_true(jsonDB.Val.HostType, ::blt::DATABASE_HOST_DEFLATE)) {
		::gpk::array_pod<char_t>							deflated;
		gpk_necall(::gpk::fileToMemory({fileName.begin(), fileName.size()}, deflated), "Failed to load file: '%s'", fileName.begin());
		gpk_necall(::gpk::arrayDeflate(deflated, jsonDB.Val.Blocks[idxBlock].Bytes), "Failed to decompress file: '%s'", fileName.begin());
		return ::gpk::jsonParse(jsonDB.Val.Blocks[idxBlock].Reader, {jsonDB.Val.Blocks[idxBlock].Bytes.begin(), jsonDB.Val.Blocks[idxBlock].Bytes.size()});
	}
	else if(0  < jsonDB.Val.EncryptionKey.size() && gbit_true(jsonDB.Val.HostType, ::blt::DATABASE_HOST_DEFLATE)) {
		::gpk::array_pod<char_t>							encrypted;
		gpk_necall(::gpk::fileToMemory({fileName.begin(), fileName.size()}, encrypted), "Failed to load file: '%s'", fileName.begin());
		::gpk::array_pod<char_t>							deflated;
		gpk_necall(::gpk::aesDecode(encrypted, jsonDB.Val.EncryptionKey, ::gpk::AES_LEVEL_256, deflated), "Failed to decompress file: '%s'", fileName.begin());
		gpk_necall(::gpk::arrayDeflate(deflated, jsonDB.Val.Blocks[idxBlock].Bytes), "Failed to decompress file: '%s'", fileName.begin());
		return ::gpk::jsonParse(jsonDB.Val.Blocks[idxBlock].Reader, {jsonDB.Val.Blocks[idxBlock].Bytes.begin(), jsonDB.Val.Blocks[idxBlock].Bytes.size()});
	}
	else if(0  < jsonDB.Val.EncryptionKey.size() && gbit_false(jsonDB.Val.HostType, ::blt::DATABASE_HOST_DEFLATE)) {
		::gpk::array_pod<char_t>							encrypted;
		gpk_necall(::gpk::fileToMemory({fileName.begin(), fileName.size()}, encrypted), "Failed to load file: '%s'", fileName.begin());
		gpk_necall(::gpk::aesDecode(encrypted, jsonDB.Val.EncryptionKey, ::gpk::AES_LEVEL_256, jsonDB.Val.Blocks[idxBlock].Bytes), "Failed to decompress file: '%s'", fileName.begin());
		return ::gpk::jsonParse(jsonDB.Val.Blocks[idxBlock].Reader, {jsonDB.Val.Blocks[idxBlock].Bytes.begin(), jsonDB.Val.Blocks[idxBlock].Bytes.size()});
	}
	return 0;
}

::gpk::error_t									crcVerifyAndRemove			(::gpk::array_pod<byte_t> & bytes)	{
	ree_if(bytes.size() < 8, "Invalid input. No CRC can be found in an array of %u bytes.", bytes.size());
	uint64_t											check						= 0;
	const uint64_t										found						= (bytes.size() >= 8) ? *(uint64_t*)&bytes[bytes.size() - 8] : (uint64_t)-1LL;
	for(uint32_t iByte = 0; iByte < bytes.size(); ++iByte)
		check											= ::gpk::noise1DBase(bytes[iByte], ::blt::CRC_SEED);
	ree_if(check != found, "CRC Check failed: Stored: %llu. Calculated: :llu.", );
	bytes.resize(bytes.size() - 8);
	return 0;
}

::gpk::error_t									blt::blockFileLoad			(::blt::TKeyValBlitterDB & jsonDB, uint32_t block)	{
	ginfo_if(0 <= ::gpk::find(block, {jsonDB.Val.BlockIndices.begin(), jsonDB.Val.BlockIndices.size()}), "Block already loaded: %u.", block);
	::gpk::array_pod<char_t>							fileName					= {};
	gpk_necall(::blt::tableFolderName(fileName, jsonDB.Key, jsonDB.Val.BlockSize), "%s", "Out of memory?");
	fileName.push_back('/');
	gpk_necall(::blt::blockFileName(fileName, jsonDB.Key, jsonDB.Val.EncryptionKey.size() > 0, jsonDB.Val.HostType, block), "%s", "Out of memory?");

	const int32_t										idxBlock					= jsonDB.Val.Blocks.push_back({});
	gpk_necall(idxBlock, "%s", "Out of memory?");
	gpk_necall(jsonDB.Val.BlockIndices.push_back(idxBlock), "%s", "Out of memory?");
	gpk_necall(jsonDB.Val.Offsets.push_back(jsonDB.Val.BlockSize ? block / jsonDB.Val.BlockSize : 0), "");
	::gpk::SJSONFile									& dbBlock					= jsonDB.Val.Blocks[idxBlock];
	if(0 == jsonDB.Val.EncryptionKey.size()) {
		if(gbit_false(jsonDB.Val.HostType, ::blt::DATABASE_HOST_DEFLATE)) {
			info_printf("Loading sson file: %s.", fileName.begin());
			gpk_necall(::gpk::fileToMemory({fileName.begin(), fileName.size()}, dbBlock.Bytes), "Failed to load file: '%s'", fileName.begin());
			gpk_necall(::crcVerifyAndRemove(dbBlock.Bytes), "CRC check failed: %s.", "??");
			return ::gpk::jsonParse(dbBlock.Reader, {dbBlock.Bytes.begin(), dbBlock.Bytes.size()});
		}
		else {
			info_printf("Loading json file: %s.", fileName.begin());
			gpk_necall(::gpk::fileToMemory({fileName.begin(), fileName.size()}, dbBlock.Bytes), "Failed to load file: '%s'", fileName.begin());
			gpk_necall(::crcVerifyAndRemove(dbBlock.Bytes), "CRC check failed: %s.", "??");
			return ::gpk::jsonParse(dbBlock.Reader, {dbBlock.Bytes.begin(), dbBlock.Bytes.size()});
		}
	}
	else {
	
	}
	return idxBlock;
}


::gpk::error_t									blt::configDatabases		(::gpk::array_obj<::blt::TKeyValBlitterDB> & databases, const ::gpk::SJSONReader & configReader, const int32_t indexConfigNode, const ::gpk::view_array<const ::gpk::view_const_string> & databasesToLoad, const ::gpk::view_const_string & folder)	{
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
	for(uint32_t iDatabase = 0, countDatabases = (uint32_t)databaseArraySize; iDatabase < countDatabases; ++iDatabase) {
		sprintf_s(temp, "[%u].name", iDatabase);
		gpk_necall(::gpk::jsonExpressionResolve(temp, configReader, indexObjectDatabases, jsonResult), "Failed to load config from json! Last contents found: %s.", jsonResult.begin());
		::blt::TKeyValBlitterDB								& jsonDB					= databases[iDatabase];
		jsonDB.Key										= jsonResult;
		{	// -- Load database block size
			sprintf_s(temp, "[%u].block", iDatabase);
			int32_t												indexBlockNode				= ::gpk::jsonExpressionResolve(temp, configReader, indexObjectDatabases, jsonResult);
			gwarn_if(errored(indexBlockNode), "Failed to load config from json! Last contents found: %s.", jsonResult.begin()) 
			else {
				::gpk::parseIntegerDecimal(jsonResult, &(jsonDB.Val.BlockSize = 0));
			}
		}
		::gpk::array_pod<char_t>							dbfilename					= {};
		gpk_necall(::blt::tableFileName(dbfilename, jsonDB.Val.HostType, jsonDB.Val.EncryptionKey.size() > 0, jsonDB.Key), "%s", "??");
		{	// -- Load database modes (remote, deflate)
			sprintf_s(temp, "[%u].source", iDatabase);
			jsonResult										= {};
			int32_t												typeFound					= ::gpk::jsonExpressionResolve(temp, configReader, indexObjectDatabases, jsonResult);
			gwarn_if(errored(typeFound), "Failed to load database type for database: %s. Defaulting to local.", dbfilename.begin());
			jsonDB.Val.HostType								= (::gpk::view_const_string{"local"} == jsonResult || errored(typeFound)) ? ::blt::DATABASE_HOST_LOCAL : ::blt::DATABASE_HOST_REMOTE;
			sprintf_s(temp, "[%u].deflate", iDatabase);
			jsonResult										= {};
			typeFound										= ::gpk::jsonExpressionResolve(temp, configReader, indexObjectDatabases, jsonResult);
			gwarn_if(errored(typeFound), "Failed to load database compression for database: %s. Defaulting to uncompressed.", dbfilename.begin());
			if(::gpk::view_const_string{"true"} == jsonResult)
				jsonDB.Val.HostType								|= ::blt::DATABASE_HOST_DEFLATE;
		}
		{	// -- Load field bindings
			sprintf_s(temp, "[%u].bind", iDatabase);
			::gpk::error_t										indexBindArray				= ::gpk::jsonExpressionResolve(temp, configReader, indexObjectDatabases, jsonResult);
			w_if(errored(indexBindArray), "No bindings found for database file: %s.", dbfilename.begin())
			else {
				::gpk::error_t										sizeBindArray				= ::gpk::jsonArraySize(*configReader[indexBindArray]);
				gpk_necall(jsonDB.Val.Bindings.resize(sizeBindArray), "%s", "Out of memory?");;
				for(uint32_t iBind = 0; iBind < jsonDB.Val.Bindings.size(); ++iBind) {
					sprintf_s(temp, "[%u]", iBind);
					gpk_necall(::gpk::jsonExpressionResolve(temp, configReader, indexBindArray, jsonResult), "Failed to load config from json! Last contents found: %s.", jsonResult.begin());
					jsonDB.Val.Bindings[iBind]						= jsonResult;
				}
			}
		}
		if(::blt::DATABASE_HOST_LOCAL != jsonDB.Val.HostType) 
			continue;
		if(jsonDB.Val.BlockSize)
			continue;	// block databases get loaded on-demand
		// -- Load json database file.
		if(0 == databasesToLoad.size()) {
			gpk_necall(::blt::tableFileLoad(jsonDB, folder), "Failed to load database: %s.", dbfilename.begin());
			//gpk_necall(::gpk::jsonFileRead(jsonDB.Val.Table, {dbfilename.begin(), dbfilename.size()}), "Failed to load database: %s.", dbfilename.begin());
		}
		else {
			for(uint32_t iDB = 0; iDB = databasesToLoad.size(); ++iDB) 
				if(databasesToLoad[iDB] == jsonDB.Key) {
					gpk_necall(::blt::tableFileLoad(jsonDB, folder), "Failed to load database: %s.", dbfilename.begin());
					break;
				}
		}
	}
	return 0;
}

::gpk::error_t									blt::loadConfig			(::blt::SBlitter & appState, const ::gpk::view_const_string & jsonFileName)	{
	gpk_necall(::gpk::jsonFileRead(appState.Config, jsonFileName), "Failed to load configuration file: %s.", jsonFileName.begin());
	const ::gpk::error_t								indexApp				= ::gpk::jsonExpressionResolve("application.blitter", appState.Config.Reader, (uint32_t)0, appState.Folder);
	const ::gpk::error_t								indexDB					= ::gpk::jsonExpressionResolve("database", appState.Config.Reader, (uint32_t)indexApp, appState.Folder);
	gpk_necall(indexDB, "'database' not found in '%s'.", "application.blitter");
	const ::gpk::error_t								indexPath				= ::gpk::jsonExpressionResolve("path.database", appState.Config.Reader, (uint32_t)indexApp, appState.Folder);
	gwarn_if(errored(indexPath), "'path.database' not found in '%s'. Using current path as database root.", "application.blitter");
	gpk_necall(::blt::configDatabases(appState.Databases, appState.Config.Reader, indexDB, {}, appState.Folder), "%s", "Failed to load query.");
	return 0;
}
