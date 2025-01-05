#include "ShapeDBFile.h"

namespace GraphEngine
{
    namespace GeoDatabase {

        CShapeDBFile::CShapeDBFile( const char * pszDBFFile, const char * pszAccess)
        {
            m_dbFile = DBFOpen(pszDBFFile, pszAccess);
            if(!m_dbFile)
            {
                throw CommonLib::CExcBase("Failed to open shape db file, file name: {0}, params: {1}", pszDBFFile, pszAccess);
            }
        }

        CShapeDBFile::CShapeDBFile( const char * pszDBFFile)
        {
            m_dbFile = DBFCreate(pszDBFFile);
            if(!m_dbFile)
            {
                throw CommonLib::CExcBase("Failed to create shape db file, file name: {0}", pszDBFFile);
            }
        }

        CShapeDBFile::~CShapeDBFile()
        {
            if(m_dbFile)
            {
                DBFClose(m_dbFile);
                m_dbFile = 0;
            }
        }

        int CShapeDBFile::GetFieldCount()
        {
            return  DBFGetFieldCount(m_dbFile);
        }

        DBFFieldType CShapeDBFile::GetFieldInfo( int iField, char *pszFieldName, int *pnWidth, int *pnDecimals)
        {
            return  DBFGetFieldInfo(m_dbFile, iField, pszFieldName, pnWidth, pnDecimals);
        }

        int  CShapeDBFile::ReadIntegerAttribute( int iShape, int iField )
        {
            return DBFReadIntegerAttribute( m_dbFile, iShape,  iField );
        }

        double CShapeDBFile::ReadDoubleAttribute( int iShape, int iField )
        {
            return DBFReadDoubleAttribute(m_dbFile, iShape, iField);
        }

        const char * CShapeDBFile::ReadStringAttribute(int iShape, int iField )
        {
            return DBFReadStringAttribute(m_dbFile, iShape, iField);
        }

        void CShapeDBFile::AddField(const char *pszFieldName, DBFFieldType eType, int nWidth, int nDecimals)
        {
            if(DBFAddField(m_dbFile, pszFieldName, eType, nWidth, nDecimals) == -1)
               throw CommonLib::CExcBase("Failed to add field, fieldname: {0}", pszFieldName);
        }



    }
}