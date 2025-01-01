#include "SpatialReferenceProj4.h"
#include "../org/ogr_spatialref.h"
#include "../cpl/cpl_conv.h"
#include "../../ThirdParty/Proj4/src/projects.h"
#include "../../ThirdParty/Proj4/src/proj_api.h"
#include "../../CommonLib/str/StrUtils.h"
#include "../../CommonLib/alloc/simpleAlloc.h"
#include "../../CommonLib/filesystem/File.h"
#include "../../CommonLib/SpatialData/GeoShape.h"
#include "CodeProj.h"

namespace GraphEngine {
    namespace Geometry {

        const wchar_t c_WGS84_ESRI_STRING[] = L"GEOGCS[\"GCS_WGS_1984\",DATUM[\"D_WGS_1984\",SPHEROID[\"WGS_1984\",6378137,298.257223563]],PRIMEM[\"Greenwich\",0],UNIT[\"Degree\",0.0174532925199433]]";
        const char c_WGS84_PROJ4_STRING[] = "+proj=longlat +ellps=WGS84 +datum=WGS84";

        double get_cut_meridian(PJ* pj)
        {
            // search for lon_0 param
            for(paralist *pl = pj->params; pl; pl = pl->next)
            {

                std::string param = pl->param;
                if(param.find("lon_0") != std::string::npos)
                {
                    size_t n = param.find("=");
                    if(n !=  std::string::npos)
                    {
                        param =  CommonLib::StringUtils::Right(param,  param.length() - n - 1);
                        double val = atof(param.c_str());
                        val += (val > 0) ? -180 : 180;
                        return val;
                    }
                }
            }
            return HUGE_VAL;
        }

        void get_parallel_range(PJ* pj, double* bottom_parallel, double* top_parallel)
        {
            // search for "proj" param
            std::string param;
            for(paralist *pl = pj->params; pl; pl = pl->next)
            {

                std::string val = pl->param;
                if(val.find("proj") != std::string::npos)
                {
                    size_t n = val.find("=");
                    if(n != std::string::npos)
                    {
                        param = CommonLib::StringUtils::Right(val, val.length() - n - 1);
                        break;
                    }
                }
            }
            if(!param.empty())
            {
                if(CommonLib::StringUtils::Equals(param, "aea",false))
                {
                    *bottom_parallel = -90.0;
                    *top_parallel = 89.9;
                }
                else if(CommonLib::StringUtils::Equals(param, "lcc", false))
                {
                    *bottom_parallel = -59.9;
                    *top_parallel = 89.9;
                }
                else
                {
                    *bottom_parallel = -89.9;
                    *top_parallel = 89.9;
                }
            }
            else
            {
                *bottom_parallel = -89.9;
                *top_parallel = 89.9;
            }
        }
        CSpatialReferenceProj4::CSpatialReferenceProj4(CommonLib::IAllocPtr pAlloc) :
                m_prjCode(0)
                ,m_prjHandle(0)
                ,m_ptrAlloc(pAlloc)
                , m_pBufferX(0)
                , m_pBufferY(0)
                , m_pBufferZ(0)
                , m_nBufferSize(0)
        {
            if(m_ptrAlloc.get() == nullptr)
                m_ptrAlloc = std::make_shared<CommonLib::CSimpleAlloc>();

            m_LeftShp = std::make_shared<CommonLib::CGeoShape>();
            m_RightShp= std::make_shared<CommonLib::CGeoShape>();
            m_BoundShape= std::make_shared<CommonLib::CGeoShape>();
        }

