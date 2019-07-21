#include "blitter.h"

#ifndef BLITTER_PROCESS_H_29038679283
#define BLITTER_PROCESS_H_29038679283

namespace blt
{
	::gpk::error_t																generate_output_for_db				
		( const ::gpk::view_array<const ::blt::TKeyValBlitterDB>	& databases
		, const ::blt::SBlitterQuery								& query
		, ::gpk::array_pod<char_t>									& output
		, int32_t													iBlock
		);

	struct SRangeBlockInfo {
		::gpk::view_const_string					OutputRecords		= {};
		::gpk::SMinMax<uint32_t>					RelativeIndices		= {};
	};

	::gpk::error_t																recordRange	
		( ::blt::TKeyValBlitterDB						& database
		, const ::gpk::SRange<uint64_t>					& range
		, const ::gpk::view_const_string				& folder
		, ::gpk::array_obj<::blt::SRangeBlockInfo>		& output_records
		, ::gpk::SRange<uint32_t>						& blockRange
		);

	::gpk::error_t																recordGet	
		( ::blt::TKeyValBlitterDB			& database
		, const uint64_t					absoluteIndex
		, ::gpk::view_const_string			& output_record
		, uint32_t							& relativeIndex
		, uint32_t							& blockIndex
		, const ::gpk::view_const_string	& folder
		);


} // namespace

#endif // BLITTER_PROCESS_H_29038679283