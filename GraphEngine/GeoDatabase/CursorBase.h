#pragma once
#include "GeoDatabase.h"
#include "FieldSet.h"
#include "../GisGeometry/Geometry.h"
#include "../GisGeometry/Envelope.h"

namespace GraphEngine
{
    namespace GeoDatabase {

        template<class I>
        class ICursorBase : public I
        {
        public:

            ICursorBase(IQueryFilterPtr ptrFilter, Geometry::ISpatialReferencePtr ptrSpatRefSource) :
             m_spatialRel(srlUndefined)
            , m_bNeedTransform(false)
            , m_ptrFilter(ptrFilter)
            {
                // Spatial queries
                if(ptrSpatRefSource.get() != nullptr)
                {
                    Geometry::ISpatialReferencePtr ptrSpatRefOutput = m_ptrFilter->GetOutputSpatialReference();
                    ISpatialFilter *pSpatialFilter = dynamic_cast<ISpatialFilter *>(m_ptrFilter.get());
                    if(pSpatialFilter)
                    {
                        m_spatialRel = pSpatialFilter->GetSpatialRel();
                        if(m_spatialRel != srlUndefined)
                        {
                            if(pSpatialFilter->GetBB().type == CommonLib::bbox_type_normal)
                            {
                                m_ptrExtentOutput = std::make_shared<Geometry::CEnvelope>(pSpatialFilter->GetBB(), ptrSpatRefOutput);
                                m_ptrExtentSource = std::make_shared<Geometry::CEnvelope>(pSpatialFilter->GetBB(), ptrSpatRefOutput);

                                m_ptrExtentOutput->Project(ptrSpatRefOutput);
                                m_ptrExtentSource->Project(ptrSpatRefSource);
                            }
                            else
                            {
                                CommonLib::IGeoShapePtr pShape = pSpatialFilter->GetShape();
                                m_ptrExtentOutput = std::make_shared<Geometry::CEnvelope>(pShape->GetBB(), ptrSpatRefOutput);
                                m_ptrExtentSource = std::make_shared<Geometry::CEnvelope>(pShape->GetBB(), ptrSpatRefOutput);

                                m_ptrExtentOutput->Project(ptrSpatRefOutput);
                                m_ptrExtentSource->Project(ptrSpatRefSource);
                            }

                        }
                        else
                        {
                            m_ptrExtentOutput = std::make_shared<Geometry::CEnvelope>(CommonLib::bbox(), ptrSpatRefOutput);
                            m_ptrExtentSource = std::make_shared<Geometry::CEnvelope>(CommonLib::bbox(), ptrSpatRefSource);
                        }

                        m_bNeedTransform = ptrSpatRefOutput.get() != nullptr
                                           && ptrSpatRefSource.get() != nullptr
                                           && !ptrSpatRefOutput->IsEqual(ptrSpatRefSource);

                    }
                }
            }



            virtual ~ICursorBase(){}


            void UpdateFields()
            {

                if(!m_ptrFieldsSet.get())
                {
                    m_ptrFieldsSet = std::make_shared<CFieldSet>();
                    for (int i = 0, sz = m_ptrSourceFields->GetFieldCount(); i < sz; ++i)
                    {
                        IFieldPtr pField = m_ptrSourceFields->GetField(i);
                        m_ptrFieldsSet->Add(pField->GetName());
                    }
                }
            }


        protected:

            IQueryFilterPtr m_ptrFilter;
            IFieldsPtr      m_ptrSourceFields;
            IFieldSetPtr      m_ptrFieldsSet;

            //spatial part
            Geometry::IEnvelopePtr  m_ptrExtentOutput;
            Geometry::IEnvelopePtr  m_ptrExtentSource;
            bool m_bNeedTransform;
            eSpatialRel				   m_spatialRel;

        };
    }
}