        CSpatialReferenceProj4::CSpatialReferenceProj4(const std::string & prj4Str, eSPRefParamType paramType, CommonLib::IAllocPtr pAlloc) :
                m_prjCode(0)
                ,m_prjHandle(0)
                ,m_ptrAlloc(pAlloc)
                , m_pBufferX(0)
                , m_pBufferY(0)
                , m_pBufferZ(0)
                , m_nBufferSize(0)
        {

            try
            {
                if(m_ptrAlloc.get() == nullptr)
                    m_ptrAlloc = std::make_shared<CommonLib::CSimpleAlloc>();

                m_LeftShp = std::make_shared<CommonLib::CGeoShape>();
                m_RightShp= std::make_shared<CommonLib::CGeoShape>();
                m_BoundShape= std::make_shared<CommonLib::CGeoShape>();

                if(paramType == eSPRefTypePRJ4String)
                {
                    m_prj4Str = prj4Str;
                    CreateProjection();
                }
                else {
                    CommonLib::file::TFilePtr ptrFile = CommonLib::file::CFileCreator::OpenFileA(prj4Str.c_str(),
                                                                                                 CommonLib::file::ofmOpenExisting,
                                                                                                 CommonLib::file::arRead,
                                                                                                 CommonLib::file::smNoMode,
                                                                                                 CommonLib::file::oftBinary);
                    uint32_t fsize = (uint32_t) ptrFile->GetFileSize();
                    std::vector<char> buf(fsize + 1);
                    ptrFile->Read((byte_t *) &buf[0], fsize);
                    buf[fsize] = 0;
                    m_prj4Str = &buf[0];
                    CreateProjection();
                }
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to create CSpatialReferenceProj4", exc);
            }



        }
        CSpatialReferenceProj4::CSpatialReferenceProj4(int prjCode, CommonLib::IAllocPtr pAlloc) :
                m_prjCode(prjCode)
                ,m_prjHandle(0)
                ,m_ptrAlloc(pAlloc)
                , m_pBufferX(0)
                , m_pBufferY(0)
                , m_pBufferZ(0)
                , m_nBufferSize(0)
        {
            if(m_ptrAlloc.get() == nullptr)
                m_ptrAlloc = std::make_shared<CommonLib::CSimpleAlloc>();

            m_LeftShp = std::make_shared<CommonLib::CGeoShape>();
            m_RightShp= std::make_shared<CommonLib::CGeoShape>();
            m_BoundShape= std::make_shared<CommonLib::CGeoShape>();

            CreateProjection();
        }
        CSpatialReferenceProj4::CSpatialReferenceProj4(Handle hHandle, CommonLib::IAllocPtr pAlloc) :
                m_prjCode(0)
                ,m_prjHandle(hHandle)
                ,m_ptrAlloc(pAlloc)
                , m_pBufferX(0)
                , m_pBufferY(0)
                , m_pBufferZ(0)
                , m_nBufferSize(0)
        {
            if(m_ptrAlloc.get() == nullptr)
                m_ptrAlloc = std::make_shared<CommonLib::CSimpleAlloc>();

            m_LeftShp = std::make_shared<CommonLib::CGeoShape>();
            m_RightShp= std::make_shared<CommonLib::CGeoShape>();
            m_BoundShape= std::make_shared<CommonLib::CGeoShape>();
        }

        CSpatialReferenceProj4::CSpatialReferenceProj4(const CommonLib::bbox& bbox, CommonLib::IAllocPtr pAlloc): m_prjCode(0)
                ,m_prjHandle(0)
                ,m_ptrAlloc(pAlloc)
                , m_pBufferX(0)
                , m_pBufferY(0)
                , m_pBufferZ(0)
                , m_nBufferSize(0)
        {

            if(m_ptrAlloc.get() == nullptr)
                m_ptrAlloc = std::make_shared<CommonLib::CSimpleAlloc>();

            m_LeftShp = std::make_shared<CommonLib::CGeoShape>();
            m_RightShp= std::make_shared<CommonLib::CGeoShape>();
            m_BoundShape= std::make_shared<CommonLib::CGeoShape>();

            bool degrees = true;

            if(bbox.xMin < -1000.0 || bbox.xMin > 1000.0)
                degrees = false;
            if(bbox.xMax < -1000.0 || bbox.xMax > 1000.0)
                degrees = false;
            if(bbox.yMin < -1000.0 || bbox.yMin > 1000.0)
                degrees = false;
            if(bbox.yMax < -1000.0 || bbox.yMax > 1000.0)
                degrees = false;

            if(degrees)
            {
                m_prj4Str = c_WGS84_PROJ4_STRING;
                CreateProjection();
            }
        }

        CSpatialReferenceProj4::~CSpatialReferenceProj4()
        {
            if (m_prjHandle)
                pj_free((PJ*)m_prjHandle);

            if (m_pBufferX)
                m_ptrAlloc->Free(m_pBufferX);

            if (m_pBufferY)
                m_ptrAlloc->Free(m_pBufferY);

            if (m_pBufferZ)
                m_ptrAlloc->Free(m_pBufferZ);
        }



        bool  CSpatialReferenceProj4::IsValid()
        {
            return m_prjHandle != 0;
        }

