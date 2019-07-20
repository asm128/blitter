#define GPK_CONSOLE_LOG_ENABLED
#define GPK_ERROR_PRINTF_ENABLED
#define GPK_WARNING_PRINTF_ENABLED
#define GPK_INFO_PRINTF_ENABLED

#include "gpk_storage.h"
#include "gpk_json.h"
#include "gpk_parse.h"
#include "gpk_find.h"
#include "gpk_deflate.h"
#include "gpk_aes.h"
#include "gpk_noise.h"

#include "blitter.h"

static constexpr const uint32_t			DEFAULT_BLOCK_SIZE				= 1024;

struct SSplitParams {
	::gpk::view_const_string				FileNameSrc						= {};	// First parameter is the only parameter, which is the name of the source file to be split.
	::gpk::view_const_string				PathWithoutExtension			= {};	// First parameter is the only parameter, which is the name of the source file to be split.
	::gpk::view_const_string				DBName							= {};	// First parameter is the only parameter, which is the name of the source file to be split.
	::gpk::view_const_string				EncryptionKey					= {};	// First parameter is the only parameter, which is the name of the source file to be split.

	bool									DeflatedOutput					= false;
};

::gpk::error_t							loadParams						(SSplitParams& params, int argc, char ** argv)		{
	for(int32_t iArg = 5; iArg < argc; ++iArg)
		info_printf("Unknown parameter: %s.", argv[iArg]);
	ree_if(2 > argc, "Usage:\n\t%s [filename] [blockSize] [deflated output (1:0)] [deflated input (1:0)] ", argv[0]);
	ree_if(65535 < argc, "Invalid parameter count: %u.", (uint32_t)argc);
	params.FileNameSrc						= {argv[1], (uint32_t)-1};	// First parameter is the only parameter, which is the name of the source file to be split.
	params.DeflatedOutput					= (argc > 2 && argv[2][0] != '0');
	if(argc > 3)
		params.EncryptionKey					= {argv[3], (uint32_t)-1};


	::gpk::error_t								indexOfDot						= ::gpk::rfind('.', params.FileNameSrc);
	::gpk::error_t								indexOfLastSlash				= ::gpk::findLastSlash(params.FileNameSrc);
	params.PathWithoutExtension				= (indexOfDot > indexOfLastSlash) ? ::gpk::view_const_string{params.FileNameSrc.begin(), (uint32_t)indexOfDot} : params.FileNameSrc;
	params.DBName							= (-1 == indexOfLastSlash) 
		? params.PathWithoutExtension
		: ::gpk::view_const_string{&params.PathWithoutExtension[indexOfLastSlash], params.PathWithoutExtension.size() - indexOfLastSlash}
		;	// First parameter is the only parameter, which is the name of the source file to be split.

	info_printf("Deflated output: %s", params.DeflatedOutput ? "true" : "false");
	if(params.EncryptionKey.size())
		info_printf("Encryption: %s.", "false");
	else
		info_printf("Encryption key: %s.", params.EncryptionKey.begin());
	return 0;
}

#define SEASON_ENABLE_VERIFICATION

struct SWriteCache {
	::gpk::array_pod<char_t>				PartFileName					= {};
	::gpk::array_pod<char_t>				PathToWriteTo					= {};
	::gpk::array_pod<char_t>				Deflated						= {};
	::gpk::array_pod<char_t>				Encrypted						= {};
#if defined SEASON_ENABLE_VERIFICATION
	::gpk::array_pod<char_t>				Verify							= {};
#endif
};

