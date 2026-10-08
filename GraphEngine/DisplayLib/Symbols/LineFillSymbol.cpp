#include "LineFillSymbol.h"
#include "SimpleLineSymbol.h"
#include "SymbolBitmapUtils.h"
#include "../DisplayUtils.h"
#include "../DisplayMath.h"
#include <cmath>

namespace GraphEngine {
    namespace Display {

        namespace
        {
            const int MaxFillLines = 20000;
        }

        CLineFillSymbol::CLineFillSymbol() : m_dAngle(45.), m_dOffset(0.), m_dSeparation(2.), m_dDeviceSeparation(0.), m_dDeviceOffset(0.)
        {
            m_nSymbolID = LineFillSymbolID;
            m_ptrLine = std::make_shared<CSimpleLineSymbol>(Color(0, 0, 0), 1., SimpleLineStyleSolid);
        }

        CLineFillSymbol::CLineFillSymbol(ILineSymbolPtr ptrLine, double dAngle, double dSeparation) :
            m_ptrLine(ptrLine), m_dAngle(dAngle), m_dOffset(0.), m_dSeparation(dSeparation), m_dDeviceSeparation(0.), m_dDeviceOffset(0.)
        {
            m_nSymbolID = LineFillSymbolID;
        }

        CLineFillSymbol::~CLineFillSymbol()
        {

        }

        Color CLineFillSymbol::GetColor() const
        {
            return m_ptrLine.get() ? m_ptrLine->GetColor() : Color();
        }

        void CLineFillSymbol::SetColor(const Color &color)
        {
            if(m_ptrLine.get())
                m_ptrLine->SetColor(color);
            m_bDirty = true;
        }

        ILineSymbolPtr CLineFillSymbol::GetLineSymbol() const
        {
            return m_ptrLine;
        }

        void CLineFillSymbol::SetLineSymbol(ILineSymbolPtr ptrLine)
        {
            m_ptrLine = ptrLine;
            m_bDirty = true;
        }

        double CLineFillSymbol::GetAngle() const
        {
            return m_dAngle;
        }

        void CLineFillSymbol::SetAngle(double dAngle)
        {
            m_dAngle = dAngle;
            m_bDirty = true;
        }

        double CLineFillSymbol::GetOffset() const
        {
            return m_dOffset;
        }

        void CLineFillSymbol::SetOffset(double dOffset)
        {
            m_dOffset = dOffset;
            m_bDirty = true;
        }

        double CLineFillSymbol::GetSeparation() const
        {
            return m_dSeparation;
        }

        void CLineFillSymbol::SetSeparation(double dSeparation)
        {
            m_dSeparation = dSeparation;
            m_bDirty = true;
        }

        bool CLineFillSymbol::CanDraw(CommonLib::IGeoShapePtr ptrShape) const
        {
            if(!ptrShape.get() || ptrShape->GetPointCnt() < 3)
                return false;
            return m_ptrLine.get() != nullptr || m_ptrBorderSymbol.get() != nullptr;
        }

        void CLineFillSymbol::Prepare(IDisplayPtr ptrDisplay)
        {
            TBase::Prepare(ptrDisplay);
            IDisplayTransformationPtr ptrTrans = ptrDisplay->GetTransformation();
            m_dDeviceSeparation = CDisplayUtils::SymbolSizeToDeviceSize(ptrTrans, m_dSeparation, GetScaleDependent());
            m_dDeviceOffset = CDisplayUtils::SymbolSizeToDeviceSize(ptrTrans, m_dOffset, GetScaleDependent());
            if(m_dDeviceSeparation > 0.)
                m_dDeviceOffset = fmod(m_dDeviceOffset, m_dDeviceSeparation);  // -separation < offset < separation

            if(m_ptrLine.get())
                m_ptrLine->Prepare(ptrDisplay);
            if(m_ptrBorderSymbol.get())
                m_ptrBorderSymbol->Prepare(ptrDisplay);
        }

        void CLineFillSymbol::Reset()
        {
            if(m_ptrLine.get())
                m_ptrLine->Reset();
            if(m_ptrBorderSymbol.get())
                m_ptrBorderSymbol->Reset();
        }

