#include "OSMAnalyzer.h"
#include <algorithm>

namespace GraphEngine {
    namespace Convertors {

        COSMAnalyzer::COSMAnalyzer(const COSMSchema& schema, COSMMapPtr ptrMap) :
                m_schema(schema), m_ptrMap(ptrMap), m_vecKeyCounts(schema.Layers().size())
        {
            m_bounds.xMin = m_bounds.yMin = (std::numeric_limits<double>::max)();
            m_bounds.xMax = m_bounds.yMax = -(std::numeric_limits<double>::max)();
            m_ptrRoutes = m_ptrMap->FindTableByType(OSMTableRoutes);
            m_ptrRestrictions = m_ptrMap->FindTableByType(OSMTableRestrictions);
            m_ptrTags = m_ptrMap->FindTableByType(OSMTableTags);
            if(m_ptrTags.get())
                m_ptrTagKey = m_ptrTags->Keys().Find(COSMSchema::TableKey(OSMTableTags));
        }

        void COSMAnalyzer::AddNode(const readosm_node& node)
        {
            ++m_nNodes;
            CheckOrder(0, node.id);

            if(node.longitude != READOSM_UNDEFINED && node.latitude != READOSM_UNDEFINED)
            {
                m_bounds.xMin = (std::min)(m_bounds.xMin, node.longitude);
                m_bounds.xMax = (std::max)(m_bounds.xMax, node.longitude);
                m_bounds.yMin = (std::min)(m_bounds.yMin, node.latitude);
                m_bounds.yMax = (std::max)(m_bounds.yMax, node.latitude);
            }

            if(node.tag_count > 0 && Add(m_schema.MatchNode(COSMTags(node.tags, node.tag_count)), node.tags, node.tag_count))
                AddTags(node.tags, node.tag_count);
        }

        void COSMAnalyzer::AddWay(const readosm_way& way)
        {
            ++m_nWays;
            CheckOrder(1, way.id);

            if(way.tag_count > 0 && way.node_ref_count >= 2)
            {
                bool bClosed = way.node_ref_count >= 4 && way.node_refs[0] == way.node_refs[way.node_ref_count - 1];
                SOSMMatch line, polygon;
                m_schema.MatchWay(COSMTags(way.tags, way.tag_count), bClosed, line, polygon);
                bool bLine = Add(line, way.tags, way.tag_count);
                bool bPolygon = Add(polygon, way.tags, way.tag_count);
                if(bLine || bPolygon)
                    AddTags(way.tags, way.tag_count);
            }
        }

        void COSMAnalyzer::AddRelation(const readosm_relation& relation)
        {
            ++m_nRelations;
            CheckOrder(2, relation.id);

            if(relation.tag_count > 0)
            {
                COSMTags tags(relation.tags, relation.tag_count);
                bool bMatched = Add(m_schema.MatchRelation(tags), relation.tags, relation.tag_count);
                const char* pszRoute = COSMSchema::MatchRoute(tags);
                if(pszRoute && m_ptrRoutes.get())
                {
                    m_ptrRoutes->AddFeature(COSMSchema::TableKey(OSMTableRoutes), pszRoute);
                    bMatched = true;
                }
                const char* pszRestriction = COSMSchema::MatchRestriction(tags);
                if(pszRestriction && m_ptrRestrictions.get())
                {
                    m_ptrRestrictions->AddFeature(COSMSchema::TableKey(OSMTableRestrictions), pszRestriction);
                    bMatched = true;
                }
                if(bMatched)
                    AddTags(relation.tags, relation.tag_count);
            }
        }

        void COSMAnalyzer::Finish()
        {
            m_ptrMap->SetCounts(m_nNodes, m_nWays, m_nRelations);
            m_ptrMap->SetSorted(m_bSorted);
            m_ptrMap->SetScanned(true);
            if(m_bounds.xMin <= m_bounds.xMax)
            {
                m_bounds.type = CommonLib::bbox_type_normal;
                m_ptrMap->SetBounds(m_bounds);
            }
            m_ptrMap->SortValues();

            // the columns: the OSM keys of the layers, the tables have their own columns
            for(int i = 0; i < m_ptrMap->GetLayerCount() && i < (int)m_vecKeyCounts.size(); ++i)
                m_schema.SetAnalyzedColumns(i, *m_ptrMap->GetLayerImpl(i), m_vecKeyCounts[i]);
            for(int i = 0; i < m_ptrMap->GetTableCount(); ++i)
            {
                COSMTablePtr ptrTable = m_ptrMap->FindTableImpl(m_ptrMap->GetTable(i)->GetName());
                for(SOSMColumn& column : ptrTable->Columns())
                    column.nCount = ptrTable->GetFeatureCount();
            }
        }

        // the feature of a layer: its value, its OSM keys (the columns)
        bool COSMAnalyzer::Add(const SOSMMatch& match, const readosm_tag* pTags, int nCount)
        {
            if(!match.IsValid())
                return false;
            m_ptrMap->GetLayerImpl(match.nLayer)->AddFeature(match.sKey, match.sValue);
            std::unordered_map<std::string, uint64_t>& keyCounts = m_vecKeyCounts[match.nLayer];
            for(int i = 0; i < nCount; ++i)
            {
                if(pTags[i].key && *pTags[i].key)
                {
                    m_sKey.assign(pTags[i].key);
                    ++keyCounts[m_sKey];
                }
            }
            return true;
        }

        // tags table: the number of records (tags), its key "key" counts the objects having an OSM key
        void COSMAnalyzer::AddTags(const readosm_tag* pTags, int nCount)
        {
            if(!m_ptrTagKey.get())
                return;
            m_ptrTags->AddCount(nCount);
            for(int i = 0; i < nCount; ++i)
                m_ptrTagKey->AddFeature(pTags[i].key ? pTags[i].key : "");
        }

        // nodes, ways, relations, every type sorted by id
        void COSMAnalyzer::CheckOrder(int nType, int64_t nId)
        {
            if(!m_bSorted)
                return;
            if(nType < m_nLastType || (nType == m_nLastType && nId <= m_nLastId))
                m_bSorted = false;
            m_nLastType = nType;
            m_nLastId = nId;
        }
    }
}
