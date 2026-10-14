#include "OSMFeatureWriter.h"
#include <chrono>
#include "OSMMapStyle.h"
#include "../../../CartographyLib/layers/FeatureLayer.h"

namespace GraphEngine {
    namespace Convertors {

        namespace
        {
            void SetTagText(COSMTableWriter& writer, const COSMTags& tags, int nField, const char* pszKey)
            {
                const char* pszValue = tags.Get(pszKey);
                if(pszValue && *pszValue)
                    writer.SetText(nField, pszValue);
            }
        }

        void OSMWriteRoute(COSMTableWriter& writer, COSMTableWriter& members, int64_t nId, const COSMTags& tags, const char* pszRoute,
                           const std::string& sNameLanguage, const SOSMMember* pMembers, int nMembers)
        {
            writer.SetInt64(0, nId);
            writer.SetText(1, pszRoute);
            const char* pszName = sNameLanguage.empty() ? nullptr : tags.Get(("name:" + sNameLanguage).c_str());
            if(pszName && *pszName)
                writer.SetText(2, pszName);
            else
                SetTagText(writer, tags, 2, "name");
            SetTagText(writer, tags, 3, "ref");
            SetTagText(writer, tags, 4, "network");
            SetTagText(writer, tags, 5, "operator");
            SetTagText(writer, tags, 6, "colour");
            SetTagText(writer, tags, 7, "from");
            SetTagText(writer, tags, 8, "to");
            writer.Insert();

            for(int i = 0; i < nMembers; ++i)
            {
                const SOSMMember& member = pMembers[i];
                members.SetInt64(0, nId);
                members.SetInt64(1, i);
                members.SetText(2, std::string(1, member.cType));
                members.SetInt64(3, member.nId);
                if(member.pszRole && *member.pszRole)
                    members.SetText(4, member.pszRole);
                members.Insert();
            }
        }

        void OSMWriteRestriction(COSMTableWriter& writer, int64_t nId, const COSMTags& tags, const char* pszRestriction,
                                 const SOSMMember* pMembers, int nMembers)
        {
            writer.SetInt64(0, nId);
            writer.SetText(1, pszRestriction);
            bool bVia = false;
            for(int i = 0; i < nMembers; ++i)
            {
                const SOSMMember& member = pMembers[i];
                if(!member.pszRole)
                    continue;
                if(strcmp(member.pszRole, "from") == 0 && member.cType == 'w')
                    writer.SetInt64(2, member.nId);
                else if(strcmp(member.pszRole, "via") == 0 && !bVia)    // a chain of via ways: the first one
                {
                    writer.SetText(3, std::string(1, member.cType));
                    writer.SetInt64(4, member.nId);
                    bVia = true;
                }
                else if(strcmp(member.pszRole, "to") == 0 && member.cType == 'w')
                    writer.SetInt64(5, member.nId);
            }
            SetTagText(writer, tags, 6, "except");
            writer.Insert();
        }

        double OSMCreateOverviewIndexes(GeoDatabase::ITransactionPtr ptrTransaction, const IOSMLayer& osmLayer, GeoDatabase::ITablePtr ptrTable)
        {
            auto start = std::chrono::steady_clock::now();
            if(!ptrTransaction.get() || !ptrTable.get() || !ptrTable->GetFields().get())
                return 0.;
            const std::string sTable = osmLayer.GetTableName();
            for(const SOSMOverviewLayer& overview : COSMMapStyle::OverviewLayers(osmLayer.GetName()))
            {
                // the column can be excluded: no overview layer then
                if(overview.sIndexColumn.empty() || ptrTable->GetFields()->FindField(overview.sIndexColumn) < 0)
                    continue;
                // the index only speeds up the query of the overview layer: a database that can't index the column
                // (bptreedb indexes strings up to 256 bytes, a text column without a length is longer) reads it without
                try
                {
                    ptrTransaction->Executequery("CREATE INDEX IF NOT EXISTS \"" + sTable + "_" + overview.sIndexColumn + "\" ON \"" + sTable +
                                                 "\"(\"" + overview.sIndexColumn + "\")");
                }
                catch (std::exception&)
                {
                }
            }
            return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        }

