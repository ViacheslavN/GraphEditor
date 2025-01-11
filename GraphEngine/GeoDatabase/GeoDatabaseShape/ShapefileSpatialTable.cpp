#include "ShapefileSpatialTable.h"
#include "ShapefileRowCursor.h"
#include "../../GisGeometry/SpatialReferenceProj4/SpatialReferenceProj4.h"
#include "../GeometryDefinition.h"
#include "../Field.h"
#include "../../CommonLib/str/str.h"

namespace GraphEngine
{
    namespace GeoDatabase {

        CShapefileSpatialTable::CShapefileSpatialTable(const std::string& sPath, const std::string& sName, const std::string& sViewName) :
            TBase(sName,  sViewName),
            m_sPath(sPath)
        {
            m_sPath=  CShapefileUtils::NormalizePath(m_sPath);
            LoadShapeFile(false);
        }

        CShapefileSpatialTable::CShapefileSpatialTable(const std::string& sPath,const std::string& sName,  const std::string& sViewName, IFieldsPtr ptrFields):
                TBase(sName,  sViewName),
                m_sPath(sPath)
        {
            m_sPath=  CShapefileUtils::NormalizePath(m_sPath);
            CreateShapeFile(ptrFields);
        }


        CShapefileSpatialTable::~CShapefileSpatialTable()
        {

        }


        IRowPtr CShapefileSpatialTable::GetRow(int64_t id)
        {
            throw CommonLib::CExcBase("CShapefileSpatialTable::GetRow not implement");
        }

        ISelectCursorPtr	CShapefileSpatialTable::Select(const std::string& sqlSelectQuery)
        {
            throw CommonLib::CExcBase("CShapefileSpatialTable::Select with SelectQuery not implement");
        }

        ISelectCursorPtr	CShapefileSpatialTable::Search(IQueryFilterPtr ptrFilter)
        {
            try
            {
                 return  std::make_shared<CShapefileRowCursor>(ptrFilter, m_ptrShp, m_ptrDBfile, m_pFields, m_ptrSpatialReference );;
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("ShapefileSpatialTable failed  to search", exc);
                throw;
            }

        }


        void CShapefileSpatialTable::LoadShapeFile( bool write)
        {


            try
            {
                std::string filePathBase = m_sPath + m_sDatasetName;


                std::string  shpFilePath = filePathBase + ".shp";
                std::string  dbfFilePath = filePathBase + ".dbf";
                std::string  prjFileName = filePathBase + ".prj";

                const char* szAccess = write ? "r+b" : "rb";
                m_ptrShp = std::make_shared<CShapeFile>(shpFilePath.c_str(), szAccess);
                m_ptrDBfile = std::make_shared<CShapeDBFile>(shpFilePath.c_str(), szAccess);

                int objectCount;
                int shapeType;
                double minBounds[4];
                double maxBounds[4];
                m_ptrShp->GetInfo( &objectCount, &shapeType, &minBounds[0], &maxBounds[0]);
                CommonLib::bbox bounds;

                if(objectCount > 0)
                {
                    bounds.type = CommonLib::bbox_type_normal;
                    bounds.xMin = minBounds[0];
                    bounds.xMax = maxBounds[0];
                    bounds.yMin = minBounds[1];
                    bounds.yMax = maxBounds[1];
                    bounds.zMin = minBounds[2];
                    bounds.zMax = maxBounds[2];
                    bounds.mMin = minBounds[3];
                    bounds.mMax = maxBounds[3];
                }


                m_ptrSpatialReference = std::make_shared< Geometry::CSpatialReferenceProj4>(prjFileName, Geometry::eSPRefTypePRJFilePath);
                if(!m_ptrSpatialReference->IsValid())
                {
                    m_ptrSpatialReference = std::make_shared< Geometry::CSpatialReferenceProj4>(bounds);
                }

                m_ptrExtent = std::make_shared< Geometry::CEnvelope>(bounds, m_ptrSpatialReference);
                bool hasZ;
                bool hasM;
                m_ShapeType = CShapefileUtils::SHPTypeToGeometryType(shapeType, &hasZ, &hasM);
                IGeometryDefinitionPtr  ptrGeometryDef = std::make_shared<CGeometryDefinition>(m_ShapeType, hasZ, hasM);
                ptrGeometryDef->SetSpatialReference(m_ptrSpatialReference);
                ptrGeometryDef->SetBaseExtent(bounds);

                int fieldCount = m_ptrDBfile->GetFieldCount();
                for(int fieldNum = 0; fieldNum < fieldCount; ++fieldNum)
                {
                    char name[33];
                    int width;
                    int dec;
                   DBFFieldType shpFieldType = m_ptrDBfile->GetFieldInfo(fieldNum, name, &width, &dec);

                    eDataTypes fieldType;
                    int length;
                    int precision;
                    int scale;
                    fieldType = CShapefileUtils::SHPFieldInfoToFieldInfo(shpFieldType, width, dec, &length, &precision, &scale);

                    IFieldPtr pFieldPtr(new CField());
                    pFieldPtr->SetName(name);
                    pFieldPtr->SetIsEditable(true);
                    pFieldPtr->SetIsNullable(true);
                    pFieldPtr->SetIsRequired(false);
                    pFieldPtr->SetType(fieldType);
                    pFieldPtr->SetLength(length);
                    pFieldPtr->SetPrecision(precision);
                    pFieldPtr->SetScale(scale);
                    m_pFields->AddField(pFieldPtr);
                }

                m_pShapeField = std::make_shared<CField>();
                m_pShapeField->SetGeometryDef(ptrGeometryDef);
                m_pShapeField->SetType(dtGeometry);

                m_sShapeFieldName = "Shape";
                int i = 0;
                while(m_pFields->FieldExists(m_sShapeFieldName))
                    m_sShapeFieldName = CommonLib::str_format::AStrFormatSafeT("Shape%{0}", i++);

                m_pShapeField->SetName(m_sShapeFieldName);
                m_pFields->AddField(m_pShapeField);
                m_sShapeFieldName = m_pShapeField->GetName();
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to load shape file, path: {0}, name: {1}", m_sPath, m_sDatasetName, exc);
            }
        }

