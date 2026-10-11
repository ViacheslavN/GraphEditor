#pragma once
#include "OSMSchema.h"
#include "readosm/readosm.h"
#include <limits>
#include <unordered_map>
#include <vector>

namespace GraphEngine {
    namespace Convertors {

        // The analysis of the objects of an OSM file (Analyze, the splitting): the bounds, the numbers of the objects,
        // the order, the layers / tables with the key / value hierarchy and the columns of the layers (the OSM keys of
        // their features). The objects come in the file order, Finish sets the results to the map.
        class COSMAnalyzer
        {
        public:
            // ptrMap - made by schema.CreateMap()
            COSMAnalyzer(const COSMSchema& schema, COSMMapPtr ptrMap);

            void AddNode(const readosm_node& node);
            void AddWay(const readosm_way& way);
            void AddRelation(const readosm_relation& relation);
            // the counts, the bounds, the order, the sorted values, the columns
            void Finish();

            uint64_t NodeCount() const { return m_nNodes; }
            uint64_t WayCount() const { return m_nWays; }
            uint64_t RelationCount() const { return m_nRelations; }
            bool     IsSorted() const { return m_bSorted; }

        private:
            bool Add(const SOSMMatch& match, const readosm_tag* pTags, int nCount);
            void AddTags(const readosm_tag* pTags, int nCount);
            void CheckOrder(int nType, int64_t nId);

        private:
            const COSMSchema& m_schema;
            COSMMapPtr        m_ptrMap;
            COSMTablePtr      m_ptrRoutes;
            COSMTablePtr      m_ptrRestrictions;
            COSMTablePtr      m_ptrTags;
            COSMTagKeyPtr     m_ptrTagKey;
            CommonLib::bbox   m_bounds;
            uint64_t          m_nNodes = 0;
            uint64_t          m_nWays = 0;
            uint64_t          m_nRelations = 0;
            bool              m_bSorted = true;
            int               m_nLastType = 0;
            int64_t           m_nLastId = (std::numeric_limits<int64_t>::min)();
            std::vector<std::unordered_map<std::string, uint64_t> > m_vecKeyCounts;   // by the layer index: OSM key -> features
            std::string       m_sKey;
        };
    }
}
