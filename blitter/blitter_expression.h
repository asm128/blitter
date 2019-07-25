#include "blitter.h"

#ifndef BLITTER_EXPRESSION_H_908237290837
#define BLITTER_EXPRESSION_H_908237290837

namespace blt
{
	::gpk::error_t								blitterExpressionResolve
		( ::gpk::array_obj<::blt::TNamedBlitterDB>	& databases
		, const ::gpk::view_const_string			& dbFolder
		, const ::gpk::SExpressionReader			& reader
		, const ::gpk::SJSONReader					& inputJSON
		, uint32_t									indexNodeJSON
		, ::gpk::view_const_string					& output
		);
} // namespace

#endif // BLITTER_EXPRESSION_H_908237290837
