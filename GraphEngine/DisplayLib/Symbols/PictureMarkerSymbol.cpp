#include "PictureMarkerSymbol.h"
#include "SymbolBitmapUtils.h"
#include <cmath>

namespace GraphEngine {
    namespace Display {

        CPictureMarkerSymbol::CPictureMarkerSymbol() :
            m_bgColor(Color::White, Color::Transparent), m_transparencyColor(Color::Black, Color::Transparent),
            m_bDrawExact(false), m_bBitmapDirty(true), m_dScale(1.)
        {
            m_nSymbolID = PictureMarkerSymbolID;
            m_Color = Color(0, 0, 0);
            m_dSize = 4.;
        }

        CPictureMarkerSymbol::CPictureMarkerSymbol(BitmapPtr ptrBitmap, double dSize) : CPictureMarkerSymbol()
        {
            m_ptrBitmap = ptrBitmap;
            m_dSize = dSize;
        }

        CPictureMarkerSymbol::~CPictureMarkerSymbol()
        {

        }

        void CPictureMarkerSymbol::SetColor(const Color &color)
        {
            TBase::SetColor(color);
            m_bBitmapDirty = true;
        }

        Color CPictureMarkerSymbol::GetBitmapTransparencyColor() const
        {
            return m_transparencyColor;
        }

        void CPictureMarkerSymbol::SetBitmapTransparencyColor(const Color &color)
        {
            m_transparencyColor = color;
            m_bBitmapDirty = true;
        }

        BitmapPtr CPictureMarkerSymbol::GetBitmap() const
        {
            return m_ptrBitmap;
        }

        void CPictureMarkerSymbol::SetBitmap(BitmapPtr ptrBitmap)
        {
            m_ptrBitmap = ptrBitmap;
            m_bBitmapDirty = true;
        }

        Color CPictureMarkerSymbol::GetBackgroundColor() const
        {
            return m_bgColor;
        }

        void CPictureMarkerSymbol::SetBackgroundColor(const Color &color)
        {
            m_bgColor = color;
            m_bBitmapDirty = true;
        }

        bool CPictureMarkerSymbol::GetDrawExact() const
        {
            return m_bDrawExact;
        }

        void CPictureMarkerSymbol::SetDrawExact(bool bDrawExact)
        {
            m_bDrawExact = bDrawExact;
        }

        bool CPictureMarkerSymbol::CanDraw(CommonLib::IGeoShapePtr ptrShape) const
        {
            if(!ptrShape.get() || ptrShape->GetPointCnt() == 0)
                return false;
            if(!m_ptrBitmap.get() || m_ptrBitmap->Width() == 0 || m_ptrBitmap->Height() == 0)
                return false;
            return m_bDrawExact || m_dSize > 0.;
        }

        void CPictureMarkerSymbol::Prepare(IDisplayPtr ptrDisplay)
        {
            TBase::Prepare(ptrDisplay);
            if(m_bBitmapDirty)
            {
                m_ptrDrawBitmap = CSymbolBitmapUtils::ToARGB(m_ptrBitmap, m_Color, m_bgColor, m_transparencyColor);
                m_bBitmapDirty = false;
            }

            m_dScale = 1.;
            if(!m_bDrawExact && m_ptrDrawBitmap.get())
            {
                double dDeviceSize = CDisplayUtils::SymbolSizeToDeviceSize(ptrDisplay->GetTransformation(), m_dSize, GetScaleDependent());
                double dBitmapSize = (double)(std::max)(m_ptrDrawBitmap->Width(), m_ptrDrawBitmap->Height());
                m_dScale = dBitmapSize > 0. ? dDeviceSize / dBitmapSize : 1.;
            }
        }

        void CPictureMarkerSymbol::DrawGeometryEx(IDisplayPtr ptrDisplay, const GPoint* points, const int* polyCounts, int polyCount)
        {
            if(!m_ptrDrawBitmap.get() || m_dScale <= 0.)
                return;

            IGraphicsPtr ptrGraphics = ptrDisplay->GetGraphics();
            for(int part = 0, offset = 0; part < polyCount; ++part)
            {
                for(int i = offset; i < offset + polyCounts[part]; ++i)
                {
                    GPoint center(points[i].x + (GUnits)m_dDeviceOffsetX, points[i].y + (GUnits)m_dDeviceOffsetY);
                    // DrawRotatedBitmap turns counter clockwise, the markers (RotateCoords) clockwise on the device
                    ptrGraphics->DrawRotatedBitmap(m_ptrDrawBitmap, center, -m_dDisplayAngle, true, 255, m_dScale, m_dScale);
                }
                offset += polyCounts[part];
            }
        }

        void CPictureMarkerSymbol::DrawDirectly(IDisplayPtr ptrDisplay, const GPoint* lpPoints, const int *lpPolyCounts, int nCount)
        {
            DrawGeometryEx(ptrDisplay, lpPoints, lpPolyCounts, nCount);
        }

        void CPictureMarkerSymbol::QueryBoundaryRectEx(IDisplayPtr ptrDisplay, const GPoint* points, const int* polyCounts, int polyCount, GRect &rect) const
        {
            double dHalf = 0.;
            if(m_ptrDrawBitmap.get())
            {
                // rotated bitmap fits into the circle of the half diagonal
                double w = (double)m_ptrDrawBitmap->Width() * m_dScale;
                double h = (double)m_ptrDrawBitmap->Height() * m_dScale;
                dHalf = sqrt(w * w + h * h) / 2.;
            }

            for(int part = 0, offset = 0; part < polyCount; ++part)
            {
                for(int i = offset; i < offset + polyCounts[part]; ++i)
                {
                    GUnits x = points[i].x + (GUnits)m_dDeviceOffsetX;
                    GUnits y = points[i].y + (GUnits)m_dDeviceOffsetY;
                    rect.ExpandRect(GRect((GUnits)(x - dHalf), (GUnits)(y - dHalf), (GUnits)(x + dHalf), (GUnits)(y + dHalf)));
                }
                offset += polyCounts[part];
            }
        }

        void CPictureMarkerSymbol::Save(CommonLib::ISerializeObjPtr pObj) const
        {
            try
            {
                TBase::Save(pObj);
                pObj->AddPropertyBool("DrawExact", m_bDrawExact);
                m_bgColor.Save(pObj, "BgColor");
                m_transparencyColor.Save(pObj, "TransparencyColor");
                if(m_ptrBitmap.get())
                    m_ptrBitmap->Save(pObj, "Bitmap");
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to save CPictureMarkerSymbol", exc);
            }
        }

        void CPictureMarkerSymbol::Load(CommonLib::ISerializeObjPtr pObj)
        {
            try
            {
                TBase::Load(pObj);
                m_bDrawExact = pObj->GetPropertyBool("DrawExact", m_bDrawExact);
                m_bgColor.Load(pObj, "BgColor");
                m_transparencyColor.Load(pObj, "TransparencyColor");

                m_ptrBitmap.reset();
                if(pObj->IsChildExists("Bitmap"))
                {
                    m_ptrBitmap = std::make_shared<CBitmap>();
                    m_ptrBitmap->Load(pObj, "Bitmap");
                }
                m_bBitmapDirty = true;
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to load CPictureMarkerSymbol", exc);
            }
        }

    }
}
