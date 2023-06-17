//#include "gpk_storage.h"
//#include "gpk_json.h"
//#include "blitter.h"
//#include "blitter_process.h"
//
//::gpk::error_t									outputComposeFormById		(::gpk::array_pod<char_t> & output, ::blt::SBlitter & instance, uint32_t iTable, uint64_t idDetail, const ::gpk::vcc uriAction)				{
//	::blt::TNamedBlitterDB								& table							= instance.Databases[iTable];
//	const ::blt::STableDescription						& tableDesc						= table.Val.Description;
//	::gpk::array_obj<::gpk::array_pod<char_t>>			valuesFromDB					= {};
//	::gpk::vcc								outputRecord					= {};
//	::blt::recordGet(instance.LoadCache, table, idDetail, outputRecord, instance.Folder);
//	output											= ::gpk::view_const_string{"Content-type: text/html\r\n\r\n"};
//	::gpk::array_obj<::gpk::SHTTPResponse>				responseFromAPIs				= {};
//	::gpk::array_obj<::gpk::SJSONReader>				jsonFromAPIs					= {};
//	::blt::retrieve_all_apis(responseFromAPIs, jsonFromAPIs, instance.APIs, tableDesc.FieldMaps);
//	::gpk::array_pod<byte_t>							jsScriptFile					= {};
//	::gpk::fileToMemory(::gpk::view_const_string{"bltapi.js"}, jsScriptFile);
//	gpk_necall(output.append_string("<script type=\"text/javascript\">")	, "%s", "Out of memory?");
//	gpk_necall(output.append(jsScriptFile)									, "%s", "Out of memory?");
//	gpk_necall(output.append_string("</script>")							, "%s", "Out of memory?");
//	gpk_necall(output.append_string("<form method=\"POST\" action=\"")		, "%s", "Out of memory?");
//	gpk_necall(output.append(uriAction)										, "%s", "Out of memory?");
//	gpk_necall(output.append_string("\" onsubmit=\"formSubmit('")			, "%s", "Out of memory?");
//	gpk_necall(output.append(tableDesc.TableName)							, "%s", "Out of memory?");
//	gpk_necall(output.append_string("', 'en-US'); return false;\" ")		, "%s", "Out of memory?");
//	gpk_necall(output.append_string("\" id=\"")								, "%s", "Out of memory?");
//	gpk_necall(output.append(tableDesc.TableName)							, "%s", "Out of memory?");
//	gpk_necall(output.append_string("\">")									, "%s", "Out of memory?");
//	// -- Generate input fields
//	gpk_necall(output.append_string("<table>"), "%s", "Out of memory?");
//	for(uint32_t iField = 0; iField < tableDesc.Fields.size(); ++iField) {
//		const ::gpk::vcc						fieldName					= tableDesc.Fields[iField].Field;
//		output.append_string("\n<tr><td>");
//		output.append(fieldName);
//		output.append_string("\n</td>");
//		output.append_string("\n<td>");
//		const ::gpk::JSON_TYPE								type							= tableDesc.Fields[iField].Type;
//		const uint32_t										indexOfJSONTable				= tableDesc.IndicesFieldToMap[iField];
//		switch(type) {
//		case ::gpk::JSON_TYPE_NULL			: output.append_string("<input id=\""); output.append(fieldName); output.append_string("\" name=\""); output.append(fieldName); output.append_string("\" type=\"\"			value=\""); output.append(valuesFromDB[iField]); output.append_string("\"/>"); break;
//		case ::gpk::JSON_TYPE_INTEGER		: output.append_string("<input id=\""); output.append(fieldName); output.append_string("\" name=\""); output.append(fieldName); output.append_string("\" type=\"number\"	value=\""); output.append(valuesFromDB[iField]); output.append_string("\"/>"); break;
//		case ::gpk::JSON_TYPE_DOUBLE		: output.append_string("<input id=\""); output.append(fieldName); output.append_string("\" name=\""); output.append(fieldName); output.append_string("\" type=\"number\"	value=\""); output.append(valuesFromDB[iField]); output.append_string("\"/>"); break;
//		case ::gpk::JSON_TYPE_BOOL			: output.append_string("<input id=\""); output.append(fieldName); output.append_string("\" name=\""); output.append(fieldName); output.append_string("\" type=\"checkbox\"	value=\""); output.append(valuesFromDB[iField]); output.append_string("\"/>"); break;
//		case ::gpk::JSON_TYPE_ARRAY			: output.append_string("<input id=\""); output.append(fieldName); output.append_string("\" name=\""); output.append(fieldName); output.append_string("\" type=\"\"			value=\""); output.append(valuesFromDB[iField]); output.append_string("\"/>"); break;
//		case ::gpk::JSON_TYPE_OBJECT		:
//			output.append_string("<select id=\"");
//			output.append(fieldName);
//			output.append_string("\" name=\"");
//			output.append(fieldName);
//			output.append_string("\" >");
//			{
//				if(-1 == indexOfJSONTable) {
//					output.append_string("</select>");
//					continue;
//				}
//				const ::gpk::SJSONReader						& fieldReader					= jsonFromAPIs[indexOfJSONTable];
//				if(-1 == indexOfJSONTable || 0 == fieldReader.Tree.size()) {
//					output.append_string("</select>");
//					continue;
//				}
//				for(uint32_t iRecord = 0, countRecords = ::gpk::jsonArraySize(fieldReader, 0); iRecord < countRecords; ++iRecord) {
//					const int32_t								indexOfItemNode					= ::gpk::jsonArrayValueGet(fieldReader, 0, iRecord);
//					if(0 > indexOfItemNode)
//						continue;
//					const int32_t								indexOfNameValueNode			= ::gpk::jsonObjectValueGet(fieldReader, indexOfItemNode, ::gpk::view_const_string{"name"	});
//					const int32_t								indexOfIdNode					= ::gpk::jsonObjectValueGet(fieldReader, indexOfItemNode, ::gpk::view_const_string{"id"		});
//					if(0 > indexOfNameValueNode || 0 > indexOfIdNode)
//						continue;
//					output.append_string("<option value=\"");
//					const ::gpk::vcc				idCurrentItem					= fieldReader.View[indexOfIdNode];
//					output.append(idCurrentItem);
//					output.append_string("\" ");
//					if(idCurrentItem == valuesFromDB[iField])
//						output.append_string("selected");
//					output.append_string(" >");
//					output.append(fieldReader.View[indexOfNameValueNode]);
//					output.append_string("</option>");
//				}
//			}
//			output.append_string("</select>");
//			break;
//		case ::gpk::JSON_TYPE_STRING		:
//			if((-1 == tableDesc.IndicesFieldToMap[iField])) {
//				output.append_string("<input  id=\"");
//				output.append(tableDesc.Fields[iField].Field);
//				output.append_string("\" name=\"");
//				output.append(fieldName);
//				output.append_string("\" type=\"text\" value=\"");
//				output.append(valuesFromDB[iField]);
//				output.append_string("\" />");
//			}
//			else {
//				output.append_string("<select id=\"");
//				output.append(fieldName);
//				output.append_string("\" name=\"");
//				output.append(fieldName);
//				output.append_string("\" >");
//				{
//					if(-1 == indexOfJSONTable) {
//						output.append_string("</select>");
//						continue;
//					}
//					const ::gpk::SJSONReader						& fieldReader					= jsonFromAPIs[indexOfJSONTable];
//					if(-1 == indexOfJSONTable || 0 == fieldReader.Tree.size()) {
//						output.append_string("</select>");
//						continue;
//					}
//					for(uint32_t iRecord = 0, countRecords = ::gpk::jsonArraySize(fieldReader, 0); iRecord < countRecords; ++iRecord) {
//						const int32_t								indexOfItemNode					= ::gpk::jsonArrayValueGet(fieldReader, 0, iRecord);
//						if(0 > indexOfItemNode)
//							continue;
//						const int32_t								indexOfNameValueNode			= ::gpk::jsonObjectValueGet(fieldReader, indexOfItemNode, ::gpk::view_const_string{"name"	});
//						const int32_t								indexOfIdNode					= ::gpk::jsonObjectValueGet(fieldReader, indexOfItemNode, ::gpk::view_const_string{"id"		});
//						if(0 > indexOfNameValueNode || 0 > indexOfIdNode)
//							continue;
//						output.append_string("<option value=\"");
//						const ::gpk::vcc				idCurrentItem					= fieldReader.View[indexOfIdNode];
//						output.append(idCurrentItem);
//						output.append_string("\" ");
//						if(idCurrentItem == valuesFromDB[iField])
//							output.append_string("selected");
//						output.append_string(" >");
//						output.append(fieldReader.View[indexOfNameValueNode]);
//						output.append_string("</option>");
//					}
//				}
//				output.append_string("</select>");
//			}
//			break;
//		default:
//			info_printf("Unhandled type: %s.", ::gpk::get_value_label(type).begin());
//			output.append_string("null");
//			break;
//		}
//		output.append_string("\n</td></tr>");
//	}
//	gpk_necall(output.append_string("</table><input type=\"submit\" value=\"Save\"/></form>"), "%s", "Out of memory?");
//	return 0;
//}
//
//::gpk::error_t									outputComposeForm			(::gpk::array_pod<char_t> & output, ::blt::SBlitter & instance, uint32_t iTable, const ::gpk::vcc uriAction)				{
//	const ::blt::STableDescription						& tableDesc						= instance.Databases[iTable].Val.Description;
//	output											= ::gpk::view_const_string{"Content-type: text/html\r\n\r\n"};
//	::gpk::array_pod<byte_t>							jsScriptFile					= {};
//	::gpk::fileToMemory(::gpk::view_const_string{"bltapi.js"}, jsScriptFile);
//	gpk_necall(output.append_string("<script type=\"text/javascript\">")	, "%s", "Out of memory?");
//	gpk_necall(output.append(jsScriptFile)									, "%s", "Out of memory?");
//	gpk_necall(output.append_string("</script>")							, "%s", "Out of memory?");
//	gpk_necall(output.append_string("<form method=\"POST\" action=\"")		, "%s", "Out of memory?");
//	gpk_necall(output.append(uriAction)										, "%s", "Out of memory?");
//	gpk_necall(output.append_string("\" onsubmit=\"formSubmit('")			, "%s", "Out of memory?");
//	gpk_necall(output.append(tableDesc.TableName)							, "%s", "Out of memory?");
//	gpk_necall(output.append_string("', 'en-US'); return false;\" ")		, "%s", "Out of memory?");
//	gpk_necall(output.append_string("\" id=\"")								, "%s", "Out of memory?");
//	gpk_necall(output.append(tableDesc.TableName)							, "%s", "Out of memory?");
//	gpk_necall(output.append_string("\">")									, "%s", "Out of memory?");
//	gpk_necall(output.append_string("<table>")								, "%s", "Out of memory?");
//	::gpk::array_obj<::gpk::SHTTPResponse>				responseFromAPIs				= {};
//	::gpk::array_obj<::gpk::SJSONReader>				jsonFromAPIs					= {};
//	::blt::retrieve_all_apis(responseFromAPIs, jsonFromAPIs, instance.APIs, tableDesc.FieldMaps);
//	for(uint32_t iField = 0; iField < tableDesc.Fields.size(); ++iField) {
//		const ::gpk::vcc						fieldName					= tableDesc.Fields[iField].Field;
//		output.append_string("\n<tr><td>");
//		output.append(fieldName);
//		output.append_string("\n</td>");
//		output.append_string("\n<td>");
//		if(fieldName == ::gpk::view_const_string{"id"})
//			output.append_string("<p id=\"id\">?</p>");
//		else {
//			const ::gpk::JSON_TYPE								type							= tableDesc.Fields[iField].Type;
//			const uint32_t										indexOfJSONTable				= tableDesc.IndicesFieldToMap[iField];
//			switch(type) {
//			case ::gpk::JSON_TYPE_NULL			: output.append_string("<input id=\""); output.append(fieldName); output.append_string("\" name=\""); output.append(fieldName); output.append_string("\" type=\"\"			value=\"\"		/>"); break;
//			case ::gpk::JSON_TYPE_INTEGER		: output.append_string("<input id=\""); output.append(fieldName); output.append_string("\" name=\""); output.append(fieldName); output.append_string("\" type=\"number\"	value=\"0\"		/>"); break;
//			case ::gpk::JSON_TYPE_DOUBLE		: output.append_string("<input id=\""); output.append(fieldName); output.append_string("\" name=\""); output.append(fieldName); output.append_string("\" type=\"number\"	value=\"0.0\"	/>"); break;
//			case ::gpk::JSON_TYPE_BOOL			: output.append_string("<input id=\""); output.append(fieldName); output.append_string("\" name=\""); output.append(fieldName); output.append_string("\" type=\"checkbox\"	value=\"\"		/>"); break;
//			case ::gpk::JSON_TYPE_ARRAY			: output.append_string("<input id=\""); output.append(fieldName); output.append_string("\" name=\""); output.append(fieldName); output.append_string("\" type=\"\"			value=\"\"		/>"); break;
//			case ::gpk::JSON_TYPE_OBJECT		:
//				output.append_string("<select id=\"");
//				output.append(fieldName);
//				output.append_string("\" name=\"");
//				output.append(fieldName);
//				output.append_string("\" >");
//				{
//					if(-1 == indexOfJSONTable) {
//						output.append_string("</select>");
//						continue;
//					}
//					const ::gpk::SJSONReader						& fieldReader					= jsonFromAPIs[indexOfJSONTable];
//					if(-1 == indexOfJSONTable || 0 == fieldReader.Tree.size()) {
//						output.append_string("</select>");
//						continue;
//					}
//					for(uint32_t iRecord = 0, countRecords = ::gpk::jsonArraySize(fieldReader, 0); iRecord < countRecords; ++iRecord) {
//						int32_t										indexOfItemNode					= ::gpk::jsonArrayValueGet(fieldReader, 0, iRecord);
//						if(0 > indexOfItemNode)
//							continue;
//						::gpk::vcc						viewOfItem						= fieldReader.View[indexOfItemNode];
//						int32_t										indexOfNameValueNode			= ::gpk::jsonObjectValueGet(fieldReader, indexOfItemNode, ::gpk::view_const_string{"name"	});
//						int32_t										indexOfIdNode					= ::gpk::jsonObjectValueGet(fieldReader, indexOfItemNode, ::gpk::view_const_string{"id"		});
//						if(0 > indexOfNameValueNode || 0 > indexOfIdNode)
//							continue;
//						output.append_string("<option value=\"");
//						output.append(jsonFromAPIs[indexOfJSONTable].View[indexOfIdNode]);
//						output.append_string("\" >");
//						output.append(jsonFromAPIs[indexOfJSONTable].View[indexOfNameValueNode]);
//						output.append_string("</option>");
//					}
//				}
//				output.append_string("</select>");
//				break;
//			case ::gpk::JSON_TYPE_STRING		:
//				if((-1 == tableDesc.IndicesFieldToMap[iField])) {
//					output.append_string("<input  id=\"");
//					output.append(fieldName);
//					output.append_string("\" name=\"");
//					output.append(fieldName);
//					output.append_string("\" type=\"text\">");
//				}
//				else {
//					output.append_string("<select id=\"");
//					output.append(fieldName);
//					output.append_string("\" name=\"");
//					output.append(fieldName);
//					output.append_string("\" >");
//					{
//						if(-1 == indexOfJSONTable) {
//							output.append_string("</select>");
//							continue;
//						}
//						const ::gpk::SJSONReader						& fieldReader					= jsonFromAPIs[indexOfJSONTable];
//						if(-1 == indexOfJSONTable || 0 == fieldReader.Tree.size()) {
//							output.append_string("</select>");
//							continue;
//						}
//						for(uint32_t iRecord = 0, countRecords = ::gpk::jsonArraySize(fieldReader, 0); iRecord < countRecords; ++iRecord) {
//							int32_t										indexOfItemNode					= ::gpk::jsonArrayValueGet(fieldReader, 0, iRecord);
//							if(0 > indexOfItemNode)
//								continue;
//							::gpk::vcc						viewOfItem						= fieldReader.View[indexOfItemNode];
//							int32_t										indexOfNameValueNode			= ::gpk::jsonObjectValueGet(fieldReader, indexOfItemNode, ::gpk::view_const_string{"name"	});
//							int32_t										indexOfIdNode					= ::gpk::jsonObjectValueGet(fieldReader, indexOfItemNode, ::gpk::view_const_string{"id"		});
//							if(0 > indexOfNameValueNode || 0 > indexOfIdNode)
//								continue;
//							output.append_string("<option value=\"");
//							output.append(jsonFromAPIs[indexOfJSONTable].View[indexOfIdNode]);
//							output.append_string("\" >");
//							output.append(jsonFromAPIs[indexOfJSONTable].View[indexOfNameValueNode]);
//							output.append_string("</option>");
//						}
//					}
//					output.append_string("</select>");
//				}
//				break;
//			default:
//				info_printf("Unhandled type: %s.", ::gpk::get_value_label(type).begin());
//				output.append_string("null");
//				break;
//			}
//		}
//		output.append_string("\n</td></tr>");
//	}
//	gpk_necall(output.append_string("</table><input type=\"submit\" value=\"Add\"/></form>"), "%s", "Out of memory?");
//	return 0;
//}
