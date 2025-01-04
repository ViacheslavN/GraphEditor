#pragma once
#include "../GeoDatabase.h"
#include "../../ThirdParty/ShapeLib/shapefil.h"


namespace GraphEngine
{
    namespace GeoDatabase {

        typedef std::shared_ptr<class CShapeDBFile> CShapeDBFilePtr;

        class CShapeDBFile
        {
        public:
            CShapeDBFile( const char * pszDBFFile, const char * pszAccess); //Open
            CShapeDBFile( const char * pszDBFFile); //Create
            ~CShapeDBFile();

            int GetFieldCount();
            DBFFieldType GetFieldInfo( int iField, char *pszFieldName, int *pnWidth, int *pnDecimals);
            int  ReadIntegerAttribute( int iShape, int iField );
            double ReadDoubleAttribute( int iShape, int iField );
            const char * ReadStringAttribute(int iShape, int iField );
        private:
            DBFHandle m_dbFile;
        };

    }
    }