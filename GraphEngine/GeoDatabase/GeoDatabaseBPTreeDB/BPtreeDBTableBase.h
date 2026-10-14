#pragma once

#include "../TableBase.h"
#include "BPtreeDBContext.h"
#include "BPtreeDBUtils.h"
#include "BPtreeDBSelectCursor.h"


namespace GraphEngine {
    namespace GeoDatabase {

        template<class I>
        class CBPtreeDBTableBase : public ITableBase<I>
        {
        public:

            typedef ITableBase<I> TBase;

            CBPtreeDBTableBase( CommonLib::CGuid workspaceId, eDatasetType datasetType, const std::string& tableName,  const std::string& viewName,
                                CBPtreeDBContextPtr ptrContext) :
                    TBase(workspaceId, datasetType, tableName, viewName),
                    m_ptrContext(ptrContext)
            {
                try
                {
                    this->m_pFields = CBPtreeDBUtils::ReadFields(m_ptrContext->GetDBTableThrow(tableName));
                }
                catch (std::exception& exc)
                {
                    CommonLib::CExcBase::RegenExc("Failed to read BPtreeDB table {0}", tableName, exc);
                    throw;
                }
            }

            // SQL of bptreedb (DB/SQLEngine): SELECT with joins, GROUP BY, ORDER BY...;
            // spatial conditions: ST_Intersects / ST_Within / ST_Contains(geom, ST_MakeEnvelope(xmin, ymin, xmax, ymax))
            virtual ISelectCursorPtr Select(const std::string& sqlSelectQuery)
            {
                try
                {
                    return std::make_shared<CBPtreeDBSelectCursor>(sqlSelectQuery, m_ptrContext);
                }
                catch (std::exception& exc)
                {
                    CommonLib::CExcBase::RegenExc("CBPtreeDBTable: Failed to Select", exc);
                    throw;
                }
            }

            virtual ISelectCursorPtr Search(IQueryFilterPtr ptrFilter)
            {
                try
                {
                    return std::make_shared<CBPtreeDBSelectCursor>(this->GetDatasetName(), this->GetShapeFieldName(), this->GetFields(), ptrFilter,
                                                                   this->GetSpatialReference(), m_ptrContext);
                }
                catch (std::exception& exc)
                {
                    CommonLib::CExcBase::RegenExc("CBPtreeDBTable: Failed to Search", exc);
                    throw;
                }
            }

            CBPtreeDBContextPtr GetContext() const
            {
                return m_ptrContext;
            }

        protected:
            CBPtreeDBContextPtr m_ptrContext;
        };

    }
}
