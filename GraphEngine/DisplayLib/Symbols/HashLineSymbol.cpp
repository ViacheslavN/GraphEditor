#include "HashLineSymbol.h"
#include "SimpleLineSymbol.h"
#include "SymbolsLoader.h"
#include "../DisplayMath.h"
#include <cmath>

namespace GraphEngine {
    namespace Display {

        CHashLineSymbol::CHashLineSymbol() : m_dAngle(90.), m_dWidth(2.), m_dDeviceWidth(0.)
        {
            m_nSymbolID = HashLineSymbolID;
            m_ptrHashSymbol = std::make_shared<CSimpleLineSymbol>(Color(0, 0, 0), 1., SimpleLineStyleSolid);
        }

        CHashLineSymbol::CHashLineSymbol(ILineSymbolPtr ptrHashSymbol, double dWidth, double dInterval) :
            m_ptrHashSymbol(ptrHashSymbol), m_dAngle(90.), m_dWidth(dWidth), m_dDeviceWidth(0.)
        {
            m_nSymbolID = HashLineSymbolID;
            m_ptrTemplate->SetInterval(dInterval);
        }

        CHashLineSymbol::~CHashLineSymbol()
        {

        }

        Color CHashLineSymbol::GetColor() const
        {
            return m_ptrHashSymbol.get() ? m_ptrHashSymbol->GetColor() : Color();
        }

        void CHashLineSymbol::SetColor(const Color &color)
        {
            if(m_ptrHashSymbol.get())
                m_ptrHashSymbol->SetColor(color);
            m_bDirty = true;
        }

        double CHashLineSymbol::GetWidth() const
        {
            return m_dWidth;
        }

        void CHashLineSymbol::SetWidth(double dWidth)
        {
            m_dWidth = dWidth;
            m_bDirty = true;
        }

        double CHashLineSymbol::GetAngle() const
        {
            return m_dAngle;
        }

        void CHashLineSymbol::SetAngle(double dAngle)
        {
            m_dAngle = dAngle;
            m_bDirty = true;
        }

        ILineSymbolPtr CHashLineSymbol::GetHashSymbol() const
        {
            return m_ptrHashSymbol;
        }

        void CHashLineSymbol::SetHashSymbol(ILineSymbolPtr ptrSymbol)
        {
            m_ptrHashSymbol = ptrSymbol;
            m_bDirty = true;
        }

        bool CHashLineSymbol::CanDraw(CommonLib::IGeoShapePtr ptrShape) const
        {
            if(!m_ptrHashSymbol.get() || m_dWidth <= 0.)
                return false;
            if(m_ptrHashSymbol->GetColor().GetA() == Color::Transparent)
                return false;
            return TBase::CanDraw(ptrShape);
        }

        void CHashLineSymbol::Prepare(IDisplayPtr ptrDisplay)
        {
            TBase::Prepare(ptrDisplay);
            m_dDeviceWidth = CDisplayUtils::SymbolSizeToDeviceSize(ptrDisplay->GetTransformation(), m_dWidth, GetScaleDependent());
            if(m_ptrHashSymbol.get())
                m_ptrHashSymbol->Prepare(ptrDisplay);
        }

        void CHashLineSymbol::Reset()
        {
            if(m_ptrHashSymbol.get())
                m_ptrHashSymbol->Reset();
        }

        void CHashLineSymbol::DrawGeometryEx(IDisplayPtr ptrDisplay, const GPoint* points, const int* polyCounts, int polyCount)
        {
            if(!m_ptrHashSymbol.get() || !m_ptrTemplate.get() || m_dDeviceWidth <= 0)
                return;

            ILineSymbolPtr ptrHash = m_ptrHashSymbol;
            double dHalf = m_dDeviceWidth / 2.;
            double dAngle = m_dAngle;

            m_ptrTemplate->ForEachMark(points, polyCounts, polyCount, m_dDeviceOffset,
                [&](const GPoint& pt, double dLineAngle)
                {
                    // hash angle is measured from the line direction counter clockwise (as in UniGIS),
                    // device y goes down - the visual counter clockwise rotation is minus
                    double dRad = DEG2RAD(dLineAngle - dAngle);
                    double c = cos(dRad) * dHalf;
                    double s = sin(dRad) * dHalf;

                    GPoint hash[2];
                    hash[0] = GPoint((GUnits)(pt.x - c), (GUnits)(pt.y - s));
                    hash[1] = GPoint((GUnits)(pt.x + c), (GUnits)(pt.y + s));
                    int nCount = 2;
                    ptrHash->DrawGeometryEx(ptrDisplay, hash, &nCount, 1);
                });
        }

        void CHashLineSymbol::QueryBoundaryRectEx(IDisplayPtr ptrDisplay, const GPoint* points, const int* polyCounts, int polyCount, GRect &rect) const
        {
            int nTotal = 0;
            for(int part = 0; part < polyCount; ++part)
                nTotal += polyCounts[part];

            for(int i = 0; i < nTotal; ++i)
                rect.ExpandRect(points[i]);

            GUnits dExpand = m_dDeviceWidth / 2 + (m_dDeviceOffset < 0 ? -m_dDeviceOffset : m_dDeviceOffset);
            rect.xMin -= dExpand;
            rect.yMin -= dExpand;
            rect.xMax += dExpand;
            rect.yMax += dExpand;
        }

        void CHashLineSymbol::Save(CommonLib::ISerializeObjPtr pObj) const
        {
            try
            {
                TBase::Save(pObj);
                pObj->AddPropertyDouble("Angle", m_dAngle);
                pObj->AddPropertyDouble("Width", m_dWidth);
                if(m_ptrHashSymbol.get())
                    m_ptrHashSymbol->Save(pObj->CreateChildNode("HashSymbol"));
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to save CHashLineSymbol", exc);
            }
        }

        void CHashLineSymbol::Load(CommonLib::ISerializeObjPtr pObj)
        {
            try
            {
                TBase::Load(pObj);
                m_dAngle = pObj->GetPropertyDouble("Angle", m_dAngle);
                m_dWidth = pObj->GetPropertyDouble("Width", m_dWidth);

                m_ptrHashSymbol.reset();
                if(pObj->IsChildExists("HashSymbol"))
                    m_ptrHashSymbol = std::dynamic_pointer_cast<ILineSymbol>(CSymbolsLoader::LoadSymbol(pObj->GetChild("HashSymbol")));
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to load CHashLineSymbol", exc);
            }
        }

    }
}
