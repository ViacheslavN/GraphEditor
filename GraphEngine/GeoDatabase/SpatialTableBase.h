#pragma once

#include "TableBase.h"

namespace GraphEngine
{
    namespace GeoDatabase {

        template<class I>
        class ISpatialTableBase : public ITableBase<I>
        {
        public:
            typedef ITableBase<I> TBase;
            ISpatialTableBase(std::string sDatasetName,  std::string sDatasetViewName) :
            TBase(dtSpatialTable, sDatasetName, sDatasetViewName),
                m_ShapeType(CommonLib::shape_type_null)
            {

            }

            virtual ~ISpatialTableBase()
            {

            }

            virtual	void SetGeometryType(CommonLib::eShapeType shapeType)
            {
                m_ShapeType = shapeType;
            }

            virtual CommonLib::eShapeType GetGeometryType() const
            {
                return m_ShapeType;
            }


            virtual Geometry::IEnvelopePtr	 GetExtent() const
            {
                return m_ptrExtent;
            }

            virtual Geometry::ISpatialReferencePtr GetSpatialReference() const
            {
                return m_ptrSpatialReference;
            }

            virtual void SetExtent(Geometry::IEnvelopePtr ptrEnvelope)
            {
                m_ptrExtent = ptrEnvelope;
            }

            virtual void SetSpatialReference(Geometry::ISpatialReferencePtr ptrSpatRef)
            {
                m_ptrSpatialReference = ptrSpatRef;
            }

        protected:
            mutable Geometry::IEnvelopePtr  m_ptrExtent;
            mutable std::string m_sShapeFieldName;
            mutable CommonLib::eShapeType m_ShapeType;
            mutable Geometry::ISpatialReferencePtr m_ptrSpatialReference;

        };


    }
    }