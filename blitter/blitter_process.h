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
} // namespace

#endif // BLITTER_PROCESS_H_29038679283