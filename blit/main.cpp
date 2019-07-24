#include "blitter.h"

#include "gpk_cgi_app_impl_v2.h"

#include "gpk_stdstring.h"
#include "gpk_find.h"
#include "gpk_process.h"

::gpk::error_t									requestProcess				(::blt::SBlitterQuery & query, const ::blt::SBlitterRequest & request, ::gpk::array_obj<::gpk::view_const_string> & expansionKeyStorage)						{
	{	// --- Retrieve query from request.
		::gpk::array_obj<::gpk::TKeyValConstString>			qsKeyVals;
		::gpk::array_obj<::gpk::view_const_string>			queryStringElements			= {};
		gpk_necall(::gpk::querystring_split({request.QueryString.begin(), request.QueryString.size()}, queryStringElements), "%s", "Out of memory?");
		gpk_necall(qsKeyVals.resize(queryStringElements.size()), "%s", "Out of memory?");
		for(uint32_t iKeyVal = 0; iKeyVal < qsKeyVals.size(); ++iKeyVal) {
			::gpk::TKeyValConstString							& keyValDst					= qsKeyVals[iKeyVal];
			::gpk::keyval_split(queryStringElements[iKeyVal], keyValDst);
		}
		gpk_necall(::blt::queryLoad(query, qsKeyVals, expansionKeyStorage), "%s", "Out of memory?");
	}
	// --- Generate response
	query.Database									= (request.Path.size() > 1)
		? (('/' == request.Path[0]) ? ::gpk::view_const_string{&request.Path[1], request.Path.size() - 1} : ::gpk::view_const_string{request.Path.begin(), request.Path.size()})
		: ::gpk::view_const_string{}
		;
	uint64_t											detail							= (uint64_t)-1LL;
	{	// --- Retrieve detail part
		::gpk::view_const_string							strDetail						= {};
		const ::gpk::error_t								indexOfLastBar					= ::gpk::rfind('/', query.Database);
		const uint32_t										startOfDetail					= (uint32_t)(indexOfLastBar + 1);
		if(indexOfLastBar > 0 && startOfDetail < query.Database.size()) {
			strDetail										= {&query.Database[startOfDetail], query.Database.size() - startOfDetail};
			query.Database									= {query.Database.begin(), (uint32_t)indexOfLastBar};
			if(strDetail.size()) {
				::gpk::stoull(strDetail, &detail);
				query.Detail									= (int64_t)detail;
			}
		}
	}
	const ::gpk::error_t								indexPath					= ::gpk::find('.', query.Database);
	if(((uint32_t)indexPath + 1) < query.Database.size())
		query.Path										= {&query.Database[indexPath + 1]	, query.Database.size() - (indexPath + 1)};

	if(0 <= indexPath)
		query.Database									= {query.Database.begin()			, (uint32_t)indexPath};

	return 0;
}

GPK_CGI_JSON_APP_IMPL();

::gpk::error_t									gpk_cgi_generate_output			(::gpk::SCGIRuntimeValues & runtimeValues, ::gpk::array_pod<char_t> & output)	{
	::blt::SBlitter										app;
	::gpk::array_obj<::gpk::TKeyValConstString>			environViews;
	gpk_necall(::gpk::environmentBlockViews(runtimeValues.EntryPointArgs.EnvironmentBlock, environViews), "%s", "If this breaks, we better know ASAP.");
	::gpk::writeCGIEnvironToFile(environViews);
	::blt::SBlitterRequest								requestReceived;
	{
		requestReceived.Method							= 0 == ::gpk::keyValVerify(environViews, "REQUEST_METHOD", "GET") ? ::gpk::HTTP_METHOD_POST : ::gpk::HTTP_METHOD_GET;
		::gpk::find("PATH_INFO"		, environViews, requestReceived.Path);
		::gpk::find("QUERY_STRING"	, environViews, requestReceived.QueryString);
		requestReceived.ContentBody							= runtimeValues.Content.Body;
	}
	if(0 == requestReceived.Path.size() && runtimeValues.EntryPointArgs.ArgsCommandLine.size() > 1) {
		requestReceived.Path							= ::gpk::view_const_string{runtimeValues.EntryPointArgs.ArgsCommandLine[1], (uint32_t)-1};
		if(requestReceived.Path.size() > 2) {
			if('"' == requestReceived.Path[0])
				requestReceived.Path							= {&requestReceived.Path[1], requestReceived.Path.size() - 2};

			int32_t												queryStringStart					= ::gpk::find('?', requestReceived.Path);
			if(0 <= queryStringStart) {
				requestReceived.QueryString						= {&requestReceived.Path[queryStringStart + 1], requestReceived.Path.size() - (uint32_t)queryStringStart - 1};
				requestReceived.Path							= {&requestReceived.Path[0], (uint32_t)queryStringStart};
			}
		}
	}

	gpk_necall(::requestProcess(app.Query, requestReceived, app.ExpansionKeyStorage), "%s", "Failed to process request.");
	//if(0 == ::gpk::keyValVerify(environViews, "REQUEST_METHOD", "GET")) {
	//	output.append(::gpk::view_const_string{"{ \"status\" : 403, \"description\" :\"forbidden\" }\r\n"});
	//	return 1;
	//}
	gpk_necall(::blt::loadConfig(app, "./blitter.json"), "%s", "Failed to load blitter configuration.");
	::gpk::view_const_string							remoteAddr;
	if(-1 != ::gpk::find("REMOTE_ADDR", environViews, remoteAddr)) {
		gpk_necall(output.append(::gpk::view_const_string{"Content-type: application/json\r\n"}), "%s", "Out of memory?");
		gpk_necall(output.append(::gpk::view_const_string{"\r\n"})								, "%s", "Out of memory?");
	}
	gpk_necall(::blt::queryProcess(app.Databases, app.Query, output, app.Folder), "%s", "Failed to load blitter databases.");
	if(output.size()) {
		OutputDebugStringA(output.begin());
		OutputDebugStringA("\n");
	}
	return 0;
}

