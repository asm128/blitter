#define GPK_CONSOLE_LOG_ENABLED
#define GPK_ERROR_PRINTF_ENABLED
#define GPK_WARNING_PRINTF_ENABLED
#define GPK_INFO_PRINTF_ENABLED

#include "blitter.h"

#include "gpk_find.h"
#include "gpk_process.h"

int												testQuery						(::blt::SBlitter & app, ::gpk::array_pod<char_t> & output, ::gpk::view_const_string command, ::gpk::view_const_string database, int64_t detail, ::gpk::view_const_string path, ::gpk::SRange<uint64_t> range, ::gpk::view_const_string expand)		{
	output.clear();
	app.Query.Command								= command;
	app.Query.Database								= database;
	app.Query.Detail								= detail;
	app.ExpansionKeyStorage.clear();
	gpk_necall(::gpk::split(::gpk::view_const_char{expand}, '.', app.ExpansionKeyStorage), "%s", "Out of memory?");
	app.Query.ExpansionKeys							= app.ExpansionKeyStorage;
	app.Query.Path									= path;
	app.Query.Range									= range;
	gpk_necall(::blt::queryProcess(app.Databases, app.ExpressionReader, app.Query, app.Folder, output), "%s", "Execution of query failed.");
	if(output.size()) {
		output.push_back(0);
		info_printf("%s", output.begin());
		OutputDebugStringA(output.begin());
		OutputDebugStringA("\n");
	}
	return 0;
}

int												main							()		{
	::blt::SBlitter										app								= {};
	gpk_necall(::blt::loadConfig(app, "./blitter.json"), "%s", "Failed to load blitter configuration.");
	::gpk::array_pod<char_t>							output							= {};
	::testQuery(app, output, "get", "user"		, -1	, "", {10	, 20}, "referral.referral"			);
	::testQuery(app, output, "get", "user"		, 0		, "", {10	, 20}, "website"					);
	::testQuery(app, output, "get", "user"		, 10	, "", {10	, 20}, "referral.referral"			);
	::testQuery(app, output, "get", "website"	, -1	, "", {1	, 20}, "publisher"					);
	::testQuery(app, output, "get", "website"	, 0		, "", {10	, 20}, "publisher.company"			);
	::testQuery(app, output, "get", "website"	, 3		, "", {10	, 20}, "publisher.company.owner"	);
	::testQuery(app, output, "get", "publisher"	, -1	, "", {1	, 20}, ""							);
	::testQuery(app, output, "get", "publisher"	, 0		, "", {10	, 20}, "company"					);
	::testQuery(app, output, "get", "publisher"	, 3		, "", {10	, 20}, "company.owner"				);
	::testQuery(app, output, "get", "company"	, -1	, "", {1	, 20}, ""							);
	::testQuery(app, output, "get", "company"	, 0		, "", {10	, 20}, ""							);
	::testQuery(app, output, "get", "company"	, 3		, "", {10	, 20}, "owner"						);
	return 0;
}