::gpk::error_t							writePart						(::SWriteCache & blockCache, const ::SSplitParams & params, ::gpk::array_pod<char_t> & partBytes)		{
	::gpk::array_pod<char_t>					& partFileName					= blockCache.PartFileName					;
	::gpk::array_pod<char_t>					& pathToWriteTo					= blockCache.PathToWriteTo					;
	::gpk::array_pod<char_t>					& deflated						= blockCache.Deflated						;
	::gpk::array_pod<char_t>					& encrypted						= blockCache.Encrypted						;
#if defined SEASON_ENABLE_VERIFICATION
	::gpk::array_pod<char_t>					& verify						= blockCache.Verify							;
#endif
	::gpk::clear(partFileName, pathToWriteTo, deflated, encrypted, verify);
	uint64_t									crcToStore						= 0;
	for(uint32_t i=0; i < partBytes.size(); ++i) 
		crcToStore								+= ::gpk::noise1DBase(partBytes[i], ::blt::CRC_SEED);

	gpk_necall(partBytes.append((char*)&crcToStore, sizeof(uint64_t)), "%s", "Out of memory?");;

	gpk_necall(::blt::tableFileName(partFileName, params.DeflatedOutput ? ::blt::DATABASE_HOST_DEFLATE : ::blt::DATABASE_HOST_LOCAL, params.EncryptionKey.size() > 0, params.DBName), "%s", "??");
	gpk_necall(pathToWriteTo.append(partFileName), "%s", "Out of memory?");
	if(false == params.DeflatedOutput) {
		if(0 == params.EncryptionKey.size()) {
			info_printf("Saving part file to disk: '%s'. Size: %u.", pathToWriteTo.begin(), partBytes.size());
			gpk_necall(::gpk::fileFromMemory({pathToWriteTo.begin(), pathToWriteTo.size()}, partBytes), "Failed to write file: %s.", pathToWriteTo.begin());
		}
		else {
			gpk_necall(::gpk::aesEncode({partBytes.begin(), partBytes.size()}, params.EncryptionKey, ::gpk::AES_LEVEL_256, encrypted), "Failed to encrypt file: %s.", pathToWriteTo.begin());
			info_printf("Saving part file to disk: '%s'. Size: %u.", pathToWriteTo.begin(), encrypted.size());
			gpk_necall(::gpk::fileFromMemory({pathToWriteTo.begin(), pathToWriteTo.size()}, encrypted), "Failed to write file: %s.", pathToWriteTo.begin());
#if defined SEASON_ENABLE_VERIFICATION
			gpk_necall(::gpk::aesDecode(encrypted, params.EncryptionKey, ::gpk::AES_LEVEL_256, verify), "Failed to inflate file: %s.", pathToWriteTo.begin());
			ree_if(verify != partBytes, "Sanity check failed: %s.", "??");
#endif
		}
	}
	else {
		gpk_necall(deflated.append((char*)&partBytes.size(), sizeof(uint32_t)), "%s", "Out of memory?");;
		gpk_necall(::gpk::arrayDeflate(partBytes, deflated), "Failed to deflate file: %s.", pathToWriteTo.begin());
		if(0 == params.EncryptionKey.size()) {
			info_printf("Saving part file to disk: '%s'. Size: %u.", pathToWriteTo.begin(), deflated.size());
			gpk_necall(::gpk::fileFromMemory({pathToWriteTo.begin(), pathToWriteTo.size()}, deflated), "Failed to write file: %s.", pathToWriteTo.begin());
#if defined SEASON_ENABLE_VERIFICATION
			gpk_necall(::gpk::arrayInflate({&deflated[sizeof(uint32_t)], deflated.size() - sizeof(uint32_t)}, verify), "Failed to inflate file: %s.", pathToWriteTo.begin());
			ree_if(verify != partBytes, "Sanity check failed: %s.", "??");
#endif
		} else {
			gpk_necall(::gpk::aesEncode(::gpk::view_const_byte{deflated.begin(), deflated.size()}, params.EncryptionKey, ::gpk::AES_LEVEL_256, encrypted), "Failed to encrypt file: %s.", pathToWriteTo.begin());
			info_printf("Saving part file to disk: '%s'. Size: %u.", pathToWriteTo.begin(), encrypted.size());
			gpk_necall(::gpk::fileFromMemory({pathToWriteTo.begin(), pathToWriteTo.size()}, encrypted), "Failed to write file: %s.", pathToWriteTo.begin());
#if defined SEASON_ENABLE_VERIFICATION
			deflated.clear();
			gpk_necall(::gpk::aesDecode(encrypted, params.EncryptionKey, ::gpk::AES_LEVEL_256, deflated), "Failed to inflate file: %s.", pathToWriteTo.begin());
			gpk_necall(::gpk::arrayInflate({&deflated[sizeof(uint32_t)], deflated.size() - sizeof(uint32_t)}, verify), "Failed to inflate file: %s.", pathToWriteTo.begin());
			ree_if(verify != partBytes, "Sanity check failed: %s.", "??");
#endif
		}
	}
	return 0;
}

// Splits a file.json into file.#blocksize.db/file.##.json parts.
int										main							(int argc, char ** argv)		{
	::SSplitParams								params							= {};
	gpk_necall(::loadParams(params, argc, argv), "%s", "");

	::gpk::array_pod<char_t>					dbFolderName					= {};
	//gpk_necall(::blt::tableFolderName(dbFolderName, params.DBName, params.BlockSize), "%s", "??");
	//gpk_necall(::gpk::pathCreate({dbFolderName.begin(), dbFolderName.size()}), "Failed to create database folder: %s.", dbFolderName.begin());
	//info_printf("Output folder: %s.", dbFolderName.begin());
	//if(dbFolderName.size() && dbFolderName[dbFolderName.size()-1] != '/' && dbFolderName[dbFolderName.size()-1] != '\\')
	//	gpk_necall(dbFolderName.push_back('/'), "%s", "Out of memory?");
	//else 
	if(0 == dbFolderName.size())
		gpk_necall(dbFolderName.append(::gpk::view_const_string{"./"}), "%s", "Out of memory?");

	::gpk::SJSONFile							jsonFileToSplit					= {};
	gpk_necall(::gpk::jsonFileRead(jsonFileToSplit, params.FileNameSrc), "Failed to load file: %s.", params.FileNameSrc.begin());
	ree_if(0 == jsonFileToSplit.Reader.Tree.size(), "Invalid input format. %s", "File content is not a JSON array.");
	ree_if(jsonFileToSplit.Reader.Object[0].Type != ::gpk::JSON_TYPE_ARRAY, "Invalid input format. %s", ::gpk::get_value_label(jsonFileToSplit.Reader.Object[0].Type).begin());
	info_printf("Loaded file: %s. Size: %u bytes.", params.FileNameSrc.begin(), jsonFileToSplit.Bytes.size());

	::SWriteCache								blockCache						= {};
	gpk_necall(::writePart(blockCache, params, jsonFileToSplit.Bytes), "%s", "Unknown error!");
	return 0; 
}
