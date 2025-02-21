//
#include "Envelope.h"


namespace GraphEngine {
    namespace Geometry {

        CEnvelope::CEnvelope()
        {}

        CEnvelope::CEnvelope(const CommonLib::bbox& box, ISpatialReferencePtr spatRef)
                : m_box(box)
                , m_pSpatialRef(spatRef)
        {}

        CEnvelope::~CEnvelope()
        {}



// IEnvelope
        const CommonLib::bbox& CEnvelope::GetBoundingBox() const
        {
            return m_box;
        }

        ISpatialReferencePtr CEnvelope::GetSpatialReference() const
        {
            return m_pSpatialRef;
        }

        CommonLib::bbox& CEnvelope::GetBoundingBox()
        {
            return m_box;
        }

        void CEnvelope::SetBoundingBox(const CommonLib::bbox& box)
        {
            m_box = box;
        }

        void CEnvelope::SetSpatialReference(ISpatialReferencePtr spatRef)
        {
            m_pSpatialRef = spatRef;
        }

        void CEnvelope::Expand(IEnvelopePtr ptrEnvelope)
        {
            if(ptrEnvelope.get() != nullptr)
                return;

            CommonLib::bbox box = ptrEnvelope->GetBoundingBox();
            ISpatialReferencePtr sr = ptrEnvelope->GetSpatialReference();
            if(sr.get() && m_pSpatialRef.get() && sr.get() != m_pSpatialRef.get())
                sr->Project(m_pSpatialRef, box);

            m_box.expand(box);
        }

        bool CEnvelope::Intersect(IEnvelopePtr ptrEnvelope)
        {
            if(ptrEnvelope.get() != nullptr)
            {
                CommonLib::bbox box = ptrEnvelope->GetBoundingBox();
                ISpatialReferencePtr sr = ptrEnvelope->GetSpatialReference();
                if(sr.get() && m_pSpatialRef.get() && sr.get() != m_pSpatialRef.get())
                    sr->Project(m_pSpatialRef, box);

                // clipping
                m_box.xMin = max(m_box.xMin, box.xMin);
                m_box.xMax = min(m_box.xMax, box.xMax);
                m_box.yMin = max(m_box.yMin, box.yMin);
                m_box.yMax = min(m_box.yMax, box.yMax);
            }

            // Test for empty
            return !((m_box.xMin > m_box.xMax) || (m_box.yMin > m_box.yMax));
        }

        void CEnvelope::Project(ISpatialReferencePtr ptrSpatRef)
        {
            if(!m_pSpatialRef.get() || !ptrSpatRef.get())
                return;

            m_pSpatialRef->Project(ptrSpatRef, m_box);
            m_pSpatialRef = ptrSpatRef;
        }


        IEnvelopePtr	CEnvelope::Clone() const
        {
            IEnvelopePtr pEnvelope(new CEnvelope());
            pEnvelope->SetBoundingBox(m_box);
            pEnvelope->SetSpatialReference(m_pSpatialRef.get() ? m_pSpatialRef->Clone() : NULL);
            return pEnvelope;
        }

     /*   CommonLib::shape_compress_params CEnvelope::GetCompressParams() const
        {
            CommonLib::shape_compress_params params;

            if(m_box.type == CommonLib::bbox_type_invalid || m_box.type == CommonLib::bbox_type_null ||
               !m_pSpatialRef.get())
                return params;

            if(m_box.xMin < 0)
                params.m_dOffsetX = fabs(m_box.xMin);
            else
                params.m_dOffsetX = -1 *m_box.xMin;

            if(m_box.yMin < 0)
                params.m_dOffsetY = fabs(m_box.yMin);
            else
                params.m_dOffsetY = -1 *m_box.yMin;


            double dMaxX = fabs(m_box.xMax + params.m_dOffsetX);
            double dMaxY = fabs(m_box.yMax + params.m_dOffsetY);
            double dMaxCoord = max(dMaxX, dMaxY);


            GisEngine::GisCommon::Units units = m_pSpatialRef->GetUnits();
            switch(units)
            {
                case GisEngine::GisCommon::UnitsDecimalDegrees:
                    params.m_nScaleX = 7; //0.0000001
                    params.m_nScaleY = 7;
                    break;
                case GisEngine::GisCommon::UnitsKilometers:
                case GisEngine::GisCommon::UnitsMiles:
                    params.m_nScaleX = 3; //0.001
                    params.m_nScaleY = 3;
                    break;
                case GisEngine::GisCommon::UnitsMeters:
                case GisEngine::GisCommon::UnitsYards:
                case GisEngine::GisCommon::UnitsFeet:
                case GisEngine::GisCommon::UnitsDecimeters:
                case GisEngine::GisCommon::UnitsInches:
                    params.m_nScaleX = 2; //0.01
                    params.m_nScaleY = 2;
                    break;
                case GisEngine::GisCommon::UnitsMillimeters:
                    params.m_nScaleX = 1;
                    params.m_nScaleY = 1;
                    break;
                default:
                    params.m_nScaleX  = 4; //0.0001
                    params.m_nScaleY  = 4;
                    break;
            }

            double dScale = 1.0/pow(10.0, params.m_nScaleX);

            int64 nMaxVal = int64(dMaxCoord/dScale);

            params.m_PointType =   CommonLib::GetCompressType(nMaxVal);


            return params;

        }*/

    }
    }