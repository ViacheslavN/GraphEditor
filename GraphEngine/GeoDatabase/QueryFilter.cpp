#include "QueryFilter.h"
#include "FieldSet.h"

namespace GraphEngine {
    namespace GeoDatabase {
        CQueryFilter::CQueryFilter(const std::string& sWhere, IFieldSetPtr fieldSet)
                : m_sWhereClause(sWhere)
                , m_pFieldSet(fieldSet)
                , m_SpatialRel(srlUndefined)
                , m_dPecision(0.0)

        {
            m_bbox.type = CommonLib::bbox_type_invalid;
        }

        CQueryFilter::~CQueryFilter()
        {}


// IQueryFilter
        IFieldSetPtr CQueryFilter::GetFieldSet() const
        {
            if(!m_pFieldSet)
                m_pFieldSet = std::make_shared<CFieldSet>();

            return m_pFieldSet;
        }

        void CQueryFilter::SetFieldSet(IFieldSetPtr fieldSet)
        {
            m_pFieldSet = fieldSet;
        }

        Geometry::ISpatialReferencePtr CQueryFilter::GetOutputSpatialReference() const
        {
            return m_pOutputSpatialRef;
        }

        void CQueryFilter::SetOutputSpatialReference(Geometry::ISpatialReferencePtr ptrSpatRef)
        {
            m_pOutputSpatialRef = ptrSpatRef;
        }

        const std::string& CQueryFilter::GetWhereClause() const
        {
            return m_sWhereClause;
        }

        void CQueryFilter::SetWhereClause(const std::string& sWhere)
        {
            m_sWhereClause = sWhere;
        }



// ISpatialFilter
        const std::string& CQueryFilter::GetShapeField() const
        {
            return m_sShapeField;
        }

        void CQueryFilter::SetShapeField(const std::string& name)
        {
            m_sShapeField = name;
        }

        CommonLib::IGeoShapePtr CQueryFilter::GetShape() const
        {
            return m_pShape;
        }

        void CQueryFilter::SetShape(CommonLib::IGeoShapePtr ptrShape)
        {
            m_pShape = ptrShape;
        }

        eSpatialRel CQueryFilter::GetSpatialRel() const
        {
            return m_SpatialRel;
        }

        void CQueryFilter::SetSpatialRel(eSpatialRel rel)
        {
            m_SpatialRel = rel;
        }

        double CQueryFilter::GetPrecision() const
        {
            return m_dPecision;
        }

        void CQueryFilter::SetPrecision(double precision)
        {
            m_dPecision = precision;
        }

        CommonLib::bbox	CQueryFilter::GetBB() const
        {
            return m_bbox;
        }
        void   CQueryFilter::SetBB(const  CommonLib::bbox& bbox )
        {
            m_bbox = bbox;
        }

        const std::vector<IJoinPtr>   CQueryFilter::GetJoins() const
        {
            return m_vecJoins;
        }

        void CQueryFilter::SetJoins(const std::vector<IJoinPtr> & vecJoins)
        {
            m_vecJoins = vecJoins;
        }

        const std::string&  CQueryFilter::GetTablePrefix() const
        {
            return  m_sTablePrefix;
        }

        void  CQueryFilter::SetTablePrefix(const std::string& tablePrefix)
        {
            m_sTablePrefix = tablePrefix;
        }
    }
}