#include "blitter_process.h"
#include "blitter_expression.h"

#include "gpk_parse.h"
#include "gpk_find.h"

static	::gpk::error_t							processDetail
	( ::blt::SLoadCache								& loadCache
	, const ::gpk::SExpressionReader				& expressionReader
	, ::gpk::array_obj<::blt::TNamedBlitterDB>		& databases
	, const uint32_t								idxDatabase
	, const ::blt::SBlitterQuery					& query
	, ::gpk::array_pod<char_t>						& output
	, const ::gpk::view_const_string				& folder
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
	if(0 == query.Expand.size() || idxExpand >= query.ExpansionKeys.size())
		gpk_necall(output.append(outputRecord), "%s", "Out of memory?");
	else {
		const ::gpk::SJSONFile								& currentDBBlock					= *databaseToRead.Val.Blocks[blockIndex];
		const int32_t										indexRecordNode						= ::gpk::jsonArrayValueGet(*currentDBBlock.Reader.Tree[0], nodeIndex);
		const ::gpk::view_const_string						fieldToExpand						= query.ExpansionKeys[idxExpand];
		const int32_t										indexValueNode						= ::gpk::jsonObjectValueGet(*currentDBBlock.Reader.Tree[indexRecordNode], currentDBBlock.Reader.View, fieldToExpand);
		const ::gpk::view_const_string						currentRecordView					= currentDBBlock.Reader.View[indexRecordNode];
		if(0 > indexValueNode) {
			info_printf("Cannot expand field. Field not found: %s.", fieldToExpand.begin());
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
				const ::gpk::view_const_string					digitsToDetailView					= currentDBBlock.Reader.View[indexValueNode];
				gpk_necall(::gpk::parseIntegerDecimal(digitsToDetailView, &nextTableRecordIndex), "%s", "Out of memory?");

				const char										* appendStart						= currentRecordView.begin();
				const char										* appendStop						= digitsToDetailView.begin();
				gpk_necall(output.append(appendStart, (uint32_t)(appendStop - appendStart)), "%s", "Out of memory?");
				bool											bFound								= false;
				for(uint32_t iDB = 0; iDB < databases.size(); ++iDB) {
					::blt::TNamedBlitterDB							& nextTable							= databases[iDB];
					if(nextTable.Key == fieldToExpand || 0 <= ::gpk::find(fieldToExpand, ::gpk::view_array<const ::gpk::view_const_string>{nextTable.Val.Bindings})) {
						bFound										= true;
						::blt::SBlitterQuery							nextQuery							= query;
						nextQuery.Database							= nextTable.Key;
						nextQuery.Detail							= nextTableRecordIndex;
						ce_if(::processDetail(loadCache, expressionReader, databases, iDB, nextQuery, output, folder, idxExpand + 1), "%s", "Failed to unroll detail.");
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
				const ::gpk::view_const_string					arrayToDetailView					= currentDBBlock.Reader.View[indexValueNode];
				const char										* appendStop						= arrayToDetailView.begin();
				gpk_necall(output.append(appendStart, (uint32_t)(appendStop - appendStart)), "%s", "Out of memory?");
				gpk_necall(output.push_back('['), "%s", "Out of memory?");
				const ::gpk::SJSONNode							& arrayNode							= *currentDBBlock.Reader.Tree[indexValueNode];
				bool											bFound								= false;
				for(uint32_t iRef = 0, refCount = ::gpk::jsonArraySize(*currentDBBlock.Reader.Tree[indexValueNode]); iRef < refCount; ++iRef) {
					uint64_t										nextTableRecordIndex					= (uint64_t)-1LL;
					const ::gpk::error_t							indexArrayElement						= ::gpk::jsonArrayValueGet(arrayNode, iRef);
					const ::gpk::view_const_string					digitsToDetailView						= currentDBBlock.Reader.View[indexArrayElement];
					gpk_necall(::gpk::parseIntegerDecimal(digitsToDetailView, &nextTableRecordIndex), "%s", "Out of memory?");
					for(uint32_t iDB = 0; iDB < databases.size(); ++iDB) {
						::blt::TNamedBlitterDB							& nextTable							= databases[iDB];
						if(nextTable.Key == fieldToExpand || 0 <= ::gpk::find(fieldToExpand, ::gpk::view_array<const ::gpk::view_const_string>{nextTable.Val.Bindings})) {
							bFound										= true;
							::blt::SBlitterQuery							nextQuery							= query;
							nextQuery.Database							= nextTable.Key;
							nextQuery.Detail							= nextTableRecordIndex;
							ce_if(::processDetail(loadCache, expressionReader, databases, iDB, nextQuery, output, folder, idxExpand + 1), "%s", "Failed to unroll detail.");
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

static	::gpk::error_t							processRange
	( ::blt::SLoadCache								& loadCache
	, const ::gpk::SExpressionReader				& expressionReader
	, ::gpk::array_obj<::blt::TNamedBlitterDB>		& databases
	, const uint32_t								idxDatabase
	, const ::blt::SBlitterQuery					& query
	, ::gpk::array_pod<char_t>						& output
	, const ::gpk::view_const_string				& folder
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

	if(0 == query.Expand.size() || idxExpand >= query.ExpansionKeys.size()) {
		for(uint32_t iView = 0; iView < rangeInfo.size(); ++iView) {
			const ::gpk::view_const_string						rangeView							= rangeInfo[iView].OutputRecords;
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
				gpk_necall(::processDetail(loadCache, expressionReader, databases, idxDatabase, elemQuery, output, folder, idxExpand), "%s", "??");
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
	( ::gpk::array_obj<::blt::TNamedBlitterDB>		& databases
	, const ::gpk::SExpressionReader				& expressionReader
	, const ::blt::SBlitterQuery					& query
	, const ::gpk::view_const_string				& folder
	, ::gpk::array_pod<char_t>						& output
	) {
	::blt::SLoadCache									loadCache			= {};
	for(uint32_t iDB = 0; iDB < databases.size(); ++iDB) {
		if(query.Database == databases[iDB].Key) {
			if(0 <= query.Detail)
				return ::processDetail(loadCache, expressionReader, databases, iDB, query, output, folder, 0);
			else
				return ::processRange(loadCache, expressionReader, databases, iDB, query, output, folder, 0);
		}
	}
	error_printf("Database not found: %s.", query.Database.begin());
	return -1;
}

::gpk::error_t									blt::recordGet
	( ::blt::SLoadCache					& loadCache
	, ::blt::TNamedBlitterDB			& database
	, const uint64_t					absoluteIndex
	, ::gpk::view_const_string			& output_record
	, uint32_t							& relativeIndex
	, uint32_t							& blockIndex
	, const ::gpk::view_const_string	& folder
	) {
	const uint32_t										indexBlock								= (0 == database.Val.BlockSize) ? (uint32_t)-1	: (uint32_t)(absoluteIndex / database.Val.BlockSize);
	const int32_t										iBlockElem								= (0 == database.Val.BlockSize)
		? ::blt::tableFileLoad(loadCache, database, folder)
		: ::blt::blockFileLoad(loadCache, database, folder, indexBlock)
		;
	gpk_necall(iBlockElem, "Failed to load database file: %s.", "??");
	const ::gpk::SJSONReader							& readerBlock							= database.Val.Blocks[iBlockElem]->Reader;
	ree_if(0 == readerBlock.Tree.size(), "%s", "Invalid block data.");

	const ::gpk::SJSONNode								& jsonRoot								= *readerBlock.Tree[0];
	ree_if(::gpk::JSON_TYPE_ARRAY != jsonRoot.Token->Type, "Invalid json type: %s", ::gpk::get_value_label(jsonRoot.Token->Type).begin());
	const uint64_t										offsetRecord							= database.Val.Offsets[iBlockElem];
	relativeIndex									= ::gpk::max(0U, (uint32_t)(absoluteIndex - offsetRecord));
	blockIndex										= iBlockElem;
	output_record									= readerBlock.View[::gpk::jsonArrayValueGet(*readerBlock[0], relativeIndex)];
	return 0;
}

::gpk::error_t									recordRangeBlock
	( const ::blt::TNamedBlitterDB					& database
	, const ::gpk::SRange<uint64_t>					& range
	, const uint32_t								idBlock
	, const uint32_t								iNewBlock
	, ::gpk::array_obj<::blt::SRangeBlockInfo>		& output_records
	) {
	const ::gpk::SJSONReader							& readerBlock		= database.Val.Blocks[iNewBlock]->Reader;
	ree_if(0 == readerBlock.Tree.size(), "%s", "Invalid block data.");
	const ::gpk::SJSONNode								& jsonRoot			= *readerBlock.Tree[0];
	ree_if(::gpk::JSON_TYPE_ARRAY != jsonRoot.Token->Type, "Invalid json type: %s", ::gpk::get_value_label(jsonRoot.Token->Type).begin());

	const uint64_t										offsetRecord		= database.Val.Offsets[iNewBlock];
	::blt::SRangeBlockInfo								rangeInfo			= {};
	// Actuallly I believe we should handle this differently than for single-file database access, but it is possible that I already thought about this before.
	rangeInfo.RelativeIndices.Min					= ::gpk::max(0, (int32_t)(range.Offset - offsetRecord));
	rangeInfo.RelativeIndices.Max					= ::gpk::min(::gpk::jsonArraySize(jsonRoot) - 1U, ::gpk::min(database.Val.BlockSize - 1, (uint32_t)((range.Offset + range.Count) - offsetRecord - 1)));
	rangeInfo.BlockIndex							= iNewBlock;
	rangeInfo.BlockId								= idBlock;
	::gpk::SMinMax<int32_t>								blockNodeIndices	=
		{ ::gpk::jsonArrayValueGet(jsonRoot, rangeInfo.RelativeIndices.Min)
		, ::gpk::jsonArrayValueGet(jsonRoot, rangeInfo.RelativeIndices.Max)
		};
	rangeInfo.OutputRecords							=
		{ readerBlock.View[(((uint32_t)blockNodeIndices.Min) < readerBlock.View.size()) ? blockNodeIndices.Min : 0].begin()
		, (uint32_t)(readerBlock.View[blockNodeIndices.Max].end() - readerBlock.View[(((uint32_t)blockNodeIndices.Min) < readerBlock.View.size()) ? blockNodeIndices.Min : 0].begin())
		};
	gpk_necall(output_records.push_back(rangeInfo), "%s", "Out of memory?");
	return 0;
}

::gpk::error_t									blt::recordRange
	( ::blt::SLoadCache								& loadCache
	, ::blt::TNamedBlitterDB						& database
	, const ::gpk::SRange<uint64_t>					& range
	, const ::gpk::view_const_string				& folder
	, ::gpk::array_obj<::blt::SRangeBlockInfo>		& output_records
	, ::gpk::SRange<uint32_t>						& blockRange
	) {
	const uint64_t										maxRecord			= ((range.Count == ::blt::MAX_TABLE_RECORD_COUNT) ? range.Count : range.Offset + range.Count);
	uint32_t											blockStart			= (0 == database.Val.BlockSize) ? 0				: (uint32_t)range.Offset / database.Val.BlockSize;
	uint32_t											blockStop			= (0 == database.Val.BlockSize) ? (uint32_t)-1	: (uint32_t)(maxRecord / database.Val.BlockSize);
	if(0 < database.Val.BlockSize) {
		if(0 == database.Val.BlocksOnDisk.size()) {
			blockStart									= 0;
			blockStop									= 0;
		}
		else {
			if(blockStop > database.Val.BlocksOnDisk[database.Val.BlocksOnDisk.size() - 1])
				blockStop									= database.Val.BlocksOnDisk[database.Val.BlocksOnDisk.size() - 1];
			if(blockStart > database.Val.BlocksOnDisk[database.Val.BlocksOnDisk.size() - 1])
				blockStart									= database.Val.BlocksOnDisk[database.Val.BlocksOnDisk.size() - 1];
		}
	}
	blockRange										= {blockStart, blockStop - blockStart + 1};

	if(0 == database.Val.BlockSize) {
		int32_t												iNewBlock			= ::blt::tableFileLoad(loadCache, database, folder);
		gpk_necall(iNewBlock, "%s", "Missing table found.");	// We need to improve this in order to support missing blocks.
		gpk_necall(::recordRangeBlock(database, range, 0, iNewBlock, output_records), "Error accessing range for block: %u.", 0);
	}
	else {
		::gpk::array_pod<uint32_t>							blocksToProcess		= {};
		for(uint32_t iBlockOnDisk = 0; iBlockOnDisk < database.Val.BlocksOnDisk.size(); ++iBlockOnDisk) {
			const uint32_t										blockOnDisk			= database.Val.BlocksOnDisk[iBlockOnDisk];
			if(::gpk::in_range(blockOnDisk, blockRange.Offset, blockRange.Offset + blockRange.Count))
				gpk_necall(blocksToProcess.push_back(blockOnDisk), "%s.", "Out of memory?");
		}
		for(uint32_t iBlock = 0; iBlock < blocksToProcess.size(); ++iBlock) {
			const uint32_t										idBlockToLoad		= blocksToProcess[iBlock];
			int32_t												iNewBlock			= ::blt::blockFileLoad(loadCache, database, folder, idBlockToLoad);
			gpk_necall(iNewBlock, "Missing block found: %u. This shouldn't happen because we already filtered which blocks are on disk, so either the block is corrupted and it has to be restored before the query executes.", idBlockToLoad);	// We need to improve this in order to support missing blocks.
			gpk_necall(::recordRangeBlock(database, range, idBlockToLoad, iNewBlock, output_records), "Error accessing range for block: %u.", idBlockToLoad);
		}
	}
	return 0;
}
