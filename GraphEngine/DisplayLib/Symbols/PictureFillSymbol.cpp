#include "PictureFillSymbol.h"
#include "SymbolBitmapUtils.h"

namespace GraphEngine {
    namespace Display {

        CPictureFillSymbol::CPictureFillSymbol() :
            m_bgColor(Color::White, Color::Transparent), m_transparencyColor(Color::Black, Color::Transparent), m_bBitmapDirty(true)
        {
            m_nSymbolID = PictureFillSymbolID;
            m_ptrNullPen = std::make_shared<CPen>(true);
            m_ptrBrush->SetType(BrushTypeTextured);
            m_ptrBrush->SetColor(Color(0, 0, 0));
        }

        CPictureFillSymbol::CPictureFillSymbol(BitmapPtr ptrBitmap) : CPictureFillSymbol()
        {
            m_ptrBitmap = ptrBitmap;
        }

        CPictureFillSymbol::~CPictureFillSymbol()
        {

        }

        void CPictureFillSymbol::SetBitmapDirty()
        {
            m_bBitmapDirty = true;
            m_bDirty = true;
        }

        void CPictureFillSymbol::SetColor(const Color &color)
        {
            TBase::SetColor(color);   // foreground of mono bitmaps
            SetBitmapDirty();
        }

        Color CPictureFillSymbol::GetBitmapTransparencyColor() const
        {
            return m_transparencyColor;
        }

        void CPictureFillSymbol::SetBitmapTransparencyColor(const Color &color)
        {
            m_transparencyColor = color;
            SetBitmapDirty();
        }

        Color CPictureFillSymbol::GetBackgroundColor() const
        {
            return m_bgColor;
        }

        void CPictureFillSymbol::SetBackgroundColor(const Color &color)
        {
            m_bgColor = color;
            SetBitmapDirty();
        }

        BitmapPtr CPictureFillSymbol::GetBitmap() const
        {
            return m_ptrBitmap;
        }

        void CPictureFillSymbol::SetBitmap(BitmapPtr ptrBitmap)
        {
            m_ptrBitmap = ptrBitmap;
            SetBitmapDirty();
        }

        bool CPictureFillSymbol::CanDraw(CommonLib::IGeoShapePtr ptrShape) const
        {
            if(!ptrShape.get() || ptrShape->GetPointCnt() < 3)
                return false;
            return (m_ptrBitmap.get() && m_ptrBitmap->Width() > 0 && m_ptrBitmap->Height() > 0) || m_ptrBorderSymbol.get() != nullptr;
        }

        void CPictureFillSymbol::Prepare(IDisplayPtr ptrDisplay)
        {
            TBase::Prepare(ptrDisplay);
            if(m_bBitmapDirty)
            {
                m_ptrDrawBitmap = CSymbolBitmapUtils::ToARGB(m_ptrBitmap, m_ptrBrush->GetColor(), m_bgColor, m_transparencyColor);
                m_ptrBrush->SetTexture(m_ptrDrawBitmap);
                m_bBitmapDirty = false;
            }

            if(m_ptrBorderSymbol.get())
                m_ptrBorderSymbol->Prepare(ptrDisplay);
        }

        void CPictureFillSymbol::Reset()
        {
            if(m_ptrBorderSymbol.get())
                m_ptrBorderSymbol->Reset();
        }

        void CPictureFillSymbol::DrawGeometryEx(IDisplayPtr ptrDisplay, const GPoint* points, const int* polyCounts, int polyCount)
        {
            if(m_ptrDrawBitmap.get())
            {
                IGraphicsPtr ptrGraphics = ptrDisplay->GetGraphics();

                // tiles aligned to the map origin, they stay on the map when it is panned
                GPoint oldOrg = ptrGraphics->GetBrushOrg();
                ptrGraphics->SetBrushOrg(GetPatternOrigin(ptrDisplay));

                if(polyCount == 1)
                    ptrGraphics->DrawPolygon(m_ptrNullPen, m_ptrBrush, points, polyCounts[0]);
                else
                    ptrGraphics->DrawPolyPolygon(m_ptrNullPen, m_ptrBrush, points, polyCounts, polyCount);

                ptrGraphics->SetBrushOrg(oldOrg);
            }

            DrawOutline(ptrDisplay, points, polyCounts, polyCount);
        }

        void CPictureFillSymbol::DrawDirectly(IDisplayPtr ptrDisplay, const GPoint* lpPoints, const int *lpPolyCounts, int nCount)
        {
            DrawGeometryEx(ptrDisplay, lpPoints, lpPolyCounts, nCount);
        }

        void CPictureFillSymbol::FillRect(IDisplayPtr ptrDisplay, const GRect& rect)
        {
            GPoint points[5] = {GPoint(rect.xMin, rect.yMin), GPoint(rect.xMax, rect.yMin), GPoint(rect.xMax, rect.yMax),
                                GPoint(rect.xMin, rect.yMax), GPoint(rect.xMin, rect.yMin)};
            int nCount = 5;
            DrawGeometryEx(ptrDisplay, points, &nCount, 1);
        }

        void CPictureFillSymbol::QueryBoundaryRectEx(IDisplayPtr ptrDisplay, const GPoint* points, const int* polyCounts, int polyCount, GRect &rect) const
        {
            GRect bounds;
            if(GetPointsBounds(points, polyCounts, polyCount, bounds))
                rect.ExpandRect(bounds);

            if(m_ptrBorderSymbol.get())
                m_ptrBorderSymbol->QueryBoundaryRectEx(ptrDisplay, points, polyCounts, polyCount, rect);
        }

        void CPictureFillSymbol::Save(CommonLib::ISerializeObjPtr pObj) const
        {
            try
            {
                TBase::Save(pObj);
                m_bgColor.Save(pObj, "BgColor");
                m_transparencyColor.Save(pObj, "TransparencyColor");
                if(m_ptrBitmap.get())
                    m_ptrBitmap->Save(pObj, "Bitmap");
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to save CPictureFillSymbol", exc);
            }
        }

        void CPictureFillSymbol::Load(CommonLib::ISerializeObjPtr pObj)
        {
            try
            {
                TBase::Load(pObj);
                m_ptrBrush->SetType(BrushTypeTextured);
                m_bgColor.Load(pObj, "BgColor");
                m_transparencyColor.Load(pObj, "TransparencyColor");

                m_ptrBitmap.reset();
                if(pObj->IsChildExists("Bitmap") || pObj->IsPropertyExists("Bitmap"))
                {
                    m_ptrBitmap = std::make_shared<CBitmap>();
                    m_ptrBitmap->Load(pObj, "Bitmap");
                }
                SetBitmapDirty();
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to load CPictureFillSymbol", exc);
            }
        }

    }
}
