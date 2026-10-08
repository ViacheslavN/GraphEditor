#include "SymbolBitmapUtils.h"
#include <cstring>

namespace GraphEngine {
    namespace Display {

        BitmapPtr CSymbolBitmapUtils::ToARGB(BitmapPtr ptrSource, const Color& fgColor, const Color& bgColor, const Color& transparencyColor)
        {
            if(!ptrSource.get() || ptrSource->Width() == 0 || ptrSource->Height() == 0)
                return BitmapPtr();

            size_t width = ptrSource->Width();
            size_t height = ptrSource->Height();
            BitmapPtr ptrResult = std::make_shared<CBitmap>(width, height, BitmapFormatType32bppARGB);
            unsigned char* pDst = ptrResult->Bits();
            size_t dstLine = ptrResult->LineSize();

            bool bTransparency = transparencyColor.GetA() != Color::Transparent;
            const Color transparent(Color::Black, Color::Transparent);

            for(size_t row = 0; row < height; ++row)
            {
                for(size_t col = 0; col < width; ++col)
                {
                    Color color;
                    if(ptrSource->Type() == BitmapFormatType1bpp)
                    {
                        const unsigned char* pSrc = ptrSource->Bits() + ptrSource->LineSize() * row;
                        bool bSet = ((pSrc[col / 8] >> (7 - (col % 8))) & 1) != 0;
                        color = bSet ? bgColor : fgColor;
                    }
                    else
                    {
                        color = ptrSource->Pixel(row, col);
                        if(bTransparency && color.GetRGB() == transparencyColor.GetRGB())
                            color = bgColor.GetA() != Color::Transparent ? bgColor : transparent;
                    }

                    unsigned char* pPixel = pDst + dstLine * row + col * 4;
                    pPixel[0] = color.GetB();
                    pPixel[1] = color.GetG();
                    pPixel[2] = color.GetR();
                    pPixel[3] = color.GetA();
                }
            }

            return ptrResult;
        }

        BitmapPtr CSymbolBitmapUtils::CreateSolid(size_t width, size_t height, const Color& color)
        {
            BitmapPtr ptrResult = std::make_shared<CBitmap>(width, height, BitmapFormatType32bppARGB);
            for(size_t row = 0; row < height; ++row)
            {
                unsigned char* pLine = ptrResult->Bits() + ptrResult->LineSize() * row;
                for(size_t col = 0; col < width; ++col)
                {
                    pLine[col * 4] = color.GetB();
                    pLine[col * 4 + 1] = color.GetG();
                    pLine[col * 4 + 2] = color.GetR();
                    pLine[col * 4 + 3] = color.GetA();
                }
            }
            return ptrResult;
        }

        GPoint GetPatternOrigin(IDisplayPtr ptrDisplay)
        {
            CommonLib::GisXYPoint mapOrigin;
            mapOrigin.x = 0.;
            mapOrigin.y = 0.;
            GPoint origin(0, 0);
            ptrDisplay->GetTransformation()->MapToDevice(&mapOrigin, &origin, 1);
            return origin;
        }

        bool GetPointsBounds(const GPoint* points, const int* polyCounts, int polyCount, GRect& rect)
        {
            int nTotal = 0;
            for(int part = 0; part < polyCount; ++part)
                nTotal += polyCounts[part];

            if(nTotal <= 0)
                return false;

            rect.Set(points[0].x, points[0].y, points[0].x, points[0].y);
            for(int i = 1; i < nTotal; ++i)
            {
                if(points[i].x < rect.xMin) rect.xMin = points[i].x;
                if(points[i].x > rect.xMax) rect.xMax = points[i].x;
                if(points[i].y < rect.yMin) rect.yMin = points[i].y;
                if(points[i].y > rect.yMax) rect.yMax = points[i].y;
            }
            return true;
        }

        CPolygonClipGuard::CPolygonClipGuard(IGraphicsPtr ptrGraphics, const GPoint* points, const int* polyCounts, int polyCount) : m_ptrGraphics(ptrGraphics)
        {
            // GetClipRect returns the rect with the viewport origin, SetClipRect adds it
            m_clipRect = m_ptrGraphics->GetClipRect();
            GPoint org = m_ptrGraphics->GetViewportOrg();
            m_clipRect.Offset(-org.x, -org.y);

            m_ptrGraphics->SetClipRgn(points, polyCounts, polyCount);
        }

        CPolygonClipGuard::~CPolygonClipGuard()
        {
            m_ptrGraphics->RemoveClip();
            m_ptrGraphics->SetClipRect(m_clipRect);
        }

    }
}
