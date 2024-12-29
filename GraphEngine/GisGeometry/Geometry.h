#pragma once

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


        class ISpatialReference
        {
        public:
            ISpatialReference(){};
            virtual ~ISpatialReference(){}

            virtual bool  IsValid() = 0;
            virtual void*  GetHandle() = 0;
            virtual bool Project(ISpatialReferencePtr destSpatRef, CommonLib::IGeoShape* pShape) = 0;
            virtual bool Project(ISpatialReferencePtr destSpatRef, CommonLib::bbox& bbox) = 0;
            virtual bool Project(ISpatialReferencePtr destSpatRef, CommonLib::GisXYPoint* pPoint) = 0;

            virtual const std::string& GetProjectionString() const = 0;
            virtual int   GetProjectionCode() const = 0;
            virtual bool  IsProjection() const = 0;

            virtual bool IsEqual(ISpatialReferencePtr pSpatRef) const= 0;
            virtual ISpatialReferencePtr clone() const = 0;

            virtual CommonLib::Units GetUnits() const = 0;

            virtual void Save(CommonLib::ISerializeObjPtr pObj, const std::string& name) const = 0;
            virtual void Load(CommonLib::ISerializeObjPtr pObj, const std::string& name)= 0;
        };



        class  ITopologicalOperator
        {

            ITopologicalOperator(){}
            virtual ~ITopologicalOperator(){}
            virtual CommonLib::IGeoShapePtr Intersect(CommonLib::IGeoShape* shape1, CommonLib::IGeoShape* shape2) const = 0;
            virtual CommonLib::IGeoShapePtr AddShapeToIntersect(CommonLib::IGeoShape* shape)  = 0;
            virtual CommonLib::IGeoShapePtr ClearIntersect()  = 0;
            virtual CommonLib::IGeoShapePtr Intersect(CommonLib::IGeoShape* shape) const = 0;
            virtual bool  IsIntersection(const CommonLib::IGeoShape* pShapeL, const CommonLib::IGeoShape* pShapeR, CommonLib::IGeoShape* pShapeRes = 0) = 0;

            virtual CommonLib::IGeoShapePtr  Union(CommonLib::IGeoShape* shape1, CommonLib::IGeoShape* shape2) const = 0;
            virtual CommonLib::IGeoShapePtr  AddShapeToUnion(CommonLib::IGeoShape* shape)  = 0;
            virtual CommonLib::IGeoShapePtr  Union(CommonLib::IGeoShape* shape) const = 0;
            virtual CommonLib::IGeoShapePtr  ClearUnion()  = 0;


            virtual CommonLib::IGeoShapePtr  GetDifference(CommonLib::IGeoShape* shape1, CommonLib::IGeoShape* shape2) const = 0;
            virtual CommonLib::IGeoShapePtr  GetSymmetricDifference(CommonLib::IGeoShape* shape1, CommonLib::IGeoShape* shape2) const = 0;
            virtual bool      Contains(CommonLib::IGeoShape* shape1, CommonLib::IGeoShape* shape2) const = 0;

            virtual CommonLib::IGeoShapePtr  Clip(CommonLib::IGeoShape* shape, const CommonLib::bbox& box) const = 0;
            virtual CommonLib::IGeoShapePtr  CalcConvexHull(CommonLib::IGeoShape* shape) const = 0;
            virtual CommonLib::IGeoShapePtr  CalcBuffer(CommonLib::IGeoShape* shape, double distance) const = 0;

            virtual double    CalcDistance(CommonLib::IGeoShape* shape1, CommonLib::IGeoShape* shape2) const = 0;
            virtual CommonLib::IGeoShapePtr  QuerySubCurve (CommonLib::IGeoShape* shape, int geomidx, double dist_from, double dist_to) const = 0;
            virtual CommonLib::GisXYPoint QuerySubPoint (CommonLib::IGeoShape* shape, int geomidx, double dist) const = 0;
            virtual double    QuerySubAngle (CommonLib::IGeoShape* shape, int geomidx, double dist) const = 0;
            virtual void QuerySubPointAndAngle(CommonLib::IGeoShape* shape, int geomidx, double dist,CommonLib::GisXYPoint *pPoint,double *pAngle, bool firstTime) const = 0;
            virtual double    CalcDistanceToPoint(CommonLib::IGeoShape* shape, const CommonLib::GisXYPoint& pnt, CommonLib::GisXYPoint* pnt_out) const = 0;
            virtual double    CalcDistanceToPoint(const CommonLib::GisXYPoint* pnt_in, size_t npnt_in, const CommonLib::GisXYPoint& pnt, CommonLib::GisXYPoint& pnt_out, size_t &outidx) const = 0;
            virtual double    CalcLineLength(CommonLib::IGeoShape* shape) const = 0;
            virtual double    CalcPolygonArea(CommonLib::IGeoShape* shape) const = 0;
            virtual double    CalcPolygonPerimeter(CommonLib::IGeoShape* shape) const = 0;

            virtual bool      IsPointInside(CommonLib::IGeoShape* shape, const CommonLib::GisXYPoint& pnt) const = 0;
            virtual bool      IsPointOutside(CommonLib::IGeoShape* shape, const CommonLib::GisXYPoint& pnt) const = 0;
            virtual bool      IsPointOnBorder(CommonLib::IGeoShape* shape, const CommonLib::GisXYPoint& pnt) const = 0;
            virtual bool      IsIntersectionNotNull(CommonLib::IGeoShape* shape1, CommonLib::IGeoShape* shape2) const = 0;
            virtual bool      IsClippingNotNull(CommonLib::IGeoShape* shape, const CommonLib::bbox& box) const = 0;

            virtual bool      IsSimple(CommonLib::IGeoShape* shape) const = 0;
            virtual void      Simplify(CommonLib::IGeoShape* shape) const = 0;

            virtual bool CreateBufferZone(const CommonLib::IGeoShape* shape, CommonLib::IGeoShape* pShapeRes, double dDelta, eTOJoinType type) const = 0;

        };

        class  IEnvelope
        {
            IEnvelope(){}
            virtual ~IEnvelope(){}

            virtual CommonLib::bbox&				GetBoundingBox() = 0;
            virtual void                       SetBoundingBox(const CommonLib::bbox& box) = 0;
            virtual ISpatialReferencePtr       GetSpatialReference() const = 0;
            virtual void                       SetSpatialReference(ISpatialReferencePtr ptrSpatRef) = 0;
            virtual void                       Expand(IEnvelopePtr ptrEnvelope) = 0;
            virtual bool                       Intersect(IEnvelopePtr ptrEnvelope) = 0;
            virtual void                       Project(ISpatialReferencePtr ptrspatRef) = 0;

            virtual IEnvelopePtr				  clone() const = 0;

        };
    }
}