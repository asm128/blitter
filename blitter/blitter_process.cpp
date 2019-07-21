#include "blitter_process.h"
#include "gpk_stdstring.h"
#include "gpk_find.h"

::gpk::error_t							processThisTable			(const ::gpk::view_const_string & missPath, const ::gpk::view_array<const ::blt::TKeyValBlitterDB>	& databases, ::gpk::view_const_string & tableName)	{
	::gpk::array_obj<::gpk::view_const_string>	fieldsToExpand;
	::gpk::split(missPath, '.', fieldsToExpand);
	const ::gpk::view_const_string				fieldToExpand				= fieldsToExpand[fieldsToExpand.size() - 1];
	for(uint32_t iTable = 0; iTable < databases.size(); ++iTable) {
		const ::blt::TKeyValBlitterDB					& dbKeyVal					= databases[iTable];
		if(dbKeyVal.Key == fieldToExpand) {
			tableName								= dbKeyVal.Key;
			return iTable;
		}
		for(uint32_t iAlias = 0; iAlias < dbKeyVal.Val.Bindings.size(); ++iAlias) {
			if(dbKeyVal.Val.Bindings[iAlias] == fieldToExpand) {
				tableName								= dbKeyVal.Key;
				return iTable;
			}
		}
	}
	return -1;
}

::gpk::error_t									blt::processQuery						
	( ::gpk::array_obj<::blt::TKeyValBlitterDB>		& databases
	, const ::blt::SBlitterQuery					& query
	, ::gpk::array_pod<char_t>						& output
	, const ::gpk::view_const_string				& folder
	) {

	for(uint32_t iDB = 0; iDB < databases.size(); ++iDB) {
		if(query.Database == databases[iDB].Key) {
			::blt::TKeyValBlitterDB								& database								= databases[iDB];
			if(0 == database.Val.BlockSize && 0 == query.Expand.size()) {
				gpk_necall(::blt::tableFileLoad(database, folder), "Failed to load table: %s.", databases[iDB].Key.begin());
				return ::blt::generate_output_for_db(databases, query, output, 0);
			}
			else {
				if(0 <= query.Detail) {
					::gpk::view_const_string								outputRecord						= {};
					uint32_t												nodeIndex							= (uint32_t)-1;
					uint32_t												blockIndex							= (uint32_t)-1;
					gpk_necall(::blt::recordGet(database, query.Detail, outputRecord, nodeIndex, blockIndex, folder), "Failed to load record range. Offset: %llu. Length: %llu.", query.Range.Offset, query.Range.Count);
					gpk_necall(output.append(outputRecord), "%s", "Out of memory?");
				}
				else {
					::gpk::array_obj<::gpk::view_const_string>				rangeViews							= {};
					::gpk::array_pod<::gpk::SMinMax<uint32_t>>				nodeIndices							= {};
					::gpk::SRange<uint32_t>									blockRange							= {};
					gpk_necall(::blt::recordRange(database, query.Range, rangeViews, nodeIndices, blockRange, folder), "Failed to load record range. Offset: %llu. Length: %llu.", query.Range.Offset, query.Range.Count);
					gpk_necall(output.push_back('['), "%s", "Out of memory?");
					for(uint32_t iView = 0; iView < rangeViews.size(); ++iView) {
						const ::gpk::view_const_string					rangeView								= rangeViews[iView];
						gpk_necall(output.append(rangeView), "%s", "Out of memory?");
						if(rangeViews.size() -1 != iView)
							gpk_necall(output.push_back(','), "%s", "Out of memory?");
					}
					gpk_necall(output.push_back(']'), "%s", "Out of memory?");
				}
				return 0;
			}
		}
	}
	error_printf("Database not found: %s.", query.Database.begin());
	return -1;
}


