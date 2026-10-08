#define GPK_CONSOLE_LOG_ENABLED
#define GPK_ERROR_PRINTF_ENABLED
#define GPK_WARNING_PRINTF_ENABLED
#define GPK_INFO_PRINTF_ENABLED

#include "gpk_log.h"
#include "gpk_file.h"
#include "gpk_path.h"
#include "gpk_json.h"
#include "gpk_parse.h"
#include "gpk_deflate.h"
#include "gpk_aes.h"
#include "gpk_noise.h"

#include "blitter.h"

static constexpr const uint32_t			DEFAULT_BLOCK_SIZE				= 1024;

struct SSplitParams {
	::gpk::vcst_t					FileNameSrc						= {};	// First parameter is the only parameter, which is the name of the source file to be split.
	::gpk::vcst_t					PathWithoutExtension			= {};	// First parameter is the only parameter, which is the name of the source file to be split.
	::gpk::vcst_t					DBName							= {};	// First parameter is the only parameter, which is the name of the source file to be split.
	::gpk::vcst_t					EncryptionKey					= {};	// First parameter is the only parameter, which is the name of the source file to be split.

	uint32_t								BlockSize						= 0;
	bool									DeflatedOutput					= false;
};

::gpk::error_t			loadParams						(SSplitParams& params, int argc, char ** argv)		{
	for(int32_t iArg = 5; iArg < argc; ++iArg)
		info_printf("Unknown parameter: %s.", argv[iArg]);
	ree_if(2 > argc, "Usage:\n\t%s [filename] [blockSize] [deflated output (1:0)] [deflated input (1:0)] ", argv[0]);
	ree_if(65535 < argc, "Invalid parameter count: %u.", (uint32_t)argc);
	params.FileNameSrc						= ::gpk::vcs{argv[1], (uint32_t)-1};	// First parameter is the only parameter, which is the name of the source file to be split.
	if(argc > 2) {	// load block size param
		::gpk::parseIntegerDecimal({argv[2], (uint32_t)-1}, params.BlockSize);
		info_printf("Using block size: %u.", params.BlockSize);
	}
	params.DeflatedOutput					= (argc > 3 && argv[3][0] != '0');
	if(argc > 4)
		params.EncryptionKey					= ::gpk::vcs{argv[4], (uint32_t)-1};

	if(0 == params.BlockSize)
		params.BlockSize						= ::DEFAULT_BLOCK_SIZE;

	::gpk::error_t								indexOfDot						= ::gpk::rfind('.', params.FileNameSrc);
	::gpk::error_t								indexOfLastSlash				= ::gpk::findLastSlash(params.FileNameSrc);
	params.PathWithoutExtension				= (indexOfDot > indexOfLastSlash) ? ::gpk::vcst_t{params.FileNameSrc.begin(), (uint32_t)indexOfDot} : params.FileNameSrc;
	params.DBName							= (-1 == indexOfLastSlash)
		? params.PathWithoutExtension
		: ::gpk::vcs{&params.PathWithoutExtension[indexOfLastSlash], params.PathWithoutExtension.size() - indexOfLastSlash}
		;	// First parameter is the only parameter, which is the name of the source file to be split.

	info_printf("Deflated output: %s", params.DeflatedOutput ? "true" : "false");
	if(params.EncryptionKey.size())
		info_printf("Encryption key: %s.", params.EncryptionKey.begin());
	else
		info_printf("Encryption: %s.", "false");
	return 0;
}

#define SEASON_ENABLE_VERIFICATION

struct SWriteCache {
	::gpk::string				PartFileName					= {};
	::gpk::string				PathToWriteTo					= {};
	::gpk::SLoadCache						LoadCache						= {};
};

::gpk::error_t			writePart						(::SWriteCache & blockCache, const ::SSplitParams & params, const ::gpk::view_const_string dbFolderName, const ::gpk::vcu0_t & partBytes, uint32_t iPart)		{
	::gpk::string					& partFileName					= blockCache.PartFileName					;
	::gpk::string					& pathToWriteTo					= blockCache.PathToWriteTo					;
	::gpk::clear(partFileName, pathToWriteTo, blockCache.LoadCache.Deflated, blockCache.LoadCache.Encrypted);
	pathToWriteTo							= dbFolderName;
	gpk_necall(::blt::blockFileName(partFileName, params.DBName, params.EncryptionKey.size() > 0, params.DeflatedOutput ? ::blt::DATABASE_HOST_DEFLATE : ::blt::DATABASE_HOST_LOCAL, iPart), "%s", "??");
	gpk_necs(pathToWriteTo.append(partFileName));
	return ::gpk::fileFromMemorySecure(blockCache.LoadCache, pathToWriteTo, params.EncryptionKey.u8(), params.DeflatedOutput, partBytes);
}

// Splits a file.json into file.#blocksize.db/file.##.json parts.
int										main							(int argc, char ** argv)		{
	::SSplitParams								params							= {};
	gpk_necall(::loadParams(params, argc, argv), "%s", "");

	::gpk::SJSONFile							jsonFileToSplit					= {};
	gpk_necall(::gpk::jsonFileRead(jsonFileToSplit, params.FileNameSrc), "Failed to load file: %s.", params.FileNameSrc.begin());
	if_zero_fef(jsonFileToSplit.Reader.Tree.size(), "Invalid input format. %s", "File content is not a JSON array.");
	if_true_fef(jsonFileToSplit.Reader.Token[0].Type != ::gpk::JSON_TYPE_ARRAY, "Invalid input format. %s", ::gpk::get_value_label(jsonFileToSplit.Reader.Token[0].Type).begin());
	info_printf("Loaded file: %s. Size: %u bytes.", params.FileNameSrc.begin(), jsonFileToSplit.Bytes.size());

	::gpk::string					dbFolderName					= {};
	if_fail_fe(::blt::tableFolderName(dbFolderName, params.DBName, params.BlockSize));
	gpk_necall(::gpk::pathCreate({dbFolderName.begin(), dbFolderName.size()}), "Failed to create database folder: %s.", dbFolderName.begin());
	info_printf("Output folder: %s.", dbFolderName.begin());
	if(dbFolderName.size() && dbFolderName[dbFolderName.size()-1] != '/' && dbFolderName[dbFolderName.size()-1] != '\\') {
		if_fail_fe(dbFolderName.push_back('/'));
	}
	else if(0 == dbFolderName.size())
		if_fail_fe(dbFolderName.append(::gpk::vcs{"./"}));

	::gpk::array_obj<::gpk::string>	outputJsons;
	if_fail_fe(::gpk::jsonArraySplit(*jsonFileToSplit.Reader.Tree[0], jsonFileToSplit.Reader.View , params.BlockSize, outputJsons));

	::SWriteCache								blockCache						= {};
	const ::gpk::view_const_string				folder							= {dbFolderName.begin(), dbFolderName.size()};
	for(uint32_t iPart = 0; iPart < outputJsons.size(); ++iPart) {
		::gpk::vcu0_t			partBytes						= outputJsons[iPart].cu8();
		gpk_necs(::writePart(blockCache, params, folder, partBytes, iPart));
	}
	return 0;
}
