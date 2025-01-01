#include "TextSymbol.h"
#include "../DisplayUtils.h"
#include "../Agg/agg_trans_affine.h"
#include "SymbolsLoader.h"

namespace GraphEngine {
    namespace Display {

        CTextSymbol::CTextSymbol()
        {
            m_nSymbolID = TextSymbolID;
            m_ptrFont = std::make_shared<CFont>();
        }

        CTextSymbol::~CTextSymbol()
        {

        }

        GUnits CTextSymbol::GetAngle() const
        {
            return m_ptrFont->GetOrientation();
        }

        void   CTextSymbol::SetAngle( GUnits dAngle )
        {
            SetDirty(true);
            m_ptrFont->SetOrientation(dAngle);
        }
        Color  CTextSymbol::GetColor() const
        {
            return m_ptrFont->GetColor();
        }

        void   CTextSymbol::SetColor(const Color &color )
        {
            SetDirty(true);
            m_ptrFont->SetColor(color);
        }

        FontPtr CTextSymbol::GetFont() const
        {
            return m_ptrFont;
        }

        void   CTextSymbol::SetFont(FontPtr font )
        {
            SetDirty(true);
            m_ptrFont = font;
        }
        void CTextSymbol::GetTextSize(IDisplayPtr ptrDisplay, const std::wstring& szText, GUnits *pxSize , GUnits *pySize, GUnits* baseLine) const
        {
            ptrDisplay->GetGraphics()->QueryTextMetrics(m_ptrFont, szText.c_str(), (int)szText.length(), pxSize, pySize, baseLine);
        }

        GUnits CTextSymbol::GetSize() const
        {
            return m_ptrFont->GetSize();
        }

        void CTextSymbol::SetSize(GUnits size)
        {
            SetDirty(true);
            m_ptrFont->SetSize(size);
        }

        const std::wstring& CTextSymbol::GetText() const
        {
            return m_sText;
        }

        void CTextSymbol::SetText(const std::wstring& szText )
        {
            SetDirty(true);
            m_sText = szText;
        }

        ITextBackgroundPtr CTextSymbol::GetTextBackground() const
        {
            return m_pTextBg;
        }

        void CTextSymbol::SetTextBackground( ITextBackgroundPtr pBg )
        {
            SetDirty(true);
            m_pTextBg = pBg;
        }

        int   CTextSymbol::GetTextDrawFlags() const
        {
            return m_nTextDrawFlags;
        }

        void  CTextSymbol::SetTextDrawFlags(int nFlags)
        {
            SetDirty(true);
            m_nTextDrawFlags = nFlags;
        }

        bool CTextSymbol::CanDraw(const CommonLib::IGeoShapePtr geom) const
        {
            if(m_ptrFont->GetColor().GetA() == Color::Transparent)
                return false;
            if(m_ptrFont->GetSize() == 0.0)
                return false;
            return true;
        }
        void CTextSymbol::DrawDirectly(IDisplayPtr ptrDisplay, const GPoint* lpPoints, const int *lpPolyCounts, int nCount )
        {
            DrawGeometryEx(ptrDisplay, lpPoints, lpPolyCounts, nCount);
        }

        void  CTextSymbol::DrawGeometryEx(IDisplayPtr ptrDisplay, const GPoint* points, const int* polyCounts, size_t polyCount)
        {
            if(m_ptrGeom && m_ptrGeom->GeneralType() == CommonLib::shape_type_general_polyline)
            {
                double tmp = m_ptrFont->GetOrientation();
                m_ptrFont->SetOrientation(0);
                ptrDisplay->GetGraphics()->DrawTextByLine(m_ptrFont, m_sText.c_str(), (int)m_sText.length(), points, polyCounts[0]);
                m_ptrFont->SetOrientation(tmp);
            }
            else
            {
                if(m_ptrGeom && m_ptrGeom->GeneralType() == CommonLib::shape_type_polygon)
                {
                    CommonLib::GisXYPoint pt;
                    PolygonCenterPoint(m_ptrGeom, &pt);

                    GPoint dpt;
                    ptrDisplay->GetTransformation()->MapToDevice(&pt, &dpt, 1);

                    eTextHAlignment oldHAlignment = m_ptrFont->GetTextHAlignment();
                    eTextVAlignment oldVAlignment = m_ptrFont->GetTextVAlignment();
                    m_ptrFont->SetTextHAlignment(TextHAlignmentCenter);
                    m_ptrFont->SetTextVAlignment(TextVAlignmentCenter);

                    if(m_pTextBg.get())
                        draw_background(ptrDisplay, dpt);
                    ptrDisplay->GetGraphics()->DrawText(m_ptrFont, m_sText.c_str(), (int)m_sText.length(), dpt, m_nTextDrawFlags);

                    m_ptrFont->SetTextHAlignment(oldHAlignment);
                    m_ptrFont->SetTextVAlignment(oldVAlignment);
                }
                else
                {
                    draw_background(ptrDisplay, points[0]);
                    ptrDisplay->GetGraphics()->DrawText(m_ptrFont, m_sText.c_str(), (int)m_sText.length(), points[0], m_nTextDrawFlags);
                }
            }
        }
        void  CTextSymbol::QueryBoundaryRectEx(IDisplayPtr ptrDisplay, const GPoint* points, const int* polyCounts, size_t polyCount,   GRect &rect) const
        {
            if(polyCount > 1 || polyCounts[0] > 1)
                return;

            QueryBoundaryRectEx1(ptrDisplay, points[0], rect);
        }

