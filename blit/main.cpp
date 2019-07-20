#include "blitter.h"

#include "gpk_cgi_app_impl_v2.h"

#include "gpk_stdstring.h"
#include "gpk_find.h"
#include "gpk_process.h"

::gpk::error_t										requestProcess		(::blt::SBlitterQuery & query, const ::blt::SBlitterRequest & request)						{
	{	// --- Retrieve query from request.
		::gpk::array_obj<::gpk::TKeyValConstString>						qsKeyVals;
		::gpk::array_obj<::gpk::view_const_string>						queryStringElements			= {};
		::gpk::querystring_split({request.QueryString.begin(), request.QueryString.size()}, queryStringElements);
		qsKeyVals.resize(queryStringElements.size());
		for(uint32_t iKeyVal = 0; iKeyVal < qsKeyVals.size(); ++iKeyVal) {
			::gpk::TKeyValConstString										& keyValDst					= qsKeyVals[iKeyVal];
			::gpk::keyval_split(queryStringElements[iKeyVal], keyValDst);
		}
		::blt::queryLoad(query, qsKeyVals);
	}
	// --- Generate response
	query.Database												= (request.Path.size() > 1) ? ::gpk::view_const_string{&request.Path[1], request.Path.size() - 1} : ::gpk::view_const_string{};;
	uint64_t														detail						= (uint64_t)-1LL;
	{	// --- Retrieve detail part 
		::gpk::view_const_string										strDetail					= {};
		const ::gpk::error_t											indexOfLastBar				= ::gpk::rfind('/', query.Database);
		const uint32_t													startOfDetail				= (uint32_t)(indexOfLastBar + 1);
		if(indexOfLastBar > 0 && startOfDetail < query.Database.size()) {
			strDetail													= {&query.Database[startOfDetail], query.Database.size() - startOfDetail};
			query.Database												= {query.Database.begin(), (uint32_t)indexOfLastBar};
			if(strDetail.size()) {
				::gpk::stoull(strDetail, &detail);
				query.Detail										= (int64_t)detail;
			}
		}
	}
	return 0;
}

GPK_CGI_JSON_APP_IMPL();

::gpk::error_t									gpk_cgi_generate_output			(::gpk::SCGIRuntimeValues & runtimeValues, ::gpk::array_pod<char_t> & output)	{	
	output.append(::gpk::view_const_string{"\r\n"});	
	::blt::SBlitter										app;
	::gpk::array_obj<::gpk::TKeyValConstString>			environViews;
	::gpk::environmentBlockViews(runtimeValues.EntryPointArgs.EnvironmentBlock, environViews);	
	::gpk::writeCGIEnvironToFile(environViews);
	::blt::SBlitterRequest								requestReceived;
	{
		requestReceived.Method							= 0 == ::gpk::keyValVerify(environViews, "REQUEST_METHOD", "GET") ? ::gpk::HTTP_METHOD_POST : ::gpk::HTTP_METHOD_GET;
		::gpk::find("PATH_INFO"		, environViews, requestReceived.Path);
		::gpk::find("QUERY_STRING"	, environViews, requestReceived.QueryString);
		requestReceived.ContentBody							= runtimeValues.Content.Body;
	}
	if(0 == requestReceived.Path.size() && runtimeValues.EntryPointArgs.ArgsCommandLine.size() > 1) {
		requestReceived.Path	= ::gpk::view_const_string{runtimeValues.EntryPointArgs.ArgsCommandLine[1], (uint32_t)-1};
	}
	gpk_necall(::requestProcess(app.Query, requestReceived), "%s", "Failed to process request.");
	//requestReceived.Path;
	/*if(0 == ::gpk::keyValVerify(environViews, "REQUEST_METHOD", "GET")) {										*/
	/*	output.append(::gpk::view_const_string{"{ \"status\" : 403, \"description\" :\"forbidden\" }\r\n"});	*/
	/*	return 1;																								*/
	/*}																											*/
	gpk_necall(::blt::loadConfig(app, "blitter.json"), "%s", "Failed to load detail.");						
	gpk_necall(::blt::processQuery(app.Databases, app.Query, output, app.Folder), "%s", "Failed to load razor databases.");	
	if(output.size()) {						
		OutputDebugStringA(output.begin());	
		OutputDebugStringA("\n");			
	}										
	return 0;								
}

