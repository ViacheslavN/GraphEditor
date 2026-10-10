#include "SymbolPreview.h"
#include "SymbolsLoader.h"
#include "../Display/Display.h"
#include "../Transformation/DisplayTransformation2D.h"
#include "../../CommonLib/Serialize/SerializeXML.h"
#include "../../CommonLib/xml/XMLNode.h"

namespace GraphEngine {
    namespace Display {

        eSymbolPreviewShape CSymbolPreview::GetShape(ISymbolPtr ptrSymbol)
        {
            ISymbol* pSymbol = ptrSymbol.get();
            if(dynamic_cast<IMarkerSymbol*>(pSymbol))
                return SymbolPreviewPoint;
            if(dynamic_cast<ILineSymbol*>(pSymbol))
                return SymbolPreviewLine;
            if(dynamic_cast<IFillSymbol*>(pSymbol))
                return SymbolPreviewPolygon;
            if(dynamic_cast<ITextSymbol*>(pSymbol))
                return SymbolPreviewText;
            return SymbolPreviewPolygon;
        }

        ISymbolPtr CSymbolPreview::CloneSymbol(ISymbolPtr ptrSymbol)
        {
            if(!ptrSymbol.get())
                return ISymbolPtr();

            try
            {
                CommonLib::xml::IXMLNodePtr ptrNode = std::make_shared<CommonLib::xml::CXMLNode>(CommonLib::xml::IXMLNodePtr(), "Symbol");
                CommonLib::ISerializeObjPtr ptrObj = std::make_shared<CommonLib::CSerializeObjXML>(ptrNode);
                ptrSymbol->Save(ptrObj);
                return CSymbolsLoader::LoadSymbol(ptrObj);
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to copy symbol", exc);
                throw;
            }
        }

        void CSymbolPreview::Draw(ISymbolPtr ptrSymbol, IGraphicsPtr ptrGraphics, const GRect& rect, double dpi, eSymbolPreviewShape shape, bool bClone)
        {
            if(!ptrSymbol.get() || !ptrGraphics.get() || rect.Width() <= 0 || rect.Height() <= 0)
                return;

            ISymbolPtr ptrDraw = bClone ? CloneSymbol(ptrSymbol) : ptrSymbol;
            if(shape == SymbolPreviewAuto)
                shape = GetShape(ptrDraw);

            ITextSymbol* pText = dynamic_cast<ITextSymbol*>(ptrDraw.get());
            if(pText)
            {
                // the text is centered on the point (the alignment of a copy only)
                if(bClone)
                {
                    if(pText->GetText().empty())
                        pText->SetText(L"AaBb");
                    if(pText->GetFont().get())
                    {
                        pText->GetFont()->SetTextHAlignment(TextHAlignmentCenter);
                        pText->GetFont()->SetTextVAlignment(TextVAlignmentCenter);
                    }
                }
                shape = SymbolPreviewPoint;
            }

            // symbol sizes are in mm: no map units, no reference scale - the size is converted by dpi only
            IDisplayTransformationPtr ptrTrans = std::make_shared<CDisplayTransformation2D>(dpi > 0. ? dpi : 96., CommonLib::UnitsUnknown, rect);
            IDisplayPtr ptrDisplay = std::make_shared<CDisplay>(ptrTrans);

            const GUnits left = rect.xMin, top = rect.yMin, right = rect.xMax, bottom = rect.yMax;
            const GUnits width = rect.Width(), height = rect.Height();
            const GUnits cx = left + width / 2, cy = top + height / 2;

            GPoint points[5];
            int nPoints = 0;
            switch(shape)
            {
                case SymbolPreviewLine:
                {
                    // zig-zag like a legend of a desktop GIS, a straight line when the rect is low
                    GUnits dy = height >= 10 ? height / 4 : 0;
                    points[0] = GPoint(left + 1, cy + dy);
                    points[1] = GPoint(left + width / 3, cy - dy);
                    points[2] = GPoint(left + width * 2 / 3, cy + dy);
                    points[3] = GPoint(right - 1, cy - dy);
                    nPoints = 4;
                    break;
                }
                case SymbolPreviewPolygon:
                case SymbolPreviewText:
                {
                    if(shape == SymbolPreviewText)
                    {
                        points[0] = GPoint(cx, cy);
                        nPoints = 1;
                        break;
                    }
                    points[0] = GPoint(left + 1, top + 1);
                    points[1] = GPoint(right - 1, top + 1);
                    points[2] = GPoint(right - 1, bottom - 1);
                    points[3] = GPoint(left + 1, bottom - 1);
                    points[4] = points[0];
                    nPoints = 5;
                    break;
                }
                case SymbolPreviewPoint:
                default:
                    points[0] = GPoint(cx, cy);
                    nPoints = 1;
                    break;
            }

            // StartDrawing / FinishDrawing of the graphics reset the clipping
            ptrDisplay->StartDrawing(ptrGraphics);
            try
            {
                ptrGraphics->SetClipRect(rect);
                ptrDraw->Init(ptrDisplay);
                ptrDraw->DrawDirectly(ptrDisplay, points, &nPoints, 1);
                ptrDraw->FlushBuffers(ptrDisplay, ITrackCancelPtr());
                ptrDraw->Reset();
                ptrDisplay->FinishDrawing();
            }
            catch (...)
            {
                ptrDisplay->FinishDrawing();
                throw;
            }
        }

        IGraphicsPtr CSymbolPreview::CreatePreview(ISymbolPtr ptrSymbol, int nWidth, int nHeight, const Color& background, double dpi, eSymbolPreviewShape shape)
        {
            if(nWidth <= 0 || nHeight <= 0)
                throw CommonLib::CExcBase("Symbol preview: wrong size {0} x {1}", nWidth, nHeight);

            IGraphicsPtr ptrGraphics = IGraphics::CreateCGraphicsAgg((GUnits)nWidth, (GUnits)nHeight, false);
            ptrGraphics->Erase(background);
            Draw(ptrSymbol, ptrGraphics, GRect(0, 0, (GUnits)nWidth, (GUnits)nHeight), dpi, shape, true);
            return ptrGraphics;
        }
    }
}
