#include "blitter_process.h"

::llc::error_t			blt::recordLoad
	( ::llc::SLoadCache					& loadCache
	, ::blt::TNamedBlitterDB			& database
	, const uint64_t					absoluteIndex
	, uint32_t							& relativeIndex
	, uint32_t							& blockIndex
	, const ::llc::vcst_t		& folder
	) {
	const uint32_t				indexBlock								= (0 == database.Val.BlockSize) ? (uint32_t)-1	: (uint32_t)(absoluteIndex / database.Val.BlockSize);
	const int32_t				iBlockElem								= (0 == database.Val.BlockSize)
		? ::blt::tableFileLoad(loadCache, database, folder)
		: ::blt::blockFileLoad(loadCache, database, folder, indexBlock)
		;
	if_fail_fef(iBlockElem, "Failed to load database file: %s.", "??");
	const uint64_t				offsetRecord							= database.Val.Offsets[iBlockElem];
	relativeIndex			= ::llc::max(0U, (uint32_t)(absoluteIndex - offsetRecord));
	blockIndex				= iBlockElem;
	return 0;
}

::llc::error_t			blt::recordGet
	( ::llc::SLoadCache			& loadCache
	, ::blt::TNamedBlitterDB	& database
	, const uint64_t			absoluteIndex
	, ::llc::vcst_t				& output_record
	, uint32_t					& relativeIndex
	, uint32_t					& blockIndex
	, const ::llc::vcst_t		& folder
	) {
	if_fail_fef(::blt::recordLoad(loadCache, database, absoluteIndex, relativeIndex, blockIndex, folder), "%s", "Failed to load record indices.");

	const ::llc::SJSONReader	& readerBlock							= database.Val.Blocks[blockIndex]->Reader;
	ree_if(0 == readerBlock.Tree.size(), "%s", "Invalid block data.");
	const ::llc::SJSONNode		& jsonRoot								= *readerBlock.Tree[0];
	ree_if(::llc::JSON_TYPE_ARRAY != jsonRoot.Token->Type, "Invalid json type: %s", ::llc::get_value_label(jsonRoot.Token->Type).begin());
	ree_if(relativeIndex >= (uint32_t)::llc::jsonArraySize(*readerBlock[0]), "Index out of range: %i. Array size: %u.", relativeIndex, ::llc::jsonArraySize(*readerBlock[0]));
	output_record			= readerBlock.View[::llc::jsonArrayValueGet(*readerBlock[0], relativeIndex)];
	return 0;
}

::llc::error_t			recordRangeBlock
	( const ::blt::TNamedBlitterDB			& database
	, const ::llc::rangeu3_t				& range
	, const uint32_t						idBlock
	, const uint32_t						iNewBlock
	, ::llc::aobj<::blt::SRangeBlockInfo>	& output_records
	) {
	const ::llc::SJSONReader	& readerBlock		= database.Val.Blocks[iNewBlock]->Reader;
	ree_if(0 == readerBlock.Tree.size(), "%s", "Invalid block data.");
	const ::llc::SJSONNode		& jsonRoot			= *readerBlock.Tree[0];
	ree_if(::llc::JSON_TYPE_ARRAY != jsonRoot.Token->Type, "Invalid json type: %s", ::llc::get_value_label(jsonRoot.Token->Type).begin());

	const uint64_t				offsetRecord		= database.Val.Offsets[iNewBlock];
	::blt::SRangeBlockInfo		rangeInfo			= {};
	// Actuallly I believe we should handle this differently than for single-file database access, but it is possible that I already thought about this before.
	rangeInfo.RelativeIndices.Min	= ::llc::max(0, (int32_t)(range.Offset - offsetRecord));
	rangeInfo.RelativeIndices.Max	= ::llc::min(::llc::jsonArraySize(jsonRoot) - 1U, ::llc::min(database.Val.BlockSize - 1, (uint32_t)((range.Offset + range.Count) - offsetRecord - 1)));
	if(rangeInfo.RelativeIndices.Max < rangeInfo.RelativeIndices.Min) {
		rangeInfo.OutputRecords	= {};
		warning_printf("Invalid range: {Offset: %i, Count: %i}", range.Offset, range.Count);
		return 0;
	}

	rangeInfo.BlockIndex	= iNewBlock;
	rangeInfo.BlockId		= idBlock;
	const ::llc::minmaxs2_t		blockNodeIndices	=
		{ ::llc::jsonArrayValueGet(jsonRoot, rangeInfo.RelativeIndices.Min)
		, ::llc::jsonArrayValueGet(jsonRoot, rangeInfo.RelativeIndices.Max)
		};
	const ::llc::vcst_t			viewRecordFirst				= readerBlock.View[(((uint32_t)blockNodeIndices.Min) < readerBlock.View.size()) ? blockNodeIndices.Min : 0];
	const ::llc::vcst_t			viewRecordLast				= readerBlock.View[blockNodeIndices.Max];
	const uint32_t				charCount	= (uint32_t)
		( viewRecordLast.end()
		- viewRecordFirst.begin()
		);
	rangeInfo.OutputRecords							=
		{ viewRecordFirst.begin()
		, charCount
		};
	if_fail_fef(output_records.push_back(rangeInfo), "%s", "Out of memory?");
	return 0;
}

