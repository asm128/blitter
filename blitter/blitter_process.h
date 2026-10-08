#include "blitter.h"
#include "gpk_minmax.h"

#ifndef BLITTER_PROCESS_H_29038679283
#define BLITTER_PROCESS_H_29038679283

namespace blt
{
	struct SRangeBlockInfo {
		::gpk::vcst_t			OutputRecords			= {};
		::gpk::minmaxu2_t		RelativeIndices			= {};
		uint32_t				BlockIndex				= 0;
		uint32_t				BlockId					= 0;
	};

	::gpk::error_t			recordRange
		( ::gpk::SLoadCache						& loadCache
		, ::blt::TNamedBlitterDB				& database
		, const ::gpk::rangeu3_t				& range
		, const ::gpk::vcst_t					& folder
		, ::gpk::aobj<::blt::SRangeBlockInfo>	& output_records
		, ::gpk::rangeu2_t						& blockRange
		);


	::gpk::error_t			recordLoad
		( ::gpk::SLoadCache			& loadCache
		, ::blt::TNamedBlitterDB	& database
		, const uint64_t			absoluteIndex
		, uint32_t					& relativeIndex
		, uint32_t					& blockIndex
		, const ::gpk::vcst_t		& folder
		);

	::gpk::error_t			recordGet
		( ::gpk::SLoadCache			& loadCache
		, ::blt::TNamedBlitterDB	& database
		, const uint64_t			absoluteIndex
		, ::gpk::vcst_t				& output_record
		, uint32_t					& relativeIndex
		, uint32_t					& blockIndex
		, const ::gpk::vcst_t			& folder
		);

	stin	::gpk::err_t	recordGet
		( ::gpk::SLoadCache			& loadCache
		, ::blt::TNamedBlitterDB	& database
		, const uint64_t			absoluteIndex
		, ::gpk::vcst_t				& output_record
		, const ::gpk::vcst_t		& folder
	) {
		uint32_t											indexRelative					= (uint32_t)-1;
		uint32_t											indexBlock						= (uint32_t)-1;
		return ::blt::recordGet(loadCache, database, absoluteIndex, output_record, indexRelative, indexBlock, folder);
	}
} // namespace

#endif // BLITTER_PROCESS_H_29038679283
