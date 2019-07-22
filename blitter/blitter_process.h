#include "blitter.h"

#ifndef BLITTER_PROCESS_H_29038679283
#define BLITTER_PROCESS_H_29038679283

namespace blt
{
	struct SRangeBlockInfo {
		::gpk::view_const_string					OutputRecords		= {};
		::gpk::SMinMax<uint32_t>					RelativeIndices		= {};
		uint32_t									BlockIndex			= 0;
	};

	::gpk::error_t								recordRange
		( ::blt::SLoadCache							& loadCache
		, ::blt::TKeyValBlitterDB					& database
		, const ::gpk::SRange<uint64_t>				& range
		, const ::gpk::view_const_string			& folder
		, ::gpk::array_obj<::blt::SRangeBlockInfo>	& output_records
		, ::gpk::SRange<uint32_t>					& blockRange
		);

	::gpk::error_t								recordGet
		( ::blt::SLoadCache							& loadCache
		, ::blt::TKeyValBlitterDB					& database
		, const uint64_t							absoluteIndex
		, ::gpk::view_const_string					& output_record
		, uint32_t									& relativeIndex
		, uint32_t									& blockIndex
		, const ::gpk::view_const_string			& folder
		);


} // namespace

#endif // BLITTER_PROCESS_H_29038679283