        void CShapefileSpatialTable::CreateShapeFile(IFieldsPtr ptrFields)
        {
            try
            {
                std::string filePathBase = m_sPath + m_sDatasetName;

                if(filePathBase.empty())
                    throw CommonLib::CExcBase("Invalid params, path: {0}, datasetname: {1}", m_sPath, m_sDatasetName);



                int shapeType = CommonLib::shape_type_null;
                int fieldCount = ptrFields->GetFieldCount();
                Geometry::ISpatialReferencePtr pSprefPtr;


                for(int i = 0; i < fieldCount; ++i)
                {
                    IFieldPtr pField = ptrFields->GetField(i);
                    switch(pField->GetType())
                    {
                        case dtInteger8:
                        case dtUInteger8:
                        case dtInteger16:
                        case dtUInteger16:
                        case dtInteger32:
                        case dtUInteger32:
                        case dtInteger64:
                        case dtUInteger64:
                        case dtString:
                        case dtDate:
                            break;

                        case dtGeometry:
                        {
                            ISpatialFieldPtr pShpField = std::static_pointer_cast<ISpatialField>(pField);
                            if(pShpField.get() == nullptr)
                                throw CommonLib::CExcBase("Wrong spatial field");

                            CommonLib::eShapeType gtype = pShpField->GetGeometryDef()->GetGeometryType();
                            if(gtype == CommonLib::shape_type_point )
                                shapeType = SHPT_MULTIPOINT;
                            else if(gtype == CommonLib::shape_type_polyline)
                            {
                                shapeType = SHPT_ARC;
                            }
                            if(gtype == CommonLib::shape_type_polygon)
                            {
                                shapeType = SHPT_POLYGON;
                            }
                            if(gtype == CommonLib::shape_type_multipatch)
                            {
                                shapeType = SHPT_MULTIPATCH;
                            }
                            pSprefPtr = pShpField->GetGeometryDef()->GetSpatialReference();
                        }
                        break;
                    }
                }

                if(shapeType ==  CommonLib::shape_type_null)
                    throw CommonLib::CExcBase("Geometry field not found");

                std::string  shpFilePath = filePathBase + ".shp";
                std::string  dbfFilePath = filePathBase + ".dbf";
                std::string  prjFileName = filePathBase + ".prj";


                m_ptrShp = std::make_shared<CShapeFile>(shpFilePath.c_str(), shapeType);
                m_ptrDBfile = std::make_shared<CShapeDBFile>(shpFilePath.c_str());

                if(pSprefPtr.get())
                {
                    FILE* pFile = fopen(prjFileName.c_str(), "wt");
                    fputs(pSprefPtr->GetProjectionString().c_str(), pFile);
                    fclose(pFile);
                }

                for(int i = 0; i < fieldCount; ++i)
                {
                    IFieldPtr pField = ptrFields->GetField(i);
                    switch(pField->GetType())
                    {
                        case dtInteger8:
                        case dtInteger16:
                        case dtInteger32:
                        case dtInteger64:
                        case dtUInteger8:
                        case dtUInteger16:
                        case dtUInteger32:
                        case dtUInteger64:
                            m_ptrDBfile->AddField(pField->GetName().c_str(), FTInteger, pField->GetPrecision(), pField->GetScale());
                            break;
                        case dtFloat:
                        case dtDouble:
                            m_ptrDBfile->AddField(pField->GetName().c_str(), FTDouble, pField->GetPrecision(), pField->GetScale());
                            break;
                        case dtString:
                            m_ptrDBfile->AddField(pField->GetName().c_str(), FTString, pField->GetLength(), 0);
                            break;
                        case dtDate:
                            m_ptrDBfile->AddField(pField->GetName().c_str(), FTDate, pField->GetLength(), 0);
                            break;
                       case dtGeometry:
                            break;
                        default:
                            throw CommonLib::CExcBase("Doesn't support type: {0}", (int)pField->GetType());
                    }
                }



            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to load shape file, path: {0}, name: {1}", m_sPath, m_sDatasetName, exc);
            }
        }

        void CShapefileSpatialTable::Save(CommonLib::ISerializeObjPtr pObj) const
        {
            try
            {

            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to save ShapefileSpatialTable", exc);
            }
        }

        void CShapefileSpatialTable::Load(CommonLib::ISerializeObjPtr pObj)
        {
            try
            {

            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to load ShapefileSpatialTable", exc);
            }
        }

    }
    }