static	::gpk::error_t							generate_record_with_expansion			(const ::gpk::view_array<const ::blt::TKeyValBlitterDB> & databases, const ::gpk::SJSONReader & databaseReader, const ::gpk::SJSONNode	& databaseNode, ::gpk::array_pod<char_t> & output, const ::gpk::view_array<const ::gpk::view_const_string> & fieldsToExpand, uint32_t indexFieldToExpand)	{
	//const ::gpk::SJSONNode								& node									= *databaseReader.Tree[iRecord];
	int32_t												partialMiss								= 0;
	if(0 == fieldsToExpand.size() || ::gpk::JSON_TYPE_OBJECT != databaseNode.Object->Type)
		::gpk::jsonWrite(&databaseNode, databaseReader.View, output);
	else {
		output.push_back('{');
		for(uint32_t iChild = 0; iChild < databaseNode.Children.size(); iChild += 2) { 
			uint32_t											indexKey								= databaseNode.Children[iChild + 0]->ObjectIndex;
			uint32_t											indexVal								= databaseNode.Children[iChild + 1]->ObjectIndex;
			const ::gpk::view_const_string						fieldToExpand							= fieldsToExpand[indexFieldToExpand];
			const bool											bExpand									= databaseReader.View[indexKey] == fieldToExpand && ::gpk::JSON_TYPE_NULL != databaseReader.Object[indexVal].Type;
			if(false == bExpand)  {
				::gpk::jsonWrite(databaseReader.Tree[indexKey], databaseReader.View, output);
				output.push_back(':');
				::gpk::jsonWrite(databaseReader.Tree[indexVal], databaseReader.View, output);
			}
			else {
				::gpk::jsonWrite(databaseReader.Tree[indexKey], databaseReader.View, output);
				output.push_back(':');
				bool												bSolved									= false;
				uint64_t											indexRecordToExpand						= 0;
				::gpk::stoull(databaseReader.View[indexVal], &indexRecordToExpand);
				for(uint32_t iDatabase = 0; iDatabase < databases.size(); ++iDatabase) {
					const ::blt::TKeyValBlitterDB						& childDatabase							= databases[iDatabase];
					bool												bAliasMatch								= -1 != ::gpk::find(fieldToExpand, {childDatabase.Val.Bindings.begin(), childDatabase.Val.Bindings.size()});
					int64_t												indexRecordToExpandRelative				= (int64_t)indexRecordToExpand - childDatabase.Val.Offsets[0];
					const ::gpk::SJSONReader							& childReader							= childDatabase.Val.Blocks[0]->Reader;
					if(0 == childReader.Tree.size()) // This database isn't loaded.
						continue;

					if(indexRecordToExpandRelative < 0) {
						info_printf("Out of range - requires reload or probably there is another database with this info.");
						continue;
					}
					if(childDatabase.Key == fieldToExpand || bAliasMatch) {
						const ::gpk::SJSONNode								& childRoot								= *childReader.Tree[0];
						if(indexRecordToExpandRelative >= childRoot.Children.size()) {
							info_printf("Out of range - requires reload or probably there is another database with this info.");
							continue;
						}
						if((indexFieldToExpand + 1) >= fieldsToExpand.size()) {
							if(indexRecordToExpandRelative < childRoot.Children.size())
								::gpk::jsonWrite(childRoot.Children[(uint32_t)indexRecordToExpandRelative], childReader.View, output);
							else
								::gpk::jsonWrite(databaseReader.Tree[indexVal], databaseReader.View, output);
						}
						else {
							if(indexRecordToExpandRelative < childRoot.Children.size()) {
								const int32_t									iRecordNode								= childRoot.Children[(uint32_t)indexRecordToExpandRelative]->ObjectIndex;
								::generate_record_with_expansion(databases, childReader, *childReader.Tree[iRecordNode], output, fieldsToExpand, indexFieldToExpand + 1);
							}
							else
								::gpk::jsonWrite(databaseReader.Tree[indexVal], databaseReader.View, output);
						}
						bSolved											= true;
					}
				}
				if(false == bSolved) {
					::gpk::jsonWrite(databaseReader.Tree[indexVal], databaseReader.View, output);
					++partialMiss;
				}
			}
			if((databaseNode.Children.size() - 2) > iChild)
				output.push_back(',');
		}
		output.push_back('}');
	}
	return partialMiss;
}