        void  CTextSymbol::Prepare(IDisplayPtr ptrDisplay)
        {
            m_ptrFont->SetSize(CDisplayUtils::SymbolSizeToDeviceSize(ptrDisplay->GetTransformation(), m_ptrFont->GetSize(), GetScaleDependent()));
            if(m_ptrFont->GetHaloSize() != 0)
                m_ptrFont->SetHaloSize(CDisplayUtils::SymbolSizeToDeviceSize(ptrDisplay->GetTransformation(),m_ptrFont->GetHaloSize(), GetScaleDependent()));
        }
        void CTextSymbol::PolygonCenterPoint(const CommonLib::IGeoShapePtr pGeom, CommonLib::GisXYPoint* pout)
        {
            int k, i, j, s1, s2, MaxNodes, nPts, PtNum;
            CommonLib::GisXYPoint gPt;
            CommonLib::GisXYPoint *pNodes;
            double D, MaxD, y;

            double t;
            MaxNodes = 16;
            pNodes = new CommonLib::GisXYPoint[sizeof(CommonLib::GisXYPoint) *  MaxNodes]; //TO DO alloc
            MaxD = 0;
            const CommonLib::GisXYPoint* pPts = pGeom->GetPoints();
            nPts = (int)pGeom->GetPointCnt();
            CommonLib::bbox bbox =  pGeom->GetBB();


            CommonLib::GisXYPoint pVec[2];
            for(k = 1; k < 4; k++)
            {
                y = bbox.yMin + (bbox.yMax - bbox.yMin) / 4 * k;
                s1 = (pPts[0].y <= y);
                PtNum = 0;
                for(i = 1; i < nPts; i++)
                {
                    s2 = (pPts[i].y <= y);
                    if(s1 != s2)
                    {
                        if(PtNum >= MaxNodes)
                        {
                            MaxNodes *=2;
                            delete pNodes;
                            pNodes = new CommonLib::GisXYPoint[sizeof(CommonLib::GisXYPoint) *  MaxNodes];  //TO DO alloc
                        }
                        pNodes[PtNum].y = y;
                        t = (y - pPts[i - 1].y) / (pPts[i].y - pPts[i-1].y);
                        pNodes[PtNum].x = (pPts[i - 1].x + (pPts[i].x - pPts[i - 1].x) * t);
                        PtNum++;
                    }
                    s1=s2;
                }
                for(i = 1; i < PtNum; i++)
                    for(j=0; j < PtNum - i; j++)
                    {
                        if((pNodes[j].x <= pNodes[j + 1].x))
                            continue;
                        gPt = pNodes[j];
                        pNodes[j] = pNodes[j + 1];
                        pNodes[j + 1] = gPt;
                    }
                for(i = 0; i < PtNum; i += 2)
                {
                    D = pNodes[i + 1].x - pNodes[i].x;
                    if(D > MaxD)
                    {
                        pVec[0] = pNodes[i];
                        pVec[1] = pNodes[i+1];
                        MaxD = D;
                        if (k == 2)
                            MaxD += (D / 10);
                    }
                    else if (k == 2 && ((MaxD - D) < (D / 10)))
                    {
                        pVec[0] = pNodes[i];
                        pVec[1] = pNodes[i + 1];
                        MaxD += (D / 10);
                    }
                }
            }

            if (MaxD != 0)
            {
                pout->x = pVec[0].x / 2 + pVec[1].x / 2;
                pout->y = pVec[0].y / 2 + pVec[1].y / 2;
            }
            else
                *pout = pPts[0];
        }
        void CTextSymbol::draw_background(IDisplayPtr ptrDisplay, const GPoint& pt)
        {
            if(!m_pTextBg.get())
                return;

            GRect rect;
            QueryBoundaryRectEx1(ptrDisplay, pt, rect);
            m_pTextBg->Draw(ptrDisplay, rect);
        }


