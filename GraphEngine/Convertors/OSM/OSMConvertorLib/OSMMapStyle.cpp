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
#include "../../../DisplayLib/Symbols/MultiLayerSymbol.h"

namespace GraphEngine {
    namespace Convertors {

        namespace
        {
            typedef Display::Color TColor;

            Display::ISymbolPtr Line(const TColor& color, double dWidth, Display::eSimpleLineStyle style = Display::SimpleLineStyleSolid)
            {
                return std::make_shared<Display::CSimpleLineSymbol>(color, dWidth, style);
            }

            // #RRGGBB of the OpenStreetMap Carto style (openstreetmap-carto), alpha 0..255
            TColor Hex(uint32_t rgb, unsigned char alpha = 255)
            {
                return TColor((unsigned char)((rgb >> 16) & 0xFF), (unsigned char)((rgb >> 8) & 0xFF), (unsigned char)(rgb & 0xFF), alpha);
            }

            // line of the Carto style: the width and the dash pattern (dash, gap, dash, gap ...) are in pixels,
            // as the line width of CSimpleLineSymbol is
            Display::ILineSymbolPtr Stroke(const TColor& color, double dWidth, std::initializer_list<double> dashes = {})
            {
                std::shared_ptr<Display::CSimpleLineSymbol> ptrLine = std::make_shared<Display::CSimpleLineSymbol>(color, dWidth, Display::SimpleLineStyleSolid);
                std::vector<double> vecDashes(dashes);
                for(size_t i = 0; i + 1 < vecDashes.size(); i += 2)
                    ptrLine->AddDash(vecDashes[i], vecDashes[i + 1]);
                return ptrLine;
            }

            // lines drawn one over another (Carto attachments: casing / fill, dark / light dashes);
            // the cache draws the first line of all the features under the second one
            Display::ISymbolPtr Lines(std::initializer_list<Display::ILineSymbolPtr> lines)
            {
                std::shared_ptr<Display::CMultiLayerLineSymbol> ptrSymbol = std::make_shared<Display::CMultiLayerLineSymbol>();
                for(const Display::ILineSymbolPtr& ptrLine : lines)
                    ptrSymbol->AddLayer(ptrLine);
                ptrSymbol->SetUseCache(true);
                return ptrSymbol;
            }

