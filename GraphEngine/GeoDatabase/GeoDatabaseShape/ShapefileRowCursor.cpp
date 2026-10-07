#include "ShapefileRowCursor.h"
#include "../../CommonLib/SpatialData/GeoShape.h"
#include "ShapefileUtils.h"
#include "../Row.h"
#include "../Utils.h"

namespace GraphEngine
{
    namespace GeoDatabase {

        // Columns of the cursor are the table fields: [dbf fields..., shape field, OID field]
        CShapefileRowCursor::CShapefileRowCursor(IQueryFilterPtr ptrFilter,  CShapeFilePtr ptrShapeFile, CShapeDBFilePtr ptrDBFile,  IFieldsPtr pFields,  Geometry::ISpatialReferencePtr ptrSpatRefSource, const std::string& sOIDFieldName) :
                TBase(pFields, ptrFilter, ptrSpatRefSource),
                m_nCurrentRowID(0)
                , m_nCurrentRecord(-1)
                , m_nOidCol(-1)
                , m_nShapeCol(-1)
                , m_nDbfFieldCount(0)
                , m_ptrShapeFile(ptrShapeFile)
                , m_ptrDBFile(ptrDBFile)
                , m_ptrSpatialReference(ptrSpatRefSource)
        {
            m_nDbfFieldCount = m_ptrDBFile->GetFieldCount();
            if(!sOIDFieldName.empty())
                m_nOidCol = m_ptrFields->FindField(sOIDFieldName);

            for(int32_t i = 0, sz = m_ptrFields->GetFieldCount(); i < sz; ++i)
            {
                if(m_ptrFields->GetField(i)->GetType() == dtGeometry)
                {
                    m_nShapeCol = i;
                    break;
                }
            }

            if(m_ptrExtentSource.get() && (m_ptrExtentSource->GetBoundingBox().type & CommonLib::bbox_type_normal))
            {
                m_ptrShapeTree = m_ptrShapeFile->CreateTree();
                m_ptrShapeTree->GetTreeFindLikelyShapes(m_ptrExtentSource->GetBoundingBox(), m_vecOids);
                std::sort(m_vecOids.begin(), m_vecOids.end());
            }
            else
            {
                // no spatial restriction - all records
                int objectCount = 0;
                int shapeType = 0;
                double minBounds[4];
                double maxBounds[4];
                m_ptrShapeFile->GetInfo(&objectCount, &shapeType, &minBounds[0], &maxBounds[0]);
                m_vecOids.reserve(objectCount);
                for(int i = 0; i < objectCount; ++i)
                    m_vecOids.push_back(i);
            }
        }

        CShapefileRowCursor::~CShapefileRowCursor()
        {

        }

        bool CShapefileRowCursor::Next()
        {
            while(!EOC())
            {
                int32_t nRecord = (int32_t)m_vecOids[m_nCurrentRowID];
                ++m_nCurrentRowID;

                CSHPObjectPtr ptrObject =  m_ptrShapeFile->ReadObject(nRecord);
                if(!m_ptrGeoShapeCache.get())
                    m_ptrGeoShapeCache = std::make_shared<CommonLib::CGeoShape>();

                CShapefileUtils::SHPObjectToGeometry(ptrObject, m_ptrGeoShapeCache);
                if(AlterShape(m_ptrGeoShapeCache))
                {
                    m_nCurrentRecord = nRecord;
                    return true;
                }
            }

            return false;
        }


        bool CShapefileRowCursor::AlterShape(CommonLib::IGeoShapePtr pShape) const
        {
            if(!m_ptrExtentOutput.get())
                return true; // no spatial filter

            if(!pShape.get())
                return !(m_ptrExtentOutput->GetBoundingBox().type & CommonLib::bbox_type_normal);

            // shape is read in the source (table) spatial reference, the filter extent is in the output one
            if (m_bNeedTransform && m_ptrSpatialReference.get())
                m_ptrSpatialReference->Project(m_ptrExtentOutput->GetSpatialReference(), pShape);

            const CommonLib::bbox& boxShape = pShape->GetBB();
            const CommonLib::bbox boxOutput = m_ptrExtentOutput->GetBoundingBox();
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
            return m_nCurrentRowID >= (int32_t)m_vecOids.size();
        }

        void CShapefileRowCursor::CheckDbfColumn(int32_t col) const
        {
            if(m_nCurrentRecord < 0)
                throw CommonLib::CExcBase("CShapefileRowCursor: no current row, call Next first");

            if(!IsDbfColumn(col))
                throw CommonLib::CExcBase("CShapefileRowCursor: column {0} isn't attribute column", col);
        }

        bool CShapefileRowCursor::ColumnIsNull(int32_t col) const
        {
            if(IsOidColumn(col))
                return false;

            if(col == m_nShapeCol)
                return !m_ptrGeoShapeCache.get();

            CheckDbfColumn(col);
            return m_ptrDBFile->IsAttributeNull(m_nCurrentRecord, col);
        }

        int8_t CShapefileRowCursor::ReadInt8(int32_t col) const
        {
            return (int8_t)ReadInt64(col);
        }

        uint8_t CShapefileRowCursor::ReadUInt8(int32_t col) const
        {
            return (uint8_t)ReadInt64(col);
        }

        int16_t CShapefileRowCursor::ReadInt16(int32_t col) const
        {
            return (int16_t)ReadInt64(col);
        }

        uint16_t CShapefileRowCursor::ReadUInt16(int32_t col) const
        {
            return (uint16_t)ReadInt64(col);
        }

        int32_t CShapefileRowCursor::ReadInt32(int32_t col) const
        {
            return (int32_t)ReadInt64(col);
        }

        uint32_t CShapefileRowCursor::ReadUInt32(int32_t col) const
        {
            return (uint32_t)ReadInt64(col);
        }

        int64_t CShapefileRowCursor::ReadInt64(int32_t col) const
        {
            CheckIfFiledIsInteger(col);
            if(IsOidColumn(col))
                return m_nCurrentRecord;

            CheckDbfColumn(col);
            return (int64_t)m_ptrDBFile->ReadIntegerAttribute(m_nCurrentRecord, col);
        }

        uint64_t CShapefileRowCursor::ReadUInt64(int32_t col) const
        {
            return (uint64_t)ReadInt64(col);
        }

        float CShapefileRowCursor::ReadFloat(int32_t col) const
        {
            return  (float)ReadDouble(col);
        }

        double CShapefileRowCursor::ReadDouble(int32_t col) const
        {
            CheckIfFiledIsDouble(col);
            CheckDbfColumn(col);
            return  m_ptrDBFile->ReadDoubleAttribute(m_nCurrentRecord, col);
        }

        void CShapefileRowCursor::ReadText(int32_t col, std::string& text) const
        {
            text = ReadText(col);
        }

        std::string CShapefileRowCursor::ReadText(int32_t col) const
        {
            CheckIfFiledIsStringType(col);
            CheckDbfColumn(col);
            const char * pszData = m_ptrDBFile->ReadStringAttribute(m_nCurrentRecord, col);
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
            std::string  sResult = ReadText(col);
            return CommonLib::StringEncoding::str_utf82w_safe(sResult);
        }

        void CShapefileRowCursor::ReadBlob(int col, byte_t **pBuf, int32_t& size) const
        {
            throw CommonLib::CExcBase("CShapefileRowCursor doesn't support blob type");
        }

        CommonLib::IGeoShapePtr CShapefileRowCursor::ReadShape(int32_t col) const
        {
            if(col != m_nShapeCol)
                throw CommonLib::CExcBase("CShapefileRowCursor: column {0} isn't shape column", col);

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
