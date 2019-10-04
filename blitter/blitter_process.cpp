#include "blitter_process.h"

#include "gpk_parse.h"
#include "gpk_find.h"
#include "gpk_view_bit.h"

static	::gpk::error_t							queryGetDetail
	( ::gpk::SLoadCache								& loadCache
	, const ::gpk::SExpressionReader				& expressionReader
	, ::gpk::array_obj<::blt::TNamedBlitterDB>		& databases
	, const uint32_t								idxDatabase
	, const ::blt::SBlitterQuery					& query
	, ::gpk::array_pod<char_t>						& output
	, const ::gpk::view_const_char					& folder
	, const uint32_t								idxExpand
	) {
	::gpk::view_const_string							outputRecord						= {};
	uint32_t											nodeIndex							= (uint32_t)-1;
	uint32_t											blockIndex							= (uint32_t)-1;
	::blt::TNamedBlitterDB								& databaseToRead					= databases[idxDatabase];
	if errored(::blt::recordGet(loadCache, databaseToRead, query.Detail, outputRecord, nodeIndex, blockIndex, folder)) {
		error_printf("Failed to load record detail. Offset: %llu. Length: %llu.", query.Range.Offset, query.Range.Count);
		gpk_necall(output.append(::gpk::view_const_string{"{}"}), "%s", "Out of memory?");
		return 1;
	}
	if(idxExpand >= query.ExpansionKeys.size())
		gpk_necall(output.append(outputRecord), "%s", "Out of memory?");
	else {
		const ::gpk::SJSONFile								& currentDBBlock					= *databaseToRead.Val.Blocks[blockIndex];
		const int32_t										indexRecordNode						= ::gpk::jsonArrayValueGet(*currentDBBlock.Reader.Tree[0], nodeIndex);
		const ::gpk::view_const_char						fieldToExpand						= query.ExpansionKeys[idxExpand];
		const int32_t										indexValueNode						= ::gpk::jsonObjectValueGet(currentDBBlock.Reader, (uint32_t)indexRecordNode, {fieldToExpand.begin(), fieldToExpand.size()});
		const ::gpk::view_const_char						currentRecordView					= currentDBBlock.Reader.View[indexRecordNode];
		if(0 > indexValueNode) {
			info_printf("Cannot expand field. Field not found: %s.", ::gpk::toString(fieldToExpand).begin());
			gpk_necall(output.append(currentRecordView), "%s", "Out of memory?");
		}
		else {
			const ::gpk::JSON_TYPE							refNodeTYpe								= currentDBBlock.Reader.Token[indexValueNode].Type;
			if(::gpk::JSON_TYPE_INTEGER != refNodeTYpe && ::gpk::JSON_TYPE_ARRAY != refNodeTYpe) {
				info_printf("Invalid value type: %s.", ::gpk::get_value_label(refNodeTYpe).begin());
				gpk_necall(output.append(currentRecordView), "%s", "Out of memory?");
			}
			else if(::gpk::JSON_TYPE_INTEGER == refNodeTYpe) {
				uint64_t										nextTableRecordIndex				= (uint64_t)-1LL;
				const ::gpk::view_const_char					digitsToDetailView					= currentDBBlock.Reader.View[indexValueNode];
				gpk_necall(::gpk::parseIntegerDecimal(digitsToDetailView, &nextTableRecordIndex), "%s", "Out of memory?");

				const char										* appendStart						= currentRecordView.begin();
				const char										* appendStop						= digitsToDetailView.begin();
				gpk_necall(output.append(appendStart, (uint32_t)(appendStop - appendStart)), "%s", "Out of memory?");
				bool											bFound								= false;
				for(uint32_t iDB = 0; iDB < databases.size(); ++iDB) {
					::blt::TNamedBlitterDB							& nextTable							= databases[iDB];
					if(nextTable.Key == fieldToExpand || 0 <= ::gpk::find(fieldToExpand, ::gpk::view_array<const ::gpk::view_const_char>{nextTable.Val.Bindings})) {
						bFound										= true;
						::blt::SBlitterQuery							nextQuery							= query;
						nextQuery.Database							= nextTable.Key;
						nextQuery.Detail							= nextTableRecordIndex;
						ce_if(::queryGetDetail(loadCache, expressionReader, databases, iDB, nextQuery, output, folder, idxExpand + 1), "%s", "Failed to unroll detail.");
						break;
					}
				}
				if(false == bFound)
					gpk_necall(output.append(::gpk::view_const_string{"{}"}), "%s", "?¿??");
				appendStart									= digitsToDetailView.end();
				appendStop									= currentRecordView.end();
				gpk_necall(output.append(appendStart, (uint32_t)(appendStop - appendStart)), "%s", "Out of memory?");
			}
			else {
				const char										* appendStart						= currentRecordView.begin();
				const ::gpk::view_const_char					arrayToDetailView					= currentDBBlock.Reader.View[indexValueNode];
				const char										* appendStop						= arrayToDetailView.begin();
				gpk_necall(output.append(appendStart, (uint32_t)(appendStop - appendStart)), "%s", "Out of memory?");
				gpk_necall(output.push_back('['), "%s", "Out of memory?");
				const ::gpk::SJSONNode							& arrayNode							= *currentDBBlock.Reader.Tree[indexValueNode];
				bool											bFound								= false;
				for(uint32_t iRef = 0, refCount = ::gpk::jsonArraySize(*currentDBBlock.Reader.Tree[indexValueNode]); iRef < refCount; ++iRef) {
					uint64_t										nextTableRecordIndex					= (uint64_t)-1LL;
					const ::gpk::error_t							indexArrayElement						= ::gpk::jsonArrayValueGet(arrayNode, iRef);
					const ::gpk::view_const_char					digitsToDetailView						= currentDBBlock.Reader.View[indexArrayElement];
					gpk_necall(::gpk::parseIntegerDecimal(digitsToDetailView, &nextTableRecordIndex), "%s", "Out of memory?");
					for(uint32_t iDB = 0; iDB < databases.size(); ++iDB) {
						::blt::TNamedBlitterDB							& nextTable							= databases[iDB];
						if(nextTable.Key == fieldToExpand || 0 <= ::gpk::find(fieldToExpand, ::gpk::view_array<const ::gpk::view_const_char>{nextTable.Val.Bindings})) {
							bFound										= true;
							::blt::SBlitterQuery							nextQuery							= query;
							nextQuery.Database							= nextTable.Key;
							nextQuery.Detail							= nextTableRecordIndex;
							ce_if(::queryGetDetail(loadCache, expressionReader, databases, iDB, nextQuery, output, folder, idxExpand + 1), "%s", "Failed to unroll detail.");
							break;
						}
					}
					if(false == bFound)
						gpk_necall(output.append(::gpk::view_const_string{"{}"}), "%s", "?¿??");
					if(refCount - 1 != iRef)
						gpk_necall(output.push_back(','), "%s", "Out of memory?");
				}
				appendStart									= arrayToDetailView.end();
				appendStop									= currentRecordView.end();
				gpk_necall(output.push_back(']'), "%s", "Out of memory?");
				gpk_necall(output.append(appendStart, (uint32_t)(appendStop - appendStart)), "%s", "Out of memory?");
			}
		}
	}
	return 0;
}

