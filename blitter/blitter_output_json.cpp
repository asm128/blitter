#include "blitter_output.h"

::gpk::error_t									blt::outputModel			(::gpk::array_pod<char_t> & output, const ::blt::STableDescription & tableDesc)				{
	gpk_necall(output.push_back('{'), "%s", "Out of memory?");
	for(uint32_t iField = 0; iField < tableDesc.Fields.size(); ++iField) {
		::gpk::append_quoted(output, tableDesc.Fields[iField].Field);
		gpk_necall(output.push_back(':'), "%s", "Out of memory?");
		const ::gpk::JSON_TYPE							type							= tableDesc.Fields[iField].Type;
		switch(type) {
		case ::gpk::JSON_TYPE_NULL			: output.append_string("null");		break;
		case ::gpk::JSON_TYPE_INTEGER		: output.append_string("0");		break;
		case ::gpk::JSON_TYPE_DOUBLE		: output.append_string("0.0");		break;
		case ::gpk::JSON_TYPE_BOOL			: output.append_string("false");	break;
		case ::gpk::JSON_TYPE_ARRAY			: output.append_string("[]");		break;
		case ::gpk::JSON_TYPE_OBJECT		: output.append_string("{}");		break;
		case ::gpk::JSON_TYPE_STRING		:
			if(-1 == tableDesc.IndicesFieldToMap[iField])
				::gpk::append_quoted(output, {});
			else
				output.append_string("-1");
			break;
		default:
			info_printf("Unhandled type: %s.", ::gpk::get_value_label(type).begin());
			output.append_string("null");
			break;
		}
		if(iField + 1 < tableDesc.Fields.size())
			output.push_back(',');
	}
	gpk_necall(output.push_back('}'), "%s", "Out of memory?");
	return 0;
}
