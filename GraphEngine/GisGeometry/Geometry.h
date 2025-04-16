#pragma once
#include "../CommonLib/CommonLib.h"
#include "../CommonLib/SpatialData/BoundaryBox.h"
#include "../CommonLib/SpatialData/IGeoShape.h"
#include "../CommonLib/SpatialData/Units.h"
#include "../CommonLib/Serialize/SerializeObj.h"

namespace GraphEngine {
    namespace Geometry {
        enum ePolygonOpType
        {
            PolyIntersection,
            PolyUnion,
            PolyDifference,
            PolyXor
        };


        enum eTOJoinType
        {
            ToJtSquare,
            ToJtRound,
            ToJtMiter
        };

        typedef  std::shared_ptr<class ISpatialReference> ISpatialReferencePtr;
        typedef  std::shared_ptr<class ITopologicalOperator> ITopologicalOperatorPtr;
        typedef  std::shared_ptr<class IEnvelope> IEnvelopePtr;


        class ISpatialReference : public CommonLib::ISerialize
        {
        public:
            ISpatialReference(){};
            virtual ~ISpatialReference(){}

            virtual bool  IsValid() = 0;
            virtual void*  GetHandle() = 0;
            virtual bool Project(ISpatialReferencePtr destSpatRef, CommonLib::IGeoShapePtr pShape) = 0;
            virtual bool Project(ISpatialReferencePtr destSpatRef, CommonLib::bbox& bbox) = 0;
            virtual bool Project(ISpatialReferencePtr destSpatRef, CommonLib::GisXYPoint* pPoint) = 0;

            virtual const std::string& GetProjectionString() const = 0;
            virtual int   GetProjectionCode() const = 0;
            virtual bool  IsProjection() const = 0;

            virtual bool IsEqual(ISpatialReferencePtr pSpatRef) const= 0;
            virtual ISpatialReferencePtr Clone() const = 0;

            virtual CommonLib::Units GetUnits() const = 0;

        };



        class  ITopologicalOperator
        {
        public:
            ITopologicalOperator(){}
            virtual ~ITopologicalOperator(){}
            virtual CommonLib::IGeoShapePtr Intersect(CommonLib::IGeoShapePtr shape1, CommonLib::IGeoShapePtr shape2) const = 0;
            virtual CommonLib::IGeoShapePtr AddShapeToIntersect(CommonLib::IGeoShapePtr shape)  = 0;
            virtual CommonLib::IGeoShapePtr ClearIntersect()  = 0;
            virtual CommonLib::IGeoShapePtr Intersect(CommonLib::IGeoShapePtr shape) const = 0;
            virtual bool  IsIntersection(const CommonLib::IGeoShapePtr pShapeL, const CommonLib::IGeoShapePtr pShapeR, CommonLib::IGeoShapePtr* pShapeRes = nullptr) = 0;

            virtual CommonLib::IGeoShapePtr  Union(CommonLib::IGeoShapePtr shape1, CommonLib::IGeoShapePtr shape2) const = 0;
            virtual CommonLib::IGeoShapePtr  AddShapeToUnion(CommonLib::IGeoShapePtr shape)  = 0;
            virtual CommonLib::IGeoShapePtr  Union(CommonLib::IGeoShapePtr shape) const = 0;
            virtual CommonLib::IGeoShapePtr  ClearUnion()  = 0;


            virtual CommonLib::IGeoShapePtr  GetDifference(CommonLib::IGeoShapePtr shape1, CommonLib::IGeoShapePtr shape2) const = 0;
            virtual CommonLib::IGeoShapePtr  GetSymmetricDifference(CommonLib::IGeoShapePtr shape1, CommonLib::IGeoShapePtr shape2) const = 0;
            virtual bool      Contains(CommonLib::IGeoShapePtr shape1, CommonLib::IGeoShapePtr shape2) const = 0;

