#include "blitter.h"

#include "gpk_find.h"
#include "gpk_process.h"

int												main							()		{
	::blt::SBlitter										app								= {};
	gpk_necall(::blt::loadConfig(app, "./blitter.json"), "%s", "Failed to load blitter configuration.");
	::gpk::array_pod<char_t>							output							= {};
	{
		app.Query.Command								= ::gpk::view_const_string{"get"};
		app.Query.Database								= ::gpk::view_const_string{"user"};
		app.Query.Detail								= -1;
		//app.Query.Expand								= ::gpk::view_const_string{"website"};
		app.ExpansionKeyStorage.clear();
		gpk_necall(::gpk::split(::gpk::view_const_char{app.Query.Expand}, '.', app.ExpansionKeyStorage), "%s", "Out of memory?");
		app.Query.ExpansionKeys							= app.ExpansionKeyStorage;
		app.Query.Path									= ::gpk::view_const_string{""};
		app.Query.Range									= {10, 20};
		gpk_necall(::blt::queryProcess(app.Databases, app.ExpressionReader, app.Query, app.Folder, output), "%s", "Failed to load blitter databases.");
	}
	if(output.size()) {
		output.push_back(0);
		OutputDebugStringA(output.begin());
		OutputDebugStringA("\n");
		output.clear();
	}
	{
		app.Query.Command								= ::gpk::view_const_string{"get"};
		app.Query.Database								= ::gpk::view_const_string{"user"};
		app.Query.Detail								= 1;
		app.Query.Expand								= ::gpk::view_const_string{"referral.referral"};
		app.ExpansionKeyStorage.clear();
		gpk_necall(::gpk::split(::gpk::view_const_char{app.Query.Expand}, '.', app.ExpansionKeyStorage), "%s", "Out of memory?");
		app.Query.ExpansionKeys							= app.ExpansionKeyStorage;
		app.Query.Path									= ::gpk::view_const_string{""};
		app.Query.Range									= {10, 20};
		gpk_necall(::blt::queryProcess(app.Databases, app.ExpressionReader, app.Query, app.Folder, output), "%s", "Failed to load blitter databases.");
	}
	if(output.size()) {
		output.push_back(0);
		OutputDebugStringA(output.begin());
		OutputDebugStringA("\n");
		output.clear();
	}

	{
		app.Query.Command								= ::gpk::view_const_string{"get"};
		app.Query.Database								= ::gpk::view_const_string{"user"};
		app.Query.Detail								= 20;
		app.Query.Expand								= ::gpk::view_const_string{"referral.referral"};
		app.ExpansionKeyStorage.clear();
		gpk_necall(::gpk::split(::gpk::view_const_char{app.Query.Expand}, '.', app.ExpansionKeyStorage), "%s", "Out of memory?");
		app.Query.ExpansionKeys							= app.ExpansionKeyStorage;
		app.Query.Path									= ::gpk::view_const_string{""};
		app.Query.Range									= {10, 20};
		gpk_necall(::blt::queryProcess(app.Databases, app.ExpressionReader, app.Query, app.Folder, output), "%s", "Failed to load blitter databases.");
	}
	if(output.size()) {
		output.push_back(0);
		OutputDebugStringA(output.begin());
		OutputDebugStringA("\n");
		output.clear();
	}
	return 0;
}

