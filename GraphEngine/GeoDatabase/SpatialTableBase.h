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
            ISpatialTableBase() : TBase(), m_ShapeType(CommonLib::shape_type_null), m_bAnnotation(false)
            {
                this->m_DatasetType = dtSpatialTable;
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

            virtual const std::string&  GetShapeFieldName() const
            {
                return m_sShapeFieldName;
            }

            virtual void SetShapeFieldName(const std::string&& sName)
            {
                m_sShapeFieldName = sName;
            }

            virtual bool GetIsAnnoClass() const
            {
                return m_bAnnotation;
            }

            virtual const std::string&	GetAnnoFieldName() const
            {
                return m_sAnnotationName;
            }

            virtual void SetIsAnnoClass(bool bAnno)
            {
                m_bAnnotation = bAnno;
            }

            virtual void SetAnnoFieldName(const std::string&& sAnnoName)
            {
                m_sAnnotationName = sAnnoName;
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
            bool m_bAnnotation;
            std::string m_sAnnotationName;
            mutable Geometry::IEnvelopePtr  m_ptrExtent;
            mutable std::string m_sShapeFieldName;
            mutable CommonLib::eShapeType m_ShapeType;
            mutable Geometry::ISpatialReferencePtr m_ptrSpatialReference;

        };


    }
    }