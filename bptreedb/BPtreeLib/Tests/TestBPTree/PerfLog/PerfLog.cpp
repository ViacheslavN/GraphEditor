#include "../pch.h"
#include "PerfLog.h"



void CPerfLog::LogPerfStorage(CommonLib::TPrefCounterPtr pStoragePerformer)
{

	Log.Info("------STORAGE PERF------");
	Log.InfoT("read io count {0}, read data {1}, total time {2}", pStoragePerformer->GetCountOperation(bptreedb::storage::IStorage::eReadData), pStoragePerformer->GetDataOperation(bptreedb::storage::IStorage::eReadData), pStoragePerformer->GetTimeOperation(bptreedb::storage::IStorage::eReadData));
	Log.InfoT("write io count {0}, write data {1}, total time {2}", pStoragePerformer->GetCountOperation(bptreedb::storage::IStorage::eWriteData), pStoragePerformer->GetDataOperation(bptreedb::storage::IStorage::eWriteData), pStoragePerformer->GetTimeOperation(bptreedb::storage::IStorage::eWriteData));

}

void CPerfLog::LogBPtreeInsertPerf(CommonLib::TPrefCounterPtr pPerf)
{
	Log.Info("------BPTREE INSERT------");
	Log.InfoT("insert total time: {0},", pPerf->GetTimeOperation(bptreedb::eInsertValue));
	Log.InfoT("Flush total time {0}", pPerf->GetTimeOperation(bptreedb::eFlush));
}

void CPerfLog::LogBPtreePerf(CommonLib::TPrefCounterPtr  pPerf)
{
	Log.Info("------BPTREE NODE------");
	Log.InfoT("save inner node counts: {0}, missed {1}, total time {2}", pPerf->GetCountOperation(bptreedb::eSaveInnerNode), pPerf->GetCountOperation(bptreedb::eMissedNode), pPerf->GetTimeOperation(bptreedb::eSaveInnerNode));
	Log.InfoT("save leaf node counts: {0}, missed {1}, total time {2}", pPerf->GetCountOperation(bptreedb::eSaveLeafNode), pPerf->GetCountOperation(bptreedb::eMissedNode), pPerf->GetTimeOperation(bptreedb::eSaveLeafNode));
	Log.InfoT("load node {0}, total time {1}", pPerf->GetCountOperation(bptreedb::eLoadNode), pPerf->GetTimeOperation(bptreedb::eLoadNode));
	Log.InfoT("get node {0}, total time {1}", pPerf->GetCountOperation(bptreedb::eGetNode), pPerf->GetTimeOperation(bptreedb::eGetNode));
	Log.InfoT("get parent node {0}, total time {1}", pPerf->GetCountOperation(bptreedb::eGetParentNode), pPerf->GetTimeOperation(bptreedb::eGetParentNode));
	Log.InfoT("add to cache  {0}, total time {1}", pPerf->GetCountOperation(bptreedb::eAddToCache), pPerf->GetTimeOperation(bptreedb::eAddToCache));
		Log.InfoT("get node from cache  {0}, total time {1}", pPerf->GetCountOperation(bptreedb::eGetNodeFromCache), pPerf->GetTimeOperation(bptreedb::eGetNodeFromCache));
}

void CPerfLog::LogBPtreeRemovePerf(CommonLib::TPrefCounterPtr pPerf)
{
	Log.Info("------BPTREE DELETE------");
	Log.InfoT("remove total time: {0},", pPerf->GetTimeOperation(bptreedb::eRemoveValue));
	Log.InfoT("Flush total time {0}", pPerf->GetTimeOperation(bptreedb::eFlush));
}

void CPerfLog::LogBPtreeFindPerf(CommonLib::TPrefCounterPtr pPerf)
{
	Log.Info("------BPTREE FIND------");
	Log.InfoT("find key total time: {0},", pPerf->GetTimeOperation(bptreedb::eFindKey));
}