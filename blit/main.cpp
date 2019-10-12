#include "blitter.h"

#include "gpk_cgi_app_impl_v2.h"

#include "gpk_find.h"
#include "gpk_process.h"

GPK_CGI_JSON_APP_IMPL();

::gpk::error_t									jsonFail						(::gpk::array_pod<char_t> & output, ::gpk::view_const_string textToLog, ::gpk::view_const_string status)	{
	error_printf("%s", textToLog.begin());
	output											= ::gpk::view_const_string{"Status: "};
	gpk_necall(output.append(status)						, "%s", "Failed to generate output message.");
	gpk_necall(output.append_string("\r\nContent-Type: application/json\r\n\r\n"), "%s", "Failed to generate output message.");
	gpk_necall(output.append_string("{ \"status\":")		, "%s", "Failed to generate output message.");
	gpk_necall(output.append(status)						, "%s", "Failed to generate output message.");
	gpk_necall(output.append_string(", \"description\" :\""), "%s", "Failed to generate output message.");
	gpk_necall(output.append(textToLog)						, "%s", "Failed to generate output message.");
	gpk_necall(output.append_string("\" }\r\n")				, "%s", "Failed to generate output message.");
	return 0;
}

::gpk::error_t									gpk_cgi_generate_output			(::gpk::SCGIRuntimeValues & runtimeValues, ::gpk::array_pod<char_t> & output)	{
	::gpk::SHTTPAPIRequest								requestReceived					= {};
	bool												isCGIEnviron					= ::gpk::httpRequestInit(requestReceived, runtimeValues, true);
	::gpk::array_obj<::gpk::TKeyValConstString>			environViews;
	gpk_necall(::gpk::environmentBlockViews(runtimeValues.EntryPointArgs.EnvironmentBlock, environViews), "%s", "If this breaks, we better know ASAP.");
	if (isCGIEnviron) {
		gpk_necall(output.append(::gpk::view_const_string{"Content-type: application/json\r\n"}), "%s", "Out of memory?");
		gpk_necall(output.append(::gpk::view_const_string{"\r\n"})								, "%s", "Out of memory?");
	}

	const ::gpk::view_const_string						configFileName					= "itwapi.json";
	::blt::SBlitter										app								= {};
	gpk_necall(::blt::requestProcess(app.ExpressionReader, app.Query, requestReceived, app.ExpansionKeyStorage), "%s", "Failed to process request.");
	gpk_necall(::blt::loadConfig(app, "./blitter.json"), "%s", "Failed to load blitter configuration.");
	const int64_t										result							= ::blt::queryProcess(app.LoadCache, app.Databases, app.ExpressionReader, app.Query, app.Folder, output);
	ree_if(0 > result, "%s", "Failed to load blitter databases.");
	if(output.size()) {
		OutputDebugStringA(output.begin());
		OutputDebugStringA("\n");
	}
	return 0;
}