        void*  CSpatialReferenceProj4::GetHandle()
        {
            return m_prjHandle;
        }
        bool CSpatialReferenceProj4::Project(ISpatialReferencePtr destSpatRef, CommonLib::IGeoShapePtr pShape)
        {
            if (destSpatRef.get() == nullptr)
                return false;

            PJ *pj = (PJ*)destSpatRef->GetHandle();
            if (!pj)
                return false;

            if(pj_is_latlong((PJ*)m_prjHandle) && !pj_is_latlong((PJ*)pj))
                ((CSpatialReferenceProj4*)destSpatRef.get())->PreTransform(pShape);

            if(!pj_is_latlong((PJ*)m_prjHandle) && pj_is_latlong((PJ*)pj))
                TestBounds(pShape);

            uint32_t pointCount = pShape->GetPointCnt();
            double *pZs = pShape->GetZs();
            CommonLib::GisXYPoint* pPt = pShape->GetPoints();
            if (pointCount > m_nBufferSize)
            {
                if(m_pBufferX)
                    m_ptrAlloc->Free(m_pBufferX);
                m_pBufferX = (double*)m_ptrAlloc->Alloc(sizeof(double) * pointCount);

                if(m_pBufferY)
                    m_ptrAlloc->Free(m_pBufferY);
                m_pBufferY = (double*)m_ptrAlloc->Alloc(sizeof(double) * pointCount);

                m_nBufferSize = pointCount;
                if (pZs)
                {
                    if(m_pBufferZ)
                        m_ptrAlloc->Free(m_pBufferZ);
                    m_pBufferZ = (double*)m_ptrAlloc->Alloc(sizeof(double) * pointCount);
                }
            }

            if (!pZs && m_pBufferZ)
            {
                m_ptrAlloc->Free(m_pBufferZ);
                m_pBufferZ = NULL;
            }


            bool is_latlong = pj_is_latlong((PJ*)m_prjHandle) != 0;
            double koef = is_latlong ? DEG_TO_RAD : 1.0;
            for (size_t idx = 0; idx < pointCount; ++idx)
            {
                m_pBufferX[idx] = pPt[idx].x*koef;
                m_pBufferY[idx] = pPt[idx].y*koef;

                if (m_pBufferZ && pZs)
                    m_pBufferZ[idx] = pZs[idx];
            }

            int ret = pj_transform((PJ*)m_prjHandle, pj, (long)pointCount, 0, m_pBufferX, m_pBufferY, m_pBufferZ);
            if (ret == 0)
            {
                koef = pj_is_latlong(pj) ? RAD_TO_DEG : 1.0;
                std::set<int> badPoints;

                for (size_t idx = 0; idx < pointCount; ++idx)
                {
                    if(m_pBufferX[idx] == HUGE_VAL || m_pBufferY[idx] == HUGE_VAL)
                        badPoints.insert((int)idx);
                }

                if(badPoints.size() == pointCount)
                {
                    pShape->Create(CommonLib::shape_type_null);
                    return false;
                }

                if(badPoints.empty())
                {
                    for (size_t idx = 0; idx < pointCount; ++idx)
                    {
                        pPt[idx].x = m_pBufferX[idx]*koef;
                        pPt[idx].y = m_pBufferY[idx]*koef;
                        if (pZs && m_pBufferZ)
                            pZs[idx] = m_pBufferZ[idx];
                    }
                }
                else
                {
                    std::vector<long> newPartsStarts;
                    std::vector<CommonLib::GisXYPoint> newPoints;
                    std::vector<double> newZs;

                    int nparts = (int)pShape->GetPartCount();

                    if(nparts != 0)
                        newPartsStarts.reserve(nparts);

                    newPoints.reserve(pointCount);
                    newZs.reserve(pointCount);



                    int npointall = 0;
                    if(nparts == 0)
                    {
                        for(size_t npoint = 0; npoint < pointCount; ++npoint, ++npointall)
                        {
                            if(badPoints.find(npointall) != badPoints.end())
                                continue;

                            CommonLib::GisXYPoint p;
                            p.x = m_pBufferX[npointall] * koef;
                            p.y = m_pBufferY[npointall] * koef;
                            newPoints.push_back(p);
                            if(pZs && m_pBufferZ)
                                newZs.push_back(m_pBufferZ[npointall]);
                        }
                    }
                    else
                    {
                        const uint32_t* npartsstarts = pShape->GetParts();
                        for(int npart = 0; npart < nparts; ++npart)
                        {
                            int npartpointcount;
                            if(npart == nparts - 1)
                                npartpointcount = (int)pointCount - npartsstarts[npart];
                            else
                                npartpointcount = npartsstarts[npart + 1] - npartsstarts[npart];

                            bool pointIsFirstInPart = true;
                            for(int npoint = 0; npoint < npartpointcount; ++npoint, ++npointall)
                            {
                                if(badPoints.find(npointall) != badPoints.end())
                                    continue;

                                if(pointIsFirstInPart)
                                {
                                    newPartsStarts.push_back((long)newPoints.size());
                                    pointIsFirstInPart = false;
                                }

                                CommonLib::GisXYPoint p;
                                p.x = m_pBufferX[npointall] * koef;
                                p.y = m_pBufferX[npointall] * koef;
                                newPoints.push_back(p);
                                if(pZs && m_pBufferZ)
                                    newZs.push_back(m_pBufferZ[npointall]);
                            }

                        }
                    }


                    pShape->Create(pShape->Type(), (uint32_t )newPoints.size(), (uint32_t )newPartsStarts.size());

                    memcpy(pShape->GetPoints(), &newPoints[0], newPoints.size() * 2* sizeof(double));
                    if(!newPartsStarts.empty())
                        memcpy(pShape->GetParts(), &newPartsStarts[0], newPartsStarts.size() * sizeof(long));
                    if(pZs)
                        memcpy(pShape->GetZs(), &newZs[0], newZs.size() *  sizeof(double));
                }
                pShape->CalcBB();

                return true;
            }

            return false;
        }
        bool CSpatialReferenceProj4::Project(ISpatialReferencePtr destSpatRef, CommonLib::bbox& bbox)
        {
            if(destSpatRef.get() == nullptr)
                return false;

            CommonLib::IGeoShapePtr ptrGeom = std::make_shared<CommonLib::CGeoShape>();
            DensifyBoundBox(ptrGeom, bbox);
            ptrGeom->CalcBB();


            if(!CSpatialReferenceProj4::Project(destSpatRef, ptrGeom))
                return false;

            ptrGeom->CalcBB();
            bbox = ptrGeom->GetBB();
            return true;

        }
        bool CSpatialReferenceProj4::Project(ISpatialReferencePtr pDestSpatRef, CommonLib::GisXYPoint* pPoint)
        {
            if(pDestSpatRef.get() == nullptr)
                return false;

            CSpatialReferenceProj4 *pDestSpatProj4 = dynamic_cast<CSpatialReferenceProj4*>(pDestSpatRef.get());
            if(!pDestSpatProj4)
                return false;

            if(IsEqual(pDestSpatProj4))
                return true;

            PJ *pj = (PJ*)((CSpatialReferenceProj4*)pDestSpatRef.get())->GetHandle();
            if (!pj)
                return false;

            double point_sp_x;
            double point_sp_y;

            bool is_latlong = pj_is_latlong((PJ*)m_prjHandle) != 0;
            double koef = is_latlong ? DEG_TO_RAD : 1.0;

            point_sp_x = pPoint->x*koef;
            point_sp_y = pPoint->y*koef;

            int ret = pj_transform((PJ*)m_prjHandle, pj, 1, 1, &point_sp_x, &point_sp_y, 0);
            if (ret)
                return false;

            koef = pj_is_latlong(pj) != 0 ? RAD_TO_DEG : 1.0;
            pPoint->x = point_sp_x * koef;
            pPoint->y = point_sp_y * koef;
            return true;

        }

