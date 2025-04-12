#pragma once

#include "DataSetBase.h"
#include "Fields.h"
#include "FieldSet.h"

namespace GraphEngine {
    namespace GeoDatabase {
        template< class I>
        class ITableBase : public IDataSetBase<I>
        {
        public:
            typedef IDataSetBase<I> TBase;

            ITableBase(eDatasetType datasetType,  std::string sDatasetName,  std::string sDatasetViewName) : TBase(
                     datasetType,   sDatasetName,sDatasetViewName)
            {
                this->m_pFields = std::make_shared<CFields>();
            }

            virtual ~ITableBase()
            {

            }

            virtual void AddField(IFieldPtr pField)
            {
                m_pFields->AddField(pField);
            }

            virtual void  DeleteField(const std::string& fieldName)
            {
                int nIndex = m_pFields->FindField(fieldName);
                if(nIndex != -1)
                    m_pFields->RemoveField(nIndex);
            }

            virtual IFieldsPtr	GetFields() const
            {
                return m_pFields;
            }

            virtual IFieldSetPtr  GetFieldsSet() const
            {
                IFieldSetPtr ptrFieldSet = std::make_shared<CFieldSet>();

                for(int32_t i = 0; i < m_pFields->GetFieldCount(); ++i)
                    ptrFieldSet->Add(m_pFields->GetField(i)->GetName());

                return  ptrFieldSet;
            }


            virtual void SetFields(IFieldsPtr ptrFields)
            {
                m_pFields = ptrFields;
            }


            virtual void SetOIDFieldName(const std::string& sOIDFieldName)
            {
                m_sOIDFieldName = sOIDFieldName;
            }


            virtual const std::string& GetOIDFieldName() const
            {
                return m_sOIDFieldName;
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
            virtual const std::string&  GetShapeFieldName() const
            {
                return  m_sShapeFieldName;
            }

            virtual void  SetShapeFieldName(const std::string& fieldName)
            {
                m_sShapeFieldName = fieldName;

            }

            virtual void   SetSpatialIndexName(const std::string& spatialIndexName)
            {
                m_spatialIndexName = spatialIndexName;
            }

            virtual const std::string& GetSpatialIndexName() const
            {
                return m_spatialIndexName;
            }

        protected:
            IFieldsPtr         m_pFields;
            std::string m_sOIDFieldName;

            //spatial part

            Geometry::IEnvelopePtr  m_ptrExtent;
            std::string m_sShapeFieldName;
            std::string m_spatialIndexName;
            CommonLib::eShapeType m_ShapeType;
            Geometry::ISpatialReferencePtr m_ptrSpatialReference;
        };
    }
}