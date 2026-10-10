#pragma once
#include "Geometry.h"
#include "../GeometryCompression/ShapeCompressor.h"

namespace GraphEngine {
    namespace Geometry {
        class  CEnvelope : public IEnvelope
        {

        public:
            CEnvelope();
            CEnvelope(const CommonLib::bbox& box, ISpatialReferencePtr spatRef = nullptr);
            virtual ~CEnvelope();
        public:
            // IEnvelope
            virtual const CommonLib::bbox& GetBoundingBox() const;
            virtual ISpatialReferencePtr  GetSpatialReference() const;



            virtual CommonLib::bbox& GetBoundingBox();
            virtual void SetBoundingBox(const CommonLib::bbox& box);
            virtual void SetSpatialReference(ISpatialReferencePtr spatRef);
            virtual void Expand(IEnvelopePtr envelope);
            virtual bool Intersect(IEnvelopePtr envelope);
            virtual void Project(ISpatialReferencePtr spatRef);
             virtual IEnvelopePtr	Clone() const;

            // the parameters of the geometry compression for the shapes inside the envelope:
            // the precision by the units of the spatial reference (GeometryCompression::CompressParamsForExtent)
            GeometryCompression::SShapeCompressParams GetCompressParams() const;
        private:
            CommonLib::bbox m_box;
            ISpatialReferencePtr m_pSpatialRef;
        };

    }
    }