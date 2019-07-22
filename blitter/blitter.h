#include "gpk_json.h"
#include "gpk_http.h"

#ifndef BLITTER_H_20190712
#define BLITTER_H_20190712

namespace blt
{
	GDEFINE_ENUM_TYPE (DATABASE_HOST, int8_t);
	GDEFINE_ENUM_VALUE(DATABASE_HOST, LOCAL				, 0);
	GDEFINE_ENUM_VALUE(DATABASE_HOST, REMOTE			, 1);
	GDEFINE_ENUM_VALUE(DATABASE_HOST, DEFLATE			, 2);
	GDEFINE_ENUM_VALUE(DATABASE_HOST, REMOTE_DEFLATE	, 3);

	static constexpr	const uint64_t											MAX_TABLE_RECORD_COUNT		= 0x7FFFFFFFFFFFFFFF;

	struct SBlitterDB {
		::gpk::array_obj<::gpk::view_const_string>			Bindings					= {};
		uint32_t											BlockSize					= 0;
		::gpk::array_obj<uint64_t>							Offsets						= {};
		::gpk::array_obj<::gpk::ptr_obj<::gpk::SJSONFile>>	Blocks						= {};
		::gpk::array_obj<uint32_t>							BlockIndices				= {};
		::gpk::view_const_string							EncryptionKey				= {};
		::blt::DATABASE_HOST								HostType					= ::blt::DATABASE_HOST_LOCAL;
		::gpk::array_obj<uint32_t>							BlocksOnDisk				= {};
	};

	struct SBlitterQuery {
		::gpk::view_const_string							Database					= "";
		::gpk::SRange<uint64_t>								Range						= {0, MAX_TABLE_RECORD_COUNT};
		::gpk::view_const_string							Expand						= "";
		int64_t												Detail						= -1;
	};

	typedef ::gpk::SKeyVal<::gpk::view_const_string, ::blt::SBlitterDB>			TKeyValBlitterDB;

	struct SLoadCache {
		::gpk::array_pod<byte_t>							Deflated;
		::gpk::array_pod<byte_t>							Encrypted;
	};

	::gpk::error_t										configDatabases						(::gpk::array_obj<::blt::TKeyValBlitterDB> & databases, const ::gpk::SJSONReader & configReader, const int32_t indexConfigNode, const ::gpk::view_array<const ::gpk::view_const_string> & databasesToLoad, const ::gpk::view_const_string & folder);
	::gpk::error_t										blockFileLoad						(::blt::SLoadCache & loadCache, ::blt::TKeyValBlitterDB & jsonDB		, const ::gpk::view_const_string & folder, uint32_t block);
	::gpk::error_t										tableFileLoad						(::blt::SLoadCache & loadCache, ::blt::TKeyValBlitterDB & jsonDB		, const ::gpk::view_const_string & folder);
	::gpk::error_t										tableFolderName						(::gpk::array_pod<char_t> & foldername	, const ::gpk::view_const_string & dbName, const uint32_t block);
	::gpk::error_t										blockFileName						(::gpk::array_pod<char_t> & filename	, const ::gpk::view_const_string & dbName, bool bEncrypted, const ::blt::DATABASE_HOST hostType, const uint32_t block);
	::gpk::error_t										tableFileName						(::gpk::array_pod<char_t> & filename	, const ::blt::DATABASE_HOST & hostType, bool bEncrypted, const ::gpk::view_const_string & jsonDBKey);

	struct SBlitter {
		::gpk::array_obj<::blt::TKeyValBlitterDB>			Databases							= {};
		::blt::SBlitterQuery								Query								= {};
		::gpk::SJSONFile									Config								= {};
		::gpk::view_const_string							Folder								= {};
	};

	//
	::gpk::error_t										loadConfig							(::blt::SBlitter & appState, const ::gpk::view_const_string & jsonFileName);
	::gpk::error_t										queryLoad							(::blt::SBlitterQuery& query, const ::gpk::view_array<const ::gpk::TKeyValConstString> keyvals);

	::gpk::error_t										processQuery
		( ::gpk::array_obj<::blt::TKeyValBlitterDB>	& databases
		, const ::blt::SBlitterQuery				& query
		, ::gpk::array_pod<char_t>					& output
		, const ::gpk::view_const_string			& folder
		);

	struct SBlitterRequest {
		::gpk::HTTP_METHOD									Method		;
		::gpk::view_const_char								Path		;
		::gpk::view_const_char								QueryString	;
		::gpk::view_const_char								ContentBody	;
	};

	static constexpr const uint32_t						CRC_SEED							= 18973;

} // namespace

#endif // BLITTER_H_20190712
