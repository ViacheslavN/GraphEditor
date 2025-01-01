#include "SimpleFillSymbol.h"

namespace GraphEngine {
    namespace Display {

        CSimpleFillSymbol::CSimpleFillSymbol() : m_FillStyle(SimpleFillStyleSolid)
        {
            m_nSymbolID = SimpleFillSymbolID;
            m_ptrPen = std::make_shared<CPen>();

            m_ptrPen->SetPenType(PenTypeNull);
        }
        CSimpleFillSymbol::~CSimpleFillSymbol()
        {

        }

//ISimpleFillSymbol
        eSimpleFillStyle CSimpleFillSymbol::GetStyle() const
        {
            return m_FillStyle;
        }

        void CSimpleFillSymbol::SetStyle(eSimpleFillStyle style)
        {
            m_FillStyle = style;
            m_ptrBrush->SetType(FillStyle2BrushType(style));
            m_bDirty = true;
        }

        Color CSimpleFillSymbol::GetBackgroundColor() const
        {
            return m_ptrBrush->GetBgColor();
        }

        void  CSimpleFillSymbol::SetBackgroundColor( Color color )
        {
            m_ptrBrush->SetBgColor(color);
            m_bDirty = true;
        }

        void CSimpleFillSymbol::Prepare(IDisplayPtr ptrDisplay)
        {
            TSymbolBase::Prepare(ptrDisplay);

            if(m_ptrBrush->GetType() == BrushTypeTextured)
            {

                m_ptrBitmap = std::make_shared<CBitmap>(8, 8, BitmapFormatType32bppARGB);

                unsigned char* bits = m_ptrBitmap->Bits();
                for(int i = 0; i < 8; i++)
                {
                    for(int j = 0; j < 8; j++)
                    {
                        Color c = m_ptrBrush->GetBgColor();
                        switch(m_FillStyle)
                        {
                            case SimpleFillStyleHorizontal:
                                if(i == 0)
                                    c = m_ptrBrush->GetColor();
                                break;
                            case SimpleFillStyleVertical:
                                if(j == 0)
                                    c =  m_ptrBrush->GetColor();
                                break;
                            case SimpleFillStyleForwardDiagonal:
                                if(i == 7 - j)
                                    c =  m_ptrBrush->GetColor();
                                break;
                            case SimpleFillStyleBackwardDiagonal:
                                if(i == j)
                                    c = m_ptrBrush->GetColor();
                                break;
                            case SimpleFillStyleCross:
                                if(i == 0 || j == 0)
                                    c =  m_ptrBrush->GetColor();
                                break;
                            case SimpleFillStyleDiagonalCross:
                                if(i == 0 || i == 7 - j)
                                    c =  m_ptrBrush->GetColor();
                                break;
                        }

                        bits[i * 8 * 4 + j * 4] = c.GetB();
                        bits[i * 8 * 4 + j * 4 + 1] = c.GetG();;
                        bits[i * 8 * 4 + j * 4 + 2] = c.GetR();;
                        bits[i * 8 * 4 + j * 4 + 3] = c.GetA();;
                    }
                }

                m_ptrBrush->SetTexture(m_ptrBitmap);
            }
        }

        void  CSimpleFillSymbol::DrawGeometryEx(IDisplayPtr ptrDisplay, const GPoint* points, const int* polyCounts, int polyCount)
        {
            IGraphicsPtr pGraphics = ptrDisplay->GetGraphics();
            if(polyCount == 1)
                pGraphics->DrawPolygon(m_ptrPen, m_ptrBrush, points, polyCounts[0]);
            else
                pGraphics->DrawPolyPolygon(m_ptrPen, m_ptrBrush, points, polyCounts, (int)polyCount);

            DrawOutline(ptrDisplay, points, polyCounts, polyCount);
        }
        void CSimpleFillSymbol::FillRect(IDisplayPtr ptrDisplay, const Display::GRect& rect)
        {
            IGraphicsPtr pGraphics = ptrDisplay->GetGraphics();

            pGraphics->DrawRect(m_ptrPen, m_ptrBrush, rect);
        }
        void CSimpleFillSymbol::DrawDirectly(IDisplayPtr ptrDisplay, const GPoint* lpPoints, const int *lpPolyCounts, int nCount )
        {
            DrawGeometryEx(ptrDisplay, lpPoints, lpPolyCounts, nCount);
        }
        void  CSimpleFillSymbol::QueryBoundaryRectEx(IDisplayPtr ptrDisplay, const GPoint* points, const int* polyCounts, int polyCount,   GRect &rect) const
        {
            for(size_t part = 0, offset = 0; part < polyCount; part++)
                for(size_t p = offset; p < offset + (size_t)polyCounts[part]; p ++)
                    rect.ExpandRect(points[p]);

            ILineSymbolPtr pOutline = GetOutlineSymbol();
            if(pOutline.get())
            {
                GPoint testPoints[] = { GPoint(100, 100), GPoint(150, 100) };
                int size = 2;
                GRect r;
                pOutline->QueryBoundaryRectEx(ptrDisplay, testPoints, &size, 1, r);
                rect.xMin -= r.Height() / 2;
                rect.yMin -= r.Height() / 2;
                rect.xMax += r.Height() / 2;
                rect.yMax += r.Height() / 2;
            }
        }

        void CSimpleFillSymbol::Save(CommonLib::ISerializeObjPtr pObj) const
        {
            try
            {

                TSymbolBase::Save(pObj);
                pObj->AddPropertyInt16("Style", m_FillStyle);
                m_ptrPen->Save(pObj, "Pen");
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to save CSimpleFillSymbol", exc);
            }
        }

        void CSimpleFillSymbol::Load(CommonLib::ISerializeObjPtr pObj)
        {
            try
            {

                TSymbolBase::Save(pObj);
                m_FillStyle = (eSimpleFillStyle)pObj->GetPropertyInt16("Style", SimpleFillStyleNull);
                m_ptrPen->Load(pObj, "Pen");
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to load CSimpleFillSymbol", exc);
            }
        }

    }
}
