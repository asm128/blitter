#include "blitter_process.h"
#include "gpk_stdstring.h"
#include "gpk_find.h"
#include "gpk_expression.h"
#include "gpk_parse.h"

static	::gpk::error_t							processDetail
	( ::blt::SLoadCache								& loadCache
	, ::gpk::array_obj<::blt::TKeyValBlitterDB>		& databases
	, const uint32_t								idxDatabase
	, const ::blt::SBlitterQuery					& query
	, ::gpk::array_pod<char_t>						& output
	, const ::gpk::view_const_string				& folder
	, const uint32_t								idxExpand
	) {
	::gpk::view_const_string							outputRecord						= {};
	uint32_t											nodeIndex							= (uint32_t)-1;
	uint32_t											blockIndex							= (uint32_t)-1;
	::blt::TKeyValBlitterDB								& databaseToRead					= databases[idxDatabase];
	gpk_necall(::blt::recordGet(loadCache, databaseToRead, query.Detail, outputRecord, nodeIndex, blockIndex, folder), "Failed to load record range. Offset: %llu. Length: %llu.", query.Range.Offset, query.Range.Count);
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
			const ::gpk::JSON_TYPE							refNodeTYpe								= currentDBBlock.Reader.Object[indexValueNode].Type;
			if(::gpk::JSON_TYPE_NUMBER != refNodeTYpe && ::gpk::JSON_TYPE_ARRAY != refNodeTYpe) {
				info_printf("Invalid value type: %s.", ::gpk::get_value_label(refNodeTYpe).begin());
				gpk_necall(output.append(currentRecordView), "%s", "Out of memory?");
			}
			else if(::gpk::JSON_TYPE_NUMBER == refNodeTYpe) {
				uint64_t										nextTableRecordIndex				= (uint64_t)-1LL;
				const ::gpk::view_const_string					digitsToDetailView					= currentDBBlock.Reader.View[indexValueNode];
				gpk_necall(::gpk::parseIntegerDecimal(digitsToDetailView, &nextTableRecordIndex), "%s", "Out of memory?");

				const char										* appendStart						= currentRecordView.begin();
				const char										* appendStop						= digitsToDetailView.begin();
				gpk_necall(output.append(appendStart, (uint32_t)(appendStop - appendStart)), "%s", "Out of memory?");
				for(uint32_t iDB = 0; iDB < databases.size(); ++iDB) {
					::blt::TKeyValBlitterDB							& nextTable							= databases[iDB];
					if(nextTable.Key == fieldToExpand || 0 <= ::gpk::find(fieldToExpand, ::gpk::view_array<const ::gpk::view_const_string>{nextTable.Val.Bindings})) {
						::blt::SBlitterQuery nextQuery;
						nextQuery.Database							= nextTable.Key;
						nextQuery.Detail							= nextTableRecordIndex;
						nextQuery.Expand							= query.Expand;
						nextQuery.ExpansionKeys						= query.ExpansionKeys;
						ce_if(::processDetail(loadCache, databases, iDB, nextQuery, output, folder, idxExpand + 1), "%s", "Failed to unroll detail.");
						break;
					}
				}
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
				const ::gpk::SJSONNode							& arrayNode								= *currentDBBlock.Reader.Tree[indexValueNode];
				for(uint32_t iRef = 0, refCount = ::gpk::jsonArraySize(*currentDBBlock.Reader.Tree[indexValueNode]); iRef < refCount; ++iRef) {
					uint64_t										nextTableRecordIndex					= (uint64_t)-1LL;
					const ::gpk::error_t							indexArrayElement						= ::gpk::jsonArrayValueGet(arrayNode, iRef);
					const ::gpk::view_const_string					digitsToDetailView						= currentDBBlock.Reader.View[indexArrayElement];
					gpk_necall(::gpk::parseIntegerDecimal(digitsToDetailView, &nextTableRecordIndex), "%s", "Out of memory?");
					for(uint32_t iDB = 0; iDB < databases.size(); ++iDB) {
						::blt::TKeyValBlitterDB							& nextTable							= databases[iDB];
						if(nextTable.Key == fieldToExpand || 0 <= ::gpk::find(fieldToExpand, ::gpk::view_array<const ::gpk::view_const_string>{nextTable.Val.Bindings})) {
							::blt::SBlitterQuery nextQuery;
							nextQuery.Database							= nextTable.Key;
							nextQuery.Detail							= nextTableRecordIndex;
							nextQuery.Expand							= query.Expand;
							nextQuery.ExpansionKeys						= query.ExpansionKeys;
							ce_if(::processDetail(loadCache, databases, iDB, nextQuery, output, folder, idxExpand + 1), "%s", "Failed to unroll detail.");
							break;
						}
					}
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

static ::gpk::error_t							fillEmptyBlocks
	( const ::gpk::view_const_char							& emptyBlockData
	, uint32_t												emptyBlocks
	, bool													appendComma
	, ::gpk::array_pod<char_t>								& output
	) {
	for(uint32_t iEmpty = 0; iEmpty < emptyBlocks; ++iEmpty) {
		gpk_necall(output.append(emptyBlockData), "%s", "Out of memory?");
		if(emptyBlocks - 1 != iEmpty)
			gpk_necall(output.push_back(','), "%s", "Out of memory?");
	}
	if(appendComma && emptyBlocks)
		gpk_necall(output.push_back(','), "%s", "Out of memory?");
	return 0;
}

static	::gpk::error_t							processRange
	( ::blt::SLoadCache								& loadCache
	, ::gpk::array_obj<::blt::TKeyValBlitterDB>		& databases
	, const uint32_t								idxDatabase
	, const ::blt::SBlitterQuery					& query
	, ::gpk::array_pod<char_t>						& output
	, const ::gpk::view_const_string				& folder
	, const uint32_t								idxExpand
	) {
	::gpk::array_pod<::gpk::SMinMax<uint32_t>>				relativeIndices						= {};
	::gpk::SRange<uint32_t>									blockRange							= {};
	::gpk::array_obj<::blt::SRangeBlockInfo>				rangeInfo							= {};
	::blt::TKeyValBlitterDB									& databaseToRead					= databases[idxDatabase];
	::gpk::array_pod<char_t>								emptyBlockData		= {};
	const ::gpk::view_const_string							empty_record		= "{},";
	for(uint32_t iRecord = 0; iRecord < databaseToRead.Val.BlockSize; ++iRecord)
		gpk_necall(emptyBlockData.append(empty_record), "%s", "Out of memory?");

	if(emptyBlockData.size() > 0) // Remove last comma.
		emptyBlockData.resize(emptyBlockData.size() - 1);

	gpk_necall(::blt::recordRange(loadCache, databaseToRead, query.Range, folder, rangeInfo, blockRange), "Failed to load record range. Offset: %llu. Length: %llu.", query.Range.Offset, query.Range.Count);

	::gpk::SMinMax<uint32_t>								minMaxBlocksOnDisk					= {};
	for(uint32_t iBlockOnDisk = 0; iBlockOnDisk < databaseToRead.Val.BlocksOnDisk.size(); ++iBlockOnDisk) {
		const uint32_t											blockOnDisk							= databaseToRead.Val.BlocksOnDisk[iBlockOnDisk];
		if(::gpk::in_range(blockOnDisk, blockRange.Offset, blockRange.Offset + blockRange.Count)) {
				 if(blockOnDisk > minMaxBlocksOnDisk.Max) minMaxBlocksOnDisk.Max = blockOnDisk;
			else if(blockOnDisk < minMaxBlocksOnDisk.Min) minMaxBlocksOnDisk.Min = blockOnDisk;
		}
	}

	const uint32_t											lastRangeInfo						= rangeInfo.size() - 1;
	gpk_necall(output.push_back('['), "%s", "Out of memory?");
	if((0 < databaseToRead.Val.BlockSize) && rangeInfo.size()) {
		const uint32_t											recordsToAvoid						= (query.Range.Offset % databaseToRead.Val.BlockSize);
		const uint32_t											emptyBlocks							= rangeInfo[0].BlockId - blockRange.Offset;
		if(0 == recordsToAvoid)
			gpk_necall(::fillEmptyBlocks(emptyBlockData, emptyBlocks, lastRangeInfo != 0, output), "%s", "Out of memory?");
		else {
			if(emptyBlocks > 0) {
				for(uint32_t iElem = recordsToAvoid; iElem < databaseToRead.Val.BlockSize; ++iElem) {
					output.append(::gpk::view_const_string{"{},"});
				}
				gpk_necall(::fillEmptyBlocks(emptyBlockData, emptyBlocks - 1, lastRangeInfo != 0, output), "%s", "Out of memory?");
			}
		}
	}

	if(0 == query.Expand.size() || idxExpand >= query.ExpansionKeys.size()) {
		for(uint32_t iView = 0; iView < rangeInfo.size(); ++iView) {
			const ::gpk::view_const_string						rangeView							= rangeInfo[iView].OutputRecords;
			gpk_necall(output.append(rangeView), "%s", "Out of memory?");
			if(rangeInfo.size() -1 != iView)
				gpk_necall(output.push_back(','), "%s", "Out of memory?");

			if(iView < lastRangeInfo) {
				const uint32_t										emptyBlocks							= rangeInfo[iView + 1].BlockId - rangeInfo[iView].BlockId;
				if(emptyBlocks > 1) {
					info_printf("Empty blocks bettween: %u and %u.", rangeInfo[iView].BlockId, rangeInfo[iView + 1].BlockId);
					gpk_necall(::fillEmptyBlocks(emptyBlockData, emptyBlocks - 1, lastRangeInfo != iView, output), "%s", "Out of memory?");
				}
			}
		}
	}
	else {
		::blt::SBlitterQuery								elemQuery							= query;
		for(uint32_t iView = 0; iView < rangeInfo.size(); ++iView) {
			const ::blt::SRangeBlockInfo						& blockInfo							= rangeInfo[iView];
			const ::gpk::SMinMax<uint32_t>						rangeToExpand						= blockInfo.RelativeIndices;
			for(uint32_t iRecord = rangeToExpand.Min; iRecord < rangeToExpand.Max + 1; ++iRecord) {
				elemQuery.Detail								= iRecord + blockInfo.BlockId * databaseToRead.Val.BlockSize;
				gpk_necall(::processDetail(loadCache, databases, idxDatabase, elemQuery, output, folder, idxExpand), "%s", "??");
				if(rangeToExpand.Max != iRecord)
					gpk_necall(output.push_back(','), "%s", "Out of memory?");
			}
			if(rangeInfo.size() - 1 != iView)
				gpk_necall(output.push_back(','), "%s", "Out of memory?");
			if(iView < lastRangeInfo) {
				const uint32_t										emptyBlocks							= rangeInfo[iView + 1].BlockId - blockInfo.BlockId;
				if(emptyBlocks > 1) {
					info_printf("Empty blocks bettween: %u and %u.", blockInfo.BlockId, rangeInfo[iView + 1].BlockId);
					gpk_necall(::fillEmptyBlocks(emptyBlockData, emptyBlocks - 1, lastRangeInfo != iView, output), "%s", "Out of memory?");
				}
			}
		}
	}
	gpk_necall(output.push_back(']'), "%s", "Out of memory?");
	return 0;
}

::gpk::error_t									blt::queryProcess
	( ::gpk::array_obj<::blt::TKeyValBlitterDB>		& databases
	, const ::blt::SBlitterQuery					& query
	, ::gpk::array_pod<char_t>						& output
	, const ::gpk::view_const_string				& folder
	) {
	::blt::SLoadCache									loadCache			= {};
	for(uint32_t iDB = 0; iDB < databases.size(); ++iDB) {
		if(query.Database == databases[iDB].Key) {
			if(0 <= query.Detail)
				return ::processDetail(loadCache, databases, iDB, query, output, folder, 0);
			else
				return ::processRange(loadCache, databases, iDB, query, output, folder, 0);
		}
	}
	error_printf("Database not found: %s.", query.Database.begin());
	return -1;
}

::gpk::error_t									blt::recordGet
	( ::blt::SLoadCache					& loadCache
	, ::blt::TKeyValBlitterDB			& database
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
	ree_if(::gpk::JSON_TYPE_ARRAY != jsonRoot.Object->Type, "Invalid json type: %s", ::gpk::get_value_label(jsonRoot.Object->Type).begin());
	const uint64_t										offsetRecord							= database.Val.Offsets[iBlockElem];
	relativeIndex									= ::gpk::max(0U, (uint32_t)(absoluteIndex - offsetRecord));
	blockIndex										= iBlockElem;
	output_record									= readerBlock.View[::gpk::jsonArrayValueGet(*readerBlock[0], relativeIndex)];
	return 0;
}

::gpk::error_t									blt::recordRange
	( ::blt::SLoadCache								& loadCache
	, ::blt::TKeyValBlitterDB						& database
	, const ::gpk::SRange<uint64_t>					& range
	, const ::gpk::view_const_string				& folder
	, ::gpk::array_obj<::blt::SRangeBlockInfo>		& output_records
	, ::gpk::SRange<uint32_t>						& blockRange
	) {
	const uint64_t										maxRecord			= ((range.Count == ::blt::MAX_TABLE_RECORD_COUNT) ? range.Count : range.Offset + range.Count);
	const uint32_t										blockStart			= (0 == database.Val.BlockSize) ? 0				: (uint32_t)range.Offset / database.Val.BlockSize;
	const uint32_t										blockStop			= (0 == database.Val.BlockSize) ? (uint32_t)-1	: (uint32_t)(maxRecord / database.Val.BlockSize);
	blockRange										= {blockStart, blockStop - blockStart + 1};

	if(0 == database.Val.BlockSize) {
		int32_t												iNewBlock			= ::blt::tableFileLoad(loadCache, database, folder);
		gpk_necall(iNewBlock, "%s", "Missing table found.");	// We need to improve this in order to support missing blocks.
		const ::gpk::SJSONReader							& readerBlock		= database.Val.Blocks[iNewBlock]->Reader;
		ree_if(0 == readerBlock.Tree.size(), "%s", "Invalid block data.");
		const ::gpk::SJSONNode								& jsonRoot			= *readerBlock.Tree[0];
		ree_if(::gpk::JSON_TYPE_ARRAY != jsonRoot.Object->Type, "Invalid json type: %s", ::gpk::get_value_label(jsonRoot.Object->Type).begin());

		const uint64_t										offsetRecord		= database.Val.Offsets[iNewBlock];
		::blt::SRangeBlockInfo								rangeInfo			= {};
		rangeInfo.RelativeIndices.Min					= ::gpk::max(0, (int32_t)(range.Offset - offsetRecord));
		rangeInfo.RelativeIndices.Max					= ::gpk::min(::gpk::jsonArraySize(*readerBlock[0]) - 1U, ::gpk::min(database.Val.BlockSize - 1, (uint32_t)((range.Offset + range.Count) - offsetRecord)));
		rangeInfo.BlockIndex							= iNewBlock;
		rangeInfo.BlockId								= 0;
		::gpk::SMinMax<int32_t>								blockNodeIndices	=
			{ ::gpk::jsonArrayValueGet(jsonRoot, rangeInfo.RelativeIndices.Min)
			, ::gpk::jsonArrayValueGet(jsonRoot, rangeInfo.RelativeIndices.Max)
			};
		rangeInfo.OutputRecords							=
			{ readerBlock.View[blockNodeIndices.Min].begin()
			, (uint32_t)(readerBlock.View[blockNodeIndices.Max].end() - readerBlock.View[blockNodeIndices.Min].begin())
			};
		gpk_necall(output_records.push_back(rangeInfo), "%s", "Out of memory?");
	}
	else {
		::gpk::array_pod<uint32_t>							blocksToProcess		= {};
		for(uint32_t iBlockOnDisk = 0; iBlockOnDisk < database.Val.BlocksOnDisk.size(); ++iBlockOnDisk) {
			const uint32_t										blockOnDisk			= database.Val.BlocksOnDisk[iBlockOnDisk];
			if(::gpk::in_range(blockOnDisk, blockRange.Offset, blockRange.Offset + blockRange.Count))
				gpk_necall(blocksToProcess.push_back(blockOnDisk), "%s.", "Out of memory?");
		}
		for(uint32_t iBlock = 0; iBlock < blocksToProcess.size(); ++iBlock) {
			const uint32_t										blockToLoad			= blocksToProcess[iBlock];
			int32_t												iNewBlock			= ::blt::blockFileLoad(loadCache, database, folder, blockToLoad);
			gpk_necall(iNewBlock, "Missing block found: %u.", blockToLoad);	// We need to improve this in order to support missing blocks.
			const ::gpk::SJSONReader							& readerBlock		= database.Val.Blocks[iNewBlock]->Reader;
			ree_if(0 == readerBlock.Tree.size(), "%s", "Invalid block data.");
			const ::gpk::SJSONNode								& jsonRoot			= *readerBlock.Tree[0];
			ree_if(::gpk::JSON_TYPE_ARRAY != jsonRoot.Object->Type, "Invalid json type: %s", ::gpk::get_value_label(jsonRoot.Object->Type).begin());

			const uint64_t										offsetRecord		= database.Val.Offsets[iNewBlock];
			::blt::SRangeBlockInfo								rangeInfo			= {};
			rangeInfo.RelativeIndices.Min					= ::gpk::max(0, (int32_t)(range.Offset - offsetRecord));
			rangeInfo.RelativeIndices.Max					= ::gpk::min(::gpk::jsonArraySize(*readerBlock[0]) - 1U, ::gpk::min(database.Val.BlockSize - 1, (uint32_t)((range.Offset + range.Count) - offsetRecord)));
			rangeInfo.BlockIndex							= iNewBlock;
			rangeInfo.BlockId								= blockToLoad;
			::gpk::SMinMax<int32_t>								blockNodeIndices	=
				{ ::gpk::jsonArrayValueGet(jsonRoot, rangeInfo.RelativeIndices.Min)
				, ::gpk::jsonArrayValueGet(jsonRoot, rangeInfo.RelativeIndices.Max)
				};
			rangeInfo.OutputRecords							=
				{ readerBlock.View[blockNodeIndices.Min].begin()
				, (uint32_t)(readerBlock.View[blockNodeIndices.Max].end() - readerBlock.View[blockNodeIndices.Min].begin())
				};
			gpk_necall(output_records.push_back(rangeInfo), "%s", "Out of memory?");
		}
	}
	return 0;
}
