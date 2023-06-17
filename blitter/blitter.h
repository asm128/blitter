
#include "gpk_json.h"
#include "gpk_http.h"
#include "gpk_expression.h"
#include "gpk_deflate.h"

#ifndef BLITTER_H_20190712
#define BLITTER_H_20190712

namespace blt
{
	GDEFINE_ENUM_TYPE(FIELD_DISPLAY, uint8_t);
	GDEFINE_ENUM_VALUE(FIELD_DISPLAY, select, 0);
	GDEFINE_ENUM_VALUE(FIELD_DISPLAY, edit, 1);

	struct SBindDescription {
		void											* HandleModule				= 0;
		void											* FuncModule				= 0;
		::gpk::vcc							TableName					= {};
		int32_t											TableIndex					= -1;
		FIELD_DISPLAY									Display						= ::blt::FIELD_DISPLAY_select	;
	};

	typedef ::gpk::SKeyVal<::gpk::vcc, SBindDescription>
													TKeyValBindDescription;

	struct STableDescription {
		::gpk::aobj<::gpk::SJSONFieldBinding	>	Fields						= {};
		::gpk::aobj<::gpk::TKeyValConstChar	>	FieldMaps					= {};
		::gpk::aobj<::blt::TKeyValBindDescription>	FieldMapDescription			= {};
		::gpk::apod<int32_t>						IndicesFieldToMap			= {};
		::gpk::apod<int32_t>						IndicesMapToField			= {};
		::gpk::vcc							TableName					= {};
		bool											Public						= false;
	};
} // namespace

namespace blt
{
	GDEFINE_ENUM_TYPE (DATABASE_HOST, int8_t);
	GDEFINE_ENUM_VALUE(DATABASE_HOST, LOCAL				, 0);
	GDEFINE_ENUM_VALUE(DATABASE_HOST, REMOTE			, 1);
	GDEFINE_ENUM_VALUE(DATABASE_HOST, DEFLATE			, 2);
	GDEFINE_ENUM_VALUE(DATABASE_HOST, REMOTE_DEFLATE	, 3);

	::gpk::error_t												tableFolderName						(::gpk::apod<char> & foldername	, const ::gpk::vcc & dbName, const uint32_t block);
	::gpk::error_t												blockFileName						(::gpk::apod<char> & filename	, const ::gpk::vcc & dbName, bool bEncrypted, const ::blt::DATABASE_HOST hostType, const uint32_t block);
	::gpk::error_t												tableFileName						(::gpk::apod<char> & filename	, const ::blt::DATABASE_HOST & hostType, bool bEncrypted, const ::gpk::vcc & jsonDBKey);
	::gpk::error_t												tableFolderInit						(::gpk::apod<char> & finalFolderName, const ::gpk::vcc & dbPath, const ::gpk::vcc & dbName, const uint32_t blockSize);

	static constexpr	const uint64_t							MAX_TABLE_RECORD_COUNT				= 0x7FFFFFFFFFFFFFFF;
	static constexpr	const uint64_t							MAX_TABLE_BLOCKS_IN_MEMORY			= 1024;
	struct SDatabase {
		::gpk::avcc						Bindings							= {};
		uint32_t						BlockSize							= 0;
		::gpk::au64						Offsets								= {};
		::gpk::apobj<::gpk::SJSONFile>	Blocks								= {};
		::gpk::au32						BlockDirty							= {};
		::gpk::au32						BlockIndices						= {};
		::gpk::au64						BlockTimes							= {};
		::gpk::vcc						EncryptionKey						= {};
		::blt::DATABASE_HOST			HostType							= ::blt::DATABASE_HOST_LOCAL;
		::gpk::au32						BlocksOnDisk						= {};
		int32_t							MaxBlockOnDisk						= -1;
		::blt::STableDescription		Description							= {};
	};

	struct SBlitterQuery {
		::gpk::vcc					Database							= "";
		::gpk::vcc					Command								= "";
		::gpk::vcc					Path								= "";
		::gpk::vcc					Record								= "";
		::gpk::rangeu64				Range								= {0, MAX_TABLE_RECORD_COUNT};
		::gpk::view<::gpk::vcc>		ExpansionKeys						= {};
		int64_t						Detail								= -1;
		::gpk::SJSONReader			RecordReader						= {};
	};

	//struct SBlitterOutput {
	//	::gpk::apod<char>									CurrentOutput;
	//	::gpk::aobj<::gpk::ptr_obj<::gpk::apod<byte_t>>>	BlocksToWrite;
	//};

	typedef ::gpk::SKeyVal<::gpk::vcs, ::blt::SDatabase>	TNamedBlitterDB;

	int64_t			queryProcess
		( ::gpk::SLoadCache						& loadCache
		, ::gpk::aobj<::blt::TNamedBlitterDB>	& databases
		, const ::gpk::SExpressionReader		& expressionReader
		, const ::blt::SBlitterQuery			& query
		, const ::gpk::vcc						& folder
		, ::gpk::apod<char>						& output
		);

	struct SWriteCache {
		::gpk::SLoadCache		& LoadCache		;
		::gpk::apod<char>		PartFileName	;
		::gpk::apod<char>		PathToWriteTo	;
	};

	struct SBlitter {
		::gpk::aobj<::blt::TNamedBlitterDB>	Databases				= {};
		::blt::SBlitterQuery				Query					= {};
		::gpk::SJSONFile					Config					= {};
		::gpk::vcs							Folder					= {};
		::gpk::aobj<::gpk::vcc>				ExpansionKeyStorage		= {};
		::gpk::SExpressionReader			ExpressionReader		= {};
		::gpk::SLoadCache					LoadCache				= {};
	};

	::gpk::error_t			blockFileLoad			(::gpk::SLoadCache & loadCache, ::blt::TNamedBlitterDB & jsonDB, const ::gpk::vcc & folder, uint32_t block);
	::gpk::error_t			tableFileLoad			(::gpk::SLoadCache & loadCache, ::blt::TNamedBlitterDB & jsonDB, const ::gpk::vcc & folder);
	::gpk::error_t			configDatabases			(::gpk::aobj<::blt::TNamedBlitterDB> & databases, const ::gpk::SJSONReader & configReader, const int32_t indexConfigNode, const ::gpk::view_array<const ::gpk::vcs> & databasesToLoad, const ::gpk::vcs & folder);

	::gpk::error_t			blockWrite				(::blt::SWriteCache & writeCache, const ::gpk::vcc & dbFolderName, const ::gpk::vcc & dbName, const ::gpk::vcc & encryptionKey, ::blt::DATABASE_HOST hostType, const ::gpk::vcu8 & partBytes, uint32_t iPart);

	::gpk::error_t			loadConfig				(::blt::SBlitter & appState, const ::gpk::vcs & jsonFileName);

	::gpk::error_t			blitterFlush			(::blt::SBlitter & appState);

	::gpk::error_t			requestProcess			(::gpk::SExpressionReader & expressionReader, ::blt::SBlitterQuery & query, const ::gpk::SHTTPAPIRequest & request, ::gpk::avcc & expansionKeyStorage);
	stacxpr	uint32_t		CRC_SEED				= 18973;
} // namespace

#endif // BLITTER_H_20190712