::gpk::error_t									blt::generate_output_for_db				
	( const ::gpk::view_array<const ::blt::TKeyValBlitterDB>	& databases
	, const ::blt::SBlitterQuery								& query
	, ::gpk::array_pod<char_t>									& output
	, int32_t													iBlock
	)
{
	int32_t												indexDB									= ::gpk::find(query.Database, ::gpk::view_array<const ::gpk::SKeyVal<::gpk::view_const_string, ::blt::SBlitterDB>>{databases.begin(), databases.size()});
	rew_if(-1 == indexDB, "Database not found : %s", query.Database.begin());
	::blt::TKeyValBlitterDB								dbObject								= databases[indexDB];
	const ::gpk::SJSONReader							& dbReader								= dbObject.Val.Blocks[iBlock]->Reader;
	if(0 == dbReader.Tree.size()) {
 		return 1;
	}
	const ::gpk::SJSONNode								& jsonRoot								= *dbReader.Tree[0];
	int32_t												partialMiss								= 0;
	int64_t												relativeDetail							= query.Detail - dbObject.Val.Offsets[iBlock];
	if(query.Detail >= 0) { // display detail
		if(0 == query.Expand.size())
			::gpk::jsonWrite(jsonRoot.Children[(uint32_t)relativeDetail], dbReader.View, output);
		else if(relativeDetail < 0 || relativeDetail >= jsonRoot.Children.size()) {
			::gpk::jsonWrite(&jsonRoot, dbReader.View, output);
			return partialMiss								= 1;
		}
		else {
			if(0 == query.Expand.size()) 
				::gpk::jsonWrite(jsonRoot.Children[(uint32_t)relativeDetail], dbReader.View, output);
			else {
				::gpk::array_obj<::gpk::view_const_string>			fieldsToExpand;
				::gpk::split(query.Expand, '.', fieldsToExpand);
				const int32_t										iRecordNode								= jsonRoot.Children[(uint32_t)relativeDetail]->ObjectIndex;
				partialMiss										+= ::generate_record_with_expansion(databases, dbReader, *dbReader[iRecordNode], output, fieldsToExpand, 0);
			}
		}
	}
	else {  // display multiple records
		if(0 == query.Expand.size() && 0 >= query.Range.Offset && query.Range.Count >= jsonRoot.Children.size())	// a larger range than available was requested and no expansion is required. Just send the whole thing
			::gpk::jsonWrite(&jsonRoot, dbReader.View, output);
		else {
			output.push_back('[');
			uint32_t											relativeQueryOffset						= (uint32_t)(query.Range.Offset - dbObject.Val.Offsets[iBlock]);
			const uint32_t										stopRecord								= (uint32_t)::gpk::min(relativeQueryOffset + query.Range.Count, (uint64_t)jsonRoot.Children.size());
			if(0 == query.Expand.size()) {
				for(uint32_t iRecord = relativeQueryOffset; iRecord < stopRecord; ++iRecord) {
					::gpk::jsonWrite(jsonRoot.Children[iRecord], dbReader.View, output);
					if((stopRecord - 1) > iRecord)
						output.push_back(',');
				}
			}
			else {
				::gpk::array_obj<::gpk::view_const_string>			fieldsToExpand;
				::gpk::split(query.Expand, '.', fieldsToExpand);
				for(uint32_t iRecord = (uint32_t)query.Range.Offset; iRecord < stopRecord; ++iRecord) {
					const int32_t										iRecordNode								= jsonRoot.Children[iRecord]->ObjectIndex;
					partialMiss										+= ::generate_record_with_expansion(databases, dbReader, *dbReader[iRecordNode], output, fieldsToExpand, 0);
					if((stopRecord - 1) > iRecord)
						output.push_back(',');
				}
			}
			output.push_back(']');
		}
	}
	return partialMiss;
}

