#include "blitter_process.h"
#include "gpk_stdstring.h"
#include "gpk_find.h"
#include "gpk_expression.h"

::gpk::error_t									processThisTable			(const ::gpk::view_const_string & missPath, const ::gpk::view_array<const ::blt::TKeyValBlitterDB>	& databases, ::gpk::view_const_string & tableName)	{
	::gpk::array_obj<::gpk::view_const_string>			fieldsToExpand;
	::gpk::split(missPath, '.', fieldsToExpand);
	const ::gpk::view_const_string						fieldToExpand				= fieldsToExpand[fieldsToExpand.size() - 1];
	for(uint32_t iTable = 0; iTable < databases.size(); ++iTable) {
		const ::blt::TKeyValBlitterDB						& dbKeyVal					= databases[iTable];
		if(dbKeyVal.Key == fieldToExpand) {
			tableName										= dbKeyVal.Key;
			return iTable;
		}
		for(uint32_t iAlias = 0; iAlias < dbKeyVal.Val.Bindings.size(); ++iAlias) {
			if(dbKeyVal.Val.Bindings[iAlias] == fieldToExpand) {
				tableName										= dbKeyVal.Key;
				return iTable;
			}
		}
	}
	return -1;
}

static	::gpk::error_t							processDetail						
	( ::gpk::array_obj<::blt::TKeyValBlitterDB>		& databases
	, const uint32_t								idxDatabase
	, const ::blt::SBlitterQuery					& query
	, ::gpk::array_pod<char_t>						& output
	, const ::gpk::view_const_string				& folder
	, const uint32_t								idxExpand
	) {
	::gpk::view_const_string								outputRecord						= {};
	uint32_t												nodeIndex							= (uint32_t)-1;
	uint32_t												blockIndex							= (uint32_t)-1;
	gpk_necall(::blt::recordGet(databases[idxDatabase], query.Detail, outputRecord, nodeIndex, blockIndex, folder), "Failed to load record range. Offset: %llu. Length: %llu.", query.Range.Offset, query.Range.Count);
	if(0 == query.Expand.size() || idxExpand >= query.Expand.size())
		gpk_necall(output.append(outputRecord), "%s", "Out of memory?");
	else {

	}
	return 0;
}

static	::gpk::error_t							processRange
	( ::gpk::array_obj<::blt::TKeyValBlitterDB>		& databases
	, const uint32_t								idxDatabase
	, const ::blt::SBlitterQuery					& query
	, ::gpk::array_pod<char_t>						& output
	, const ::gpk::view_const_string				& folder
	, const uint32_t								idxExpand
	) {
	::gpk::array_obj<::gpk::view_const_string>				rangeViews							= {};
	::gpk::array_pod<::gpk::SMinMax<uint32_t>>				nodeIndices							= {};
	::gpk::SRange<uint32_t>									blockRange							= {};
	gpk_necall(::blt::recordRange(databases[idxDatabase], query.Range, rangeViews, nodeIndices, blockRange, folder), "Failed to load record range. Offset: %llu. Length: %llu.", query.Range.Offset, query.Range.Count);
	if(0 == query.Expand.size() || idxExpand >= query.Expand.size()) {
		gpk_necall(output.push_back('['), "%s", "Out of memory?");
		for(uint32_t iView = 0; iView < rangeViews.size(); ++iView) {
			const ::gpk::view_const_string					rangeView								= rangeViews[iView];
			gpk_necall(output.append(rangeView), "%s", "Out of memory?");
			if(rangeViews.size() -1 != iView)
				gpk_necall(output.push_back(','), "%s", "Out of memory?");
		}
		gpk_necall(output.push_back(']'), "%s", "Out of memory?");
	}
	else {
	}
	return 0;
}

::gpk::error_t									blt::processQuery						
	( ::gpk::array_obj<::blt::TKeyValBlitterDB>		& databases
	, const ::blt::SBlitterQuery					& query
	, ::gpk::array_pod<char_t>						& output
	, const ::gpk::view_const_string				& folder
	) {
	for(uint32_t iDB = 0; iDB < databases.size(); ++iDB) {
		if(query.Database == databases[iDB].Key) {
			if(0 <= query.Detail)
				return ::processDetail(databases, iDB, query, output, folder, 0);
			else 
				return ::processRange(databases, iDB, query, output, folder, 0);
		}
	}
	error_printf("Database not found: %s.", query.Database.begin());
	return -1;
}