        const std::string& CSpatialReferenceProj4::GetProjectionString() const
        {
            return m_prj4Str;
        }
        int   CSpatialReferenceProj4::GetProjectionCode() const
        {
            return m_prjCode;
        }
        bool  CSpatialReferenceProj4::IsProjection() const
        {
            return true;
        }

        void CSpatialReferenceProj4::Save(CommonLib::ISerializeObjPtr pObj) const
        {
            try
            {
                pObj->AddPropertyString("proj", m_prj4Str);
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to save CSpatialReferenceProj4", exc);
            }
        }

        void CSpatialReferenceProj4::Load(CommonLib::ISerializeObjPtr pObj)
        {
            try
            {
                m_prj4Str = pObj->GetPropertyString("proj", "");
                CreateProjection();
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to load CSpatialReferenceProj4", exc);
            }
        }



        void CSpatialReferenceProj4::CreateProjection()
        {
            if (m_prjHandle)
                pj_free((PJ*)m_prjHandle);


            if(!m_prj4Str.empty())
            {
                m_prjHandle = pj_init_plus(m_prj4Str.c_str());
                if(m_prjHandle != 0)
                {
                    PrepareGeometries();
                    return;
                }
                else
                {

                    OGRSpatialReference spatRef(m_prj4Str.c_str());
                    char* prj4String;

                    if(spatRef.morphToESRI() != 0)
                        return;

                    if(spatRef.exportToProj4(&prj4String) != 0)
                        return;

                    m_prj4Str = prj4String;
                    CPLFree(prj4String);
                    m_prjHandle = pj_init_plus(m_prj4Str.c_str());
                    if(m_prjHandle != 0)
                    {
                        PrepareGeometries();
                        return;
                    }

                }
            }

            if(m_prjCode != 0)
            {
                m_prj4Str = CCodeProjConvertor::CodeToProj4Str(m_prjCode);
                m_prjHandle = pj_init_plus(m_prj4Str.c_str());
                if(m_prjHandle != 0)
                {
                    PrepareGeometries();
                }
            }


        }
        bool CSpatialReferenceProj4::IsEqual(ISpatialReferencePtr pSpRef) const
        {
            if(!pSpRef)
                return false;

            std::vector<std::string> orgParams;
            std::vector<std::string> clnParams;

            PJ* orgPrj = (PJ*)m_prjHandle;
            PJ* clnPrj = (PJ*)(pSpRef->GetHandle());
            if(orgPrj == clnPrj)
                return true;

            paralist* pl;
            pl = orgPrj->params;
            while(pl)
            {
                if(pl->used && strcmp(pl->param, "no_defs") != 0)
                    orgParams.push_back(pl->param);
                pl = pl->next;
            }
            std::sort(orgParams.begin(), orgParams.end());

            pl = clnPrj->params;
            while(pl)
            {
                if(pl->used && strcmp(pl->param, "no_defs") != 0)
                    clnParams.push_back(pl->param);
                pl = pl->next;
            }
            std::sort(clnParams.begin(), clnParams.end());
            if(orgParams.size() != clnParams.size())
                return false;

            for(int ind = 0; ind < (int)orgParams.size(); ++ind)
                if(orgParams[ind] != clnParams[ind])
                    return false;

            return true;
        }

