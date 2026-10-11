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
	::llc::aobj<::llc::vcsc_t> expansionKeys = {};

	BLT_TEST_CHECK(checkCount, 0 <= ::blt::requestProcess(expressionReader, query, request, expansionKeys), "%s", "requestProcess() rejected a valid request.");
	BLT_TEST_CHECK(checkCount, query.Database == LLC_CXS("company"), "Unexpected database: '%.*s'.", LLC_VIEW_ARGS(query.Database));
	BLT_TEST_CHECK(checkCount, query.Command == LLC_CXS("get"), "Unexpected command: '%.*s'.", LLC_VIEW_ARGS(query.Command));
	BLT_TEST_CHECK(checkCount, query.Detail == 3, "Unexpected detail: %" LLC_FMT_S3 ".", query.Detail);
	BLT_TEST_CHECK(checkCount, query.Range.Offset == 4, "Unexpected range offset: %" LLC_FMT_U3 ".", query.Range.Offset);
	BLT_TEST_CHECK(checkCount, query.Range.Count == 2, "Unexpected range count: %" LLC_FMT_U3 ".", query.Range.Count);
	BLT_TEST_CHECK(checkCount, expansionKeys.size() == 2, "Unexpected expansion count: %" LLC_FMT_U2 ".", expansionKeys.size());
	BLT_TEST_CHECK(checkCount, expansionKeys[0] == LLC_CXS("owner"), "Unexpected first expansion: '%.*s'.", LLC_VIEW_ARGS(expansionKeys[0]));
	BLT_TEST_CHECK(checkCount, expansionKeys[1] == LLC_CXS("company"), "Unexpected second expansion: '%.*s'.", LLC_VIEW_ARGS(expansionKeys[1]));
	BLT_TEST_CHECK(checkCount, query.ExpansionKeys.begin() == expansionKeys.begin(), "%s", "Query expansion view does not reference its storage.");
	BLT_TEST_CHECK(checkCount, query.Record == request.ContentBody, "%s", "Request body was not retained as the query record.");
	BLT_TEST_CHECK(checkCount, query.RecordReader.Tree.size() == 1, "Unexpected record root count: %" LLC_FMT_U2 ".", query.RecordReader.Tree.size());
	return 0;
}

sttc ::llc::err_t testModel(::llc::u2_t & checkCount) {
	::blt::STableDescription description = {};
	llc_necall(description.Fields.push_back({"name", ::llc::JSON_TYPE_STRING}), "%s", "Failed to add name field.");
	llc_necall(description.Fields.push_back({"active", ::llc::JSON_TYPE_BOOLEAN}), "%s", "Failed to add active field.");
	llc_necall(description.IndicesFieldToMap.push_back(-1), "%s", "Failed to add name mapping.");
	llc_necall(description.IndicesFieldToMap.push_back(-1), "%s", "Failed to add active mapping.");

	::llc::string output = {};
	BLT_TEST_CHECK(checkCount, 0 <= ::blt::outputModel(output, description), "%s", "outputModel() failed.");
	BLT_TEST_CHECK(checkCount, output == LLC_CXS("{\"name\":\"\",\"active\":false}"), "Unexpected model: '%.*s'.", LLC_VIEW_ARGS(output));

	::blt::TNamedBlitterDB database = {"company", {}};
	database.Val.Description = description;
	::llc::aobj<::blt::TNamedBlitterDB> databases = {};
	llc_necall(databases.push_back(database), "%s", "Failed to add database.");
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
	llc_necall(database.Val.Blocks.push_back({}), "%s", "Failed to add block.");
	llc_necall(database.Val.BlockIndices.push_back(0), "%s", "Failed to add block index.");
	llc_necall(database.Val.Offsets.push_back(0), "%s", "Failed to add block offset.");
	llc_necall(database.Val.BlockTimes.push_back(0), "%s", "Failed to add block time.");
	::llc::SJSONFile & block = *database.Val.Blocks[0];
	block.Bytes = "[\"alpha\",\"beta\"]";
	llc_necall(::llc::jsonParse(block.Reader, block.Bytes), "%s", "Failed to parse record block.");

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
