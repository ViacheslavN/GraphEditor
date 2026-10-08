#include "LineTemplate.h"
#include "../DisplayUtils.h"
#include "../DisplayMath.h"
#include <cmath>

namespace GraphEngine {
    namespace Display {

        namespace
        {
            const int MaxMarksPerPart = 100000;   // protection against a tiny interval on a long line
        }

        CLineTemplate::CLineTemplate(double dInterval) : m_dInterval(dInterval), m_dDeviceInterval(0.)
        {

        }

        CLineTemplate::~CLineTemplate()
        {

        }

        void CLineTemplate::AddPatternElement(double dMark, double dGap)
        {
            SPatternElement element;
            element.dMark = dMark < 0. ? 0. : dMark;
            element.dGap = dGap < 0. ? 0. : dGap;
            m_vecElements.push_back(element);
        }

        void CLineTemplate::RemovePatternElement(int nIndex)
        {
            if(nIndex < 0 || nIndex >= (int)m_vecElements.size())
                throw CommonLib::CExcBase("LineTemplate: element index out of range: {0}", nIndex);

            m_vecElements.erase(m_vecElements.begin() + nIndex);
        }

        void CLineTemplate::ClearPatternElements()
        {
            m_vecElements.clear();
        }

        int CLineTemplate::GetPatternElementCount() const
        {
            return (int)m_vecElements.size();
        }

        void CLineTemplate::GetPatternElement(int nIndex, double* pMark, double* pGap) const
        {
            if(nIndex < 0 || nIndex >= (int)m_vecElements.size())
                throw CommonLib::CExcBase("LineTemplate: element index out of range: {0}", nIndex);

            if(pMark)
                *pMark = m_vecElements[nIndex].dMark;
            if(pGap)
                *pGap = m_vecElements[nIndex].dGap;
        }

        double CLineTemplate::GetInterval() const
        {
            return m_dInterval;
        }

        void CLineTemplate::SetInterval(double dInterval)
        {
            m_dInterval = dInterval;
        }

        void CLineTemplate::Prepare(IDisplayTransformationPtr ptrTrans, bool bScaleDependent)
        {
            m_dDeviceInterval = ptrTrans.get() ? (double)CDisplayUtils::SymbolSizeToDeviceSize(ptrTrans, m_dInterval, bScaleDependent) : m_dInterval;
        }

        bool CLineTemplate::IsValid() const
        {
            if(m_dDeviceInterval <= 0.)
                return false;

            if(m_vecElements.empty())
                return true;

            for(size_t i = 0; i < m_vecElements.size(); ++i)
            {
                if(m_vecElements[i].dMark + m_vecElements[i].dGap > 0.)
                    return true;
            }
            return false;
        }

        void CLineTemplate::ForEachMark(const GPoint* points, const int* polyCounts, int polyCount, GUnits dOffset, const TMarkFunc& func) const
        {
            if(!IsValid())
                return;

            for(int part = 0, offset = 0; part < polyCount; ++part)
            {
                ForEachMarkInPart(points + offset, polyCounts[part], dOffset, func);
                offset += polyCounts[part];
            }
        }

        void CLineTemplate::ForEachMarkInPart(const GPoint* points, int count, GUnits dOffset, const TMarkFunc& func) const
        {
            if(count < 2)
                return;

            std::vector<double> vecStations(count, 0.);  // distance from the part start to every vertex
            for(int i = 1; i < count; ++i)
                vecStations[i] = vecStations[i - 1] + CDisplayMath::CalcDistance(points[i - 1].x, points[i - 1].y, points[i].x, points[i].y);

            double dLength = vecStations[count - 1];
            if(dLength <= 0.)
                return;

            // the step must be at least a pixel, otherwise a zoomed out map draws forever
            std::vector<SPatternElement> vecElements = m_vecElements;
            if(vecElements.empty())
                vecElements.push_back(SPatternElement{1., 0.});

            double dPatternLength = 0.;
            for(size_t i = 0; i < vecElements.size(); ++i)
                dPatternLength += (vecElements[i].dMark + vecElements[i].dGap) * m_dDeviceInterval;
            if(dPatternLength < 1.)
                return;

            int nSegment = 0;
            double dStation = 0.;
            size_t nElement = 0;
            for(int nMarks = 0; nMarks < MaxMarksPerPart; ++nMarks)
            {
                const SPatternElement& element = vecElements[nElement];
                double dMark = element.dMark * m_dDeviceInterval;
                double dMiddle = dStation + dMark / 2.;
                if(dMiddle > dLength)
                    break;

                // segment that contains the middle of the mark (stations grow, the segment only moves forward)
                while(nSegment < count - 2 && vecStations[nSegment + 1] < dMiddle)
                    ++nSegment;

                double dSegLength = vecStations[nSegment + 1] - vecStations[nSegment];
                const GPoint& p0 = points[nSegment];
                const GPoint& p1 = points[nSegment + 1];
                double dx = p1.x - p0.x;
                double dy = p1.y - p0.y;

                if(dSegLength > 0.)
                {
                    double t = (dMiddle - vecStations[nSegment]) / dSegLength;
                    GPoint pt((GUnits)(p0.x + dx * t), (GUnits)(p0.y + dy * t));
                    if(dOffset != 0)
                    {
                        // left of the direction on the screen (y goes down): (dy, -dx)
                        pt.x += (GUnits)(dy / dSegLength * dOffset);
                        pt.y += (GUnits)(-dx / dSegLength * dOffset);
                    }
                    func(pt, RAD2DEG(atan2(dy, dx)));
                }

                dStation += dMark + element.dGap * m_dDeviceInterval;
                nElement = (nElement + 1) % vecElements.size();
            }
        }

        void CLineTemplate::Save(CommonLib::ISerializeObjPtr pObj, const std::string& name) const
        {
            CommonLib::ISerializeObjPtr ptrNode = pObj->CreateChildNode(name);
            ptrNode->AddPropertyDouble("Interval", m_dInterval);
            for(size_t i = 0; i < m_vecElements.size(); ++i)
            {
                CommonLib::ISerializeObjPtr ptrElement = ptrNode->CreateChildNode("Element");
                ptrElement->AddPropertyDouble("Mark", m_vecElements[i].dMark);
                ptrElement->AddPropertyDouble("Gap", m_vecElements[i].dGap);
            }
        }

        void CLineTemplate::Load(CommonLib::ISerializeObjPtr pObj, const std::string& name)
        {
            m_vecElements.clear();
            if(!pObj->IsChildExists(name))
                return;

            CommonLib::ISerializeObjPtr ptrNode = pObj->GetChild(name);
            m_dInterval = ptrNode->GetPropertyDouble("Interval", m_dInterval);

            std::vector<CommonLib::ISerializeObjPtr> vecElements = ptrNode->GetChilds("Element");
            for(size_t i = 0; i < vecElements.size(); ++i)
                AddPatternElement(vecElements[i]->GetPropertyDouble("Mark", 1.), vecElements[i]->GetPropertyDouble("Gap", 0.));
        }

    }
}