            Display::ISymbolPtr Fill(const TColor& color, const TColor* pOutline = nullptr, double dOutlineWidth = 0.1)
            {
                std::shared_ptr<Display::CSimpleFillSymbol> ptrFill = std::make_shared<Display::CSimpleFillSymbol>();
                ptrFill->SetColor(color);
                if(pOutline)
                    ptrFill->SetOutlineSymbol(std::make_shared<Display::CSimpleLineSymbol>(*pOutline, dOutlineWidth, Display::SimpleLineStyleSolid));
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
                Cartography::ISymbolSelectorPtr ptrCasingSelector;     // drawn first by its own renderer (not in the legend): road casings
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
                            {"pitch", Fill(Hex(0x88e0be))}, {"parking", Fill(TColor(238, 238, 238))}},
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
                    // Carto water.mss, zoom 16: @water-color, river 8, canal 3 * 1.4, stream 3, ditch / drain 2 pixels
                    TColor water = Hex(0xaad3df);
                    style.ptrSelector = ByType({{"drain", Stroke(water, 2.)}, {"ditch", Stroke(water, 2.)}, {"stream", Stroke(water, 3.)},
                                                {"canal", Stroke(water, 4.2)}, {"river", Stroke(water, 8.)}}, Stroke(water, 2.));
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
                    // Carto roads.mss, railways at zoom 15-17 (widths and dashes in pixels);
                    // the values go from the minor to the main ones: a later symbol is drawn over the earlier ones
                    const TColor white = Hex(0xffffff);
                    style.ptrSelector = ByType({
                            {"disused",      Lines({Stroke(Hex(0xaaaaaa), 2., {2, 4})})},
                            {"miniature",    Lines({Stroke(Hex(0x999999), 1.2), Stroke(Hex(0x999999), 3., {1, 10})})},
                            {"tram",         Lines({Stroke(Hex(0x6e6e6e), 1.5)})},
                            {"subway",       Lines({Stroke(Hex(0x999999), 2.)})},
                            {"funicular",    Lines({Stroke(Hex(0x666666), 2.)})},
                            {"narrow_gauge", Lines({Stroke(Hex(0x666666), 2.)})},
                            {"light_rail",   Lines({Stroke(Hex(0x666666), 2.)})},
                            {"monorail",     Lines({Stroke(Hex(0xffffff, 102), 4.), Stroke(Hex(0x777777), 3., {2, 3})})},
                            {"preserved",    Lines({Stroke(Hex(0x666666), 3.), Stroke(white, 1., {0, 1, 8, 1})})},
                            {"rail",         Lines({Stroke(Hex(0x707070), 3.), Stroke(white, 1., {8, 8})})}},
                            Lines({Stroke(Hex(0x707070), 2.)}));
                }
                else if(sName == "roads")
                {
                    // Carto roads.mss at zoom 16 (pixels): the casing renderer draws the full width W with the casing color
                    // under all the roads, the fill renderer draws W - 2 * casing width over them;
                    // footways, cycleways, tracks: a translucent white background and a dashed line.
                    // The values go from the minor to the main roads: the main ones are drawn over the minor ones
                    struct SRoad
                    {
                        const char* pszType;
                        uint32_t    casing;
                        unsigned char casingAlpha;
                        double      dWidth;        // W, the casing
                        uint32_t    fill;
                        double      dFillWidth;
                        std::vector<double> vecDashes;
                    };
                    const double c = 0.6, major = 0.7;   // @casing-width-z16, @major-casing-width-z16 (also secondary)
                    const std::vector<SRoad> roads =
                    {
                        {"path",          0xffffff, 102, 1.3 + 2,  0xfa8072, 1.3, {1, 3, 2, 4}},
                        {"footway",       0xffffff, 102, 1.3 + 2,  0xfa8072, 1.3, {1, 3, 2, 4}},
                        {"cycleway",      0xffffff, 102, 0.9 + 2,  0x0000ff, 0.9, {1, 3, 2, 4}},
                        {"bridleway",     0xffffff, 102, 1.2 + 2,  0x008000, 1.2, {4, 2}},
                        {"steps",         0xffffff, 102, 3. + 2,   0xfa8072, 3.,  {2, 1}},
                        {"track",         0xffffff, 102, 1.5 + 2,  0x996600, 1.5, {5, 4, 2, 4}},
                        {"service",       0xbbbbbb, 255, 3.5,      0xffffff, 3.5 - 2 * c, {}},
                        {"road",          0xbbbbbb, 255, 3.5,      0xdddddd, 3.5 - 2 * c, {}},
                        {"pedestrian",    0x999999, 255, 6.,       0xdddde8, 6. - 2 * c, {}},
                        {"living_street", 0xbbbbbb, 255, 6.,       0xededed, 6. - 2 * c, {}},
                        {"unclassified",  0xbbbbbb, 255, 6.,       0xffffff, 6. - 2 * c, {}},
                        {"residential",   0xbbbbbb, 255, 6.,       0xffffff, 6. - 2 * c, {}},
                        {"tertiary_link", 0x8f8f8f, 255, 7.,       0xffffff, 7. - 2 * c, {}},
                        {"tertiary",      0x8f8f8f, 255, 10.,      0xffffff, 10. - 2 * c, {}},
                        {"secondary_link",0x707d05, 255, 7.,       0xf7fabf, 7. - 2 * major, {}},
                        {"secondary",     0x707d05, 255, 10.,      0xf7fabf, 10. - 2 * major, {}},
                        {"primary_link",  0xa06b00, 255, 7.8,      0xfcd6a4, 7.8 - 2 * major, {}},
                        {"primary",       0xa06b00, 255, 10.,      0xfcd6a4, 10. - 2 * major, {}},
                        {"trunk_link",    0xc84e2f, 255, 7.8,      0xf9b29c, 7.8 - 2 * major, {}},
                        {"trunk",         0xc84e2f, 255, 10.,      0xf9b29c, 10. - 2 * major, {}},
                        {"motorway_link", 0xdc2a67, 255, 7.8,      0xe892a2, 7.8 - 2 * major, {}},
                        {"motorway",      0xdc2a67, 255, 10.,      0xe892a2, 10. - 2 * major, {}}
                    };

                    std::shared_ptr<Cartography::CUniqueValueSymbolSelector> ptrCasing = std::make_shared<Cartography::CUniqueValueSymbolSelector>("type");
                    std::shared_ptr<Cartography::CUniqueValueSymbolSelector> ptrFill = std::make_shared<Cartography::CUniqueValueSymbolSelector>("type");
                    for(const SRoad& road : roads)
                    {
                        std::vector<CommonLib::CVariant> value(1, CommonLib::CVariant(std::string(road.pszType)));
                        ptrCasing->AddValue(value, Lines({Stroke(Hex(road.casing, road.casingAlpha), road.dWidth)}), road.pszType);

                        std::shared_ptr<Display::CSimpleLineSymbol> ptrLine = std::make_shared<Display::CSimpleLineSymbol>(Hex(road.fill), road.dFillWidth, Display::SimpleLineStyleSolid);
                        for(size_t i = 0; i + 1 < road.vecDashes.size(); i += 2)
                            ptrLine->AddDash(road.vecDashes[i], road.vecDashes[i + 1]);
                        ptrFill->AddValue(value, Lines({ptrLine}), road.pszType);
                    }
                    // other highways as Carto "road"
                    ptrCasing->SetDefaultSymbol(Lines({Stroke(Hex(0xbbbbbb), 3.5)}));
                    ptrCasing->SetUseDefaultSymbol(true);
                    ptrFill->SetDefaultSymbol(Lines({Stroke(Hex(0xdddddd), 3.5 - 2 * c)}));
                    ptrFill->SetDefaultLabel("Other roads");
                    ptrFill->SetUseDefaultSymbol(true);
                    style.ptrCasingSelector = ptrCasing;
                    style.ptrSelector = ptrFill;
                    style.ptrLabelSelector = Simple(Text(2.4, TColor(40, 40, 40, 255)));
                    style.dLabelMinScale = 25000.;
                    style.labeling.m_lineOrientation = Cartography::LineLabelOrientationCurved;
                    style.labeling.m_duplicateStrategy = Cartography::DuplicateStrategyDistance;
                    style.labeling.m_nPriority = 2;
                }
                else if(sName == "boundaries")
                {
                    // Carto admin.mss @admin-boundaries; one width / dash for all the admin levels (Carto has them per level)
                    std::shared_ptr<Display::CSimpleFillSymbol> ptrFill = std::make_shared<Display::CSimpleFillSymbol>();
                    ptrFill->SetStyle(Display::SimpleFillStyleNull);
                    ptrFill->SetColor(TColor(255, 255, 255, 0));
                    ptrFill->SetOutlineSymbol(Stroke(Hex(0x8d618b), 1.5, {18, 1, 4, 1}));
                    style.ptrSelector = Simple(ptrFill);
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

        Display::IFillSymbolPtr COSMMapStyle::CreateBackground()
        {
            std::shared_ptr<Display::CSimpleFillSymbol> ptrFill = std::make_shared<Display::CSimpleFillSymbol>();
            ptrFill->SetColor(Hex(0xf2efe9));
            return ptrFill;
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

            if(style.ptrCasingSelector.get())
            {
                // under the fills of all the features: the renderers flush their cached symbols in their order
                std::shared_ptr<Cartography::CFeatureRenderer> ptrCasing = std::make_shared<Cartography::CFeatureRenderer>();
                ptrCasing->SetSymbolSelector(style.ptrCasingSelector);
                ptrCasing->SetShowInLegend(false);
                ptrLayer->AddRenderer(ptrCasing);
            }

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
