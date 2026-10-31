#include "OSMRelationReader.h"

#include <cstring>

namespace GraphEngine {
    namespace Convertors {
        namespace bptree {

            CRelationReader::CRelationReader(const SOSMBPTreeSettings& settings, CRelationResolver& resolver, CTagWriter* pTags,
                                             Display::ITrackCancelPtr ptrCancel) :
                    m_settings(settings), m_resolver(resolver), m_pTags(pTags), m_ptrCancel(ptrCancel)
            {
            }

            void CRelationReader::Read(const std::string& sPath)
            {
                try
                {
                    if (COSMReader::Read(sPath, *this, COSMReader::ReadRelations, m_settings.nThreads))
                    {
                        if (m_batch.Count() != 0)
                            m_resolver.Submit(m_batch);
                        if (m_tags.Count() != 0)
                            m_pTags->Push(m_tags);
                    }
                }
                catch (...)
                {
                    m_error = std::current_exception();
                }
                m_resolver.InputDone();
            }

            void CRelationReader::RethrowError()
            {
                if (m_error)
                    std::rethrow_exception(m_error);
            }

            bool CRelationReader::IsArea(const readosm_relation& relation)
            {
                for (int i = 0; i < relation.tag_count; ++i)
                {
                    const readosm_tag& tag = relation.tags[i];
                    if (tag.key && tag.value && strcmp(tag.key, "type") == 0)
                        return strcmp(tag.value, "multipolygon") == 0 || strcmp(tag.value, "boundary") == 0;
                }
                return false;
            }

            bool CRelationReader::OnRelation(const readosm_relation& relation)
            {
                m_nRead.fetch_add(1, std::memory_order_relaxed);
                m_batch.Add(relation, IsArea(relation));
                if (m_pTags && relation.tag_count > 0)
                {
                    m_tags.Add(relation);
                    if (m_tags.data.size() >= m_settings.nTagBatchBytes && !m_pTags->Push(m_tags))
                        return false;
                }
                if (m_batch.memberRefs.size() >= m_settings.nRelBatchMembers)
                    return m_resolver.Submit(m_batch);    // false: the resolver stopped, the reading stops
                return true;
            }

            bool CRelationReader::OnProgress(uint64_t nBytesRead, uint64_t nBytesTotal)
            {
                m_nBytesRead = nBytesRead;
                m_nBytesTotal = nBytesTotal;
                if (m_bStop)
                    return false;
                return !m_ptrCancel.get() || m_ptrCancel->Continue();
            }

            void CRelationReader::OnReaderStats(const SOSMReaderStats& stats)
            {
                std::lock_guard<std::mutex> lock(m_statsMutex);
                m_readerStats = stats;
            }

            SOSMReaderStats CRelationReader::ReaderStats()
            {
                std::lock_guard<std::mutex> lock(m_statsMutex);
                return m_readerStats;
            }
        }
    }
}
