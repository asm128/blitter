
#include "llc_json.h"
#include "llc_expression.h"
#include "llc_deflate.h"
#include "llc_range.h"
#include "llc_string.h"

#ifndef BLITTER_H_20190712
#define BLITTER_H_20190712

namespace blt
{
	GDEFINE_ENUM_TYPE(FIELD_DISPLAY, uint8_t);
	GDEFINE_ENUM_VALUE(FIELD_DISPLAY, select, 0);
	GDEFINE_ENUM_VALUE(FIELD_DISPLAY, edit, 1);

	struct SBindDescription {
		::llc::vcst_t								TableName				= {};
		int32_t										TableIndex				= -1;
		FIELD_DISPLAY								Display					= ::blt::FIELD_DISPLAY_select	;
	};

	typedef ::llc::keyval<::llc::vcst_t, SBindDescription>
													TKeyValBindDescription;

	struct STableDescription {
		::llc::aobj<::llc::SJSONFieldBinding>		Fields					= {};
		::llc::aobj<::llc::TKeyValConstChar	>		FieldMaps				= {};
		::llc::aobj<::blt::TKeyValBindDescription>	FieldMapDescription		= {};
		::llc::apod<int32_t>						IndicesFieldToMap		= {};
		::llc::apod<int32_t>						IndicesMapToField		= {};
		::llc::vcst_t								TableName				= {};
		bool										Public					= false;
	};
} // namespace

namespace blt
{
	GDEFINE_ENUM_TYPE (DATABASE_HOST, int8_t);
	GDEFINE_ENUM_VALUE(DATABASE_HOST, LOCAL				, 0);
	GDEFINE_ENUM_VALUE(DATABASE_HOST, REMOTE			, 1);
	GDEFINE_ENUM_VALUE(DATABASE_HOST, DEFLATE			, 2);
	GDEFINE_ENUM_VALUE(DATABASE_HOST, REMOTE_DEFLATE	, 3);

	::llc::error_t					tableFolderName						(::llc::string & foldername		, ::llc::vcst_t dbName, const uint32_t block);
	::llc::error_t					blockFileName						(::llc::string & filename		, ::llc::vcst_t dbName, bool bEncrypted, const ::blt::DATABASE_HOST hostType, const uint32_t block);
	::llc::error_t					tableFileName						(::llc::string & filename		, const ::blt::DATABASE_HOST & hostType, bool bEncrypted, ::llc::vcst_t jsonDBKey);
	::llc::error_t					tableFolderInit						(::llc::string & finalFolderName, ::llc::vcst_t dbPath, ::llc::vcst_t dbName, const uint32_t blockSize);

	static constexpr	const uint64_t							MAX_TABLE_RECORD_COUNT				= 0x7FFFFFFFFFFFFFFF;
	static constexpr	const uint64_t							MAX_TABLE_BLOCKS_IN_MEMORY			= 1024;
	struct SDatabase {
		::llc::aobj<::llc::vcst_t>		Bindings							= {};
		uint32_t						BlockSize							= 0;
		::llc::au3_t					Offsets								= {};
		::llc::apobj<::llc::SJSONFile>	Blocks								= {};
		::llc::au2_t					BlockDirty							= {};
		::llc::au2_t					BlockIndices						= {};
		::llc::au3_t					BlockTimes							= {};
		::llc::vcst_t					EncryptionKey						= {};
		::blt::DATABASE_HOST			HostType							= ::blt::DATABASE_HOST_LOCAL;
		::llc::au2_t					BlocksOnDisk						= {};
		int32_t							MaxBlockOnDisk						= -1;
		::blt::STableDescription		Description							= {};
	};

	struct SBlitterQuery {
		::llc::vcst_t					Database							= "";
		::llc::vcst_t					Command								= "";
		::llc::vcst_t					Path								= "";
		::llc::vcst_t					Record								= "";
		::llc::rangeu3_t				Range								= {0, MAX_TABLE_RECORD_COUNT};
		::llc::view<::llc::vcst_t>		ExpansionKeys						= {};
		int64_t							Detail								= -1;
		::llc::SJSONReader				RecordReader						= {};
	};

	struct SBlitterRequest {
		::llc::vcst_t					Path								= {};
		::llc::vcst_t					ContentBody						= {};
		::llc::view<cnst ::llc::TKeyValConstString>	QueryStringKeyVals		= {};
	};

	//struct SBlitterOutput {
	//	::llc::apod<char>									CurrentOutput;
	//	::llc::aobj<::llc::ptr_obj<::llc::apod<byte_t>>>	BlocksToWrite;
	//};

	typedef ::llc::keyval<::llc::vcs, ::blt::SDatabase>	TNamedBlitterDB;

	int64_t			queryProcess
		( ::llc::SLoadCache						& loadCache
		, ::llc::aobj<::blt::TNamedBlitterDB>	& databases
		, const ::llc::SExpressionReader		& expressionReader
		, const ::blt::SBlitterQuery			& query
		, const ::llc::vcst_t					& folder
		, ::llc::string							& output
		);

	struct SWriteCache {
		::llc::SLoadCache					& LoadCache		;
		::llc::string						PartFileName	;
		::llc::string						PathToWriteTo	;
	};

	struct SBlitter {
		::llc::aobj<::blt::TNamedBlitterDB>	Databases				= {};
		::blt::SBlitterQuery				Query					= {};
		::llc::SJSONFile					Config					= {};
		::llc::vcs							Folder					= {};
		::llc::aobj<::llc::vcst_t>			ExpansionKeyStorage		= {};
		::llc::SExpressionReader			ExpressionReader		= {};
		::llc::SLoadCache					LoadCache				= {};
	};

	::llc::error_t			blockFileLoad			(::llc::SLoadCache & loadCache, ::blt::TNamedBlitterDB & jsonDB, ::llc::vcst_t folder, uint32_t block);
	::llc::error_t			tableFileLoad			(::llc::SLoadCache & loadCache, ::blt::TNamedBlitterDB & jsonDB, ::llc::vcst_t folder);
	::llc::error_t			configDatabases			(::llc::aobj<::blt::TNamedBlitterDB> & databases, const ::llc::SJSONReader & configReader, const int32_t indexConfigNode, const ::llc::view_array<const ::llc::vcs> & databasesToLoad, ::llc::vcst_t folder);

	::llc::error_t			blockWrite				(::blt::SWriteCache & writeCache, ::llc::vcst_t dbFolderName, ::llc::vcst_t dbName, ::llc::vcst_t encryptionKey, ::blt::DATABASE_HOST hostType, const ::llc::vcu0_t & partBytes, uint32_t iPart);

	::llc::error_t			loadConfig				(::blt::SBlitter & appState, ::llc::vcst_t jsonFileName);

	::llc::error_t			blitterFlush			(::blt::SBlitter & appState);

	::llc::error_t			requestProcess			(::llc::SExpressionReader & expressionReader, ::blt::SBlitterQuery & query, const ::blt::SBlitterRequest & request, ::llc::aobj<::llc::vcst_t> & expansionKeyStorage);
	stacxpr	uint32_t		CRC_SEED				= 18973;
} // namespace

#endif // BLITTER_H_20190712