static	::gpk::error_t							fillEmptyBlocks				(const ::gpk::view_const_char & emptyBlockData, const uint32_t emptyBlocks, bool appendComma, ::gpk::array_pod<char_t> & output)	{
	for(uint32_t iEmpty = 0; iEmpty < emptyBlocks; ++iEmpty) {
		gpk_necall(output.append(emptyBlockData), "%s", "Out of memory?");
		if(emptyBlocks - 1 != iEmpty)
			gpk_necall(output.push_back(','), "%s", "Out of memory?");
	}
	if(appendComma && emptyBlocks)
		gpk_necall(output.push_back(','), "%s", "Out of memory?");
	return 0;
}

static	::gpk::error_t							nextBlock					(const uint32_t iRangeInfo, const uint32_t lastRangeInfo, const ::gpk::view_array<const ::blt::SRangeBlockInfo>	& rangeInfo, const ::gpk::view_const_char & emptyBlockData, ::gpk::array_pod<char_t> & output) {
	if(rangeInfo.size() -1 != iRangeInfo)
		gpk_necall(output.push_back(','), "%s", "Out of memory?");

	if(iRangeInfo < lastRangeInfo) {	// Fill inner empty blocks
		const uint32_t										emptyBlocks							= rangeInfo[iRangeInfo + 1].BlockId - rangeInfo[iRangeInfo].BlockId;
		if(emptyBlocks > 1) {
			info_printf("Empty blocks between: %u and %u.", rangeInfo[iRangeInfo].BlockId, rangeInfo[iRangeInfo + 1].BlockId);
			gpk_necall(::fillEmptyBlocks(emptyBlockData, emptyBlocks - 1, lastRangeInfo != iRangeInfo, output), "%s", "Out of memory?");
		}
	}
	return 0;
}

