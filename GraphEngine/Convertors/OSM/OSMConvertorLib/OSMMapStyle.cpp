#include "OSMMapStyle.h"
#include "../../../CartographyLib/layers/FeatureLayer.h"
#include "../../../CartographyLib/renders/FeatureRenderer.h"
#include "../../../CartographyLib/renders/LabelRenderer.h"
#include "../../../CartographyLib/selectors/SimpleSymbolSelector.h"
#include "../../../CartographyLib/selectors/UniqueValueSymbolSelector.h"
#include "../../../DisplayLib/Symbols/SimpleLineSymbol.h"
#include "../../../DisplayLib/Symbols/SimpleFillSymbol.h"
#include "../../../DisplayLib/Symbols/SimpleMarketSymbol.h"
#include "../../../DisplayLib/Symbols/TextSymbol.h"

namespace GraphEngine {
    namespace Convertors {

        namespace
        {
            typedef Display::Color TColor;

            Display::ISymbolPtr Line(const TColor& color, double dWidth, Display::eSimpleLineStyle style = Display::SimpleLineStyleSolid)
            {
                return std::make_shared<Display::CSimpleLineSymbol>(color, dWidth, style);
            }

            Display::ISymbolPtr Fill(const TColor& color, const TColor* pOutline = nullptr, double dOutlineWidth = 0.1)
            {
                std::shared_ptr<Display::CSimpleFillSymbol> ptrFill = std::make_shared<Display::CSimpleFillSymbol>();
                ptrFill->SetColor(color);
                if(pOutline)
                    ptrFill->SetOutlineSymbol(std::make_shared<Display::CSimpleLineSymbol>(*pOutline, dOutlineWidth, Display::SimpleLineStyleSolid));
                return ptrFill;
            }

            Display::ISymbolPtr Outline(const TColor& color, double dWidth, Display::eSimpleLineStyle style)
            {
                std::shared_ptr<Display::CSimpleFillSymbol> ptrFill = std::make_shared<Display::CSimpleFillSymbol>();
                ptrFill->SetStyle(Display::SimpleFillStyleNull);
                ptrFill->SetColor(TColor(255, 255, 255, 0));
                ptrFill->SetOutlineSymbol(std::make_shared<Display::CSimpleLineSymbol>(color, dWidth, style));
                return ptrFill;
            }

            Display::ISymbolPtr Marker(const TColor& color, double dSize)
            {
                std::shared_ptr<Display::CSimpleMarketSymbol> ptrMarker = std::make_shared<Display::CSimpleMarketSymbol>();
                ptrMarker->SetStyle(Display::SimpleMarkerStyleCircle);
                ptrMarker->SetColor(color);
                ptrMarker->SetSize(dSize);
                ptrMarker->SetOutline(true);
                ptrMarker->SetOutlineColor(TColor(255, 255, 255, 255));
                ptrMarker->SetOutlineSize(0.2);
                return ptrMarker;
            }

            Display::ITextSymbolPtr Text(double dSize, const TColor& color, int nStyle = Display::FontStyleRegular)
            {
                std::shared_ptr<Display::CTextSymbol> ptrText = std::make_shared<Display::CTextSymbol>();
                ptrText->SetSize(dSize);
                ptrText->SetColor(color);
                ptrText->GetFont()->SetStyle(nStyle);
                ptrText->GetFont()->SetHaloSize(0.3);
                ptrText->GetFont()->SetBgColor(TColor(255, 255, 255, 255));
                return ptrText;
            }

            struct SValueSymbol
            {
                const char*         pszValue;
                Display::ISymbolPtr ptrSymbol;
            };

            Cartography::ISymbolSelectorPtr ByType(const std::vector<SValueSymbol>& values, Display::ISymbolPtr ptrDefault)
            {
                std::shared_ptr<Cartography::CUniqueValueSymbolSelector> ptrSelector = std::make_shared<Cartography::CUniqueValueSymbolSelector>("type");
                for(size_t i = 0; i < values.size(); ++i)
                    ptrSelector->AddValue(std::vector<CommonLib::CVariant>(1, CommonLib::CVariant(std::string(values[i].pszValue))), values[i].ptrSymbol, values[i].pszValue);
                ptrSelector->SetDefaultSymbol(ptrDefault);
                ptrSelector->SetUseDefaultSymbol(ptrDefault.get() != nullptr);
                return ptrSelector;
            }

            Cartography::ISymbolSelectorPtr Simple(Display::ISymbolPtr ptrSymbol)
            {
                return std::make_shared<Cartography::CSimpleSymbolSelector>(ptrSymbol);
            }

            struct SLayerStyle
            {
                Cartography::ISymbolSelectorPtr ptrSelector;
                double                          dMinScale = 0.;        // visible up to 1:dMinScale, 0 - all scales
                Cartography::ISymbolSelectorPtr ptrLabelSelector;      // null - no labels
                double                          dLabelMinScale = 0.;
                Cartography::SLabelingOptions   labeling;
            };

