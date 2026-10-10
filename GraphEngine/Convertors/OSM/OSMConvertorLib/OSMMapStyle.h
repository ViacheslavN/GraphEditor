#pragma once
#include "OSMConvertorLib.h"

namespace GraphEngine {
    namespace Convertors {

        // Map layers of the converted OSM layers: symbology by the "type" field, labels by "name", scale ranges.
        // The layers keep a cartographic order (landuse at the bottom, places on the top) also when they are
        // added one by one.
        class COSMMapStyle
        {
        public:
            // rank of the layer in the drawing order (bigger - higher), unknown layers go on the top
            static int LayerRank(const std::string& sLayerName);

            // creates the feature layer of the table (styled by the OSM layer name), the label renderer if the layer has names
            static Cartography::IFeatureLayerPtr CreateLayer(const IOSMLayer& osmLayer, GeoDatabase::ITablePtr ptrTable);

            // map background of the Carto style (@land-color #f2efe9)
            static Display::IFillSymbolPtr CreateBackground();
        };
    }
}