static	::gpk::error_t							queryGetRange
	( ::gpk::SLoadCache								& loadCache
	, const ::gpk::SExpressionReader				& expressionReader
	, ::gpk::array_obj<::blt::TNamedBlitterDB>		& databases
	, const uint32_t								idxDatabase
	, const ::blt::SBlitterQuery					& query
	, ::gpk::array_pod<char_t>						& output
	, const ::gpk::view_const_char					& folder
	, const uint32_t								idxExpand
	) {
	if(0 == query.Range.Count) {
		gpk_necall(output.append(::gpk::view_const_string{"[]"}), "%s", "Out of memory?");
		return 0;
	}
	::gpk::array_pod<::gpk::SMinMax<uint32_t>>				relativeIndices						= {};
	::gpk::SRange<uint32_t>									blockRange							= {};
	::gpk::array_obj<::blt::SRangeBlockInfo>				rangeInfo							= {};
	::blt::TNamedBlitterDB									& databaseToRead					= databases[idxDatabase];
	::gpk::array_pod<char_t>								emptyBlockData						= {};
	const ::gpk::view_const_string							empty_record						= "{},";
	for(uint32_t iRecord = 0; iRecord < databaseToRead.Val.BlockSize; ++iRecord)
		gpk_necall(emptyBlockData.append(empty_record), "%s", "Out of memory?");
	if(emptyBlockData.size() > 0) // Remove last comma.
		gpk_necall(emptyBlockData.resize(emptyBlockData.size() - 1), "%s", "Out of memory?");

	gpk_necall(::blt::recordRange(loadCache, databaseToRead, query.Range, folder, rangeInfo, blockRange), "Failed to load record range. Offset: %llu. Length: %llu.", query.Range.Offset, query.Range.Count);
	const uint32_t											lastRangeInfo						= rangeInfo.size() - 1;
	gpk_necall(output.push_back('['), "%s", "Out of memory?");
	if((0 < databaseToRead.Val.BlockSize) && rangeInfo.size()) {	// Fill leading records if the blocks don't exist.
		const uint32_t											recordsToAvoid						= (query.Range.Offset % databaseToRead.Val.BlockSize);
		const uint32_t											emptyBlocks							= rangeInfo[0].BlockId - blockRange.Offset;
		if(0 == recordsToAvoid)
			gpk_necall(::fillEmptyBlocks(emptyBlockData, emptyBlocks, lastRangeInfo != 0, output), "%s", "Out of memory?");
		else {
			if(emptyBlocks > 0) {
				for(uint32_t iElem = recordsToAvoid; iElem < databaseToRead.Val.BlockSize; ++iElem)
					gpk_necall(output.append(::gpk::view_const_string{"{},"}), "%s", "Out of memory?");
				gpk_necall(::fillEmptyBlocks(emptyBlockData, emptyBlocks - 1, lastRangeInfo != 0, output), "%s", "Out of memory?");
			}
		}
	}

	if(idxExpand >= query.ExpansionKeys.size()) {
		for(uint32_t iView = 0; iView < rangeInfo.size(); ++iView) {
			const ::gpk::view_const_char							rangeView							= rangeInfo[iView].OutputRecords;
			gpk_necall(output.append(rangeView), "%s", "Out of memory?");
			gpk_necall(::nextBlock(iView, lastRangeInfo, rangeInfo, emptyBlockData, output), "%s", "??");
		}
	}
	else {
		::blt::SBlitterQuery								elemQuery							= query;
		for(uint32_t iView = 0; iView < rangeInfo.size(); ++iView) {
			const ::blt::SRangeBlockInfo						& blockInfo							= rangeInfo[iView];
			const ::gpk::SMinMax<uint32_t>						rangeToExpand						= blockInfo.RelativeIndices;
			for(uint32_t iRecord = rangeToExpand.Min; iRecord < rangeToExpand.Max + 1; ++iRecord) {
				elemQuery.Detail								= iRecord + blockInfo.BlockId * (int64_t)databaseToRead.Val.BlockSize;
				gpk_necall(::queryGetDetail(loadCache, expressionReader, databases, idxDatabase, elemQuery, output, folder, idxExpand), "%s", "??");
				if(rangeToExpand.Max != iRecord)
					gpk_necall(output.push_back(','), "%s", "Out of memory?");
			}
			gpk_necall(::nextBlock(iView, lastRangeInfo, rangeInfo, emptyBlockData, output), "%s", "??");
		}
	}

	if((0 < databaseToRead.Val.BlockSize && databaseToRead.Val.BlocksOnDisk.size())) {	// Fill trailing records if the blocks don't exist.
		const uint32_t										recordsToAvoid						= (uint32_t)((query.Range.Offset + query.Range.Count) % databaseToRead.Val.BlockSize);
		const uint32_t										lastBlockId							= rangeInfo.size() ? rangeInfo[rangeInfo.size() - 1].BlockId : 0;
		const uint32_t										emptyBlocks							= (blockRange.Offset + blockRange.Count - 1) - lastBlockId;
		if(emptyBlocks > 0) {
			if(0 == recordsToAvoid)
				gpk_necall(::fillEmptyBlocks(emptyBlockData, emptyBlocks - 1, false, output), "%s", "Out of memory?");
			else {
				gpk_necall(output.push_back(','), "%s", "Out of memory?");
				if(emptyBlocks > 1)
					gpk_necall(::fillEmptyBlocks(emptyBlockData, emptyBlocks - 1, true, output), "%s", "Out of memory?");
				for(uint32_t iElem = 0; iElem < recordsToAvoid; ++iElem)
					gpk_necall(output.append(::gpk::view_const_string{"{},"}), "%s", "Out of memory?");
				gpk_necall(output.resize(output.size()-1), "%s", "Out of memory?");
			}
		}
	}

	gpk_necall(output.push_back(']'), "%s", "Out of memory?");
	return 0;
}

