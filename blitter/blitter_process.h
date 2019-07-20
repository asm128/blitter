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

	::gpk::error_t																recordRange	
		( ::blt::TKeyValBlitterDB						& database
		, const ::gpk::SRange<uint64_t>					& range
		, ::gpk::array_obj<::gpk::view_const_string>	& output_records
		, ::gpk::array_pod<::gpk::SMinMax<uint32_t>>	& nodeIndices
		, ::gpk::SRange<uint32_t>						& blockRange
		);

	::gpk::error_t																recordGet	
	( ::blt::TKeyValBlitterDB	& database
	, const uint64_t			absoluteIndex
	, ::gpk::view_const_string	& output_record
	, uint32_t					& blockIndex
	, uint32_t					& nodeIndex
	);


} // namespace

#endif // BLITTER_PROCESS_H_29038679283