            virtual CommonLib::IGeoShapePtr  Clip(CommonLib::IGeoShapePtr shape, const CommonLib::bbox& box) const = 0;
            virtual CommonLib::IGeoShapePtr  CalcConvexHull(CommonLib::IGeoShapePtr shape) const = 0;
            virtual CommonLib::IGeoShapePtr  CalcBuffer(CommonLib::IGeoShapePtr shape, double distance) const = 0;

            virtual double    CalcDistance(CommonLib::IGeoShapePtr shape1, CommonLib::IGeoShapePtr shape2) const = 0;
            virtual CommonLib::IGeoShapePtr  QuerySubCurve (CommonLib::IGeoShapePtr shape, int geomidx, double dist_from, double dist_to) const = 0;
            virtual CommonLib::GisXYPoint QuerySubPoint (CommonLib::IGeoShapePtr shape, int geomidx, double dist) const = 0;
            virtual double    QuerySubAngle (CommonLib::IGeoShapePtr shape, int geomidx, double dist) const = 0;
            virtual void QuerySubPointAndAngle(CommonLib::IGeoShapePtr shape, int geomidx, double dist,CommonLib::GisXYPoint *pPoint,double *pAngle, bool firstTime) const = 0;
            virtual double    CalcDistanceToPoint(CommonLib::IGeoShapePtr shape, const CommonLib::GisXYPoint& pnt, CommonLib::GisXYPoint* pnt_out) const = 0;
            virtual double    CalcDistanceToPoint(const CommonLib::GisXYPoint* pnt_in, size_t npnt_in, const CommonLib::GisXYPoint& pnt, CommonLib::GisXYPoint& pnt_out, size_t &outidx) const = 0;
            virtual double    CalcLineLength(CommonLib::IGeoShapePtr shape) const = 0;
            virtual double    CalcPolygonArea(CommonLib::IGeoShapePtr shape) const = 0;
            virtual double    CalcPolygonPerimeter(CommonLib::IGeoShapePtr shape) const = 0;

            virtual bool      IsPointInside(CommonLib::IGeoShapePtr shape, const CommonLib::GisXYPoint& pnt) const = 0;
            virtual bool      IsPointOutside(CommonLib::IGeoShapePtr shape, const CommonLib::GisXYPoint& pnt) const = 0;
            virtual bool      IsPointOnBorder(CommonLib::IGeoShapePtr shape, const CommonLib::GisXYPoint& pnt) const = 0;
            virtual bool      IsIntersectionNotNull(CommonLib::IGeoShapePtr shape1, CommonLib::IGeoShape* shape2) const = 0;
            virtual bool      IsClippingNotNull(CommonLib::IGeoShapePtr shape, const CommonLib::bbox& box) const = 0;

            virtual bool      IsSimple(CommonLib::IGeoShapePtr shape) const = 0;
            virtual void      Simplify(CommonLib::IGeoShapePtr shape) const = 0;

            virtual bool CreateBufferZone(const CommonLib::IGeoShapePtr shape, CommonLib::IGeoShapePtr pShapeRes, double dDelta, eTOJoinType type) const = 0;

        };

        class  IEnvelope
        {
        public:
            IEnvelope(){}
            virtual ~IEnvelope(){}

            virtual CommonLib::bbox&		    GetBoundingBox() = 0;
            virtual void                       SetBoundingBox(const CommonLib::bbox& box) = 0;
            virtual ISpatialReferencePtr       GetSpatialReference() const = 0;
            virtual void                       SetSpatialReference(ISpatialReferencePtr ptrSpatRef) = 0;
            virtual void                       Expand(IEnvelopePtr ptrEnvelope) = 0;
            virtual bool                       Intersect(IEnvelopePtr ptrEnvelope) = 0;
            virtual void                       Project(ISpatialReferencePtr ptrspatRef) = 0;
            virtual IEnvelopePtr			   Clone() const = 0;

        };
    }
}