#include "blitter.h"

#include "gpk_cgi_app_impl_v2.h"

#include "gpk_find.h"
#include "gpk_process.h"

GPK_CGI_JSON_APP_IMPL();

::gpk::error_t									gpk_cgi_generate_output			(::gpk::SCGIRuntimeValues & runtimeValues, ::gpk::array_pod<char_t> & output)	{
	::gpk::array_obj<::gpk::TKeyValConstString>			environViews;
	gpk_necall(::gpk::environmentBlockViews(runtimeValues.EntryPointArgs.EnvironmentBlock, environViews), "%s", "If this breaks, we better know ASAP.");
	{	// Find out if the program is being called as a CGI script.
		::gpk::view_const_string							remoteAddr;
		if(-1 != ::gpk::find("REMOTE_ADDR", environViews, remoteAddr)) {
			::gpk::writeCGIEnvironToFile(environViews);
			gpk_necall(output.append(::gpk::view_const_string{"Content-type: application/json\r\n"}), "%s", "Out of memory?");
			gpk_necall(output.append(::gpk::view_const_string{"\r\n"})								, "%s", "Out of memory?");
			if(0 == ::gpk::keyValVerify(environViews, "REQUEST_METHOD", "GET")) {
				output.append(::gpk::view_const_string{"{ \"status\" : 403, \"description\" :\"forbidden\" }\r\n"});
				return 1;
			}
		}
	}

	::blt::SBlitter										app								= {};
	::blt::SBlitterRequest								requestReceived					= {};
	{	// Try to load query from querystring and request body
		requestReceived.Method							= 0 == ::gpk::keyValVerify(environViews, "REQUEST_METHOD", "POST") ? ::gpk::HTTP_METHOD_GET : ::gpk::HTTP_METHOD_POST;
		::gpk::find("PATH_INFO"		, environViews, requestReceived.Path);
		::gpk::find("QUERY_STRING"	, environViews, requestReceived.QueryString);
		requestReceived.ContentBody						= runtimeValues.Content.Body;
	}
	if(0 == requestReceived.Path.size() && runtimeValues.EntryPointArgs.ArgsCommandLine.size() > 1) {	// Get query from command line instead of CGI environ
		requestReceived.Path							= ::gpk::view_const_string{runtimeValues.EntryPointArgs.ArgsCommandLine[1], (uint32_t)-1};
		if(requestReceived.Path.size() > 2) {
			if('"' == requestReceived.Path[0])
				requestReceived.Path							= {&requestReceived.Path[1], requestReceived.Path.size() - 2};

			const int32_t										queryStringStart					= ::gpk::find('?', requestReceived.Path);
			if(0 <= queryStringStart) {
				requestReceived.QueryString						= {&requestReceived.Path[queryStringStart + 1], requestReceived.Path.size() - (uint32_t)queryStringStart - 1};
				requestReceived.Path							= {&requestReceived.Path[0], (uint32_t)queryStringStart};
			}
		}
	}
	gpk_necall(::blt::requestProcess(app.ExpressionReader, app.Query, requestReceived, app.ExpansionKeyStorage), "%s", "Failed to process request.");
	gpk_necall(::blt::loadConfig(app, "./blitter.json"), "%s", "Failed to load blitter configuration.");
	gpk_necall(::blt::queryProcess(app.Databases, app.ExpressionReader, app.Query, app.Folder, output), "%s", "Failed to load blitter databases.");
	if(output.size()) {
		OutputDebugStringA(output.begin());
		OutputDebugStringA("\n");
	}
	return 0;
}
