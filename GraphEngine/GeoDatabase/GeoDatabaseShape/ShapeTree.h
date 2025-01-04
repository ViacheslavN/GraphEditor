#pragma once
#include "../GeoDatabase.h"
#include "../../ThirdParty/ShapeLib/shapefil.h"
#

namespace GraphEngine
{
    namespace GeoDatabase {

        typedef  std::shared_ptr<class CShapeTree> CShapeTreePtr;

        class CShapeTree
        {
        public:
            CShapeTree(SHPTree* shpTree);
            ~CShapeTree();

           void GetTreeFindLikelyShapes(CommonLib::bbox& bbox,  std::vector<int64_t>& oids );

        private:
            SHPTree* m_shpTree;
        };

    }
    }