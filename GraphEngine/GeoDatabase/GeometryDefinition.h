#pragma once
#include "GeoDatabase.h"

namespace GraphEngine {
    namespace GeoDatabase {

        class CGeometryDefinition : public IGeometryDefinition
        {

        public:
            CGeometryDefinition(CommonLib::eShapeType = CommonLib::shape_type_null, bool hasZ = false, bool hasM = false);
            virtual ~CGeometryDefinition();

        public:
            // GeometryDefinition
            virtual CommonLib::eShapeType      GetGeometryType() const;
            virtual void SetGeometryType(CommonLib::eShapeType type);
            virtual bool GetHasZ() const;
            virtual void SetHasZ(bool flag);
            virtual bool GetHasM() const;
            virtual void SetHasM(bool flag);
            virtual Geometry::ISpatialReferencePtr GetSpatialReference() const;
            virtual void SetSpatialReference(Geometry::ISpatialReferencePtr pSpRef);
            virtual CommonLib::bbox	GetBaseExtent() const;
            virtual void SetBaseExtent(const CommonLib::bbox& box);
            virtual IGeometryDefinitionPtr Clone() const;
        private:
            CommonLib::eShapeType m_geometryType;
            bool m_bIsHasZ;
            bool m_bHasM;
            Geometry::ISpatialReferencePtr m_pSpatialRefPtr;
            CommonLib::bbox m_baseExtent;
        };

    }
}