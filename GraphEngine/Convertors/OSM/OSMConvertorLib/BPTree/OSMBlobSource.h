#pragma once

// TBlobSource: the source of TBPMap::BulkLoad of a blob tree (the loading thread): the blobs of the resolved jobs in the
// order (TResolver::Next: TResult with ids / offsets / sizes / data, the ids sorted in a job), the entries out of the id
// order aside (inserted after BulkLoad). The ways (CWayResolver, SWayShapes), the relations (CRelationResolver,
// SRelationRecords).

#include "OSMWayTreeTypes.h"
#include "../OSMConvertorLib.h"

#include <algorithm>
#include <functional>
#include <limits>
#include <vector>

namespace GraphEngine {
    namespace Convertors {
        namespace bptree {

            template <class TResolver, class TResult>
            class TBlobSource
            {
            public:
                struct SAside
                {
                    int64_t id;
                    std::vector<byte_t> blob;
                };

                // fnTaken - a job is taken (its counts), fnProgress - before a job is taken
                TBlobSource(TResolver& resolver, TWayShapeTree& tree, Display::ITrackCancelPtr ptrCancel,
                            std::function<void(const TResult&)> fnTaken, std::function<void()> fnProgress) :
                        m_resolver(resolver), m_tree(tree), m_ptrCancel(ptrCancel), m_fnTaken(fnTaken), m_fnProgress(fnProgress)
                {}

                bool Next(int64_t& nId, bptreedb::BlobValue& value)
                {
                    for (;;)
                    {
                        if (m_nPos == m_result.ids.size())
                        {
                            if (m_ptrCancel.get() && !m_ptrCancel->Continue())
                            {
                                m_bCanceled = true;
                                return false;
                            }
                            m_fnProgress();
                            if (!m_resolver.Next(m_result))
                                return false;
                            m_nPos = 0;
                            m_fnTaken(m_result);
                            continue;
                        }
                        size_t i = m_nPos++;
                        const byte_t* pBlob = m_result.data.data() + m_result.offsets[i];
                        uint32_t nSize = m_result.sizes[i];
                        m_nBytes += nSize;
                        if (m_result.ids[i] <= m_nLastId)
                        {
                            m_aside.push_back(SAside{ m_result.ids[i], std::vector<byte_t>(pBlob, pBlob + nSize) });
                            continue;
                        }
                        m_nLastId = m_result.ids[i];
                        nId = m_result.ids[i];
                        AddId(nId);
                        value = m_tree.WriteBlob(pBlob, nSize);
                        ++m_nWritten;
                        return true;
                    }
                }

                // after BulkLoad: the entries out of the order inserted, the duplicates skipped (their count)
                uint64_t InsertAside()
                {
                    uint64_t nDuplicates = 0;
                    std::stable_sort(m_aside.begin(), m_aside.end(), [](const SAside& a, const SAside& b) { return a.id < b.id; });
                    for (const SAside& entry : m_aside)
                    {
                        if (!m_tree.find(entry.id).IsNull())
                        {
                            ++nDuplicates;
                            m_tree.CheckCache();
                            continue;
                        }
                        m_tree.insert(entry.id, entry.blob.data(), (int64_t)entry.blob.size());
                        m_tree.CheckCache();
                        AddId(entry.id);
                        ++m_nWritten;
                    }
                    return nDuplicates;
                }

                bool     IsCanceled() const { return m_bCanceled; }
                size_t   AsideCount() const { return m_aside.size(); }
                uint64_t Written() const { return m_nWritten; }     // the entries of the tree
                uint64_t Bytes() const { return m_nBytes; }         // the blobs taken
                bool     HasIds() const { return m_nIds != 0; }
                int64_t  MinId() const { return m_nMinId; }
                int64_t  MaxId() const { return m_nMaxId; }

            private:
                void AddId(int64_t nId)
                {
                    if (m_nIds++ == 0)
                        m_nMinId = m_nMaxId = nId;
                    m_nMinId = (std::min)(m_nMinId, nId);
                    m_nMaxId = (std::max)(m_nMaxId, nId);
                }

            private:
                TResolver& m_resolver;
                TWayShapeTree& m_tree;
                Display::ITrackCancelPtr m_ptrCancel;
                std::function<void(const TResult&)> m_fnTaken;
                std::function<void()> m_fnProgress;
                TResult  m_result;
                size_t   m_nPos = 0;
                int64_t  m_nLastId = (std::numeric_limits<int64_t>::min)();
                uint64_t m_nWritten = 0;
                uint64_t m_nBytes = 0;
                uint64_t m_nIds = 0;
                int64_t  m_nMinId = 0;
                int64_t  m_nMaxId = 0;
                bool     m_bCanceled = false;
                std::vector<SAside> m_aside;
            };
        }
    }
}
