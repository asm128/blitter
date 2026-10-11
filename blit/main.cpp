#include "blitter.h"

#include "gpk_cgi_app_impl_v2.h"

#include "gpk_process.h"

GPK_CGI_JSON_APP_IMPL();

::gpk::error_t			jsonFail						(::gpk::achar & output, ::gpk::vcs textToLog, ::gpk::vcs status)	{
	error_printf("%s", textToLog.begin());
	output											= ::gpk::vcs{"Status: "};
	if_fail_fef(output.append(status)						, "%s", "Failed to generate output message.");
	if_fail_fef(output.append_string("\r\nContent-Type: application/json\r\n\r\n"), "%s", "Failed to generate output message.");
	if_fail_fef(output.append_string("{ \"status\":")		, "%s", "Failed to generate output message.");
	if_fail_fef(output.append(status)						, "%s", "Failed to generate output message.");
	if_fail_fef(output.append_string(", \"description\" :\""), "%s", "Failed to generate output message.");
	if_fail_fef(output.append(textToLog)						, "%s", "Failed to generate output message.");
	if_fail_fef(output.append_string("\" }\r\n")				, "%s", "Failed to generate output message.");
	return 0;
}

::gpk::error_t			gpk_cgi_generate_output			(::gpk::SCGIRuntimeValues & runtimeValues, ::gpk::achar & output)	{
	::gpk::SHTTPAPIRequest								requestReceived					= {};
	bool												isCGIEnviron					= ::gpk::httpRequestInit(requestReceived, runtimeValues, true);
	::gpk::array_obj<::gpk::TKeyValConstString>			environViews;
	if_fail_fef(::gpk::environmentBlockViews(runtimeValues.EntryPointArgs.EnvironmentBlock, environViews), "%s", "If this breaks, we better know ASAP.");
	if (isCGIEnviron) {
		if_fail_fe(output.append_string("Content-type: application/json\r\n"));
		if_fail_fe(output.append_string("\r\n"));
	}

	const ::gpk::vcs						configFileName					= "itwapi.json";
	::blt::SBlitter										app								= {};
	if_fail_fef(::blt::requestProcess(app.ExpressionReader, app.Query, requestReceived, app.ExpansionKeyStorage), "%s", "Failed to process request.");
	if_fail_fef(::blt::loadConfig(app, "./blitter.json"), "%s", "Failed to load blitter configuration.");
	const int64_t										result							= ::blt::queryProcess(app.LoadCache, app.Databases, app.ExpressionReader, app.Query, app.Folder, output);
	ree_if(0 > result, "%s", "Failed to load blitter databases.");
	if(output.size()) {
		OutputDebugStringA(output.begin());
		OutputDebugStringA("\n");
	}
	return 0;
}