::llc::error_t			blt::recordRange
	( ::llc::SLoadCache						& loadCache
	, ::blt::TNamedBlitterDB				& database
	, const ::llc::rangeu3_t					& range
	, const ::llc::vcst_t						& folder
	, ::llc::aobj<::blt::SRangeBlockInfo>	& output_records
	, ::llc::rangeu2_t						& blockRange
	) {
	const uint64_t				maxRecord			= ((range.Count == ::blt::MAX_TABLE_RECORD_COUNT) ? range.Count : range.Offset + range.Count);
	uint32_t					blockStart			= (0 == database.Val.BlockSize) ? 0				: (uint32_t)range.Offset / database.Val.BlockSize;
	uint32_t					blockStop			= (0 == database.Val.BlockSize) ? (uint32_t)-1	: (uint32_t)(maxRecord / database.Val.BlockSize);
	if(0 < database.Val.BlockSize) {
		if(0 == database.Val.BlocksOnDisk.size()) {
			blockStart										= 0;
			blockStop										= 0;
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
		int32_t						iNewBlock			= ::blt::tableFileLoad(loadCache, database, folder);
		if_fail_fef(iNewBlock, "%s", "Missing table found.");	// We need to improve this in order to support missing blocks.
		if_fail_fef(::recordRangeBlock(database, range, 0, iNewBlock, output_records), "Error accessing range for block: %u.", 0);
	}
	else {
		::llc::au2_t				blocksToProcess		= {};
		for(uint32_t iBlockOnDisk = 0; iBlockOnDisk < database.Val.BlocksOnDisk.size(); ++iBlockOnDisk) {
			const uint32_t				blockOnDisk			= database.Val.BlocksOnDisk[iBlockOnDisk];
			if(::llc::in_range(blockOnDisk, blockRange.Offset, blockRange.Offset + blockRange.Count))
				if_fail_fef(blocksToProcess.push_back(blockOnDisk), "%s.", "Out of memory?");
		}
		for(uint32_t iBlock = 0; iBlock < blocksToProcess.size(); ++iBlock) {
			const uint32_t				idBlockToLoad		= blocksToProcess[iBlock];
			int32_t						iNewBlock			= ::blt::blockFileLoad(loadCache, database, folder, idBlockToLoad);
			if_fail_fef(iNewBlock, "Missing block found: %u. This shouldn't happen because we already filtered which blocks are on disk, so either the block is corrupted and it has to be restored before the query executes.", idBlockToLoad);	// We need to improve this in order to support missing blocks.
			if_fail_fef(::recordRangeBlock(database, range, idBlockToLoad, iNewBlock, output_records), "Error accessing range for block: %u.", idBlockToLoad);
		}
	}
	return 0;
}
