#include "blitter_process.h"
#include "blitter_output.h"

#define BLT_TEST_CHECK(checkCount, condition, format, ...) do { ++checkCount; if(!(condition)) { always_printf(format, __VA_ARGS__); return -1; } } while(0)

sttc ::llc::err_t testFileNames(::llc::u2_t & checkCount) {
	::llc::string result = {};
	BLT_TEST_CHECK(checkCount, 0 <= ::blt::tableFolderName(result, "company", 128), "%s", "tableFolderName() failed.");
	BLT_TEST_CHECK(checkCount, result == LLC_CXS("company.128.db"), "Unexpected table folder: '%.*s'.", LLC_VIEW_ARGS(result));

	result.clear();
	BLT_TEST_CHECK(checkCount, 0 <= ::blt::blockFileName(result, "company", false, ::blt::DATABASE_HOST_LOCAL, 7), "%s", "blockFileName() failed for plain storage.");
	BLT_TEST_CHECK(checkCount, result == LLC_CXS("company.7.sson"), "Unexpected plain block file: '%.*s'.", LLC_VIEW_ARGS(result));

	result.clear();
	BLT_TEST_CHECK(checkCount, 0 <= ::blt::blockFileName(result, "company", true, ::blt::DATABASE_HOST_DEFLATE, 7), "%s", "blockFileName() failed for secure compressed storage.");
	BLT_TEST_CHECK(checkCount, result == LLC_CXS("company.7.czon"), "Unexpected secure compressed block file: '%.*s'.", LLC_VIEW_ARGS(result));

	result.clear();
	BLT_TEST_CHECK(checkCount, 0 <= ::blt::tableFileName(result, ::blt::DATABASE_HOST_DEFLATE, false, "company"), "%s", "tableFileName() failed.");
	BLT_TEST_CHECK(checkCount, result == LLC_CXS("company.zson"), "Unexpected table file: '%.*s'.", LLC_VIEW_ARGS(result));
	return 0;
}

sttc ::llc::err_t testRequest(::llc::u2_t & checkCount) {
	::llc::TKeyValConstString queryValues[] = {{"offset", "4"}, {"limit", "2"}, {"expand", "owner.company"}};
	::blt::SBlitterRequest request = {"/company/get/3", "{}", queryValues};
	::blt::SBlitterQuery query = {};
	::llc::SExpressionReader expressionReader = {};
	::llc::aobj<::llc::vcst_t> expansionKeys = {};

	BLT_TEST_CHECK(checkCount, 0 <= ::blt::requestProcess(expressionReader, query, request, expansionKeys), "%s", "requestProcess() rejected a valid request.");
	BLT_TEST_CHECK(checkCount, query.Database == LLC_CXS("company"), "Unexpected database: '%.*s'.", LLC_VIEW_ARGS(query.Database));
	BLT_TEST_CHECK(checkCount, query.Command == LLC_CXS("get"), "Unexpected command: '%.*s'.", LLC_VIEW_ARGS(query.Command));
	BLT_TEST_CHECK(checkCount, query.Path.size() == 0, "Unexpected expression path: '%.*s'.", LLC_VIEW_ARGS(query.Path));
	BLT_TEST_CHECK(checkCount, query.Detail == 3, "Unexpected detail: %" LLC_FMT_S3 ".", query.Detail);
	BLT_TEST_CHECK(checkCount, query.Range.Offset == 4, "Unexpected range offset: %" LLC_FMT_U3 ".", query.Range.Offset);
	BLT_TEST_CHECK(checkCount, query.Range.Count == 2, "Unexpected range count: %" LLC_FMT_U3 ".", query.Range.Count);
	BLT_TEST_CHECK(checkCount, expansionKeys.size() == 2, "Unexpected expansion count: %" LLC_FMT_U2 ".", expansionKeys.size());
	BLT_TEST_CHECK(checkCount, expansionKeys[0] == LLC_CXS("owner"), "Unexpected first expansion: '%.*s'.", LLC_VIEW_ARGS(expansionKeys[0]));
	BLT_TEST_CHECK(checkCount, expansionKeys[1] == LLC_CXS("company"), "Unexpected second expansion: '%.*s'.", LLC_VIEW_ARGS(expansionKeys[1]));
	BLT_TEST_CHECK(checkCount, query.ExpansionKeys.begin() == expansionKeys.begin(), "%s", "Query expansion view does not reference its storage.");
	BLT_TEST_CHECK(checkCount, query.Record == request.ContentBody, "%s", "Request body was not retained as the query record.");
	BLT_TEST_CHECK(checkCount, query.RecordReader.Tree.size() == 1, "Unexpected record root count: %" LLC_FMT_U2 ".", query.RecordReader.Tree.size());

	::blt::SBlitterRequest expressionRequest = {"/company.owner/get", "{}", {}};
	::blt::SBlitterQuery expressionQuery = {};
	::llc::SExpressionReader expressionPathReader = {};
	::llc::aobj<::llc::vcst_t> expressionExpansionKeys = {};
	BLT_TEST_CHECK(checkCount, 0 <= ::blt::requestProcess(expressionPathReader, expressionQuery, expressionRequest, expressionExpansionKeys), "%s", "requestProcess() rejected a valid expression path.");
	BLT_TEST_CHECK(checkCount, expressionQuery.Database == LLC_CXS("company"), "Unexpected expression database: '%.*s'.", LLC_VIEW_ARGS(expressionQuery.Database));
	BLT_TEST_CHECK(checkCount, expressionQuery.Command == LLC_CXS("get"), "Unexpected expression command: '%.*s'.", LLC_VIEW_ARGS(expressionQuery.Command));
	BLT_TEST_CHECK(checkCount, expressionQuery.Path == LLC_CXS("owner"), "Unexpected expression path: '%.*s'.", LLC_VIEW_ARGS(expressionQuery.Path));
	BLT_TEST_CHECK(checkCount, expressionQuery.Detail == -1, "Unexpected expression detail: %" LLC_FMT_S3 ".", expressionQuery.Detail);
	BLT_TEST_CHECK(checkCount, expressionQuery.Range.Offset == 0, "Unexpected default range offset: %" LLC_FMT_U3 ".", expressionQuery.Range.Offset);
	BLT_TEST_CHECK(checkCount, expressionQuery.Range.Count == ::blt::MAX_TABLE_RECORD_COUNT, "Unexpected default range count: %" LLC_FMT_U3 ".", expressionQuery.Range.Count);
	BLT_TEST_CHECK(checkCount, expressionQuery.Record == expressionRequest.ContentBody, "%s", "Expression request body was not retained as the query record.");
	BLT_TEST_CHECK(checkCount, expressionQuery.RecordReader.Tree.size() == 1, "Unexpected expression record root count: %" LLC_FMT_U2 ".", expressionQuery.RecordReader.Tree.size());
	return 0;
}