::gpk::error_t									blt::recordGet	
	( ::blt::TKeyValBlitterDB			& database
	, const uint64_t					absoluteIndex
	, ::gpk::view_const_string			& output_record
	, uint32_t							& relativeIndex
	, uint32_t							& blockIndex
	, const ::gpk::view_const_string	& folder
	) {
	const uint32_t										indexBlock								= (0 == database.Val.BlockSize) ? (uint32_t)-1	: (uint32_t)(absoluteIndex / database.Val.BlockSize);
	const int32_t										iBlockElem								= ::blt::blockFileLoad(database, folder, indexBlock);
	gpk_necall(iBlockElem, "Failed to load database block: %s.", "??");

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
	( ::blt::TKeyValBlitterDB						& database
	, const ::gpk::SRange<uint64_t>					& range
	, ::gpk::array_obj<::gpk::view_const_string>	& output_records
	, ::gpk::array_pod<::gpk::SMinMax<uint32_t>>	& relativeIndices
	, ::gpk::SRange<uint32_t>						& blockRange
	, const ::gpk::view_const_string				& folder
	) {
	const uint64_t										maxRecord			= ((range.Count == ::blt::MAX_TABLE_RECORD_COUNT) ? range.Count : range.Offset + range.Count);
	const uint32_t										blockStart			= (0 == database.Val.BlockSize) ? 0				: (uint32_t)range.Offset / database.Val.BlockSize;
	const uint32_t										blockStop			= (0 == database.Val.BlockSize) ? (uint32_t)-1	: (uint32_t)(maxRecord / database.Val.BlockSize);
	blockRange										= {blockStart, blockStop - blockStart};
	for(uint32_t iBlock = blockStart; iBlock < blockStop; ++iBlock) {
		int32_t												iNewBlock			= ::blt::blockFileLoad(database, folder, iBlock);
		bi_if(-1 == iNewBlock, "Stop block found: %u.", iNewBlock);	// We need to improve this in order to support missing blocks.
		gpk_necall(iNewBlock, "Failed to load database block: %s.", "??");
		const ::gpk::SJSONReader							& readerBlock		= database.Val.Blocks[iNewBlock]->Reader;
		ree_if(0 == readerBlock.Tree.size(), "%s", "Invalid block data.");
		const ::gpk::SJSONNode								& jsonRoot			= *readerBlock.Tree[0];
		ree_if(::gpk::JSON_TYPE_ARRAY != jsonRoot.Object->Type, "Invalid json type: %s", ::gpk::get_value_label(jsonRoot.Object->Type).begin()); 

		const uint64_t										offsetRecord		= database.Val.Offsets[iNewBlock];
		const uint32_t										startRecordRelative	= ::gpk::max(0, (int32_t)(range.Offset - offsetRecord));
		const uint32_t										stopRecordRelative	= ::gpk::min(::gpk::jsonArraySize(*readerBlock[0]) - 1U, ::gpk::min(database.Val.BlockSize - 1, (uint32_t)((range.Offset + range.Count) - offsetRecord)));
		::gpk::SMinMax<int32_t>								blockNodeIndices	= 
			{ ::gpk::jsonArrayValueGet(jsonRoot, startRecordRelative)
			, ::gpk::jsonArrayValueGet(jsonRoot, stopRecordRelative)
			};
		gpk_necall(relativeIndices.push_back({startRecordRelative, stopRecordRelative}), "%s", "Out of memory?");

		::gpk::view_const_string							record				= 
			{ readerBlock.View[blockNodeIndices.Min].begin()
			, (uint32_t)(readerBlock.View[blockNodeIndices.Max].end() - readerBlock.View[blockNodeIndices.Min].begin())
			};
		gpk_necall(output_records.push_back(record), "%s", "Out of memory?");
	}
	return 0;
}
