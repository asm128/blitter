#include "blitter_process.h"
#include "blitter_output.h"

#include "gpk_parse.h"
#include "gpk_view_bit.h"
#include "gpk_bit.h"
#include "gpk_chrono.h"

::gpk::error_t									blt::blockWrite							(::blt::SWriteCache & writeCache, const ::gpk::vcc & dbFolderName, const ::gpk::vcc & dbName, const ::gpk::vcc & encryptionKey, ::blt::DATABASE_HOST hostType, const ::gpk::vcu8 & partBytes, uint32_t iPart)		{
	::gpk::achar							partFileName							= writeCache.PartFileName;
	::gpk::achar							pathToWriteTo							= writeCache.PathToWriteTo;
	::gpk::clear(partFileName, pathToWriteTo, writeCache.LoadCache.Deflated, writeCache.LoadCache.Encrypted);
	pathToWriteTo									= dbFolderName;
	if(pathToWriteTo.size() && pathToWriteTo[pathToWriteTo.size() - 1] != '/')
		pathToWriteTo.push_back('/');
	gpk_necall(::blt::blockFileName(partFileName, dbName, encryptionKey.size() > 0, hostType, iPart), "%s", "??");
	gpk_necall(pathToWriteTo.append(partFileName), "%s", "Out of memory?");
	return ::gpk::fileFromMemorySecure(writeCache.LoadCache, pathToWriteTo, encryptionKey.cu8(), ::gpk::bit_true(hostType, ::blt::DATABASE_HOST_DEFLATE), partBytes.cu8());
}

::gpk::error_t									databaseFlush							(::blt::SWriteCache & cache, ::blt::TNamedBlitterDB & db, ::gpk::vcc folder) {
	::gpk::view_bit<const uint32_t>						dirty									= {db.Val.BlockDirty.begin(), db.Val.Blocks.size()};
	for(uint32_t iBlock = 0; iBlock < db.Val.Blocks.size(); ++iBlock) {
		if(dirty[iBlock]) {
			::gpk::achar						folderName								= {};
			gpk_necall(::blt::tableFolderInit(folderName, folder, db.Key, db.Val.BlockSize), "Failed to initialize folder for table '%s'. Not enough permissions?", ::gpk::toString(db.Key).begin());
			gpk_necs(::blt::blockWrite(cache, folderName, db.Key, db.Val.EncryptionKey.cc(), db.Val.HostType, db.Val.Blocks[iBlock]->Bytes.cu8(), db.Val.BlockIndices[iBlock]));
		}
	}
	memset(db.Val.BlockDirty.begin(), 0, sizeof(uint32_t) * db.Val.BlockDirty.size());
	return 0;
}

::gpk::error_t									blt::blitterFlush						(::blt::SBlitter & appState) {
	::blt::SWriteCache									cache									= {appState.LoadCache};
	for(uint32_t iDatabase = 0; iDatabase < appState.Databases.size(); ++iDatabase) {
		::blt::TNamedBlitterDB								& db									= appState.Databases[iDatabase];
		gpk_necall(::databaseFlush(cache, db, appState.Folder), "Failed to flush database '%s'.", ::gpk::toString(db.Key).begin());
	}
	return 0;
}