        void CTextSymbol::QueryBoundaryRectEx1(IDisplayPtr ptrDisplay, const GPoint& point, GRect& rect) const
        {
            GUnits width, height, baseline;
            ptrDisplay->GetGraphics()->QueryTextMetrics(m_ptrFont, m_sText.c_str(), (int)m_sText.length(), &width, &height, &baseline);

            switch(m_ptrFont->GetTextVAlignment())
            {
                case TextVAlignmentTop:
                    rect.yMin = point.y;
                    rect.yMax = point.y + height;
                    break;
                case TextVAlignmentCenter:
                    rect.yMin = point.y - height / 2;
                    rect.yMax = point.y + height / 2;
                    break;
                case TextVAlignmentBaseline:
                    rect.yMin = point.y - (height - baseline);
                    rect.yMax = point.y + baseline;
                    break;
                case TextVAlignmentBottom:
                    rect.yMin = point.y - height;
                    rect.yMax = point.y;
                    break;
            }

            switch(m_ptrFont->GetTextHAlignment())
            {
                case TextHAlignmentLeft:
                    rect.xMin = point.x;
                    rect.xMax = point.x + width;
                    break;
                case TextHAlignmentCenter:
                    rect.xMin = point.x - width / 2;
                    rect.xMax = point.x + width / 2;
                    break;
                case TextHAlignmentRight:
                    rect.xMin = point.x - width;
                    rect.xMax = point.x;
                    break;
            }

            if(m_ptrFont->GetOrientation() != 0.0)
            {
                CommonLib::GisXYPoint p[4];
                p[0].x = rect.xMin;
                p[0].y = rect.yMin;
                p[1].x = rect.xMax;
                p[1].y = rect.yMin;
                p[2].x = rect.xMax;
                p[2].y = rect.yMax;
                p[3].x = rect.xMin;
                p[3].y = rect.yMax;

                agg::trans_affine mtx;
                mtx *= agg::trans_affine_translation(-point.x, -point.y);
                mtx *= agg::trans_affine_rotation(DEG2RAD(m_ptrFont->GetOrientation()));
                mtx *= agg::trans_affine_translation(point.x, point.y);
                mtx.transform(&p[0].x, &p[0].y);
                mtx.transform(&p[1].x, &p[1].y);
                mtx.transform(&p[2].x, &p[2].y);
                mtx.transform(&p[3].x, &p[3].y);

                rect.xMin = (GUnits)min(p[0].x, min(p[1].x, min(p[2].x, p[3].x)));
                rect.xMax = (GUnits)max(p[0].x, max(p[1].x, max(p[2].x, p[3].x)));
                rect.yMin = (GUnits)min(p[0].y, min(p[1].y, min(p[2].y, p[3].y)));
                rect.yMax = (GUnits)max(p[0].y, max(p[1].y, max(p[2].y, p[3].y)));
            }
        }


//Serialize

        void CTextSymbol::Save(CommonLib::ISerializeObjPtr pObj) const
        {
            try
            {

                TBase::Save(pObj);
                pObj->AddPropertyWString("Text", m_sText);
                pObj->AddPropertyInt32("DrawFlags", m_nTextDrawFlags);
                if(m_pTextBg.get())
                {
                    CommonLib::ISerializeObjPtr textBgObj = pObj->CreateChildNode("TextBg");
                    m_pTextBg->Save(textBgObj);
                }

                m_ptrFont->Save(pObj, "Font");

            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to save CTextSymbol", exc);
            }
        }

        void CTextSymbol::Load(CommonLib::ISerializeObjPtr pObj)
        {
            try
            {
                TBase::Load(pObj);
                m_sText = pObj->GetPropertyWString("Text", m_sText);
                m_nTextDrawFlags = pObj->GetPropertyInt32("DrawFlags", m_nTextDrawFlags);

                if(pObj->IsChildExists("TextBg"))
                {
                    CommonLib::ISerializeObjPtr ptrTextBg = pObj->GetChild("TextBg");
                    m_pTextBg = std::dynamic_pointer_cast<ITextBackground>(CSymbolsLoader::LoadSymbol(ptrTextBg));
                }

                m_ptrFont->Load(pObj, "Font");
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to load CTextSymbol", exc);
            }
        }
    }
}