        ISpatialReferencePtr CSpatialReferenceProj4::Clone() const
        {
            ISpatialReferencePtr pSpRef = std::make_shared<CSpatialReferenceProj4>(m_prj4Str, eSPRefTypePRJ4String, m_ptrAlloc);
            return pSpRef;
        }
        void  CSpatialReferenceProj4::PreTransform(CommonLib::IGeoShapePtr ptrShp) const
        {

        }

        void CSpatialReferenceProj4::PrepareGeometries()
        {
            if(pj_is_latlong((PJ*)m_prjHandle))
                return;

            double cut_meridian = get_cut_meridian((PJ*)m_prjHandle);
            if(cut_meridian == HUGE_VAL)
                return;

            double bottom_parallel;
            double top_parallel;

            get_parallel_range((PJ*)m_prjHandle, &bottom_parallel, &top_parallel);

            PrepareGeometry(m_LeftShp, cut_meridian - 359.9, bottom_parallel, cut_meridian - 0.01, top_parallel);
            PrepareGeometry(m_RightShp, cut_meridian + 0.01, bottom_parallel, cut_meridian + 359.9, top_parallel);
            PrepareCutMeridian(cut_meridian, bottom_parallel, top_parallel);


            PrepareBoundShape(m_RightShp->GetBB());
            PrepareBoundShape(m_LeftShp->GetBB());

            if(m_pTopoOp.get())
            {
                m_pTopoOp->ClearIntersect();
                m_pTopoOp->AddShapeToIntersect(m_BoundShape);
            }

        }
        void CSpatialReferenceProj4::PrepareCutMeridian(double cut_meridian, double bottom_parallel, double top_parallel)
        {
            m_cutMeridian.type = CommonLib::bbox_type_normal;
            m_cutMeridian.xMin = cut_meridian - 0.01;
            m_cutMeridian.yMin = bottom_parallel;
            m_cutMeridian.xMax = cut_meridian + 0.01;
            m_cutMeridian.yMax = top_parallel;
        }

