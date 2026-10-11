#pragma once
#include "OSMConvertorLib.h"

namespace GraphEngine {
    namespace Convertors {

        // Map layers of the converted OSM layers: symbology by the "type" field, labels by "name", scale ranges.
        // The layers keep a cartographic order (landuse at the bottom, places on the top) also when they are
        // added one by one.
        // a small scale layer over a big one (the view of a continent / the world): only the main features of the table
        // (a definition query), shown at the scales smaller than the minimum scale of the full layer; the query column
        // gets an index, so only the matching rows are read
        struct SOSMOverviewLayer
        {
            std::string sName;          // the display name of the map layer
            std::string sQuery;         // the definition query (SQL where)
            std::string sIndexColumn;   // the column of the query: indexed by the export
        };

        class COSMMapStyle
        {
        public:
            // the scale of the zoom level of the web maps (256 pixel tiles, 96 dpi): 1 : 559082264 / 2^zoom
            static double ZoomScale(int nZoom);
            // the minimum scale of the layer (visible up to 1 : scale, 0 - all the scales): the zoom level from which
            // OpenStreetMap Carto draws most of its features
            static double MinScale(const std::string& sLayerName);
            // the small scale layers drawn instead of the layer
            static std::vector<SOSMOverviewLayer> OverviewLayers(const std::string& sLayerName);

            // rank of the layer in the drawing order (bigger - higher), unknown layers go on the top
            static int LayerRank(const std::string& sLayerName);

            // creates the feature layer of the table (styled by the OSM layer name), the label renderer if the layer has names
            static Cartography::IFeatureLayerPtr CreateLayer(const IOSMLayer& osmLayer, GeoDatabase::ITablePtr ptrTable);

            // map background of the Carto style (@land-color #f2efe9)
            static Display::IFillSymbolPtr CreateBackground();
        };
    }
}
