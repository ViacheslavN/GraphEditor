#include "ShapeFile.h"


namespace GraphEngine
{
    namespace GeoDatabase {

        CShapeFile::CShapeFile(const char * pszShapeFile, const char * pszAccess)
        {
            m_file = SHPOpen(pszShapeFile, pszAccess);
            if(!m_file)
            {
                throw CommonLib::CExcBase("Failed to open shapefile, file name: {0}, params: {1}", pszShapeFile, pszAccess);
            }

        }

        CShapeFile::CShapeFile(const char * pszShapeFile, int shapeType)
        {
            m_file = SHPCreate(pszShapeFile, shapeType);
            if(!m_file)
            {
                throw CommonLib::CExcBase("Failed to create shapefile, file name: {0}, params: {1}", pszShapeFile, shapeType);
            }
        }

        CShapeFile::~CShapeFile()
        {
            if(m_file != 0)
                SHPClose(m_file);

            m_file = 0;
        }

        void CShapeFile::GetInfo( int * pnEntities, int * pnShapeType,  double * padfMinBound, double * padfMaxBound )
        {
            SHPGetInfo(m_file, pnEntities, pnShapeType,   padfMinBound,  padfMaxBound);
        }

        CSHPObjectPtr CShapeFile::ReadObject(int nRow)
        {
            SHPObject* pCacheObject = SHPReadObject(m_file, nRow);
            if(pCacheObject == nullptr)
                throw CommonLib::CExcBase("CShapeFile: Failed to read object, row: {1}", nRow);

            return std::make_shared<CSHPObject>(pCacheObject);

        }

        CShapeTreePtr  CShapeFile::CreateTree()
        {
            SHPTree* shpTree = SHPCreateTree(m_file, 2, 10, nullptr, nullptr);
            if(shpTree == nullptr)
                throw CommonLib::CExcBase("CShapeFile: Failed to create shape tree");

            SHPTreeTrimExtraNodes(shpTree);

            return  std::make_shared<CShapeTree>(shpTree );
        }


    }
    }