#include "llc_log.h"
#include "llc_path.h"
#include "llc_json.h"
#include "llc_parse.h"
#include "llc_deflate.h"

#include "blitter.h"

static constexpr const ::llc::u2_t		DEFAULT_BLOCK_SIZE				= 1024;

struct SSplitParams {
	::llc::vcst_t					FileNameSrc						= {};	// First parameter is the only parameter, which is the name of the source file to be split.
	::llc::vcst_t					PathWithoutExtension			= {};	// First parameter is the only parameter, which is the name of the source file to be split.
	::llc::vcst_t					DBName							= {};	// First parameter is the only parameter, which is the name of the source file to be split.
	::llc::vcst_t					EncryptionKey					= {};	// First parameter is the only parameter, which is the name of the source file to be split.

	::llc::u2_t						BlockSize						= 0;
	bool									DeflatedOutput					= false;
};

::llc::err_t			loadParams						(SSplitParams& params, int argc, char ** argv)		{
	for(::llc::s2_t iArg = 5; iArg < argc; ++iArg)
		info_printf("Unknown parameter: %s.", argv[iArg]);
	if_true_fef(2 > argc, "Usage:\n\t%s [filename] [blockSize] [deflated output (1:0)] [deflated input (1:0)] ", argv[0]);
	if_true_fef(65535 < argc, "Invalid parameter count: %u.", (::llc::u2_t)argc);
	params.FileNameSrc						= {argv[1], (::llc::u2_t)-1};	// First parameter is the only parameter, which is the name of the source file to be split.
	if(argc > 2) {	// load block size param
		::llc::parseIntegerDecimal({argv[2], (::llc::u2_t)-1}, params.BlockSize);
		info_printf("Using block size: %u.", params.BlockSize);
	}
	params.DeflatedOutput					= (argc > 3 && argv[3][0] != '0');
	if(argc > 4)
		params.EncryptionKey					= {argv[4], (::llc::u2_t)-1};

	if(0 == params.BlockSize)
		params.BlockSize						= ::DEFAULT_BLOCK_SIZE;

	::llc::err_t								indexOfDot						= ::llc::rfind('.', params.FileNameSrc);
	::llc::err_t								indexOfLastSlash				= ::llc::findLastSlash(params.FileNameSrc);
	params.PathWithoutExtension				= (indexOfDot > indexOfLastSlash) ? ::llc::vcst_t{params.FileNameSrc.begin(), (::llc::u2_t)indexOfDot} : params.FileNameSrc;
	params.DBName							= (-1 == indexOfLastSlash)
		? params.PathWithoutExtension
		: ::llc::vcst_t{&params.PathWithoutExtension[indexOfLastSlash], params.PathWithoutExtension.size() - indexOfLastSlash}
		;	// First parameter is the only parameter, which is the name of the source file to be split.

	info_printf("Deflated output: %s", params.DeflatedOutput ? "true" : "false");
	if(params.EncryptionKey.size())
		info_printf("Encryption key: %.*s.", LLC_VIEW_ARGS(params.EncryptionKey));
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

::llc::err_t			writePart						(::SWriteCache & blockCache, const ::SSplitParams & params, const ::llc::vcst_t dbFolderName, const ::llc::vcu0_t & partBytes, ::llc::u2_t iPart)		{
	::llc::string					& partFileName					= blockCache.PartFileName					;
	::llc::string					& pathToWriteTo					= blockCache.PathToWriteTo					;
	::llc::clear(partFileName, pathToWriteTo, blockCache.LoadCache.Deflated, blockCache.LoadCache.Encrypted);
	pathToWriteTo							= dbFolderName;
	if_fail_fef(::blt::blockFileName(partFileName, params.DBName, params.EncryptionKey.size() > 0, params.DeflatedOutput ? ::blt::DATABASE_HOST_DEFLATE : ::blt::DATABASE_HOST_LOCAL, iPart), "%s", "??");
	if_fail_fe(pathToWriteTo.append(partFileName));
	return ::llc::fileFromMemorySecure(blockCache.LoadCache, pathToWriteTo, params.EncryptionKey.cu8(), params.DeflatedOutput, partBytes);
}

// Splits a file.json into file.#blocksize.db/file.##.json parts.
int										main							(int argc, char ** argv)		{
	::SSplitParams								params							= {};
	if_fail_fef(::loadParams(params, argc, argv), "%s", "");

	::llc::SJSONFile							jsonFileToSplit					= {};
	if_fail_fef(::llc::jsonFileRead(jsonFileToSplit, params.FileNameSrc), "Failed to load file: %.*s.", LLC_VIEW_ARGS(params.FileNameSrc));
	if_zero_fef(jsonFileToSplit.Reader.Tree.size(), "Invalid input format. %s", "File content is not a JSON array.");
	if_true_fef(jsonFileToSplit.Reader.Token[0].Type != ::llc::JSON_TYPE_ARRAY, "Invalid input format. %.*s", LLC_VIEW_ARGS(::llc::get_value_label(jsonFileToSplit.Reader.Token[0].Type)));
	info_printf("Loaded file: %.*s. Size: %u bytes.", LLC_VIEW_ARGS(params.FileNameSrc), jsonFileToSplit.Bytes.size());

	::llc::string					dbFolderName					= {};
	if_fail_fe(::blt::tableFolderName(dbFolderName, params.DBName, params.BlockSize));
	if_fail_fef(::llc::pathCreate(dbFolderName), "Failed to create database folder: %.*s.", LLC_VIEW_ARGS(dbFolderName));
	info_printf("Output folder: %.*s.", LLC_VIEW_ARGS(dbFolderName));
	if(dbFolderName.size() && dbFolderName[dbFolderName.size()-1] != '/' && dbFolderName[dbFolderName.size()-1] != '\\') {
		if_fail_fe(dbFolderName.push_back('/'));
	}
	else if(0 == dbFolderName.size())
		if_fail_fe(dbFolderName.append("./"));

	::llc::aobj<::llc::string>	outputJsons;
	if_fail_fe(::llc::jsonArraySplit(*jsonFileToSplit.Reader.Tree[0], jsonFileToSplit.Reader.View , params.BlockSize, outputJsons));

	::SWriteCache								blockCache						= {};
	const ::llc::vcst_t						folder							= dbFolderName;
	for(::llc::u2_t iPart = 0; iPart < outputJsons.size(); ++iPart) {
		::llc::vcu0_t			partBytes						= outputJsons[iPart].cu8();
		if_fail_fe(::writePart(blockCache, params, folder, partBytes, iPart));
	}
	return 0;
}
