#include "SimpleLineSymbol.h"

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

        }

//CSymbol
        void  CSimpleLineSymbol::DrawGeometryEx(IDisplayPtr ptrDisplay, const GPoint* points, const int* polyCounts, int polyCount)
        {
            IGraphicsPtr pGraphics = ptrDisplay->GetGraphics();
            for(int idx = 0, offset = 0; idx < (int)polyCount; ++idx)
            {
                pGraphics->DrawLine(m_ptrPen, points + offset, polyCounts[idx]);
                offset += polyCounts[idx];
            }

        }
        void  CSimpleLineSymbol::QueryBoundaryRectEx(IDisplayPtr ptrDisplay, const GPoint* points, const int* polyCounts, int polyCount,  GRect &rect) const
        {
            for(size_t part = 0, offset = 0; part < polyCount; part++)
                for(size_t p = offset; p < offset + (size_t)polyCounts[part]; p ++)
                    rect.ExpandRect(points[p]);

            // TODO: more accuracy calculation of boudary rect
            rect.xMin -= m_ptrPen->GetWidth() / 2;
            rect.yMin -= m_ptrPen->GetWidth() / 2;
            rect.xMax += m_ptrPen->GetWidth()  / 2;
            rect.yMax += m_ptrPen->GetWidth()  / 2;
        }

//ILineSymbol
        Color  CSimpleLineSymbol::GetColor() const
        {
            return m_ptrPen->GetColor();
        }
        void   CSimpleLineSymbol::SetColor(const Color &color)
        {
            m_ptrPen->SetColor(color);
        }
        double CSimpleLineSymbol::GetWidth() const
        {
            return m_ptrPen->GetWidth();
        }
        void   CSimpleLineSymbol::SetWidth(double width)
        {
            m_ptrPen->SetWidth(width);
        }

//ISimpleLineSymbol
        ePenType	CSimpleLineSymbol::GetStyle() const
        {
            return m_ptrPen->GetPenType();
        }
        void  CSimpleLineSymbol::SetStyle( ePenType style )
        {
            m_ptrPen->SetPenType(style);
        }
        eCapType  CSimpleLineSymbol::GetCapType() const
        {
            return m_ptrPen->GetCapType();
        }
        void  CSimpleLineSymbol::SetCapType( eCapType cap )
        {
            m_ptrPen->SetCapType(cap);
        }
        eJoinType    CSimpleLineSymbol::GetJoinType() const
        {
            return m_ptrPen->GetJoinType();
        }
        void  CSimpleLineSymbol::SetJoinType( eJoinType join )
        {
            return m_ptrPen->SetJoinType(join);
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
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to load CSimpleLineSymbol", exc);
            }
        }
    }
}