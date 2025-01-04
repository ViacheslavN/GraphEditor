#include "ShapefileUtils.h"
#include "../../CommonLib/str/StrUtils.h"

namespace GraphEngine
{
    namespace GeoDatabase {

        std::string  CShapefileUtils::NormalizePath(const  std::string& path)
        {
            if(path.empty())
                return path;

            std::string normalizedPath = path;
#ifdef WIN32
          CommonLib::StringUtils::Lower(normalizedPath);
#endif
            if(normalizedPath[-1 + (int)normalizedPath.length()] != '\\' &&
               normalizedPath[-1 + (int)normalizedPath.length()] != '/')
            {
#ifdef WIN32
                normalizedPath += "\\";
#else
                normalizedPath += "/";
#endif
            }

            return normalizedPath;
        }
        eDataTypes CShapefileUtils::SHPFieldInfoToFieldInfo(DBFFieldType ftype, int width, int dec, int* length, int* precision, int* scale)
        {
            *length = 0;
            *precision = 0;
            *scale = 0;

            switch(ftype)
            {
                case FTString:
                    *length = width;
                    return dtString;
                case FTInteger:
                    *precision = width - 1;
                    return dtInteger32;
                case FTDouble:
                    *precision = width - 1;
                    *scale = dec;
                    if(dec == 0)
                    {
                        if(width < 10)
                            return dtInteger32;
                        return dtInteger64;
                    }
                    else
                        return dtDouble;
                case FTDate:
                    return dtDate;
            }
            return dtUnknown;
        }

        CommonLib::eShapeType CShapefileUtils::SHPTypeToGeometryType(int shpType, bool* hasZ, bool* hasM)
        {
            CommonLib::eShapeType geomType = CommonLib::shape_type_null;

            switch((size_t)shpType)
            {
                case SHPT_NULL:
                    geomType = CommonLib::shape_type_null;
                    break;
                case SHPT_POINT:
                case SHPT_POINTZ:
                case SHPT_POINTM:
                    geomType = CommonLib::shape_type_point;
                    break;
                case SHPT_ARC:
                case SHPT_ARCZ:
                case SHPT_ARCM:
                    geomType = CommonLib::shape_type_polyline;
                    break;
                case SHPT_POLYGON:
                case SHPT_POLYGONZ:
                case SHPT_POLYGONM:
                    geomType = CommonLib::shape_type_polygon;
                    break;
                case SHPT_MULTIPOINT:
                case SHPT_MULTIPOINTZ:
                case SHPT_MULTIPOINTM:
                    geomType = CommonLib::shape_type_multipoint;
                    break;
                case SHPT_MULTIPATCH:
                    geomType = CommonLib::shape_type_multipatch;
                    break;
            }

            if(hasZ)
            {
                if(shpType == SHPT_POINTZ || shpType == SHPT_ARCZ ||
                   shpType == SHPT_POLYGONZ || shpType == SHPT_MULTIPOINTZ ||
                   shpType == SHPT_MULTIPOINTZ)
                    *hasZ = true;
                else
                    *hasZ = false;
            }

            if(hasM)
            {
                if(shpType == SHPT_POINTM || shpType == SHPT_ARCM ||
                   shpType == SHPT_POLYGONM || shpType == SHPT_MULTIPOINTM)
                    *hasM = true;
                else
                    *hasM = false;
            }

            return geomType;
        }

        void CShapefileUtils::SHPObjectToGeometry(CSHPObjectPtr ptrObj, CommonLib::IGeoShapePtr ptrResult)
        {
            ptrResult->Create(SHPTypeToGeometryType(ptrObj->GetSHPType()), ptrObj->GetVertices(), ptrObj->GetParts());

            CommonLib::GisXYPoint* pPt = ptrResult->GetPoints();
            double* zs = ptrResult->GetZs();
            double* ms = ptrResult->GetMs();
            int i;
            for(i = 0; i < ptrObj->GetVertices(); ++i, ++pPt, zs += zs ? 1 : 0, ms += ms ? 1 : 0)
            {
                pPt->x = ptrObj->GetPadfX(i);
                pPt->y = ptrObj->GetPadfY(i);
                if(ptrObj->HasZ() && zs)
                    *zs = ptrObj->GetPadfZ(i);
                if(ptrObj->HasM() && ms)
                    *ms = ptrObj->GetPadfM(i);
            }

            uint32_t* parts = ptrResult->GetParts();
            CommonLib::patch_type* partTypes = ptrResult->GetPartsTypes();
            if ( ptrObj->GetParts() != 0 && parts)
            {
                for(i = 0; i < ptrObj->GetParts(); ++i, ++parts, partTypes += partTypes ? 1 : 0)
                {
                    *parts = (long)ptrObj->GetPanPartStart(i);
                    if(ptrObj->HasPanPartType() && partTypes)
                    {
                        switch(ptrObj->GetPanPartType(i))
                        {
                            case SHPP_TRISTRIP:
                                *partTypes = CommonLib::patch_type_triangle_strip;
                                break;
                            case SHPP_TRIFAN:
                                *partTypes = CommonLib::patch_type_triangle_fan;
                                break;
                            case SHPP_OUTERRING:
                                *partTypes = CommonLib::patch_type_outer_ring;
                                break;
                            case SHPP_INNERRING:
                                *partTypes = CommonLib::patch_type_inner_ring;
                                break;
                            case SHPP_FIRSTRING:
                                *partTypes = CommonLib::patch_type_first_ring;
                                break;
                            case SHPP_RING:
                                *partTypes = CommonLib::patch_type_ring;
                                break;
                        }
                    }
                }
            }

            ptrResult->CalcBB();
        }
    }

}
