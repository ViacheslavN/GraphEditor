#include "BPtreeDBContext.h"
#include "BPtreeDBUtils.h"
#include <filesystem>

namespace GraphEngine {
    namespace GeoDatabase {

        void SBPtreeDBPendingIndexes::AddExtent(const CommonLib::bbox& bb)
        {
            if(!IsNormalBox(bb))
                return;

            if(!IsNormalBox(extent))
            {
                extent.type = CommonLib::bbox_type_normal;
                extent.xMin = bb.xMin;
                extent.xMax = bb.xMax;
                extent.yMin = bb.yMin;
                extent.yMax = bb.yMax;
                return;
            }

            extent.xMin = (std::min)(extent.xMin, bb.xMin);
            extent.xMax = (std::max)(extent.xMax, bb.xMax);
            extent.yMin = (std::min)(extent.yMin, bb.yMin);
            extent.yMax = (std::max)(extent.yMax, bb.yMax);
        }

        SBPtreeDBPendingIndexes::~SBPtreeDBPendingIndexes()
        {
            DropBoxes();
        }

        void SBPtreeDBPendingIndexes::AddBox(const CommonLib::bbox& bb, int64_t nRowId)
        {
            if(!bCompleteBoxes)
                return;

            m_boxes.push_back(SBox{ bb.xMin, bb.yMin, bb.xMax, bb.yMax, nRowId });
            if(m_boxes.size() >= MaxMemoryBoxes)
                Spill();
        }

        void SBPtreeDBPendingIndexes::Spill()
        {
            try
            {
                if(!m_ptrFile.get())
                {
                    m_ptrFile.reset(new std::ofstream(std::filesystem::u8path(sTempPath), std::ios::binary | std::ios::trunc));
                    if(!m_ptrFile->is_open())
                        throw CommonLib::CExcBase("can't create the file");
                }
                m_ptrFile->write((const char*)m_boxes.data(), (std::streamsize)(m_boxes.size() * sizeof(SBox)));
                if(!m_ptrFile->good())
                    throw CommonLib::CExcBase("failed to write the file");
                m_nSpilled += m_boxes.size();
                m_boxes.clear();
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("BPtreeDB: failed to write the boxes of the spatial index to {0}", sTempPath, exc);
                throw;
            }
        }

        bool SBPtreeDBPendingIndexes::TakeBoxes(std::vector<std::pair<CommonLib::bbox, int64_t> >& entries)
        {
            entries.clear();
            if(!bCompleteBoxes)
            {
                DropBoxes();
                return false;
            }

            try
            {
                entries.reserve((size_t)m_nSpilled + m_boxes.size());
                auto add = [&](const SBox& box)
                {
                    CommonLib::bbox bb;
                    bb.type = CommonLib::bbox_type_normal;
                    bb.xMin = box.xMin;
                    bb.yMin = box.yMin;
                    bb.xMax = box.xMax;
                    bb.yMax = box.yMax;
                    entries.emplace_back(bb, box.nRowId);
                };

                if(m_ptrFile.get())
                {
                    m_ptrFile->close();
                    m_ptrFile.reset();
                    std::ifstream file(std::filesystem::u8path(sTempPath), std::ios::binary);
                    std::vector<SBox> chunk(1u << 16);
                    uint64_t nLeft = m_nSpilled;
                    while(nLeft > 0)
                    {
                        size_t nCount = (size_t)(std::min)(nLeft, (uint64_t)chunk.size());
                        file.read((char*)chunk.data(), (std::streamsize)(nCount * sizeof(SBox)));
                        if(!file.good())
                            throw CommonLib::CExcBase("failed to read the file");
                        for(size_t i = 0; i < nCount; ++i)
                            add(chunk[i]);
                        nLeft -= nCount;
                    }
                }

                for(const SBox& box : m_boxes)
                    add(box);
                DropBoxes();
                return true;
            }
            catch (std::exception& exc)
            {
                DropBoxes();
                CommonLib::CExcBase::RegenExc("BPtreeDB: failed to read the boxes of the spatial index from {0}", sTempPath, exc);
                throw;
            }
        }

        void SBPtreeDBPendingIndexes::DropBoxes()
        {
            std::vector<SBox>().swap(m_boxes);
            if(m_ptrFile.get())
            {
                m_ptrFile->close();
                m_ptrFile.reset();
            }
            if(m_nSpilled > 0 || !sTempPath.empty())
            {
                std::error_code ec;
                std::filesystem::remove(std::filesystem::u8path(sTempPath), ec);
            }
            m_nSpilled = 0;
        }