            SLayerStyle StyleOf(const std::string& sName)
            {
                SLayerStyle style;
                const TColor black(0, 0, 0, 255);

                if(sName == "landuse")
                {
                    TColor forest(173, 209, 158), grass(205, 235, 176), residential(224, 223, 223), industrial(235, 219, 232),
                           commercial(242, 218, 217), farmland(238, 240, 213), cemetery(170, 203, 175);
                    style.ptrSelector = ByType({
                            {"forest", Fill(forest)}, {"wood", Fill(forest)}, {"scrub", Fill(TColor(200, 215, 171))},
                            {"grass", Fill(grass)}, {"meadow", Fill(grass)}, {"park", Fill(TColor(200, 250, 204))}, {"garden", Fill(TColor(205, 235, 176))},
                            {"grassland", Fill(grass)}, {"recreation_ground", Fill(TColor(223, 252, 226))}, {"village_green", Fill(grass)},
                            {"residential", Fill(residential)}, {"industrial", Fill(industrial)}, {"commercial", Fill(commercial)}, {"retail", Fill(TColor(255, 214, 209))},
                            {"farmland", Fill(farmland)}, {"farmyard", Fill(TColor(245, 220, 186))}, {"orchard", Fill(TColor(174, 223, 163))},
                            {"cemetery", Fill(cemetery)}, {"grave_yard", Fill(cemetery)}, {"wetland", Fill(TColor(215, 230, 220))},
                            {"beach", Fill(TColor(255, 241, 186))}, {"sand", Fill(TColor(245, 233, 198))}, {"bare_rock", Fill(TColor(238, 229, 220))},
                            {"pitch", Fill(TColor(170, 224, 203))}, {"parking", Fill(TColor(238, 238, 238))}},
                            Fill(TColor(230, 230, 220)));
                    style.ptrLabelSelector = Simple(Text(2.3, TColor(90, 110, 80, 255)));
                    style.dLabelMinScale = 25000.;
                    style.labeling.m_polygonPlacement = Cartography::PolygonLabelPlacementHorizontal;
                    style.labeling.m_nPriority = 5;
                }
                else if(sName == "water")
                {
                    style.ptrSelector = Simple(Fill(TColor(170, 211, 223)));
                    style.ptrLabelSelector = Simple(Text(2.6, TColor(50, 90, 160, 255), Display::FontStyleItalic));
                    style.labeling.m_polygonPlacement = Cartography::PolygonLabelPlacementMixed;
                    style.labeling.m_nPriority = 3;
                }
                else if(sName == "waterways")
                {
                    TColor water(140, 190, 215);
                    style.ptrSelector = ByType({{"river", Line(water, 0.9)}, {"canal", Line(water, 0.7)}, {"stream", Line(water, 0.3)},
                                                {"drain", Line(water, 0.2)}, {"ditch", Line(water, 0.2)}}, Line(water, 0.25));
                    style.ptrLabelSelector = Simple(Text(2.4, TColor(50, 90, 160, 255), Display::FontStyleItalic));
                    style.labeling.m_lineOrientation = Cartography::LineLabelOrientationCurved;
                    style.labeling.m_linePosition = Cartography::LineLabelPositionAboveBelow;
                    style.labeling.m_duplicateStrategy = Cartography::DuplicateStrategyDistance;
                    style.labeling.m_nPriority = 3;
                }
                else if(sName == "buildings")
                {
                    TColor outline(196, 182, 171);
                    style.ptrSelector = Simple(Fill(TColor(217, 208, 201), &outline, 0.1));
                    style.dMinScale = 25000.;
                    style.ptrLabelSelector = Simple(Text(2., TColor(90, 80, 70, 255)));
                    style.dLabelMinScale = 5000.;
                    style.labeling.m_nPriority = 6;
                }
                else if(sName == "railways")
                {
                    TColor rail(110, 110, 110);
                    style.ptrSelector = ByType({{"rail", Line(rail, 0.5)}, {"subway", Line(TColor(150, 150, 150), 0.4, Display::SimpleLineStyleDash)},
                                                {"tram", Line(TColor(120, 120, 120), 0.3)}}, Line(rail, 0.3));
                }
                else if(sName == "roads")
                {
                    style.ptrSelector = ByType({
                            {"motorway", Line(TColor(232, 146, 162), 1.6)}, {"motorway_link", Line(TColor(232, 146, 162), 0.8)},
                            {"trunk", Line(TColor(249, 178, 156), 1.4)}, {"trunk_link", Line(TColor(249, 178, 156), 0.7)},
                            {"primary", Line(TColor(252, 214, 164), 1.2)}, {"primary_link", Line(TColor(252, 214, 164), 0.6)},
                            {"secondary", Line(TColor(235, 225, 120), 1.0)}, {"tertiary", Line(TColor(200, 200, 200), 0.9)},
                            {"residential", Line(TColor(190, 190, 190), 0.6)}, {"unclassified", Line(TColor(190, 190, 190), 0.6)},
                            {"living_street", Line(TColor(200, 200, 200), 0.5)}, {"service", Line(TColor(205, 205, 205), 0.35)},
                            {"track", Line(TColor(153, 102, 51), 0.3, Display::SimpleLineStyleDash)},
                            {"footway", Line(TColor(250, 128, 114), 0.25, Display::SimpleLineStyleDot)},
                            {"path", Line(TColor(250, 128, 114), 0.25, Display::SimpleLineStyleDot)},
                            {"cycleway", Line(TColor(80, 80, 255), 0.25, Display::SimpleLineStyleDot)},
                            {"pedestrian", Line(TColor(220, 220, 230), 0.6)}, {"steps", Line(TColor(250, 128, 114), 0.4, Display::SimpleLineStyleDot)}},
                            Line(TColor(200, 200, 200), 0.3));
                    style.ptrLabelSelector = Simple(Text(2.4, TColor(40, 40, 40, 255)));
                    style.dLabelMinScale = 25000.;
                    style.labeling.m_lineOrientation = Cartography::LineLabelOrientationCurved;
                    style.labeling.m_duplicateStrategy = Cartography::DuplicateStrategyDistance;
                    style.labeling.m_nPriority = 2;
                }
                else if(sName == "boundaries")
                {
                    style.ptrSelector = Simple(Outline(TColor(172, 70, 172), 0.5, Display::SimpleLineStyleDash));
                }
                else if(sName == "places")
                {
                    style.ptrSelector = ByType({{"city", Marker(black, 2.)}, {"town", Marker(black, 1.6)}}, Marker(TColor(60, 60, 60), 1.));
                    std::shared_ptr<Cartography::CUniqueValueSymbolSelector> ptrText = std::make_shared<Cartography::CUniqueValueSymbolSelector>("type");
                    ptrText->AddValue(std::vector<CommonLib::CVariant>(1, CommonLib::CVariant(std::string("city"))), Text(4., black, Display::FontStyleBold));
                    ptrText->AddValue(std::vector<CommonLib::CVariant>(1, CommonLib::CVariant(std::string("town"))), Text(3.2, black, Display::FontStyleBold));
                    ptrText->AddValue(std::vector<CommonLib::CVariant>(1, CommonLib::CVariant(std::string("village"))), Text(2.6, black));
                    ptrText->SetDefaultSymbol(Text(2.2, TColor(60, 60, 60, 255)));
                    ptrText->SetUseDefaultSymbol(true);
                    style.ptrLabelSelector = ptrText;
                    style.labeling.m_nPriority = 0;
                }
                else if(sName == "pois")
                {
                    style.ptrSelector = Simple(Marker(TColor(214, 118, 35), 1.4));
                    style.dMinScale = 15000.;
                    style.ptrLabelSelector = Simple(Text(2.1, TColor(120, 70, 20, 255)));
                    style.dLabelMinScale = 7500.;
                    style.labeling.m_nPriority = 4;
                }
                return style;
            }