sttc ::llc::err_t testModel(::llc::u2_t & checkCount) {
	::blt::STableDescription description = {};
	if_fail_fef(description.Fields.push_back({"name", ::llc::JSON_TYPE_STRING}), "%s", "Failed to add name field.");
	if_fail_fef(description.Fields.push_back({"active", ::llc::JSON_TYPE_BOOLEAN}), "%s", "Failed to add active field.");
	if_fail_fef(description.IndicesFieldToMap.push_back(-1), "%s", "Failed to add name mapping.");
	if_fail_fef(description.IndicesFieldToMap.push_back(-1), "%s", "Failed to add active mapping.");

	::llc::string output = {};
	BLT_TEST_CHECK(checkCount, 0 <= ::blt::outputModel(output, description), "%s", "outputModel() failed.");
	BLT_TEST_CHECK(checkCount, output == LLC_CXS("{\"name\":\"\",\"active\":false}"), "Unexpected model: '%.*s'.", LLC_VIEW_ARGS(output));

	::blt::TNamedBlitterDB database = {"company", {}};
	database.Val.Description = description;
	::llc::aobj<::blt::TNamedBlitterDB> databases = {};
	if_fail_fef(databases.push_back(database), "%s", "Failed to add database.");
	::blt::SBlitterQuery query = {};
	query.Database = "company";
	query.Command = "model";
	::llc::SLoadCache loadCache = {};
	::llc::SExpressionReader expressionReader = {};
	output.clear();
	BLT_TEST_CHECK(checkCount, 0 <= ::blt::queryProcess(loadCache, databases, expressionReader, query, {}, output), "%s", "queryProcess() rejected a model query.");
	BLT_TEST_CHECK(checkCount, output == LLC_CXS("{\"name\":\"\",\"active\":false}"), "Unexpected query model: '%.*s'.", LLC_VIEW_ARGS(output));
	return 0;
}

sttc ::llc::err_t testRecord(::llc::u2_t & checkCount) {
	::blt::TNamedBlitterDB database = {"company", {}};
	database.Val.BlockSize = 2;
	if_fail_fef(database.Val.Blocks.push_back({}), "%s", "Failed to add block.");
	database.Val.Blocks[0].create();
	BLT_TEST_CHECK(checkCount, database.Val.Blocks[0], "%s", "Block owner did not create a JSON file.");
	if_fail_fef(database.Val.BlockIndices.push_back(0), "%s", "Failed to add block index.");
	if_fail_fef(database.Val.Offsets.push_back(0), "%s", "Failed to add block offset.");
	if_fail_fef(database.Val.BlockTimes.push_back(0), "%s", "Failed to add block time.");
	::llc::SJSONFile & block = *database.Val.Blocks[0];
	block.Bytes = "[\"alpha\",\"beta\"]";
	if_fail_fef(::llc::jsonParse(block.Reader, block.Bytes), "%s", "Failed to parse record block.");

	::llc::SLoadCache loadCache = {};
	::llc::vcst_t record = {};
	::llc::u2_t relativeIndex = (uint32_t)-1;
	::llc::u2_t blockIndex = (uint32_t)-1;
	BLT_TEST_CHECK(checkCount, 0 <= ::blt::recordGet(loadCache, database, 1, record, relativeIndex, blockIndex, {}), "%s", "recordGet() rejected a resident record.");
	BLT_TEST_CHECK(checkCount, blockIndex == 0, "Unexpected block index: %" LLC_FMT_U2 ".", blockIndex);
	BLT_TEST_CHECK(checkCount, relativeIndex == 1, "Unexpected relative index: %" LLC_FMT_U2 ".", relativeIndex);
	BLT_TEST_CHECK(checkCount, record == LLC_CXS("beta"), "Unexpected record: '%.*s'.", LLC_VIEW_ARGS(record));
	return 0;
}

int main() {
	::llc::setupDefaultLogCallbacks();
	::llc::u2_t checkCount = {};
	if_fail_fef(::testFileNames(checkCount), "%s", "File-name tests failed.");
	if_fail_fef(::testRequest(checkCount), "%s", "Request tests failed.");
	if_fail_fef(::testModel(checkCount), "%s", "Model tests failed.");
	if_fail_fef(::testRecord(checkCount), "%s", "Record tests failed.");
	always_printf("Blitter tests passed: %" LLC_FMT_U2 " checks.", checkCount);
	return 0;
}
