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
	::gpk::view_const_string								outputRecord						= {};
	uint32_t												nodeIndex							= (uint32_t)-1;
	uint32_t												blockIndex							= (uint32_t)-1;
	::blt::TKeyValBlitterDB									& databaseToRead					= databases[idxDatabase];
	gpk_necall(::blt::recordGet(loadCache, databaseToRead, query.Detail, outputRecord, nodeIndex, blockIndex, folder), "Failed to load record range. Offset: %llu. Length: %llu.", query.Range.Offset, query.Range.Count);
	if(0 == query.Expand.size() || idxExpand >= query.ExpansionKeys.size())
		gpk_necall(output.append(outputRecord), "%s", "Out of memory?");
	else {
		const ::gpk::SJSONFile							& currentDBBlock						= *databaseToRead.Val.Blocks[blockIndex];
		const int32_t									indexRecordNode							= ::gpk::jsonArrayValueGet(*currentDBBlock.Reader.Tree[0], nodeIndex);
		const ::gpk::view_const_string					fieldToExpand							= query.ExpansionKeys[idxExpand];
		const int32_t									indexValueNode							= ::gpk::jsonObjectValueGet(*currentDBBlock.Reader.Tree[indexRecordNode], currentDBBlock.Reader.View, fieldToExpand);
		const ::gpk::view_const_string					currentRecordView						= currentDBBlock.Reader.View[indexRecordNode];
		if(0 > indexValueNode) {
			info_printf("Cannot expand field. Field not found: %s.", fieldToExpand.begin());
			gpk_necall(output.append(currentRecordView), "%s", "Out of memory?");
		}
		else if(::gpk::JSON_TYPE_NUMBER != currentDBBlock.Reader.Object[indexValueNode].Type) {
			info_printf("Invalid value type: %s.", ::gpk::get_value_label(currentDBBlock.Reader.Object[indexValueNode].Type).begin());
			gpk_necall(output.append(currentRecordView), "%s", "Out of memory?");
		}
		else {
			uint64_t										nextTableRecordIndex					= (uint64_t)-1LL;
			const ::gpk::view_const_string					digitsToDetailView						= currentDBBlock.Reader.View[indexValueNode];
			gpk_necall(::gpk::parseIntegerDecimal(digitsToDetailView, &nextTableRecordIndex), "%s", "Out of memory?");

			const char*	appendStart						= currentRecordView.begin();
			const char*	appendStop						= digitsToDetailView.begin();
			gpk_necall(output.append(appendStart, (uint32_t)(appendStop - appendStart)), "%s", "Out of memory?");
			for(uint32_t iDB = 0; iDB < databases.size(); ++iDB) {
				::blt::TKeyValBlitterDB							& nextTable								= databases[iDB];
				if(nextTable.Key == fieldToExpand || 0 <= ::gpk::find(fieldToExpand, ::gpk::view_array<const ::gpk::view_const_string>{nextTable.Val.Bindings})) {
					::blt::SBlitterQuery nextQuery;
					nextQuery.Database							= nextTable.Key;
					nextQuery.Detail							= nextTableRecordIndex;
					nextQuery.Expand							= query.Expand;
					ce_if(::processDetail(loadCache, databases, iDB, nextQuery, output, folder, idxExpand + 1), "%s", "Failed to unroll detail.");
					break;
				}
			}
			appendStart									= digitsToDetailView.end();
			appendStop									= currentRecordView.end();
			gpk_necall(output.append(appendStart, (uint32_t)(appendStop - appendStart)), "%s", "Out of memory?");
		}
	}
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
	gpk_necall(::blt::recordRange(loadCache, databaseToRead, query.Range, folder, rangeInfo, blockRange), "Failed to load record range. Offset: %llu. Length: %llu.", query.Range.Offset, query.Range.Count);
	if(0 == query.Expand.size() || idxExpand >= query.ExpansionKeys.size()) {
		gpk_necall(output.push_back('['), "%s", "Out of memory?");
		for(uint32_t iView = 0; iView < rangeInfo.size(); ++iView) {
			const ::gpk::view_const_string					rangeView								= rangeInfo[iView].OutputRecords;
			gpk_necall(output.append(rangeView), "%s", "Out of memory?");
			if(rangeInfo.size() -1 != iView)
				gpk_necall(output.push_back(','), "%s", "Out of memory?");
		}
		gpk_necall(output.push_back(']'), "%s", "Out of memory?");
	}
	else {
		gpk_necall(output.push_back('['), "%s", "Out of memory?");
		const char*										appendStart								= 0;	// use this to keep track of the pointer of the source that we append to the output
		const char*										appendStop								= 0;	// use this to keep track of the pointer of the source that we append to the output
		for(uint32_t iView = 0; iView < rangeInfo.size(); ++iView) {
			const ::blt::SRangeBlockInfo					& blockInfo								= rangeInfo[iView];
			const ::gpk::view_const_string					rangeView								= blockInfo.OutputRecords;
			const ::gpk::SJSONFile							& currentDBBlock						= *databaseToRead.Val.Blocks[blockInfo.BlockIndex];
			const ::gpk::SMinMax<uint32_t>					rangeToExpand							= blockInfo.RelativeIndices;
			const ::gpk::view_const_string					fieldToExpand							= query.ExpansionKeys[idxExpand];
			for(uint32_t iRecord = rangeToExpand.Min; iRecord < rangeToExpand.Max; ++iRecord) {
				const int32_t									indexRecordNode							= ::gpk::jsonArrayValueGet(*currentDBBlock.Reader.Tree[0], iRecord);
				const int32_t									indexValueNode							= ::gpk::jsonObjectValueGet(*currentDBBlock.Reader.Tree[indexRecordNode], currentDBBlock.Reader.View, fieldToExpand);
				const ::gpk::view_const_string					currentRecordView						= currentDBBlock.Reader.View[indexRecordNode];
				if(0 > indexValueNode) {
					info_printf("%s", "Field not found.");
					gpk_necall(output.append(currentRecordView), "%s", "Out of memory?");
				}
				else if(::gpk::JSON_TYPE_NUMBER != currentDBBlock.Reader.Object[indexValueNode].Type) {
					info_printf("Invalid value type: %s.", ::gpk::get_value_label(currentDBBlock.Reader.Object[indexValueNode].Type).begin());
					gpk_necall(output.append(currentRecordView), "%s", "Out of memory?");
				}
				else {
					uint64_t										nextTableRecordIndex					= (uint64_t)-1LL;
					const ::gpk::view_const_string					digitsToDetailView						= currentDBBlock.Reader.View[indexValueNode];
					gpk_necall(::gpk::parseIntegerDecimal(digitsToDetailView, &nextTableRecordIndex), "%s", "Out of memory?");

					appendStart									= currentRecordView.begin();
					appendStop									= digitsToDetailView.begin();
					gpk_necall(output.append(appendStart, (uint32_t)(appendStop - appendStart)), "%s", "Out of memory?");
					for(uint32_t iDB = 0; iDB < databases.size(); ++iDB) {
						::blt::TKeyValBlitterDB							& nextTable								= databases[iDB];
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
				if(rangeToExpand.Max - 1 != iRecord)
					gpk_necall(output.push_back(','), "%s", "Out of memory?");
			}
			if(rangeInfo.size() - 1 != iView)
				gpk_necall(output.push_back(','), "%s", "Out of memory?");
		}
		gpk_necall(output.push_back(']'), "%s", "Out of memory?");
	}
	return 0;
}

::gpk::error_t									blt::processQuery
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
	blockRange										= {blockStart, blockStop - blockStart};

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
			const uint32_t blockOnDisk = database.Val.BlocksOnDisk[iBlockOnDisk];
			if(::gpk::in_range(blockOnDisk, blockStart, blockStop + 1))
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
