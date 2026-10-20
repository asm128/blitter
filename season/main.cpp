#define LLC_CONSOLE_LOG_ENABLED
#define LLC_ERROR_PRINTF_ENABLED
#define LLC_WARNING_PRINTF_ENABLED
#define LLC_INFO_PRINTF_ENABLED

#include "llc_log.h"
#include "llc_file.h"
#include "llc_path.h"
#include "llc_json.h"
#include "llc_parse.h"
#include "llc_deflate.h"
#include "llc_aes.h"
#include "llc_noise.h"

#include "blitter.h"

static constexpr const uint32_t			DEFAULT_BLOCK_SIZE				= 1024;

struct SSplitParams {
	::llc::vcst_t					FileNameSrc						= {};	// First parameter is the only parameter, which is the name of the source file to be split.
	::llc::vcst_t					PathWithoutExtension			= {};	// First parameter is the only parameter, which is the name of the source file to be split.
	::llc::vcst_t					DBName							= {};	// First parameter is the only parameter, which is the name of the source file to be split.
	::llc::vcst_t					EncryptionKey					= {};	// First parameter is the only parameter, which is the name of the source file to be split.

	uint32_t								BlockSize						= 0;
	bool									DeflatedOutput					= false;
};

::llc::err_t			loadParams						(SSplitParams& params, int argc, char ** argv)		{
	for(int32_t iArg = 5; iArg < argc; ++iArg)
		info_printf("Unknown parameter: %s.", argv[iArg]);
	if_true_fef(2 > argc, "Usage:\n\t%s [filename] [blockSize] [deflated output (1:0)] [deflated input (1:0)] ", argv[0]);
	if_true_fef(65535 < argc, "Invalid parameter count: %u.", (uint32_t)argc);
	params.FileNameSrc						= ::llc::vcst_t{argv[1], (uint32_t)-1};	// First parameter is the only parameter, which is the name of the source file to be split.
	if(argc > 2) {	// load block size param
		::llc::parseIntegerDecimal({argv[2], (uint32_t)-1}, params.BlockSize);
		info_printf("Using block size: %u.", params.BlockSize);
	}
	params.DeflatedOutput					= (argc > 3 && argv[3][0] != '0');
	if(argc > 4)
		params.EncryptionKey					= ::llc::vcst_t{argv[4], (uint32_t)-1};

	if(0 == params.BlockSize)
		params.BlockSize						= ::DEFAULT_BLOCK_SIZE;

	::llc::err_t								indexOfDot						= ::llc::rfind('.', params.FileNameSrc);
	::llc::err_t								indexOfLastSlash				= ::llc::findLastSlash(params.FileNameSrc);
	params.PathWithoutExtension				= (indexOfDot > indexOfLastSlash) ? ::llc::vcst_t{params.FileNameSrc.begin(), (uint32_t)indexOfDot} : params.FileNameSrc;
	params.DBName							= (-1 == indexOfLastSlash)
		? params.PathWithoutExtension
		: ::llc::vcst_t{&params.PathWithoutExtension[indexOfLastSlash], params.PathWithoutExtension.size() - indexOfLastSlash}
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
	::llc::string				PartFileName					= {};
	::llc::string				PathToWriteTo					= {};
	::llc::SLoadCache						LoadCache						= {};
};

::llc::err_t			writePart						(::SWriteCache & blockCache, const ::SSplitParams & params, const ::llc::vcst_t dbFolderName, const ::llc::vcu0_t & partBytes, uint32_t iPart)		{
	::llc::string					& partFileName					= blockCache.PartFileName					;
	::llc::string					& pathToWriteTo					= blockCache.PathToWriteTo					;
	::llc::clear(partFileName, pathToWriteTo, blockCache.LoadCache.Deflated, blockCache.LoadCache.Encrypted);
	pathToWriteTo							= dbFolderName;
	llc_necall(::blt::blockFileName(partFileName, params.DBName, params.EncryptionKey.size() > 0, params.DeflatedOutput ? ::blt::DATABASE_HOST_DEFLATE : ::blt::DATABASE_HOST_LOCAL, iPart), "%s", "??");
	llc_necs(pathToWriteTo.append(partFileName));
	return ::llc::fileFromMemorySecure(blockCache.LoadCache, pathToWriteTo, params.EncryptionKey.u8(), params.DeflatedOutput, partBytes);
}

// Splits a file.json into file.#blocksize.db/file.##.json parts.
int										main							(int argc, char ** argv)		{
	::SSplitParams								params							= {};
	llc_necall(::loadParams(params, argc, argv), "%s", "");

	::llc::SJSONFile							jsonFileToSplit					= {};
	llc_necall(::llc::jsonFileRead(jsonFileToSplit, params.FileNameSrc), "Failed to load file: %s.", params.FileNameSrc.begin());
	if_true_fef(0 == jsonFileToSplit.Reader.Tree.size(), "Invalid input format. %s", "File content is not a JSON array.");
	if_true_fef(jsonFileToSplit.Reader.Token[0].Type != ::llc::JSON_TYPE_ARRAY, "Invalid input format. %s", ::llc::get_value_label(jsonFileToSplit.Reader.Token[0].Type).begin());
	info_printf("Loaded file: %s. Size: %u bytes.", params.FileNameSrc.begin(), jsonFileToSplit.Bytes.size());

	::llc::string					dbFolderName					= {};
	llc_necall(::blt::tableFolderName(dbFolderName, params.DBName, params.BlockSize), "%s", "??");
	llc_necall(::llc::pathCreate({dbFolderName.begin(), dbFolderName.size()}), "Failed to create database folder: %s.", dbFolderName.begin());
	info_printf("Output folder: %s.", dbFolderName.begin());
	if(dbFolderName.size() && dbFolderName[dbFolderName.size()-1] != '/' && dbFolderName[dbFolderName.size()-1] != '\\')
		llc_necall(dbFolderName.push_back('/'), "%s", "Out of memory?");
	else if(0 == dbFolderName.size())
		llc_necall(dbFolderName.append(::llc::vcst_t{"./"}), "%s", "Out of memory?");

	::llc::array_obj<::llc::string>	outputJsons;
	llc_necall(::llc::jsonArraySplit(*jsonFileToSplit.Reader.Tree[0], jsonFileToSplit.Reader.View , params.BlockSize, outputJsons), "%s", "Unknown error!");

	::SWriteCache								blockCache						= {};
	const ::llc::vcst_t				folder							= {dbFolderName.begin(), dbFolderName.size()};
	for(uint32_t iPart = 0; iPart < outputJsons.size(); ++iPart) {
		::llc::vcu0_t			partBytes						= outputJsons[iPart].cu8();
		llc_necs(::writePart(blockCache, params, folder, partBytes, iPart));
	}
	return 0;
}
