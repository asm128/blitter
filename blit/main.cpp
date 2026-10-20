#include "blitter.h"

#include "llc_cgi_app_impl_v2.h"

#include "llc_process.h"

LLC_CGI_JSON_APP_IMPL();

::llc::err_t			jsonFail						(::llc::string & output, ::llc::vcst_t textToLog, ::llc::vcst_t status)	{
	error_printf("%s", textToLog.begin());
	output											= ::llc::vcst_t{"Status: "};
	llc_necall(output.append(status)						, "%s", "Failed to generate output message.");
	llc_necall(output.append_string("\r\nContent-Type: application/json\r\n\r\n"), "%s", "Failed to generate output message.");
	llc_necall(output.append_string("{ \"status\":")		, "%s", "Failed to generate output message.");
	llc_necall(output.append(status)						, "%s", "Failed to generate output message.");
	llc_necall(output.append_string(", \"description\" :\""), "%s", "Failed to generate output message.");
	llc_necall(output.append(textToLog)						, "%s", "Failed to generate output message.");
	llc_necall(output.append_string("\" }\r\n")				, "%s", "Failed to generate output message.");
	return 0;
}

::llc::err_t			llc_cgi_generate_output			(::llc::SCGIRuntimeValues & runtimeValues, ::llc::string & output)	{
	::llc::SHTTPAPIRequest								requestReceived					= {};
	bool												isCGIEnviron					= ::llc::httpRequestInit(requestReceived, runtimeValues, true);
	::llc::array_obj<::llc::TKeyValConstString>			environViews;
	llc_necall(::llc::environmentBlockViews(runtimeValues.EntryPointArgs.EnvironmentBlock, environViews), "%s", "If this breaks, we better know ASAP.");
	if (isCGIEnviron) {
		llc_necs(output.append_string("Content-type: application/json\r\n"));
		llc_necs(output.append_string("\r\n"));
	}

	const ::llc::vcst_t						configFileName					= "itwapi.json";
	::blt::SBlitter										app								= {};
	llc_necall(::blt::requestProcess(app.ExpressionReader, app.Query, requestReceived, app.ExpansionKeyStorage), "%s", "Failed to process request.");
	llc_necall(::blt::loadConfig(app, "./blitter.json"), "%s", "Failed to load blitter configuration.");
	const int64_t										result							= ::blt::queryProcess(app.LoadCache, app.Databases, app.ExpressionReader, app.Query, app.Folder, output);
	if_true_fef(0 > result, "%s", "Failed to load blitter databases.");
	if(output.size()) {
		OutputDebugStringA(output.begin());
		OutputDebugStringA("\n");
	}
	return 0;
}