        CBPtreeDBContext::CBPtreeDBContext(bptreedb::IDatabasePtr ptrDatabase) :
                m_ptrDatabase(ptrDatabase),
                m_nDefaultStringLength(CBPtreeDBUtils::DefaultStringLength)
        {

        }

        CBPtreeDBContext::~CBPtreeDBContext()
        {
            try
            {
                if(m_ptrDatabase.get() && m_ptrDatabase->IsOpen())
                    m_ptrDatabase->Close();
            }
            catch (...)
            {
            }
        }

        bptreedb::ITablePtr CBPtreeDBContext::GetDBTable(const std::string& sTable) const
        {
            return m_ptrDatabase->GetSchema()->GetTable(sTable);
        }

        bptreedb::ITablePtr CBPtreeDBContext::GetDBTableThrow(const std::string& sTable) const
        {
            bptreedb::ITablePtr ptrTable = GetDBTable(sTable);
            if(!ptrTable.get())
                throw CommonLib::CExcBase("BPtreeDB: table {0} not found", sTable);

            return ptrTable;
        }

        bool CBPtreeDBContext::IsBulkLoad() const
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_bBulkLoad;
        }

        bool CBPtreeDBContext::IsInsertOnRight() const
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_bInsertOnRight || m_bBulkLoad;
        }

        void CBPtreeDBContext::SetInsertOnRight(bool bOnRight)
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_bInsertOnRight = bOnRight;
        }

        void CBPtreeDBContext::SetBulkLoad(bool bBulk)
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_bBulkLoad = bBulk;
        }

        SBPtreeDBSpatialIndexParams CBPtreeDBContext::GetDefaultSpatialIndex() const
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_defaultSpatialIndex;
        }

        void CBPtreeDBContext::SetDefaultSpatialIndex(const SBPtreeDBSpatialIndexParams& params)
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_defaultSpatialIndex = params;
        }

        uint32_t CBPtreeDBContext::GetDefaultStringLength() const
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_nDefaultStringLength;
        }

        void CBPtreeDBContext::SetDefaultStringLength(uint32_t nLength)
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_nDefaultStringLength = nLength;
        }

        void CBPtreeDBContext::RegisterWriteTransaction(bptreedb::ITransactionPtr ptrTran)
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_ptrWriteTran = ptrTran;
            m_writeThread = std::this_thread::get_id();
        }

        void CBPtreeDBContext::UnregisterWriteTransaction(const bptreedb::ITransaction* pTran)
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            bptreedb::ITransactionPtr ptrTran = m_ptrWriteTran.lock();
            if(!ptrTran.get() || ptrTran.get() == pTran)
            {
                m_ptrWriteTran.reset();
                m_writeThread = std::thread::id();
            }
        }

        bptreedb::ITransactionPtr CBPtreeDBContext::GetThreadWriteTransaction() const
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if(m_writeThread != std::this_thread::get_id())
                return bptreedb::ITransactionPtr();

            bptreedb::ITransactionPtr ptrTran = m_ptrWriteTran.lock();
            if(ptrTran.get() && !ptrTran->IsActive())
                return bptreedb::ITransactionPtr();

            return ptrTran;
        }

        bptreedb::ITransactionPtr CBPtreeDBContext::GetWriteTransaction() const
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            bptreedb::ITransactionPtr ptrTran = m_ptrWriteTran.lock();
            if(ptrTran.get() && !ptrTran->IsActive())
                return bptreedb::ITransactionPtr();

            return ptrTran;
        }

        bool CBPtreeDBContext::HasWriteTransaction() const
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            bptreedb::ITransactionPtr ptrTran = m_ptrWriteTran.lock();
            return ptrTran.get() && ptrTran->IsActive();
        }

        void CBPtreeDBContext::RunInWriteTransaction(bool bDDL, const std::function<void (bptreedb::ITransactionPtr)>& fn)
        {
            bptreedb::ITransactionPtr ptrTran = GetWriteTransaction();
            if(ptrTran.get())
            {
                if(bDDL && ptrTran->GetTransactionDataType() != bptreedb::eTT_DDL)
                    throw CommonLib::CExcBase("BPtreeDB: the schema can't be changed in the open modify transaction, "
                                              "commit it first or start a ttDDL transaction (or use the bulk load mode)");
                fn(ptrTran);
                return;
            }

            ptrTran = m_ptrDatabase->StartTransaction(bDDL ? bptreedb::eTT_DDL : bptreedb::eTT_MODIFY, bptreedb::eTT2PL);
            try
            {
                fn(ptrTran);
                ptrTran->Commit();
            }
            catch (std::exception&)
            {
                try
                {
                    if(ptrTran->IsActive())
                        ptrTran->Rollback();
                }
                catch (...)
                {
                }
                throw;
            }
        }

        bptreedb::ITransactionPtr CBPtreeDBContext::GetReadTransaction(bool& bOwn)
        {
            bptreedb::ITransactionPtr ptrTran = GetThreadWriteTransaction();
            if(ptrTran.get())
            {
                bOwn = false;
                return ptrTran;
            }

            bOwn = true;
            return m_ptrDatabase->StartTransaction(bptreedb::eTT_SELECT, bptreedb::eTTDirectRead);
        }

        SBPtreeDBPendingIndexesPtr CBPtreeDBContext::GetPendingIndexes(const std::string& sTable) const
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_pending.find(sTable);
            return it == m_pending.end() ? SBPtreeDBPendingIndexesPtr() : it->second;
        }

        SBPtreeDBPendingIndexesPtr CBPtreeDBContext::AddPendingIndexes(const std::string& sTable)
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            SBPtreeDBPendingIndexesPtr& ptrPending = m_pending[sTable];
            if(!ptrPending.get())
            {
                std::string sTempPath = (m_sDatabasePath.empty() ? (std::filesystem::temp_directory_path() / "bptreedb").u8string() : m_sDatabasePath) +
                                        "-sidx" + std::to_string(++m_nTempFiles) + ".tmp";
                ptrPending = std::make_shared<SBPtreeDBPendingIndexes>(sTempPath);
            }

            return ptrPending;
        }

        void CBPtreeDBContext::ClearPendingIndexes()
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_pending.clear();
        }

        void CBPtreeDBContext::InvalidatePendingBoxes()
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            for(auto it = m_pending.begin(); it != m_pending.end(); ++it)
            {
                it->second->bCompleteBoxes = false;
                it->second->DropBoxes();
            }
        }

        bool CBPtreeDBContext::HasPendingIndexes() const
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            return !m_pending.empty();
        }

        void CBPtreeDBContext::BuildPendingIndexes(const std::string& sTable, bptreedb::ITransactionPtr ptrTran)
        {
            SBPtreeDBPendingIndexesPtr ptrPending;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                auto it = m_pending.find(sTable);
                if(it == m_pending.end())
                    return;

                ptrPending = it->second;
                m_pending.erase(it);
            }

            BuildPendingIndexesImpl(sTable, ptrPending, ptrTran);
        }

        void CBPtreeDBContext::BuildAllPendingIndexes(bptreedb::ITransactionPtr ptrTran)
        {
            std::map<std::string, SBPtreeDBPendingIndexesPtr> pending;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                pending.swap(m_pending);
            }

            for(auto it = pending.begin(); it != pending.end(); ++it)
                BuildPendingIndexesImpl(it->first, it->second, ptrTran);
        }

        void CBPtreeDBContext::BuildPendingIndexesImpl(const std::string& sTable, SBPtreeDBPendingIndexesPtr ptrPending, bptreedb::ITransactionPtr ptrTran)
        {
            try
            {
                bptreedb::ITablePtr ptrTable = GetDBTable(sTable);
                if(!ptrTable.get())
                    return; // dropped

                if(ptrTran->GetTransactionDataType() != bptreedb::eTT_DDL)
                    throw CommonLib::CExcBase("the indexes are built in a DDL transaction");

                for(const std::string& sField : ptrPending->vecUniqueFields)
                {
                    if(ptrTable->GetIndex(sField).get() == nullptr)
                        ptrTable->CreateIndex(sField, bptreedb::SIndexProp(bptreedb::itUnique), ptrTran);
                }

                if(!ptrPending->sSpatialField.empty() && ptrTable->GetIndex(ptrPending->sSpatialField).get() == nullptr)
                {
                    bptreedb::SIndexProp prop = CBPtreeDBUtils::MakeSpatialIndexProp(ptrPending->spatialParams, ptrPending->extent);
                    // the boxes collected by the insert cursors: the geometry isn't read back
                    std::vector<std::pair<CommonLib::bbox, int64_t> > entries;
                    if(ptrPending->TakeBoxes(entries))
                        prop.m_pSpatialEntries = &entries;
                    ptrTable->CreateIndex(ptrPending->sSpatialField, prop, ptrTran);
                }
                else
                    ptrPending->DropBoxes();
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("BPtreeDB: failed to build the indexes of the table {0}", sTable, exc);
                throw;
            }
        }

    }
}
