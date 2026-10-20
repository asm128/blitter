#include "blitter.h"
#include "llc_minmax.h"

#ifndef BLITTER_PROCESS_H_29038679283
#define BLITTER_PROCESS_H_29038679283

namespace blt
{
	struct SRangeBlockInfo {
		::llc::vcst_t		OutputRecords			= {};
		::llc::minmaxu2_t	RelativeIndices			= {};
		uint32_t			BlockIndex				= 0;
		uint32_t			BlockId					= 0;
	};

	::llc::err_t		recordRange
		( ::llc::SLoadCache						& loadCache
		, ::blt::TNamedBlitterDB				& database
		, const ::llc::rangeu3_t				& range
		, const ::llc::vcst_t					& folder
		, ::llc::aobj<::blt::SRangeBlockInfo>	& output_records
		, ::llc::rangeu2_t						& blockRange
		);


	::llc::err_t		recordLoad
		( ::llc::SLoadCache			& loadCache
		, ::blt::TNamedBlitterDB	& database
		, const uint64_t			absoluteIndex
		, uint32_t					& relativeIndex
		, uint32_t					& blockIndex
		, const ::llc::vcst_t		& folder
		);

	::llc::err_t		recordGet
		( ::llc::SLoadCache			& loadCache
		, ::blt::TNamedBlitterDB	& database
		, const uint64_t			absoluteIndex
		, ::llc::vcst_t				& output_record
		, uint32_t					& relativeIndex
		, uint32_t					& blockIndex
		, const ::llc::vcst_t		& folder
		);

	stin ::llc::err_t	recordGet
		( ::llc::SLoadCache			& loadCache
		, ::blt::TNamedBlitterDB	& database
		, const uint64_t			absoluteIndex
		, ::llc::vcst_t				& output_record
		, const ::llc::vcst_t		& folder
	) {
		uint32_t					indexRelative		= (uint32_t)-1;
		uint32_t					indexBlock			= (uint32_t)-1;
		return ::blt::recordGet(loadCache, database, absoluteIndex, output_record, indexRelative, indexBlock, folder);
	}
} // namespace

#endif // BLITTER_PROCESS_H_29038679283