static	::gpk::error_t							queryGetDetail
	( ::gpk::SLoadCache								& loadCache
	, const ::gpk::SExpressionReader				& expressionReader
	, ::gpk::array_obj<::blt::TNamedBlitterDB>		& databases
	, const uint32_t								idxDatabase
	, const ::blt::SBlitterQuery					& query
	, ::gpk::achar						& output
	, const ::gpk::vcc					& folder
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
		const ::gpk::vcc						fieldToExpand						= query.ExpansionKeys[idxExpand];
		const int32_t										indexValueNode						= ::gpk::jsonObjectValueGet(currentDBBlock.Reader, (uint32_t)indexRecordNode, {fieldToExpand.begin(), fieldToExpand.size()});
		const ::gpk::vcc						currentRecordView					= currentDBBlock.Reader.View[indexRecordNode];
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
				int64_t											nextTableRecordIndex				= -1LL;
				const ::gpk::vcc					digitsToDetailView					= currentDBBlock.Reader.View[indexValueNode];
				gpk_necall(::gpk::parseIntegerDecimal(digitsToDetailView, nextTableRecordIndex), "%s", "Out of memory?");

				const char										* appendStart						= currentRecordView.begin();
				const char										* appendStop						= digitsToDetailView.begin();
				gpk_necall(output.append(appendStart, (uint32_t)(appendStop - appendStart)), "%s", "Out of memory?");
				bool											bFound								= false;
				for(uint32_t iDB = 0; iDB < databases.size(); ++iDB) {
					::blt::TNamedBlitterDB							& nextTable							= databases[iDB];
					if(nextTable.Key == fieldToExpand || 0 <= ::gpk::find(fieldToExpand, ::gpk::view_array<const ::gpk::vcc>{nextTable.Val.Bindings})) {
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
				const char					* appendStart						= currentRecordView.begin();
				const ::gpk::vcc			arrayToDetailView					= currentDBBlock.Reader.View[indexValueNode];
				const char					* appendStop						= arrayToDetailView.begin();
				gpk_necall(output.append(appendStart, (uint32_t)(appendStop - appendStart)), "%s", "Out of memory?");
				gpk_necall(output.push_back('['), "%s", "Out of memory?");
				const ::gpk::SJSONNode		& arrayNode							= *currentDBBlock.Reader.Tree[indexValueNode];
				bool						bFound								= false;
				for(uint32_t iRef = 0, refCount = ::gpk::jsonArraySize(*currentDBBlock.Reader.Tree[indexValueNode]); iRef < refCount; ++iRef) {
					uint64_t					nextTableRecordIndex					= (uint64_t)-1LL;
					const ::gpk::error_t		indexArrayElement						= ::gpk::jsonArrayValueGet(arrayNode, iRef);
					const ::gpk::vcc			digitsToDetailView						= currentDBBlock.Reader.View[indexArrayElement];
					gpk_necall(::gpk::parseIntegerDecimal(digitsToDetailView, nextTableRecordIndex), "%s", "Out of memory?");
					for(uint32_t iDB = 0; iDB < databases.size(); ++iDB) {
						::blt::TNamedBlitterDB							& nextTable							= databases[iDB];
						if(nextTable.Key == fieldToExpand || 0 <= ::gpk::find(fieldToExpand, ::gpk::view_array<const ::gpk::vcc>{nextTable.Val.Bindings})) {
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

static	::gpk::error_t							fillEmptyBlocks				(const ::gpk::vcc & emptyBlockData, const uint32_t emptyBlocks, bool appendComma, ::gpk::achar & output)	{
	for(uint32_t iEmpty = 0; iEmpty < emptyBlocks; ++iEmpty) {
		gpk_necall(output.append(emptyBlockData), "%s", "Out of memory?");
		if(emptyBlocks - 1 != iEmpty)
			gpk_necall(output.push_back(','), "%s", "Out of memory?");
	}
	if(appendComma && emptyBlocks)
		gpk_necall(output.push_back(','), "%s", "Out of memory?");
	return 0;
}

static	::gpk::error_t							nextBlock					(const uint32_t iRangeInfo, const uint32_t lastRangeInfo, const ::gpk::view_array<const ::blt::SRangeBlockInfo>	& rangeInfo, const ::gpk::vcc & emptyBlockData, ::gpk::achar & output) {
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
	( ::gpk::SLoadCache						& loadCache
	, const ::gpk::SExpressionReader		& expressionReader
	, ::gpk::aobj<::blt::TNamedBlitterDB>	& databases
	, const uint32_t						idxDatabase
	, const ::blt::SBlitterQuery			& query
	, ::gpk::achar							& output
	, const ::gpk::vcc						& folder
	, const uint32_t						idxExpand
	) {
	if(0 == query.Range.Count) {
		gpk_necall(output.append(::gpk::view_const_string{"[]"}), "%s", "Out of memory?");
		return 0;
	}
	::gpk::apod<::gpk::minmaxu32>				relativeIndices						= {};
	::gpk::rangeu32								blockRange							= {};
	::gpk::aobj<::blt::SRangeBlockInfo>				rangeInfo							= {};
	::blt::TNamedBlitterDB									& databaseToRead					= databases[idxDatabase];
	::gpk::achar								emptyBlockData						= {};
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
			const ::gpk::vcc							rangeView							= rangeInfo[iView].OutputRecords;
			gpk_necall(output.append(rangeView), "%s", "Out of memory?");
			gpk_necall(::nextBlock(iView, lastRangeInfo, rangeInfo, emptyBlockData, output), "%s", "??");
		}
	}
	else {
		::blt::SBlitterQuery			elemQuery							= query;
		for(uint32_t iView = 0; iView < rangeInfo.size(); ++iView) {
			const ::blt::SRangeBlockInfo	& blockInfo							= rangeInfo[iView];
			const ::gpk::minmaxu32			rangeToExpand						= blockInfo.RelativeIndices;
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

static	int64_t			pushNewBlock
	( ::gpk::SLoadCache			& loadCache
	, ::blt::TNamedBlitterDB	& database
	, const ::gpk::vcc			& folder
	, const ::gpk::SJSONReader	& recordToAdd
	, const int32_t				idMaxBlockOnDisk
	) {
	const uint32_t					idNewBlock			= idMaxBlockOnDisk + 1;
	if(database.Val.Blocks.size() < ::blt::MAX_TABLE_BLOCKS_IN_MEMORY) {
		::gpk::pobj<::gpk::SJSONFile>	newBlock;
		newBlock->Bytes.push_back('[');
		loadCache.Deflated.clear();
		ree_if(0 == recordToAdd.Tree.size(), "Invalid json record! %s.", "Forgot to build the reader from the record?")
		gpk_necall(::gpk::jsonWrite(recordToAdd[0], recordToAdd.View, *(::gpk::achar*)&loadCache.Deflated), "Failed to write json record! %s", "Invalid format?");
		newBlock->Bytes.append(loadCache.Deflated.cc());
		newBlock->Bytes.push_back(']');
		gpk_necall(::gpk::jsonParse(newBlock->Reader, newBlock->Bytes), "%s", "Failed to parse new block.");
		const uint32_t										indexBlock			= database.Val.Blocks.push_back(newBlock);
		database.Val.BlockIndices.push_back(idNewBlock);
		database.Val.BlocksOnDisk.push_back(idNewBlock);
		database.Val.MaxBlockOnDisk						= ::gpk::max(database.Val.MaxBlockOnDisk, (int32_t)idNewBlock);
		database.Val.Offsets.push_back(idNewBlock * database.Val.BlockSize);
		database.Val.BlockDirty.resize((database.Val.Blocks.size() / 32) + 1, 0);
		database.Val.BlockTimes.push_back(::gpk::timeCurrentInUs());
		::gpk::view_bit<uint32_t>{database.Val.BlockDirty.begin(), database.Val.Blocks.size()}[indexBlock]	= true;
	}
	else {
		const uint64_t										* time						= 0;
		const uint32_t										idxBlock					= ::gpk::min(::gpk::view_array<const uint64_t>{database.Val.BlockTimes}, &time);
		::gpk::view_bit<uint32_t>							dirty						= {database.Val.BlockDirty.begin(), database.Val.Blocks.size()};
		if(dirty[idxBlock]) {
			::blt::SWriteCache								writeCache						= {loadCache};
			gpk_necall(::databaseFlush(writeCache, database, folder), "Failed to flush table '%s'. Not enough permissions?", ::gpk::toString(database.Key).begin());
		}
		gpk_necall(idxBlock, "%s", "Out of memory?");
		::gpk::pobj<::gpk::SJSONFile>	& pBlock					= database.Val.Blocks[idxBlock];
		::gpk::SJSONFile				& newBlock					= *pBlock;
		newBlock.Bytes.clear();
		newBlock.Reader.Reset();
		newBlock.Bytes.push_back('[');
		loadCache.Deflated.clear();
		ree_if(0 == recordToAdd.Tree.size(), "Invalid json record! %s.", "Forgot to build the reader from the record?")
		gpk_necall(::gpk::jsonWrite(recordToAdd[0], recordToAdd.View, *(::gpk::achar*)&loadCache.Deflated), "Failed to write json record! %s", "Invalid format?");
		newBlock.Bytes.append(loadCache.Deflated.cc());
		newBlock.Bytes.push_back(']');
		gpk_necall(::gpk::jsonParse(newBlock.Reader, newBlock.Bytes), "%s", "Failed to parse new block.");
		database.Val.BlockIndices	[idxBlock]			= idNewBlock;
		database.Val.BlocksOnDisk.push_back(idNewBlock);
		database.Val.Offsets		[idxBlock]			= idNewBlock * database.Val.BlockSize;
		database.Val.BlockTimes		[idxBlock]			= ::gpk::timeCurrentInUs();
		database.Val.MaxBlockOnDisk						= ::gpk::max(database.Val.MaxBlockOnDisk, (int32_t)idNewBlock);
		::gpk::view_bit<uint32_t>{database.Val.BlockDirty.begin(), database.Val.Blocks.size()}[idxBlock]	= true;
	}
	return idNewBlock * (uint64_t)database.Val.BlockSize;
}

static	int64_t								appendRecord
	( ::gpk::SLoadCache								& loadCache
	, ::blt::SDatabase								& database
	, const uint32_t								indexBlock
	, const ::gpk::SJSONReader						& recordToAdd
	) {
	loadCache.Deflated.clear();
	loadCache.Deflated.push_back(',');
	gpk_necall(::gpk::jsonWrite(recordToAdd.Tree[0], recordToAdd.View, *(::gpk::achar*)&loadCache.Deflated), "Failed to write json record! %s", "Invalid format?");
	::gpk::SJSONFile									& block				= *database.Blocks[indexBlock];
	const uint32_t										insertPos			= block.Bytes.size() - 1;
	const uint32_t										indexOfToken		= block.Reader.Token.size();
	gpk_necs(block.Bytes.insert(insertPos, loadCache.Deflated.cc()));
	::gpk::SJSONReaderState								& stateReader										= block.Reader.StateRead;
	block.Reader.Token[0].Span.End					= block.Reader.Token[0].Span.Begin;
	stateReader.Escaping							= false;
	stateReader.ExpectingSeparator					= true;
	stateReader.InsideString						= false;
	stateReader.NestLevel							= 1;
	stateReader.IndexCurrentElement					= 0;
	stateReader.CurrentElement						= &block.Reader.Token[stateReader.IndexCurrentElement];
	stateReader.DoneReading							= false;
	for(stateReader.IndexCurrentChar = insertPos; stateReader.IndexCurrentChar < (int32_t)block.Bytes.size(); ++stateReader.IndexCurrentChar) {
		gpk_necall(::gpk::jsonParseStep(block.Reader, block.Bytes), "%s", "Parse step failed.");
		bi_if(stateReader.DoneReading, "%i json characters read.", stateReader.IndexCurrentChar + 1);
	}
	ree_if(stateReader.NestLevel, "Nest level: %i (Needs to be zero).", stateReader.NestLevel);
	block.Reader.View.resize(block.Reader.Token.size());
	for(uint32_t iView = 0; iView < block.Reader.Token.size(); ++iView) {
		::gpk::SJSONToken												& currentElement									= block.Reader.Token[iView];
		block.Reader.View[iView]									= ::gpk::vcc{&block.Bytes[currentElement.Span.Begin], currentElement.Span.End - currentElement.Span.Begin};
	}
	block.Reader.Tree.resize(block.Reader.Token.size());
	for(uint32_t iToken = indexOfToken; iToken < block.Reader.Token.size(); ++iToken) {
		::gpk::pobj<::gpk::SJSONNode>	& pcurrentNode				= block.Reader.Tree[iToken];
		pcurrentNode->Token			= &block.Reader.Token[iToken];
		::gpk::SJSONNode				& currentNode				= *pcurrentNode;
		currentNode.ObjectIndex		= iToken;
		//if(-1 != currentNode.Token->ParentIndex)
		//	currentNode.Parent							= block.Reader.Tree[currentNode.Token->ParentIndex];
	}
	//const int32_t										idxRecordNode			= ::gpk::jsonArrayValueGet(block.Reader, 0, ::gpk::jsonArraySize(block.Reader, 0) - 1);
	::gpk::pobj<::gpk::SJSONNode>	& recordNode			= block.Reader.Tree[indexOfToken];
	if(-1 != recordNode->Token->ParentIndex)
		recordNode->Parent			= block.Reader.Tree[recordNode->Token->ParentIndex];
	::gpk::SJSONNode				& arrayNode				= *block.Reader.Tree[0];
	arrayNode.Children.push_back(recordNode);
	//::gpk::jsonTreeRebuild(block.Reader.Token, block.Reader.Tree);
	//block.Reader.Reset();
	//gpk_necall(::gpk::jsonParse(block.Reader, block.Bytes), "%s", "Failed to read JSON!");
	database.BlockDirty.resize((database.Blocks.size() / 32) + 1, 0);
	::gpk::view_bit<uint32_t>{database.BlockDirty.begin(), database.Blocks.size()}[indexBlock]	= true;
	return database.Offsets[indexBlock] + arrayNode.Children.size() - 1;
}

int64_t											blt::queryProcess
	( ::gpk::SLoadCache								& loadCache
	, ::gpk::array_obj<::blt::TNamedBlitterDB>		& databases
	, const ::gpk::SExpressionReader				& expressionReader
	, const ::blt::SBlitterQuery					& query
	, const ::gpk::vcc					& folder
	, ::gpk::achar						& output
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
	else if(query.Command == ::gpk::view_const_string{"model"}) {
		for(uint32_t iDB = 0; iDB < databases.size(); ++iDB) {
			if(query.Database == databases[iDB].Key)
				return ::blt::outputModel(output, databases[iDB].Val.Description);
		}
	}
	else if(query.Command == ::gpk::view_const_string{"form"}) {
		for(uint32_t iDB = 0; iDB < databases.size(); ++iDB) {
			if(query.Database == databases[iDB].Key)
				return 0;
		}
	}
	else if(query.Command == ::gpk::view_const_string{"push_back"}) {
		for(uint32_t iDB = 0; iDB < databases.size(); ++iDB) {
			::blt::TNamedBlitterDB								& database			= databases[iDB];
			if(query.Database != database.Key)
				continue;
			if(0 == database.Val.BlocksOnDisk.size())
				return ::pushNewBlock(loadCache, database, folder, query.RecordReader, -1);
			else {
				const uint32_t										idBlock				= database.Val.MaxBlockOnDisk;//::gpk::max(::gpk::view_const_uint32{database.Val.BlocksOnDisk});
				uint32_t											indexRecord			= (uint32_t)-1;
				uint32_t											indexBlock			= (uint32_t)-1;
				gpk_necall(::blt::recordLoad(loadCache, database, idBlock * (uint64_t)database.Val.BlockSize, indexRecord, indexBlock, folder), "Failed to load block for record: %lli.", query.Detail);
				::gpk::SJSONFile									& block				= *database.Val.Blocks[indexBlock];
				const ::gpk::SJSONNode								& tableNode			= *block.Reader[0];
				if(database.Val.BlockSize && tableNode.Children.size() >= database.Val.BlockSize)
					return ::pushNewBlock(loadCache, database, folder, query.RecordReader, idBlock);
				else
					return ::appendRecord(loadCache, database.Val, indexBlock, query.RecordReader);
			}
		}
	}
	else if(query.Command == ::gpk::view_const_string{"pop_back"}) {
		for(uint32_t iDB = 0; iDB < databases.size(); ++iDB) {
			if(query.Database == databases[iDB].Key) {
				return -1;	// Not implemented
		//		uint32_t											idBlock				= 0;;
		//		::gpk::max(::gpk::view_const_uint32{databases[iDB].Val.BlocksOnDisk}, &idBlock);
			}
		}
	}
	else if(query.Command == ::gpk::view_const_string{"replace"}) {
		for(uint32_t iDB = 0; iDB < databases.size(); ++iDB) {
			if(query.Database == databases[iDB].Key) {
				return -1;	// Not implemented
		//		uint32_t											idBlock				= 0;;
		//		::gpk::max(::gpk::view_const_uint32{databases[iDB].Val.BlocksOnDisk}, &idBlock);
			}
		}
	}

	error_printf("Database not found: %s.", ::gpk::toString(query.Database).begin());
	return -1;
}
