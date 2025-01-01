#pragma once


#include "../Geometry.h"

namespace GraphEngine {
    namespace Geometry {
        enum eSPRefParamType
        {
            eSPRefTypePRJFilePath = 1,
            eSPRefTypePRJ4String = 2
        };


        class CSpatialReferenceProj4 : public ISpatialReference {
        public:
            typedef void *Handle;

            CSpatialReferenceProj4(CommonLib::IAllocPtr ptrAlloc = nullptr);

            CSpatialReferenceProj4(const std::string &prj4Str, eSPRefParamType paramType = eSPRefTypePRJ4String,
                                   CommonLib::IAllocPtr ptrAlloc = nullptr);

            CSpatialReferenceProj4(int prjCode, CommonLib::IAllocPtr ptrAlloc = nullptr);

            CSpatialReferenceProj4(Handle hHandle, CommonLib::IAllocPtr ptrAlloc = nullptr);

            CSpatialReferenceProj4(const CommonLib::bbox &bbox, CommonLib::IAllocPtr ptrAlloc = nullptr);

            virtual ~CSpatialReferenceProj4();

            virtual bool IsValid();

            virtual void *GetHandle();

            virtual bool Project(ISpatialReferencePtr destSpatRef, CommonLib::IGeoShapePtr ptrShape);

            virtual bool Project(ISpatialReferencePtr destSpatRef, CommonLib::bbox &bbox);

            virtual bool Project(ISpatialReferencePtr destSpatRef, CommonLib::GisXYPoint *pPoint);

            virtual CommonLib::Units GetUnits() const;

            virtual const std::string &GetProjectionString() const;

            virtual int GetProjectionCode() const;

            virtual bool IsProjection() const;

            virtual bool IsEqual(ISpatialReferencePtr pSpatRef) const;

            virtual ISpatialReferencePtr Clone() const;

            virtual void Save(CommonLib::ISerializeObjPtr pObj) const;

            virtual void Load(CommonLib::ISerializeObjPtr pObj);

        private:
            void CreateProjection();

            void PrepareGeometries();

            void PreTransform(CommonLib::IGeoShapePtr shp) const;

            bool IsEqual(CSpatialReferenceProj4 *pSpRef) const;

            void TestBounds(CommonLib::IGeoShapePtr geom) const;

            void PrepareGeometry(CommonLib::IGeoShapePtr pShp, double left_meridian, double bottom_parallel,
                                 double right_meridian, double top_parallel);

            void PrepareCutMeridian(double cut_meridian, double bottom_parallel, double top_parallel);

            void PrepareBoundShape(const CommonLib::bbox &bbox);

            void DensifyBoundBox(CommonLib::IGeoShapePtr shp, const CommonLib::bbox &bbox, int precision = 10) const;

        private:
            Handle m_prjHandle;
            int m_prjCode;
            std::string m_prj4Str;

            CommonLib::IGeoShapePtr m_LeftShp;
            CommonLib::IGeoShapePtr m_RightShp;
            CommonLib::IGeoShapePtr m_BoundShape;
            CommonLib::bbox m_cutMeridian;
            CommonLib::bbox m_fullExtent;

            mutable size_t m_nBufferSize;
            mutable double *m_pBufferX;
            mutable double *m_pBufferY;
            mutable double *m_pBufferZ;

            CommonLib::IAllocPtr m_ptrAlloc;
            ITopologicalOperatorPtr m_pTopoOp;
        };
    }
    }
