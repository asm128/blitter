#define GPK_CONSOLE_LOG_ENABLED
#define GPK_ERROR_PRINTF_ENABLED
#define GPK_WARNING_PRINTF_ENABLED
#define GPK_INFO_PRINTF_ENABLED

#include "blitter.h"

#include "gpk_timer.h"
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
	const int64_t										result							= ::blt::queryProcess(app.LoadCache, app.Databases, app.ExpressionReader, app.Query, app.Folder, output);
	ree_if(0 > result, "%s", "Execution of query failed.");
	if(output.size()) {
		output.push_back(0);
		if(output.size() < 1024*3)
			info_printf("command: %s, Database: %s, Detail: %lli, Path: %s, Range: %u, %u, Output: %s"
				, ::gpk::toString(app.Query.Command			).begin()
				, ::gpk::toString(app.Query.Database		).begin()
				, app.Query.Detail
				, ::gpk::toString(app.Query.Path			).begin()
				, app.Query.Range.Offset
				, app.Query.Range.Count
				, output.begin()
				);
		OutputDebugStringA(output.begin());
		OutputDebugStringA("\n");
	}
	return 0;
}

int												testPush						(::blt::SBlitter & app, ::gpk::array_pod<char_t> & output, ::gpk::view_const_string command, ::gpk::view_const_string database, ::gpk::view_const_string record)		{
	output.clear();
	app.Query.Command								= command;
	app.Query.Database								= database;
	app.Query.Record								= record;
	app.Query.RecordReader.Reset();
	::gpk::jsonParse(app.Query.RecordReader, record);
	const int64_t										result							= ::blt::queryProcess(app.LoadCache, app.Databases, app.ExpressionReader, app.Query, app.Folder, output);
	ree_if(0 > result, "%s", "Execution of query failed.");
	if(output.size()) {
		output.push_back(0);
		if(output.size() < 1024*3)
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
	//::testQuery(app, output, "get", "user"		, -1	, "", {10	, 20}, "referral.referral"			);
	//::testQuery(app, output, "get", "user"		, 0		, "", {10	, 20}, "website"					);
	//::testQuery(app, output, "get", "user"		, 10	, "", {10	, 20}, "referral.referral"			);
	//::testQuery(app, output, "get", "website"	, -1	, "", {1	, 20}, "publisher"					);
	//::testQuery(app, output, "get", "website"	, 0		, "", {10	, 20}, "publisher.company"			);
	//::testQuery(app, output, "get", "website"	, 3		, "", {10	, 20}, "publisher.company.owner"	);
	//::testQuery(app, output, "get", "publisher"	, -1	, "", {1	, 20}, ""							);
	//::testQuery(app, output, "get", "publisher"	, 0		, "", {10	, 20}, "company"					);
	//::testQuery(app, output, "get", "publisher"	, 3		, "", {10	, 20}, "company.owner"				);
	//::testQuery(app, output, "get", "company"	, -1	, "", {1	, 20}, ""							);
	//::testQuery(app, output, "get", "company"	, 0		, "", {10	, 20}, ""							);
	//::testQuery(app, output, "get", "company"	, 3		, "", {10	, 20}, "owner"						);
	//
	//::testQuery(app, output, "get", "user"		, -1	, "", {0	, 10000}, "referral.referral"			);
	//::testQuery(app, output, "get", "user"		, 0		, "", {1	, 10000}, "website"						);
	//::testQuery(app, output, "get", "user"		, 10	, "", {0	, 10000}, "referral.referral"			);
	//::testQuery(app, output, "get", "website"	, -1	, "", {1	, 10000}, "publisher"					);
	//::testQuery(app, output, "get", "website"	, 0		, "", {0	, 10000}, "publisher.company"			);
	//::testQuery(app, output, "get", "website"	, 3		, "", {1	, 10000}, "publisher.company.owner"		);
	//::testQuery(app, output, "get", "publisher"	, -1	, "", {0	, 10000}, ""							);
	//::testQuery(app, output, "get", "publisher"	, 0		, "", {1	, 10000}, "company"						);
	//::testQuery(app, output, "get", "publisher"	, 3		, "", {0	, 10000}, "company.owner"				);
	//::testQuery(app, output, "get", "company"	, -1	, "", {1	, 10000}, ""							);
	//::testQuery(app, output, "get", "company"	, 0		, "", {0	, 10000}, ""							);
	//::testQuery(app, output, "get", "company"	, 3		, "", {1	, 10000}, "owner"						);
	::gpk::STimer										timerTotal;
	::gpk::STimer										timerStep;
	const uint32_t										iterations						= 1000000;
	::gpk::array_pod<double>							times;
	{
		timerTotal.Reset();
		double												timeTotal						= 0;
		for(uint32_t i = 0; i < iterations; ++i) {
			timerStep.Reset();
			//::testPush(app, output, "push_back", "company"		, "1");
			//::testPush(app, output, "push_back", "company"		, "{ \"name\" : \"testA\" }");
			::testPush(app, output, "push_back", "company"		, "{ \"name\" : \"testB\" }");
			timerStep.Frame();
			timeTotal											+= timerStep.LastTimeSeconds;
		}
		timerTotal.Frame();
		always_printf("------ COMPANY - Insert time: %g seconds. Record count: %u. Average: %g seconds.", timeTotal, iterations, timerTotal.LastTimeSeconds / iterations);
	}

	{
		timerTotal.Reset();
		double												timeTotal						= 0;
		for(uint32_t i = 0; i < iterations; ++i) {
			timerStep.Reset();
			//::testPush(app, output, "push_back", "user"			, "1.5");
		//	//////::testQuery(app, output, "get", "user"		, -1	, "", {0, 1000}, ""								);
			//::testPush(app, output, "push_back", "user"			, "{ \"name\" : \"test1\" }");
		//	////::testQuery(app, output, "get", "user"		, -1	, "", {0, 1000}, ""								);
			::testPush(app, output, "push_back", "user"			, "{ \"name\" : \"test2\" }");
			timerStep.Frame();
			timeTotal											+= timerStep.LastTimeSeconds;
		}
		timerTotal.Frame();
		always_printf("------ USER - Insert time: %g seconds. Record count: %u. Average: %g seconds.", timeTotal, iterations, timerTotal.LastTimeSeconds / iterations);
	}

	{
		timerTotal.Reset();
		double												timeTotal						= 0;
		for(uint32_t i = 0; i < iterations; ++i) {
			timerStep.Reset();
			//::testPush(app, output, "push_back", "publisher"	, "false");
			//::testPush(app, output, "push_back", "publisher"			, "{ \"name\" : \"test2\" }");
			::testPush(app, output, "push_back", "publisher"	, "{ \"name\" : \"test7\" }");
			//::testPush(app, output, "push_back", "publisher"	, "true");
			timerStep.Frame();
			timeTotal											+= timerStep.LastTimeSeconds;
		}
		timerTotal.Frame();
		always_printf("------ PUBLISHER - Insert time: %g seconds. Record count: %u. Average: %g seconds.", timeTotal, iterations, timerTotal.LastTimeSeconds / iterations);
	}

	{
		timerTotal.Reset();
		double												timeTotal						= 0;
		for(uint32_t i = 0; i < iterations; ++i) {
			timerStep.Reset();
			//::testPush(app, output, "push_back", "user"			, "1.5");
		//	//////::testQuery(app, output, "get", "user"		, -1	, "", {0, 1000}, ""								);
			//::testPush(app, output, "push_back", "user"			, "{ \"name\" : \"test1\" }");
		//	////::testQuery(app, output, "get", "user"		, -1	, "", {0, 1000}, ""								);
			::testPush(app, output, "push_back", "user"			, "{ \"name\" : \"test2\" }");
		//	////::testQuery(app, output, "get", "user"		, -1	, "", {0, 1000}, ""								);
			////::testPush(app, output, "push_back", "website"		, "null");
			////::testPush(app, output, "push_back", "website"		, "{ \"name\" : \"test4\" }");
			////::testPush(app, output, "push_back", "website"		, "{ \"name\" : \"test5\" }");
			//::testPush(app, output, "push_back", "publisher"	, "false");
			//::testPush(app, output, "push_back", "publisher"			, "{ \"name\" : \"test2\" }");
			::testPush(app, output, "push_back", "publisher"	, "{ \"name\" : \"test7\" }");
			//::testPush(app, output, "push_back", "publisher"	, "true");
			//::testPush(app, output, "push_back", "company"		, "1");
			//::testPush(app, output, "push_back", "company"		, "{ \"name\" : \"testA\" }");
			::testPush(app, output, "push_back", "company"		, "{ \"name\" : \"testB\" }");
			timerStep.Frame();
			timeTotal											+= timerStep.LastTimeSeconds;
		}
		timerTotal.Frame();
		always_printf("------ Insert time: %g seconds. Record count: %u. Average: %g seconds.", timeTotal, iterations * 3, timerTotal.LastTimeSeconds / (iterations * 3));
	}

	//info_printf("Test ------------------------------------------");
 	//{
		//::testQuery(app, output, "get", "user"		, -1	, "", {0, ::blt::MAX_TABLE_RECORD_COUNT}, ""						);
		//::testQuery(app, output, "get", "user"		, 0		, "", {0, ::blt::MAX_TABLE_RECORD_COUNT}, "website"					);
		//::testQuery(app, output, "get", "user"		, 10	, "", {0, ::blt::MAX_TABLE_RECORD_COUNT}, "referral.referral"		);
		//::testQuery(app, output, "get", "website"	, -1	, "", {0, ::blt::MAX_TABLE_RECORD_COUNT}, "publisher"				);
		//::testQuery(app, output, "get", "website"	, 0		, "", {0, ::blt::MAX_TABLE_RECORD_COUNT}, "publisher.company"		);
		//::testQuery(app, output, "get", "website"	, 3		, "", {0, ::blt::MAX_TABLE_RECORD_COUNT}, "publisher.company.owner"	);
		//::testQuery(app, output, "get", "publisher"	, -1	, "", {0, ::blt::MAX_TABLE_RECORD_COUNT}, ""						);
		//::testQuery(app, output, "get", "publisher"	, 0		, "", {0, ::blt::MAX_TABLE_RECORD_COUNT}, "company"					);
		//::testQuery(app, output, "get", "publisher"	, 3		, "", {0, ::blt::MAX_TABLE_RECORD_COUNT}, "company.owner"			);
		//::testQuery(app, output, "get", "company"	, -1	, "", {0, ::blt::MAX_TABLE_RECORD_COUNT}, ""						);
		//::testQuery(app, output, "get", "company"	, 0		, "", {0, ::blt::MAX_TABLE_RECORD_COUNT}, ""						);
		//::testQuery(app, output, "get", "company"	, 3		, "", {0, ::blt::MAX_TABLE_RECORD_COUNT}, "owner"					);
	//}
	//::blt::blitterFlush(app);
	return 0;
}


