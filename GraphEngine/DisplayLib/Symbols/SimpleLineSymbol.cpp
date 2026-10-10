#include "SimpleLineSymbol.h"
#include "../DisplayUtils.h"

namespace GraphEngine {
    namespace Display {

        CSimpleLineSymbol::CSimpleLineSymbol()
        {
            m_ptrPen = std::make_shared<CPen>();
            m_nSymbolID = SimpleLineSymbolID;
        }

        CSimpleLineSymbol::CSimpleLineSymbol( const Color &color, double width, eSimpleLineStyle style)
        {
            m_ptrPen = std::make_shared<CPen>();
            m_ptrPen->SetColor(color);
            m_ptrPen->SetWidth(width);
            m_ptrPen->SetPenType(LineStyle2PenType( style));

            m_nSymbolID = SimpleLineSymbolID;
        }

        CSimpleLineSymbol::~CSimpleLineSymbol()
        {

        }
        void CSimpleLineSymbol::DrawDirectly(IDisplayPtr ptrDisplay, const GPoint* lpPoints, const int *lpPolyCounts, int nCount )
        {
            DrawGeometryEx(ptrDisplay, lpPoints, lpPolyCounts, nCount);
        }

//CSymbol
        void  CSimpleLineSymbol::DrawGeometryEx(IDisplayPtr ptrDisplay, const GPoint* points, const int* polyCounts, int polyCount)
        {
            IGraphicsPtr pGraphics = ptrDisplay->GetGraphics();
            for(int idx = 0, offset = 0; idx < (int)polyCount; ++idx)
            {
                pGraphics->DrawLine(DrawPen(), points + offset, polyCounts[idx]);
                offset += polyCounts[idx];
            }

        }
        void  CSimpleLineSymbol::QueryBoundaryRectEx(IDisplayPtr ptrDisplay, const GPoint* points, const int* polyCounts, int polyCount,  GRect &rect) const
        {
            for(size_t part = 0, offset = 0; part < polyCount; part++)
                for(size_t p = offset; p < offset + (size_t)polyCounts[part]; p ++)
                    rect.ExpandRect(points[p]);

            // TODO: more accuracy calculation of boudary rect
            const GUnits halfWidth = DrawPen()->GetWidth() / 2;
            rect.xMin -= halfWidth;
            rect.yMin -= halfWidth;
            rect.xMax += halfWidth;
            rect.yMax += halfWidth;
        }

        void CSimpleLineSymbol::Prepare(IDisplayPtr ptrDisplay)
        {
            m_ptrDevicePen.reset();
            if(!GetScaleDependent() || !ptrDisplay.get())
                return;

            IDisplayTransformationPtr ptrTrans = ptrDisplay->GetTransformation();
            if(!ptrTrans.get() || ptrTrans->GetScale() <= 0. || !ptrTrans->UseReferenceScale())
                return;

            // the width is in pixels at the reference scale
            const double dFactor = ptrTrans->GetReferenceScale() / ptrTrans->GetScale();
            if(CDisplayMath::Equals(dFactor, 1.))
                return;

            m_ptrDevicePen = std::make_shared<CPen>(*m_ptrPen);
            m_ptrDevicePen->SetWidth((GUnits)(m_ptrPen->GetWidth() * dFactor));
            if(!m_ptrPen->GetTemplates().empty())
            {
                m_ptrDevicePen->ClearTmplates();
                for(const auto& dash : m_ptrPen->GetTemplates())
                    m_ptrDevicePen->AddTemplate((GUnits)(dash.first * dFactor), (GUnits)(dash.second * dFactor));
            }
        }

        void CSimpleLineSymbol::Changed()
        {
            m_ptrDevicePen.reset();   // made again by Prepare
            m_bDirty = true;
        }

//ILineSymbol
        Color  CSimpleLineSymbol::GetColor() const
        {
            return m_ptrPen->GetColor();
        }
        void   CSimpleLineSymbol::SetColor(const Color &color)
        {
            m_ptrPen->SetColor(color);
            Changed();
        }
        double CSimpleLineSymbol::GetWidth() const
        {
            return m_ptrPen->GetWidth();
        }
        void   CSimpleLineSymbol::SetWidth(double width)
        {
            m_ptrPen->SetWidth(width);
            Changed();
        }

//ISimpleLineSymbol
        ePenType	CSimpleLineSymbol::GetStyle() const
        {
            return m_ptrPen->GetPenType();
        }
        void  CSimpleLineSymbol::SetStyle( ePenType style )
        {
            m_ptrPen->SetPenType(style);
            Changed();
        }
        void CSimpleLineSymbol::AddDash(double dDash, double dGap)
        {
            m_ptrPen->AddTemplate((GUnits)dDash, (GUnits)dGap);
            Changed();
        }

        void CSimpleLineSymbol::ClearDashes()
        {
            m_ptrPen->ClearTmplates();
            Changed();
        }

        const TPenTemplates& CSimpleLineSymbol::GetDashes() const
        {
            return m_ptrPen->GetTemplates();
        }

        eCapType  CSimpleLineSymbol::GetCapType() const
        {
            return m_ptrPen->GetCapType();
        }
        void  CSimpleLineSymbol::SetCapType( eCapType cap )
        {
            m_ptrPen->SetCapType(cap);
            Changed();
        }
        eJoinType    CSimpleLineSymbol::GetJoinType() const
        {
            return m_ptrPen->GetJoinType();
        }
        void  CSimpleLineSymbol::SetJoinType( eJoinType join )
        {
            m_ptrPen->SetJoinType(join);
            Changed();
        }

        //ISerialize

        void CSimpleLineSymbol::Save(CommonLib::ISerializeObjPtr pObj) const
        {
            try
            {

                TBase::Save(pObj);
                m_ptrPen->Save(pObj, "Pen");
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to save CSimpleLineSymbol", exc);
            }
        }

        void CSimpleLineSymbol::Load(CommonLib::ISerializeObjPtr pObj)
        {
            try
            {
                TBase::Load(pObj);
                m_ptrPen->Load(pObj, "Pen");
                Changed();
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to load CSimpleLineSymbol", exc);
            }
        }
    }
}