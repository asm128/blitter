#include "blitter.h"

#ifndef BLITTER_PROCESS_H_29038679283
#define BLITTER_PROCESS_H_29038679283

namespace blt
{
	struct SRangeBlockInfo {
		::gpk::vcc				OutputRecords			= {};
		::gpk::minmaxu32		RelativeIndices			= {};
		uint32_t				BlockIndex				= 0;
		uint32_t				BlockId					= 0;
	};

	::gpk::error_t			recordRange
		( ::gpk::SLoadCache						& loadCache
		, ::blt::TNamedBlitterDB				& database
		, const ::gpk::rangeu64					& range
		, const ::gpk::vcc						& folder
		, ::gpk::aobj<::blt::SRangeBlockInfo>	& output_records
		, ::gpk::rangeu32						& blockRange
		);


	::gpk::error_t			recordLoad
		( ::gpk::SLoadCache			& loadCache
		, ::blt::TNamedBlitterDB	& database
		, const uint64_t			absoluteIndex
		, uint32_t					& relativeIndex
		, uint32_t					& blockIndex
		, const ::gpk::vcc			& folder
		);

	::gpk::error_t			recordGet
		( ::gpk::SLoadCache			& loadCache
		, ::blt::TNamedBlitterDB	& database
		, const uint64_t			absoluteIndex
		, ::gpk::vcc				& output_record
		, uint32_t					& relativeIndex
		, uint32_t					& blockIndex
		, const ::gpk::vcc			& folder
		);

	static inline	::gpk::error_t				recordGet
		( ::gpk::SLoadCache				& loadCache
		, ::blt::TNamedBlitterDB		& database
		, const uint64_t				absoluteIndex
		, ::gpk::vcc					& output_record
		, const ::gpk::vcc				& folder
	) {
		uint32_t											indexRelative					= (uint32_t)-1;
		uint32_t											indexBlock						= (uint32_t)-1;
		return ::blt::recordGet(loadCache, database, absoluteIndex, output_record, indexRelative, indexBlock, folder);
	}
} // namespace

#endif // BLITTER_PROCESS_H_29038679283