        void CLineFillSymbol::DrawLines(IDisplayPtr ptrDisplay, const GRect& bounds)
        {
            if(!m_ptrLine.get() || m_dDeviceSeparation < 1.)
                return;

            // device y goes down: the counter clockwise direction is (cos, -sin), the normal across the lines (sin, cos)
            double dRad = DEG2RAD(m_dAngle);
            double dirX = cos(dRad), dirY = -sin(dRad);
            double nX = sin(dRad), nY = cos(dRad);

            GPoint origin = GetPatternOrigin(ptrDisplay);
            double corners[4][2] = {{bounds.xMin, bounds.yMin}, {bounds.xMax, bounds.yMin}, {bounds.xMax, bounds.yMax}, {bounds.xMin, bounds.yMax}};

            double dMin = 0., dMax = 0., tMin = 0., tMax = 0.;
            for(int i = 0; i < 4; ++i)
            {
                double px = corners[i][0] - origin.x;
                double py = corners[i][1] - origin.y;
                double d = px * nX + py * nY;
                double t = px * dirX + py * dirY;
                if(i == 0 || d < dMin) dMin = d;
                if(i == 0 || d > dMax) dMax = d;
                if(i == 0 || t < tMin) tMin = t;
                if(i == 0 || t > tMax) tMax = t;
            }

            double kStart = ceil((dMin - m_dDeviceOffset) / m_dDeviceSeparation);
            double kEnd = floor((dMax - m_dDeviceOffset) / m_dDeviceSeparation);
            if(kEnd - kStart > MaxFillLines)
                return;

            for(double k = kStart; k <= kEnd; k += 1.)
            {
                double d = m_dDeviceOffset + k * m_dDeviceSeparation;
                double bx = origin.x + nX * d;
                double by = origin.y + nY * d;

                GPoint line[2];
                line[0] = GPoint((GUnits)(bx + dirX * tMin), (GUnits)(by + dirY * tMin));
                line[1] = GPoint((GUnits)(bx + dirX * tMax), (GUnits)(by + dirY * tMax));
                int nCount = 2;
                m_ptrLine->DrawGeometryEx(ptrDisplay, line, &nCount, 1);
            }
        }

        void CLineFillSymbol::DrawGeometryEx(IDisplayPtr ptrDisplay, const GPoint* points, const int* polyCounts, int polyCount)
        {
            GRect bounds;
            if(!GetPointsBounds(points, polyCounts, polyCount, bounds))
                return;

            // only the visible part, a big polygon on a big scale may be much larger than the window
            GRect deviceRect = ptrDisplay->GetTransformation()->GetDeviceRect();
            if(bounds.xMin < deviceRect.xMin) bounds.xMin = deviceRect.xMin;
            if(bounds.yMin < deviceRect.yMin) bounds.yMin = deviceRect.yMin;
            if(bounds.xMax > deviceRect.xMax) bounds.xMax = deviceRect.xMax;
            if(bounds.yMax > deviceRect.yMax) bounds.yMax = deviceRect.yMax;

            if(bounds.xMin < bounds.xMax && bounds.yMin < bounds.yMax)
            {
                CPolygonClipGuard clip(ptrDisplay->GetGraphics(), points, polyCounts, polyCount);
                DrawLines(ptrDisplay, bounds);
            }

            DrawOutline(ptrDisplay, points, polyCounts, polyCount);
        }

        void CLineFillSymbol::DrawDirectly(IDisplayPtr ptrDisplay, const GPoint* lpPoints, const int *lpPolyCounts, int nCount)
        {
            DrawGeometryEx(ptrDisplay, lpPoints, lpPolyCounts, nCount);
        }

        void CLineFillSymbol::FillRect(IDisplayPtr ptrDisplay, const GRect& rect)
        {
            GPoint points[5] = {GPoint(rect.xMin, rect.yMin), GPoint(rect.xMax, rect.yMin), GPoint(rect.xMax, rect.yMax),
                                GPoint(rect.xMin, rect.yMax), GPoint(rect.xMin, rect.yMin)};
            int nCount = 5;
            DrawGeometryEx(ptrDisplay, points, &nCount, 1);
        }

        void CLineFillSymbol::QueryBoundaryRectEx(IDisplayPtr ptrDisplay, const GPoint* points, const int* polyCounts, int polyCount, GRect &rect) const
        {
            GRect bounds;
            if(GetPointsBounds(points, polyCounts, polyCount, bounds))
                rect.ExpandRect(bounds);

            if(m_ptrBorderSymbol.get())
                m_ptrBorderSymbol->QueryBoundaryRectEx(ptrDisplay, points, polyCounts, polyCount, rect);
        }

        void CLineFillSymbol::Save(CommonLib::ISerializeObjPtr pObj) const
        {
            try
            {
                TBase::Save(pObj);
                pObj->AddPropertyDouble("Angle", m_dAngle);
                pObj->AddPropertyDouble("Offset", m_dOffset);
                pObj->AddPropertyDouble("Separation", m_dSeparation);
                if(m_ptrLine.get())
                    m_ptrLine->Save(pObj->CreateChildNode("LineSymbol"));
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to save CLineFillSymbol", exc);
            }
        }

        void CLineFillSymbol::Load(CommonLib::ISerializeObjPtr pObj)
        {
            try
            {
                TBase::Load(pObj);
                m_dAngle = pObj->GetPropertyDouble("Angle", m_dAngle);
                m_dOffset = pObj->GetPropertyDouble("Offset", m_dOffset);
                m_dSeparation = pObj->GetPropertyDouble("Separation", m_dSeparation);

                m_ptrLine.reset();
                if(pObj->IsChildExists("LineSymbol"))
                    m_ptrLine = std::dynamic_pointer_cast<ILineSymbol>(CSymbolsLoader::LoadSymbol(pObj->GetChild("LineSymbol")));
                m_bDirty = true;
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to load CLineFillSymbol", exc);
            }
        }

    }
}
