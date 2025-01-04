#include "GeometryDefinition.h"

namespace GraphEngine {
    namespace GeoDatabase {

        CGeometryDefinition::CGeometryDefinition(CommonLib::eShapeType type, bool hasZ, bool hasM)
                : m_geometryType(type)
                , m_bIsHasZ(hasZ)
                , m_bHasM(hasM)
        {
            m_baseExtent.type = CommonLib::bbox_type_null;
        }

        CGeometryDefinition::~CGeometryDefinition()
        {}


        // IGeometryDef
        CommonLib::eShapeType CGeometryDefinition::GetGeometryType() const
        {
            return m_geometryType;
        }

        void CGeometryDefinition::SetGeometryType(CommonLib::eShapeType type)
        {
            m_geometryType = type;
        }

        bool CGeometryDefinition::GetHasZ() const
        {
            return m_bIsHasZ;
        }

        void CGeometryDefinition::SetHasZ(bool flag)
        {
            m_bIsHasZ = flag;
        }

        bool CGeometryDefinition::GetHasM() const
        {
            return m_bHasM;
        }

        void CGeometryDefinition::SetHasM(bool flag)
        {
            m_bHasM = flag;
        }

        Geometry::ISpatialReferencePtr CGeometryDefinition::GetSpatialReference() const
        {
            return m_pSpatialRefPtr;
        }

        void CGeometryDefinition::SetSpatialReference(Geometry::ISpatialReferencePtr pSpRef)
        {
            m_pSpatialRefPtr = pSpRef;
        }

        CommonLib::bbox CGeometryDefinition::GetBaseExtent() const
        {
            return m_baseExtent;
        }

        void CGeometryDefinition::SetBaseExtent(const CommonLib::bbox& box)
        {
            m_baseExtent = box;
        }

        IGeometryDefinitionPtr CGeometryDefinition::Clone() const
        {
            IGeometryDefinitionPtr ptrGeometryDefinition = std::make_shared<CGeometryDefinition>();
            ptrGeometryDefinition->SetBaseExtent(GetBaseExtent());
            ptrGeometryDefinition->SetHasM(GetHasM());
            ptrGeometryDefinition->SetHasZ(GetHasZ());
            ptrGeometryDefinition->SetGeometryType(GetGeometryType());
            if(GetSpatialReference().get() != nullptr)
                ptrGeometryDefinition->SetSpatialReference(GetSpatialReference()->Clone());

            return ptrGeometryDefinition;
        }

    }
}