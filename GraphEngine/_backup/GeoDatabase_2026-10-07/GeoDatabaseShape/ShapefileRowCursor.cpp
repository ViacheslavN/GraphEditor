#include "ShapefileRowCursor.h"
#include "../../CommonLib/SpatialData/GeoShape.h"
#include "ShapefileUtils.h"
#include "../Row.h"
#include "../Utils.h"

namespace GraphEngine
{
    namespace GeoDatabase {
        CShapefileRowCursor::CShapefileRowCursor(IQueryFilterPtr ptrFilter,  CShapeFilePtr ptrShapeFile, CShapeDBFilePtr ptrDBFile,  IFieldsPtr pFields,  Geometry::ISpatialReferencePtr ptrSpatRefSource) :
                TBase(pFields, ptrFilter, ptrSpatRefSource),
                m_ptrShapeFile(ptrShapeFile)
                , m_ptrDBFile(ptrDBFile)
                , m_nCurrentRowID(0)
        {

            m_ptrShapeTree = m_ptrShapeFile->CreateTree();
            m_ptrShapeTree = m_ptrShapeFile->CreateTree();
            m_ptrShapeTree->GetTreeFindLikelyShapes(m_ptrExtentSource->GetBoundingBox(), m_vecOids);
            std::sort(m_vecOids.begin(), m_vecOids.end());


        }

        CShapefileRowCursor::~CShapefileRowCursor()
        {

        }



        bool CShapefileRowCursor::Next()
        {

            bool recordGood = false;
            while(!recordGood)
            {
                if(EOC())
                {
                    return false;
                }

                CSHPObjectPtr ptrObject =  m_ptrShapeFile->ReadObject((int)m_vecOids[m_nCurrentRowID]);
                if(!m_ptrGeoShapeCache.get())
                    m_ptrGeoShapeCache = std::make_shared<CommonLib::CGeoShape>();

                CShapefileUtils::SHPObjectToGeometry(ptrObject, m_ptrGeoShapeCache);
                if(AlterShape(m_ptrGeoShapeCache))
                {
                    recordGood = true;
                }

                ++m_nCurrentRowID;

            }
            return true;
        }


        bool CShapefileRowCursor::AlterShape(CommonLib::IGeoShapePtr pShape) const
        {
            if(!pShape.get())
                return !(m_ptrExtentOutput->GetBoundingBox().type & CommonLib::bbox_type_normal);

            if (m_bNeedTransform && pShape)
                m_ptrExtentOutput->GetSpatialReference()->Project(m_ptrExtentOutput->GetSpatialReference(), pShape);


            CommonLib::bbox& boxShape = pShape->GetBB();
            CommonLib::bbox& boxOutput = m_ptrExtentOutput->GetBoundingBox();
            if((boxShape.type & CommonLib::bbox_type_normal) && (boxOutput.type & CommonLib::bbox_type_normal))
            {
                if (boxShape.xMin > boxOutput.xMax || boxShape.xMax < boxOutput.xMin ||
                    boxShape.yMin > boxOutput.yMax || boxShape.yMax < boxOutput.yMin)
                {
                    return false;
                }
            }

            return true;
        }


        bool CShapefileRowCursor::EOC()
        {
            return m_nCurrentRowID >= m_vecOids.size();
        }

        bool CShapefileRowCursor::ColumnIsNull(int32_t col) const
        {
            return false;
        }

        int8_t CShapefileRowCursor::ReadInt8(int32_t col) const
        {
            CheckIfFiledIsInteger(col);
            return (int8_t)m_ptrDBFile->ReadIntegerAttribute(m_nCurrentRowID, col);
        }

        uint8_t CShapefileRowCursor::ReadUInt8(int32_t col) const
        {
            CheckIfFiledIsInteger(col);
            return (uint8_t)m_ptrDBFile->ReadIntegerAttribute(m_nCurrentRowID, col);
        }

        int16_t CShapefileRowCursor::ReadInt16(int32_t col) const
        {
            CheckIfFiledIsInteger(col);
            return (int16_t)m_ptrDBFile->ReadIntegerAttribute(m_nCurrentRowID, col);
        }

        uint16_t CShapefileRowCursor::ReadUInt16(int32_t col) const
        {
            CheckIfFiledIsInteger(col);
            return (uint16_t)m_ptrDBFile->ReadIntegerAttribute(m_nCurrentRowID, col);
        }

        int32_t CShapefileRowCursor::ReadInt32(int32_t col) const
        {
            CheckIfFiledIsInteger(col);
            return (int32_t)m_ptrDBFile->ReadIntegerAttribute(m_nCurrentRowID, col);
        }

        uint32_t CShapefileRowCursor::ReadUInt32(int32_t col) const
        {
            CheckIfFiledIsInteger(col);
            return (uint32_t)m_ptrDBFile->ReadIntegerAttribute(m_nCurrentRowID, col);
        }

        int64_t CShapefileRowCursor::ReadInt64(int32_t col) const
        {
            CheckIfFiledIsInteger(col);
            return (int64_t)m_ptrDBFile->ReadIntegerAttribute(m_nCurrentRowID, col);
        }

        uint64_t CShapefileRowCursor::ReadUInt64(int32_t col) const
        {
            CheckIfFiledIsInteger(col);
            return (uint64_t)m_ptrDBFile->ReadIntegerAttribute(m_nCurrentRowID, col);
        }

        float CShapefileRowCursor::ReadFloat(int32_t col) const
        {
            CheckIfFiledIsDouble(col);
            return  (float)m_ptrDBFile->ReadDoubleAttribute(m_nCurrentRowID, col);
        }

        double CShapefileRowCursor::ReadDouble(int32_t col) const
        {
            CheckIfFiledIsDouble(col);
            return  m_ptrDBFile->ReadDoubleAttribute(m_nCurrentRowID, col);
        }

        void CShapefileRowCursor::ReadText(int32_t col, std::string& text) const
        {
            text = ReadText(col);
        }

        std::string CShapefileRowCursor::ReadText(int32_t col) const
        {
            CheckIfFiledIsStringType(col);
            const char * pszData = m_ptrDBFile->ReadStringAttribute(m_nCurrentRowID, col);
            if(pszData == NULL)
                return  std::string();

            return  pszData;
        }

        void CShapefileRowCursor::ReadTextW(int32_t col, std::wstring& text) const
        {
            text = ReadTextW(col);
        }

        std::wstring CShapefileRowCursor::ReadTextW(int32_t col) const
        {
            CheckIfFiledIsStringType(col);
            std::string  sResult = ReadText(col);
            return CommonLib::StringEncoding::str_utf82w_safe(sResult);
        }

        void CShapefileRowCursor::ReadBlob(int col, byte_t **pBuf, int32_t& size) const
        {
            throw CommonLib::CExcBase("CShapefileRowCursor doesn't support blob type");
        }

        CommonLib::IGeoShapePtr CShapefileRowCursor::ReadShape(int32_t col) const
        {
            return m_ptrGeoShapeCache;
        }

        CommonLib::CGuid  CShapefileRowCursor::ReadGuid(int32_t col) const
        {
            try
            {
                std::string textGuid = ReadText(col);
                return  CommonLib::CGuid(textGuid);
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExcT("CShapefileRowCursor failed to read guid, cod: {0}", col , exc);
                throw;
            }
        }

        IRowPtr CShapefileRowCursor::CreateRow() const
        {
            return std::make_shared<CRow>(m_ptrFields);
        }
        void CShapefileRowCursor::FillRow(IRowPtr ptrRow) const
        {
            CGeoDatabaseUtils::FillRow((ISelectCursor*)this, ptrRow);
        }

    }
    }