        void CSpatialReferenceProj4::PrepareBoundShape(const CommonLib::bbox& bbox)
        {
            CommonLib::IGeoShapePtr shp = std::make_shared<CommonLib::CGeoShape>();
            CommonLib::bbox bbox_ = bbox;

            const int precision = 20;
            if(bbox_.xMin < -180) bbox_.xMin = -180;
            if(bbox_.xMax > 180) bbox_.xMax = 180;
            bbox_.yMax -= 1;
            bbox_.yMin += 1;
            bbox_.xMax -= 0.01;
            bbox_.xMin += 0.01;


            DensifyBoundBox(shp, bbox_, precision);

            CommonLib::GisXYPoint* pPt = shp->GetPoints();
            for (size_t i = 0, sz = shp->GetPointCnt(); i < sz; ++i)
            {
                projUV pnt;
                pnt.u = pPt[i].x * DEG_TO_RAD;
                pnt.v = pPt[i].y * DEG_TO_RAD;
                pnt = pj_fwd(pnt, (PJ*)m_prjHandle);

                pPt[i].x  = pnt.u;
                pPt[i].y = pnt.v;
            }


            m_BoundShape = shp;
            m_BoundShape->CalcBB();
        }

        void CSpatialReferenceProj4::TestBounds(CommonLib::IGeoShapePtr pShape) const
        {
            if(m_BoundShape->Type() == CommonLib::shape_type_null || m_pTopoOp.get() == NULL)
                return;

            CommonLib::IGeoShapePtr pRes = m_pTopoOp->Intersect(pShape);
            if(pRes.get())
                pShape = pRes;
        }
        bool CSpatialReferenceProj4::IsEqual(CSpatialReferenceProj4* pSpRef) const
        {
            if(!pSpRef)
                return false;

            std::vector<std::string> orgParams;
            std::vector<std::string> clnParams;

            PJ* orgPrj = (PJ*)m_prjHandle;
            PJ* clnPrj = (PJ*)(pSpRef->GetHandle());
            if(orgPrj == clnPrj)
                return true;

            paralist* pl;
            pl = orgPrj->params;
            while(pl)
            {
                if(pl->used && strcmp(pl->param, "no_defs") != 0)
                    orgParams.push_back(pl->param);
                pl = pl->next;
            }
            std::sort(orgParams.begin(), orgParams.end());

            pl = clnPrj->params;
            while(pl)
            {
                if(pl->used && strcmp(pl->param, "no_defs") != 0)
                    clnParams.push_back(pl->param);
                pl = pl->next;
            }
            std::sort(clnParams.begin(), clnParams.end());
            if(orgParams.size() != clnParams.size())
                return false;

            for(int ind = 0; ind < (int)orgParams.size(); ++ind)
                if(orgParams[ind] != clnParams[ind])
                    return false;

            return true;
        }

        void CSpatialReferenceProj4::PrepareGeometry(CommonLib::IGeoShapePtr pShp, double left_meridian, double bottom_parallel, double right_meridian, double top_parallel)
        {
            CommonLib::bbox bbox;
            bbox.xMin = left_meridian;
            bbox.xMax = right_meridian;
            bbox.yMin = bottom_parallel;
            bbox.yMax = top_parallel;

            CommonLib::IGeoShapePtr geom = std::make_shared<CommonLib::CGeoShape>();
            DensifyBoundBox(geom, bbox, 20);

            pShp = geom;
            m_fullExtent.expand(geom->GetBB());
        }

        CommonLib::Units CSpatialReferenceProj4::GetUnits() const
        {

            if(!m_prjHandle)
                return CommonLib::UnitsUnknown;
            return (pj_is_latlong((PJ*)m_prjHandle) != 0) ? CommonLib::UnitsDecimalDegrees : CommonLib::UnitsMeters;
        }

        void CSpatialReferenceProj4::DensifyBoundBox(CommonLib::IGeoShapePtr pShp, const CommonLib::bbox &bbox, int precision) const
        {
            const int pntCount = precision * 4;

            double w = bbox.xMax - bbox.xMin;
            double h = bbox.yMax - bbox.yMin;

            w /= precision;
            h /= precision;

            pShp->Create(CommonLib::shape_type_polygon, pntCount + 1, 0);
            CommonLib::GisXYPoint* pPt = pShp->GetPoints();

            for(int i = 0; i < precision; i++)
            {
                pPt[i + 0].x = bbox.xMin + w * i;
                pPt[i + 0].y = bbox.yMin;
                pPt[i + precision].x = bbox.xMax;
                pPt[i + precision].y = bbox.yMin + h * i;
                pPt[i + 2*precision].x = bbox.xMax - w * i;
                pPt[i + 2*precision].y = bbox.yMax;
                pPt[i + 3*precision].x = bbox.xMin;
                pPt[i + 3*precision].y = bbox.yMax - h * i;
            }
            pPt[pntCount].x = bbox.xMin;
            pPt[pntCount].y = bbox.yMin;

            pShp->CalcBB();

        }

    }
    }