::gpk::error_t									blt::recordRange
	( ::blt::TKeyValBlitterDB						& database
	, const ::gpk::SRange<uint64_t>					& range
	, ::gpk::array_obj<::gpk::view_const_string>	& output_records
	, ::gpk::array_pod<::gpk::SMinMax<uint32_t>>	& nodeIndices
	, ::gpk::SRange<uint32_t>						& blockRange
	, const ::gpk::view_const_string				& folder
	) {
	const uint64_t										maxRecord			= ((range.Count == ::blt::MAX_TABLE_RECORD_COUNT) ? range.Count : range.Offset + range.Count);
	const uint32_t										blockStart			= (0 == database.Val.BlockSize) ? 0				: (uint32_t)range.Offset / database.Val.BlockSize;
	const uint32_t										blockStop			= (0 == database.Val.BlockSize) ? (uint32_t)-1	: (uint32_t)(maxRecord / database.Val.BlockSize + one_if(maxRecord % database.Val.BlockSize) + 1);
	blockRange										= {blockStart, blockStop - blockStart};
	for(uint32_t iBlock = blockStart; iBlock < blockStop; ++iBlock) {
		int32_t												iNewBlock			= ::blt::blockFileLoad(database, folder, iBlock);
		bi_if(-1 == iNewBlock, "Stop block found: %u.", iNewBlock)
		//	continue;
		gpk_necall(iNewBlock, "Failed to load database block: %s.", "??");
		const ::gpk::SJSONReader							& readerBlock		= database.Val.Blocks[iNewBlock]->Reader;
		ree_if(0 == readerBlock.Tree.size(), "%s", "Invalid block data.");
		const ::gpk::SJSONNode								& jsonRoot			= *readerBlock.Tree[0];
		ree_if(::gpk::JSON_TYPE_ARRAY != jsonRoot.Object->Type, "Invalid json type: %s", ::gpk::get_value_label(jsonRoot.Object->Type).begin()); 
		const uint64_t										offsetRecord		= database.Val.Offsets[iNewBlock];
		const uint32_t										startRecordRelative	= ::gpk::max(0, (int32_t)(range.Offset - offsetRecord));
		const uint32_t										stopRecordRelative	= ::gpk::min(::gpk::jsonArraySize(*readerBlock[0]) - 1U, ::gpk::min(database.Val.BlockSize - 1, (uint32_t)((range.Offset + range.Count) - offsetRecord)));
		//::gpk::error_t										nodeIndexMax			= ::gpk::jsonArrayValueGet(*readerBlock[0], stopRecordRelative);
		::gpk::SMinMax<uint32_t>							blockNodeIndices		= {};
		blockNodeIndices.Min							= ::gpk::jsonArrayValueGet(*readerBlock[0], startRecordRelative);
		blockNodeIndices.Max							= ::gpk::jsonArrayValueGet(*readerBlock[0], stopRecordRelative);
		//	(-1 == nodeIndexMax)
		//	? ::gpk::jsonArrayValueGet(*readerBlock[0], ::gpk::jsonArraySize(*readerBlock[0]) - 1)
		//	: ::gpk::jsonArrayValueGet(*readerBlock[0], stopRecordRelative)
		//	;
		gpk_necall(nodeIndices.push_back(blockNodeIndices), "%s", "Out of memory?");
		::gpk::view_const_string							record				= 
			{ readerBlock.View[blockNodeIndices.Min].begin()
			, (uint32_t)(readerBlock.View[blockNodeIndices.Max].end() - readerBlock.View[blockNodeIndices.Min].begin())
			};
		gpk_necall(output_records.push_back(record), "%s", "Out of memory?");
	}
	return 0;
}


::gpk::error_t									blt::recordGet	
	( ::blt::TKeyValBlitterDB			& database
	, const uint64_t					absoluteIndex
	, ::gpk::view_const_string			& output_record
	, uint32_t							& nodeIndex
	, uint32_t							& blockIndex
	, const ::gpk::view_const_string	& folder
	) {
	const uint32_t										indexBlock								= (0 == database.Val.BlockSize) ? (uint32_t)-1	: (uint32_t)(absoluteIndex / database.Val.BlockSize);
	const int32_t										iBlockElem								= ::blt::blockFileLoad(database, folder, indexBlock);
	gpk_necall(iBlockElem, "Failed to load database block: %s.", "??");

	const ::gpk::SJSONReader							& readerBlock		= database.Val.Blocks[iBlockElem]->Reader;
	ree_if(0 == readerBlock.Tree.size(), "%s", "Invalid block data.");

	const ::gpk::SJSONNode								& jsonRoot			= *readerBlock.Tree[0];
	ree_if(::gpk::JSON_TYPE_ARRAY != jsonRoot.Object->Type, "Invalid json type: %s", ::gpk::get_value_label(jsonRoot.Object->Type).begin()); 
	const uint64_t										offsetRecord		= database.Val.Offsets[iBlockElem];
	const uint32_t										startRecordRelative	= ::gpk::max(0U, (uint32_t)(absoluteIndex - offsetRecord));

	blockIndex										= iBlockElem;
	nodeIndex										= ::gpk::jsonArrayValueGet(*readerBlock[0], startRecordRelative);
	output_record									= readerBlock.View[nodeIndex];
	return 0;
}