::gpk::error_t									blt::queryProcess
	( ::gpk::SLoadCache								& loadCache
	, ::gpk::array_obj<::blt::TNamedBlitterDB>		& databases
	, const ::gpk::SExpressionReader				& expressionReader
	, const ::blt::SBlitterQuery					& query
	, const ::gpk::view_const_char					& folder
	, ::gpk::array_pod<char_t>						& output
	) {
	if(query.Command == ::gpk::view_const_string{"get"}) {
		for(uint32_t iDB = 0; iDB < databases.size(); ++iDB) {
			if(query.Database == databases[iDB].Key)
				return (0 <= query.Detail)
					? ::queryGetDetail	(loadCache, expressionReader, databases, iDB, query, output, folder, 0)
					: ::queryGetRange	(loadCache, expressionReader, databases, iDB, query, output, folder, 0)
					;
		}
	}
	else if(query.Command == ::gpk::view_const_string{"push_back"}) {
		for(uint32_t iDB = 0; iDB < databases.size(); ++iDB) {
			::blt::TNamedBlitterDB								& database			= databases[iDB];
			if(query.Database == database.Key) {
				if(database.Val.BlocksOnDisk.size()) {
					const uint32_t										idBlock				= ::gpk::max(::gpk::view_const_uint32{database.Val.BlocksOnDisk});
					uint32_t											indexRecord			= (uint32_t)-1;
					uint32_t											indexBlock			= (uint32_t)-1;
					gpk_necall(::blt::recordLoad(loadCache, database, idBlock * database.Val.BlockSize, indexRecord, indexBlock, folder), "Failed to load block for record: %lli.", query.Detail);
					::gpk::SJSONFile									& block				= *database.Val.Blocks[indexBlock];
					if(block.Reader[0]->Children.size() < database.Val.BlockSize) {
						::gpk::SJSONReader									recordReader		= {};
						gpk_necall(::gpk::jsonParse(recordReader, query.Record), "%s", "Failed to parse JSON record!");
						loadCache.Deflated.clear();
						loadCache.Deflated.push_back(',');
						gpk_necall(::gpk::jsonWrite(recordReader.Tree[0], recordReader.View, loadCache.Deflated), "Failed to write json record! %s", "Invalid format?");
						gpk_necall(block.Bytes.insert(block.Bytes.size() - 1, loadCache.Deflated), "Failed to append record! %s", "Out of memory?");
						block.Reader.Reset();
						gpk_necall(::gpk::jsonParse(block.Reader, block.Bytes), "%s", "Failed to read JSON!");
						::gpk::view_bit<uint32_t>{database.Val.BlockDirty.begin(), database.Val.Blocks.size()}[indexBlock]	= true;
					}
					else {

					}
					return 0;
				}
			}
		}
	}
	else if(query.Command == ::gpk::view_const_string{"pop_back"}) {
		//for(uint32_t iDB = 0; iDB < databases.size(); ++iDB) {
		//	if(query.Database == databases[iDB].Key) {
		//		uint32_t											idBlock				= 0;;
		//		::gpk::max(::gpk::view_const_uint32{databases[iDB].Val.BlocksOnDisk}, &idBlock);
		//	}
		//}
	}
	else if(query.Command == ::gpk::view_const_string{"replace"}) {
		//for(uint32_t iDB = 0; iDB < databases.size(); ++iDB) {
		//	if(query.Database == databases[iDB].Key) {
		//		uint32_t											idBlock				= 0;;
		//		::gpk::max(::gpk::view_const_uint32{databases[iDB].Val.BlocksOnDisk}, &idBlock);
		//	}
		//}
	}

	error_printf("Database not found: %s.", ::gpk::toString(query.Database).begin());
	return -1;
}
