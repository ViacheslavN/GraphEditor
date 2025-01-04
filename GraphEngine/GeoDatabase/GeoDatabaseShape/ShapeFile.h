#pragma once
#include "../GeoDatabase.h"
#include "../../ThirdParty/ShapeLib/shapefil.h"
#include "SHPObject.h"
#include "ShapeTree.h"

namespace GraphEngine
{
    namespace GeoDatabase {

        typedef std::shared_ptr<class CShapeFile> CShapeFilePtr;

        class CShapeFile
        {
        public:
            CShapeFile(const char * pszShapeFile, const char * pszAccess); //Open
            CShapeFile(const char * pszShapeFile, int shapeType); //Create

            ~CShapeFile();

            void GetInfo( int * pnEntities, int * pnShapeType,  double * padfMinBound, double * padfMaxBound );
            CSHPObjectPtr ReadObject(int nRow);
            CShapeTreePtr  CreateTree();


        private:
            SHPHandle m_file;
        };


    }
    }
