
#include "gpk_json.h"
#include "gpk_http.h"
#include "gpk_expression.h"
#include "gpk_deflate.h"

#ifndef BLITTER_H_20190712
#define BLITTER_H_20190712

namespace blt
{
	GDEFINE_ENUM_TYPE (DATABASE_HOST, int8_t);
	GDEFINE_ENUM_VALUE(DATABASE_HOST, LOCAL				, 0);
	GDEFINE_ENUM_VALUE(DATABASE_HOST, REMOTE			, 1);
	GDEFINE_ENUM_VALUE(DATABASE_HOST, DEFLATE			, 2);
	GDEFINE_ENUM_VALUE(DATABASE_HOST, REMOTE_DEFLATE	, 3);

	::gpk::error_t												tableFolderName						(::gpk::array_pod<char_t> & foldername	, const ::gpk::view_const_string & dbName, const uint32_t block);
	::gpk::error_t												blockFileName						(::gpk::array_pod<char_t> & filename	, const ::gpk::view_const_string & dbName, bool bEncrypted, const ::blt::DATABASE_HOST hostType, const uint32_t block);
	::gpk::error_t												tableFileName						(::gpk::array_pod<char_t> & filename, const ::blt::DATABASE_HOST & hostType, bool bEncrypted, const ::gpk::view_const_string & jsonDBKey);

	static constexpr	const uint64_t							MAX_TABLE_RECORD_COUNT				= 0x7FFFFFFFFFFFFFFF;
	struct SBlitterDB {
		::gpk::array_obj<::gpk::view_const_char>					Bindings							= {};
		uint32_t													BlockSize							= 0;
		::gpk::array_obj<uint64_t>									Offsets								= {};
		::gpk::array_obj<::gpk::ptr_obj<::gpk::SJSONFile>>			Blocks								= {};
		::gpk::array_obj<uint32_t>									BlockIndices						= {};
		::gpk::view_const_char										EncryptionKey						= {};
		::blt::DATABASE_HOST										HostType							= ::blt::DATABASE_HOST_LOCAL;
		::gpk::array_obj<uint32_t>									BlocksOnDisk						= {};
	};

	struct SBlitterQuery {
		::gpk::view_const_char										Database							= "";
		::gpk::view_const_char										Command							= "";
		::gpk::view_const_char										Path								= "";
		::gpk::SRange<uint64_t>										Range								= {0, MAX_TABLE_RECORD_COUNT};
		::gpk::view_array<::gpk::view_const_char>					ExpansionKeys						= {};
		int64_t														Detail								= -1;
	};

	struct SBlitterOutput {
		::gpk::array_pod<char_t>									CurrentOutput;
		::gpk::array_obj<::gpk::ptr_obj<::gpk::array_pod<byte_t>>>	BlocksToWrite;
	};

	typedef ::gpk::SKeyVal<::gpk::view_const_string, ::blt::SBlitterDB>
																TNamedBlitterDB;
	::gpk::error_t												queryProcess
		( ::gpk::array_obj<::blt::TNamedBlitterDB>	& databases
		, const ::gpk::SExpressionReader			& expressionReader
		, const ::blt::SBlitterQuery				& query
		, const ::gpk::view_const_char				& folder
		, ::gpk::array_pod<char_t>					& output
		);

	struct SBlitter {
		::gpk::array_obj<::blt::TNamedBlitterDB>					Databases							= {};
		::blt::SBlitterQuery										Query								= {};
		::gpk::SJSONFile											Config								= {};
		::gpk::view_const_string									Folder								= {};
		::gpk::array_obj<::gpk::view_const_char>					ExpansionKeyStorage					= {};
		::gpk::SExpressionReader									ExpressionReader					= {};
	};

	::gpk::error_t												blockFileLoad						(::gpk::SLoadCache & loadCache, ::blt::TNamedBlitterDB & jsonDB, const ::gpk::view_const_char & folder, uint32_t block);
	::gpk::error_t												tableFileLoad						(::gpk::SLoadCache & loadCache, ::blt::TNamedBlitterDB & jsonDB, const ::gpk::view_const_char & folder);
	::gpk::error_t												configDatabases						(::gpk::array_obj<::blt::TNamedBlitterDB> & databases, const ::gpk::SJSONReader & configReader, const int32_t indexConfigNode, const ::gpk::view_array<const ::gpk::view_const_string> & databasesToLoad, const ::gpk::view_const_string & folder);

	::gpk::error_t												loadConfig							(::blt::SBlitter & appState, const ::gpk::view_const_string & jsonFileName);

	::gpk::error_t												requestProcess						(::gpk::SExpressionReader & expressionReader, ::blt::SBlitterQuery & query, const ::gpk::SHTTPAPIRequest & request, ::gpk::array_obj<::gpk::view_const_char> & expansionKeyStorage);
	static constexpr const uint32_t								CRC_SEED							= 18973;

} // namespace

#endif // BLITTER_H_20190712
