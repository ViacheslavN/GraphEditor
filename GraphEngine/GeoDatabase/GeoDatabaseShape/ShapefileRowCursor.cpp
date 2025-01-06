#include "ShapefileRowCursor.h"
#include "ShapeRow.h"
#include "../../CommonLib/SpatialData/GeoShape.h"
#include "ShapefileUtils.h"

namespace GraphEngine
{
    namespace GeoDatabase {
        CShapefileRowCursor::CShapefileRowCursor(IQueryFilterPtr ptrFilter,  CShapeFilePtr ptrShapeFile, CShapeDBFilePtr ptrDBFile,  IFieldsPtr pSourceFields,  Geometry::ISpatialReferencePtr ptrSpatRefSource) :
                TBase(ptrFilter, ptrSpatRefSource),
                m_ptrShapeFile(ptrShapeFile)
                , m_ptrDBFile(ptrDBFile)
                , m_nCurrentRowID(0)
        {
            m_ptrFieldsSet = m_ptrFilter->GetFieldSet();
            m_ptrSourceFields = pSourceFields;
            m_ptrShapeTree = m_ptrShapeFile->CreateTree();
            m_ptrShapeTree = m_ptrShapeFile->CreateTree();
            m_ptrShapeTree->GetTreeFindLikelyShapes(m_ptrExtentSource->GetBoundingBox(), m_vecOids);
            std::sort(m_vecOids.begin(), m_vecOids.end());

            TBase::UpdateFields();



        }

        CShapefileRowCursor::~CShapefileRowCursor()
        {

        }

        ISelectRowPtr CShapefileRowCursor::GetCurrentRow()
        {
            return m_ptrCurrentRow;
        }


        bool CShapefileRowCursor::NextRow()
        {
             if(m_ptrCurrentRow.get() == nullptr)
                 m_ptrCurrentRow = std::make_shared<CShapeRow>(m_ptrDBFile, m_ptrShapeFile, m_ptrSourceFields);


            bool recordGood = false;

            while(!recordGood)
            {
                if(EOC())
                {
                    m_ptrCurrentRow.reset();
                    return false;
                }

                CSHPObjectPtr ptrObject =  m_ptrShapeFile->ReadObject((int)m_vecOids[m_nCurrentRowID]);
                if(!m_ptrGeoShapeCache.get())
                    m_ptrGeoShapeCache = std::make_shared<CommonLib::CGeoShape>();

                CShapefileUtils::SHPObjectToGeometry(ptrObject, m_ptrGeoShapeCache);
                if(AlterShape(m_ptrGeoShapeCache))
                {
                    CShapeRow* pShapeRow = (CShapeRow*)m_ptrCurrentRow.get();
                    pShapeRow->SetRow((int)m_vecOids[m_nCurrentRowID]);
                    pShapeRow->SetShape(m_ptrGeoShapeCache);
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


    }
    }