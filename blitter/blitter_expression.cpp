//#include "blitter_expression.h"
//#include "gpk_process.h"
//#include "gpk_parse.h"
//
//#define gpk_jexpr_info_printf
//
//static ::gpk::error_t							evaluateExpression
//	( ::gpk::array_obj<::blt::TNamedBlitterDB>		& databases
//	, const ::gpk::view_const_string				& dbFolder
//	, const ::gpk::SExpressionReader				& readerExpression
//	, uint32_t										indexNodeExpression
//	, const ::gpk::SJSONReader						& inputJSON
//	, uint32_t										indexNodeJSON
//	, ::gpk::view_const_string						& output
//	) {
//	ree_if(indexNodeJSON >= inputJSON.Tree.size(), "Invalid input json or index node: %i", indexNodeJSON)
//	const ::gpk::SExpressionNode						* nodeExpression						= readerExpression.Tree[indexNodeExpression];	// Take this
//	uint32_t											indexRootJSONNode						= indexNodeJSON;
//	int32_t												indexJSONResult							= -1;
//	::gpk::array_pod<char_t>							bufferFormat							= {};
//	const ::gpk::SExpressionNode						& dbNode								= *nodeExpression->Children[0];
//	const ::gpk::error_t								iDB										= ::gpk::find(readerExpression.View[dbNode.ObjectIndex], ::gpk::view_array<const ::blt::TNamedBlitterDB>{databases.begin(), databases.size()});
//	for(uint32_t iFirstLevelExpression = 1; iFirstLevelExpression < nodeExpression->Children.size(); ++iFirstLevelExpression) {
//		const ::gpk::SExpressionNode						& childToSolve								= *nodeExpression->Children[iFirstLevelExpression];
//		const ::gpk::SJSONNode								& currentJSON								= *inputJSON.Tree[indexNodeJSON];
//		if(::gpk::EXPRESSION_READER_TYPE_KEY == childToSolve.Object->Type) {
//			const ::gpk::view_const_string						& strKey									= readerExpression.View[childToSolve.ObjectIndex];
//			gpk_necall(bufferFormat.resize(strKey.size() + 1024), "%s", "Out of memory?");
//			sprintf_s(bufferFormat.begin(), bufferFormat.size(), "Only objects can be accessed by key. JSON type: %%s. Key to solve: %%.%us.", strKey.size());
//			ree_if(currentJSON.Object->Type != ::gpk::JSON_TYPE_OBJECT, bufferFormat.begin(), ::gpk::get_value_label(currentJSON.Object->Type).begin(), strKey.begin());
//			indexJSONResult									= ::gpk::jsonObjectValueGet(*inputJSON.Tree[indexNodeJSON], inputJSON.View, strKey);
//			if errored(indexJSONResult) {
//				sprintf_s(bufferFormat.begin(), bufferFormat.size(), "Key not found: %%.%us.", strKey.size());
//				warning_printf(bufferFormat.begin(), strKey.begin());
//				return -1;
//			}
//			output											= inputJSON.View[indexJSONResult];
//		}
//		else if(::gpk::EXPRESSION_READER_TYPE_INDEX == childToSolve.Object->Type) {
//			uint64_t											numberRead								= 0;
//			const ::gpk::view_const_string						& viewOfIndex							= readerExpression.View[childToSolve.ObjectIndex];
//			uint32_t											countDigits								= (uint32_t)::gpk::parseIntegerDecimal(viewOfIndex, &numberRead);
//			gwarn_if(countDigits != viewOfIndex.size(), "countDigits: %u, viewOfIndex: %u.", countDigits, viewOfIndex.size())
//			if(iFirstLevelExpression == 1) {
//				// Here we need to call recordGet()
//			}
//			else {
//				ree_if(currentJSON.Object->Type != ::gpk::JSON_TYPE_ARRAY, "Only arrays can be accessed by key. JSON type: %s.", ::gpk::get_value_label(currentJSON.Object->Type).begin());
//				indexJSONResult									= ::gpk::jsonArrayValueGet(currentJSON, (uint32_t)numberRead);
//				ree_if(errored(indexJSONResult), "Value not found for index: %lli.", numberRead);
//				output											= inputJSON.View[indexJSONResult];
//			}
//		}
//		else if(::gpk::EXPRESSION_READER_TYPE_EXPRESSION_INDEX == childToSolve.Object->Type) {
//			ree_if(currentJSON.Object->Type != ::gpk::JSON_TYPE_ARRAY, "Only arrays can be accessed by key. JSON type: %s.", ::gpk::get_value_label(currentJSON.Object->Type).begin());
//			::gpk::view_const_string							viewOfIndex								= {};
//			const int32_t										indexOfResolvedSubExpression			= ::evaluateExpression(databases, dbFolder, readerExpression, childToSolve.ObjectIndex, inputJSON, indexRootJSONNode, viewOfIndex);
//			if errored(indexOfResolvedSubExpression) {
//				gpk_necall(bufferFormat.resize(readerExpression.View[childToSolve.ObjectIndex].size() + 512), "%s", "Out of memory?");
//				sprintf_s(bufferFormat.begin(), bufferFormat.size(), "Failed to solve expression: %%.%us.", readerExpression.View[childToSolve.ObjectIndex].size());
//				warning_printf(bufferFormat.begin(), readerExpression.View[childToSolve.ObjectIndex].begin());
//				return -1;
//			}
//			uint64_t											numberRead								= 0;
//			gpk_necall(::gpk::parseIntegerDecimal(viewOfIndex, &numberRead), "%s", "Out of memory?");
//			indexJSONResult									= ::gpk::jsonArrayValueGet(currentJSON, (uint32_t)numberRead);
//			ree_if(errored(indexJSONResult), "Value not found for index: %lli.", numberRead);
//			output											= inputJSON.View[indexJSONResult];
//		}
//		else if(::gpk::EXPRESSION_READER_TYPE_EXPRESSION_KEY == childToSolve.Object->Type) {
//			ree_if(currentJSON.Object->Type != ::gpk::JSON_TYPE_OBJECT, "Only objects can be accessed by key. JSON type: %s.", ::gpk::get_value_label(currentJSON.Object->Type).begin());
//			::gpk::view_const_string							strKey									= {};
//			const int32_t										indexOfResolvedSubExpression			= ::evaluateExpression(databases, dbFolder, readerExpression, childToSolve.ObjectIndex, inputJSON, indexRootJSONNode, strKey);
//			(void)indexOfResolvedSubExpression;
//			indexJSONResult									= ::gpk::jsonObjectValueGet(*inputJSON.Tree[indexNodeJSON], inputJSON.View, strKey);
//			if errored(indexJSONResult) {
//				gpk_necall(bufferFormat.resize(strKey.size() + 512), "%s", "Out of memory?");
//				sprintf_s(bufferFormat.begin(), bufferFormat.size(), "Key not found: %%.%us.", strKey.size());
//				warning_printf(bufferFormat.begin(), strKey.begin());
//				return -1;
//			}
//			output											= inputJSON.View[indexJSONResult];
//		}
//		else {
//			warning_printf("Unrecognized expression reader type: '%s'.", ::gpk::get_value_label(childToSolve.Object->Type).begin());
//			return -1;
//		}
//		indexNodeJSON									= indexJSONResult;
//	}
//	inputJSON, indexNodeJSON, output;
//	return indexJSONResult;
//}
//
//::gpk::error_t									blt::blitterExpressionResolve
//	( ::gpk::array_obj<::blt::TNamedBlitterDB>		& databases
//	, const ::gpk::view_const_string				& dbFolder
//	, const ::gpk::SExpressionReader				& reader
//	, const ::gpk::SJSONReader						& inputJSON
//	, uint32_t										indexNodeJSON
//	, ::gpk::view_const_string						& output
//	) {
//	::gpk::view_const_string							resultOfExpressionEval					= {};
//	::gpk::array_pod<char_t>							bufferFormat							= {};
//	//gpk_necall(bufferFormat.resize(expression.size() + 1024), "%s", "Out of memory?");
//	//sprintf_s(bufferFormat.begin(), bufferFormat.size(), "Failed to read JSONeN expression: '%%.%us'.", expression.size());
//	//gpk_necall(::gpk::expressionReaderParse(reader, expression), bufferFormat.begin(), expression.begin());
//	const ::gpk::array_obj<::gpk::view_const_string>	& expressionViews						= reader.View;
//	for(uint32_t iView = 0; iView < expressionViews.size(); ++iView) {
//		const ::gpk::view_const_string						& viewExpression						= expressionViews[iView];
//		const ::gpk::SExpressionReaderType					& typeExpression						= reader.Object[iView];
//		if(viewExpression.size()) {
//			uint32_t											lenString								= viewExpression.size();
//			gpk_necall(bufferFormat.resize(lenString + 1024), "%s", "Out of memory?");
//			sprintf_s(bufferFormat.begin(), bufferFormat.size(), "Expression element: %%.%us. Type: %s. Parent: %i. Begin: %u. End: %u.", lenString, ::gpk::get_value_label(typeExpression.Type).begin(), typeExpression.ParentIndex, typeExpression.Span.Begin, typeExpression.Span.End);
//			gpk_jexpr_info_printf(bufferFormat.begin(), viewExpression.begin());
//		}
//	}
//	::gpk::view_const_string							evaluated								= {};
//	int32_t												jsonNodeResultOfEvaluation				= -1;
//	if(reader.Tree.size()) {
//#if defined(GPK_JSON_EXPRESSION_DEBUG)
//		::printNode(reader.Tree[0], expression);
//#endif
//		jsonNodeResultOfEvaluation						= ::evaluateExpression(databases, dbFolder, reader, 0, inputJSON, indexNodeJSON, output);
//		if errored(jsonNodeResultOfEvaluation) {
//			//gpk_necall(bufferFormat.resize(expression.size() + 1024), "%s", "Out of memory?");
//			//sprintf_s(bufferFormat.begin(), bufferFormat.size(), "Failed to evaluate expression: %%.%us.", expression.size());
//			//warning_printf(bufferFormat.begin(), expression.begin());
//			return -1;
//		}
//		evaluated										= output;
//	}
//	if(evaluated.size()) {
//		uint32_t											lenString								= evaluated.size();
//		gpk_necall(bufferFormat.resize(lenString + 1024), "%s", "Out of memory?");
//		sprintf_s(bufferFormat.begin(), bufferFormat.size(), "Result of expression evaluation: %%.%us.", lenString);
//		gpk_jexpr_info_printf(bufferFormat.begin(), evaluated.begin());
//	}
//	return jsonNodeResultOfEvaluation;
//}