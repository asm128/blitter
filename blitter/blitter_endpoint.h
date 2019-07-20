
#ifndef RAZOR_ENDPOINT_H_20190713
#define RAZOR_ENDPOINT_H_20190713

#define RAZOR_READONLY_ENDPOINT_IMPL(_configJsonFileName)																											\
GPK_CGI_JSON_APP_IMPL();																																			\
																																									\
::gpk::error_t									gpk_cgi_generate_output			(::gpk::SCGIRuntimeValues & runtimeValues, ::gpk::array_pod<char_t> & output)	{	\
	output.append(::gpk::view_const_string{"\r\n"});																												\
	::blt::SBlitter										app;																										\
	::gpk::array_obj<::gpk::TKeyValConstString>			environViews;																								\
	::gpk::environmentBlockViews(runtimeValues.EntryPointArgs.EnvironmentBlock, environViews);																		\
	::gpk::writeCGIEnvironToFile(environViews);																														\
	/*if(0 == ::gpk::keyValVerify(environViews, "REQUEST_METHOD", "GET")) {																							*/\
	/*	output.append(::gpk::view_const_string{"{ \"status\" : 403, \"description\" :\"forbidden\" }\r\n"});														*/\
	/*	return 1;																																					*/\
	/*}																																								*/\
	gpk_necall(::blt::loadConfig(app, _configJsonFileName, runtimeValues.QueryStringKeyVals, environViews), "%s", "Failed to load detail.");						\
	gpk_necall(::blt::processQuery(app.Databases, app.Query, _endpointName, output), "%s", "Failed to load razor databases.");										\
	if(output.size()) {																																				\
		OutputDebugStringA(output.begin());																															\
		OutputDebugStringA("\n");																																	\
	}																																								\
	return 0;																																						\
}

#endif // RAZOR_ENDPOINT_H_20190713
