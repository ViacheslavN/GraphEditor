#include "Geometry.h"

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
        private:
            CommonLib::bbox m_box;
            ISpatialReferencePtr m_pSpatialRef;
        };

    }
    }