        Cartography::IFeatureLayerPtr COSMMapLayers::CreateLayer(const IOSMLayer& osmLayer, GeoDatabase::ITablePtr ptrTable, CommonLib::ISerializeObjPtr ptrStyle) const
        {
            // the symbols saved with the split files (they can be edited there), otherwise the default style of the layer
            if(!ptrStyle.get())
                return COSMMapStyle::CreateLayer(osmLayer, ptrTable);

            std::shared_ptr<Cartography::CFeatureLayer> ptrLoaded = std::make_shared<Cartography::CFeatureLayer>();
            ptrLoaded->Load(ptrStyle);
            ptrLoaded->SetLayerTable(ptrTable);
            // the label field isn't in the table (the column is excluded): no labels
            const std::string& sLabelField = ptrLoaded->GetLabelFieldName();
            if(!sLabelField.empty() && ptrTable.get() && ptrTable->GetFields().get() && ptrTable->GetFields()->FindField(sLabelField) < 0)
                ptrLoaded->SetLabelRenderer(Cartography::ILabelRendererPtr());
            // a style saved without the scale range (all the scales): the scale of the style, a big map can't draw
            // all its features at the small scales
            if(ptrLoaded->GetMinimumScale() == 0.)
                ptrLoaded->SetMinimumScale(COSMMapStyle::MinScale(osmLayer.GetName()));
            return ptrLoaded;
        }

        void COSMMapLayers::Insert(Cartography::ILayerPtr ptrLayer, int nRank, int nAfter)
        {
            Cartography::ILayersPtr ptrLayers = m_ptrMap->GetLayers();
            int nInsert = -1;
            if(nAfter >= 0)
                nInsert = nAfter + 1 < ptrLayers->GetLayerCount() ? nAfter + 1 : -1;
            else
            {
                // under the first added layer with a higher rank (index 0 is drawn first)
                for(int i = 0; i < ptrLayers->GetLayerCount() && nInsert < 0; ++i)
                {
                    Cartography::ILayerPtr ptrMapLayer = ptrLayers->GetLayer(i);
                    for(size_t j = 0; j < m_vecLayers.size(); ++j)
                    {
                        if(m_vecLayers[j].second == ptrMapLayer && m_vecLayers[j].first > nRank)
                        {
                            nInsert = i;
                            break;
                        }
                    }
                }
            }
            ptrLayers->InsertLayer(ptrLayer, nInsert);
            m_vecLayers.push_back(std::make_pair(nRank, ptrLayer));
        }

        void COSMMapLayers::AddLayer(const IOSMLayer& osmLayer, GeoDatabase::ITablePtr ptrTable, CommonLib::ISerializeObjPtr ptrStyle)
        {
            if(!m_ptrMap->GetSpatialReference().get())
            {
                m_ptrMap->SetSpatialReference(m_projection.CreateSpatialReference());
                m_ptrMap->SetMapUnits(m_projection.GetUnits());
            }
            if(!m_ptrMap->GetBackgroundSymbol().get())
                m_ptrMap->SetBackgroundSymbol(COSMMapStyle::CreateBackground());

            Cartography::IFeatureLayerPtr ptrLayer = CreateLayer(osmLayer, ptrTable, ptrStyle);
            const int nRank = COSMMapStyle::LayerRank(osmLayer.GetName());
            Insert(ptrLayer, nRank, -1);

            // the small scale layers: the same symbols, only the main features, where the layer isn't shown
            const double dMinScale = ptrLayer->GetMinimumScale();
            if(dMinScale == 0.)
                return;
            for(const SOSMOverviewLayer& overview : COSMMapStyle::OverviewLayers(osmLayer.GetName()))
            {
                if(ptrTable.get() && ptrTable->GetFields().get() && ptrTable->GetFields()->FindField(overview.sIndexColumn) < 0)
                    continue;
                Cartography::IFeatureLayerPtr ptrOverview = CreateLayer(osmLayer, ptrTable, ptrStyle);
                ptrOverview->SetName(overview.sName);
                ptrOverview->SetDefinitionQuery(overview.sQuery);
                ptrOverview->SetMinimumScale(0.);
                ptrOverview->SetMaximumScale(dMinScale);

                // right over the layer
                Cartography::ILayersPtr ptrLayers = m_ptrMap->GetLayers();
                int nLayer = -1;
                for(int i = 0; i < ptrLayers->GetLayerCount() && nLayer < 0; ++i)
                {
                    if(ptrLayers->GetLayer(i) == Cartography::ILayerPtr(ptrLayer))
                        nLayer = i;
                }
                Insert(ptrOverview, nRank, nLayer);
            }
        }
    }
}