            Display::ISymbolPtr DefaultSymbol(eOSMGeometryType type)
            {
                switch(type)
                {
                    case OSMGeometryPoint:   return Marker(TColor(80, 80, 80), 1.2);
                    case OSMGeometryLine:    return Line(TColor(100, 100, 100), 0.3);
                    default:                 return Fill(TColor(220, 220, 220));
                }
            }
        }

        int COSMMapStyle::LayerRank(const std::string& sLayerName)
        {
            static const char* order[] = {"landuse", "water", "waterways", "buildings", "railways", "roads", "boundaries", "pois", "places"};
            for(int i = 0; i < (int)(sizeof(order) / sizeof(order[0])); ++i)
            {
                if(sLayerName == order[i])
                    return i;
            }
            return 100;
        }

        Cartography::IFeatureLayerPtr COSMMapStyle::CreateLayer(const IOSMLayer& osmLayer, GeoDatabase::ITablePtr ptrTable)
        {
            SLayerStyle style = StyleOf(osmLayer.GetName());
            if(!style.ptrSelector.get())
                style.ptrSelector = Simple(DefaultSymbol(osmLayer.GetGeometryType()));

            std::shared_ptr<Cartography::CFeatureLayer> ptrLayer = std::make_shared<Cartography::CFeatureLayer>();
            ptrLayer->SetName(osmLayer.GetDisplayName());
            ptrLayer->SetLayerTable(ptrTable);
            ptrLayer->SetVisible(true);
            ptrLayer->SetSelectable(true);
            ptrLayer->SetMinimumScale(style.dMinScale);

            std::shared_ptr<Cartography::CFeatureRenderer> ptrRenderer = std::make_shared<Cartography::CFeatureRenderer>();
            ptrRenderer->SetSymbolSelector(style.ptrSelector);
            ptrLayer->AddRenderer(ptrRenderer);

            if(style.ptrLabelSelector.get())
            {
                std::shared_ptr<Cartography::CLabelRenderer> ptrLabels = std::make_shared<Cartography::CLabelRenderer>(style.ptrLabelSelector);
                ptrLabels->SetMinimumScale(style.dLabelMinScale);
                ptrLabels->SetClassIndex(LayerRank(osmLayer.GetName()));
                ptrLayer->SetLabelRenderer(ptrLabels);
                ptrLayer->SetLabelFieldName("name");
                ptrLayer->SetLabelingOptions(style.labeling);
            }
            return ptrLayer;
        }
    }
}
