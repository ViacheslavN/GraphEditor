#include "ShapeTree.h"
namespace GraphEngine
{
namespace GeoDatabase
{

    CShapeTree::CShapeTree(SHPTree* shpTree) : m_shpTree(shpTree)
    {

    }

    CShapeTree::~CShapeTree()
    {
        if(m_shpTree)
        {
            SHPDestroyTree(m_shpTree);
            m_shpTree = 0;
        }
    }

    void CShapeTree::GetTreeFindLikelyShapes(CommonLib::bbox& bbox, std::vector<int64_t>& oids )
    {

        double mapMin[2] = {bbox.xMin, bbox.yMin};
        double mapMax[2] = {bbox.xMax, bbox.yMax};


        int hitCount;
        int* ids = SHPTreeFindLikelyShapes(m_shpTree, mapMin, mapMax, &hitCount);

        for (int i = 0; i < hitCount; ++i)
            oids.push_back(ids[i]);

        if(ids != NULL)
            free(ids);
    }

}
}