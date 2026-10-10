#include "OSMConvertSession.h"
#include "OSMReader.h"
#include "OSMTableWriter.h"
#include "OSMWayStore.h"
#include "OSMMapStyle.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>

namespace GraphEngine {
    namespace Convertors {

        namespace
        {
            const uint64_t ProgressStep = 100000;
            const char* LayerPrefix = "layer:";
            const char* TablePrefix = "table:";

            typedef std::vector<std::pair<std::string, std::string> > TTagList;

            // the tags of a stored object as readosm tags (for COSMTags)
            class CStoredTags
            {
            public:
                explicit CStoredTags(const TTagList& tags)
                {
                    m_tags.reserve(tags.size());
                    for(size_t i = 0; i < tags.size(); ++i)
                    {
                        readosm_tag tag = {tags[i].first.c_str(), tags[i].second.c_str()};
                        m_tags.push_back(tag);
                    }
                }
                COSMTags Tags() const { return COSMTags(m_tags.empty() ? nullptr : &m_tags[0], (int)m_tags.size()); }

            private:
                std::vector<readosm_tag> m_tags;
            };

            bool ParseInteger(const char* pszValue, int64_t& nValue)
            {
                if(!pszValue)
                    return false;
                while(*pszValue == ' ')
                    ++pszValue;
                const char* p = pszValue;
                if(*p == '-' || *p == '+')
                    ++p;
                if(*p < '0' || *p > '9')
                    return false;
                nValue = strtoll(pszValue, nullptr, 10);
                return true;
            }

            bool IsYes(const char* pszValue)
            {
                return pszValue && (strcmp(pszValue, "yes") == 0 || strcmp(pszValue, "true") == 0 || strcmp(pszValue, "1") == 0);
            }

            char MemberType(int nType)
            {
                switch(nType)
                {
                    case READOSM_MEMBER_NODE: return 'n';
                    case READOSM_MEMBER_WAY:  return 'w';
                    default:                  return 'r';
                }
            }

            // a feature table of a thematic layer: osm_id, osm_type, name, class (the key), type (the value), the fields of the layer
            class CFeatureWriter
            {
            public:
                CFeatureWriter(const SOSMLayerDef& def, const std::string& sTableName, const std::string& sNameKey,
                               GeoDatabase::IDatabaseWorkspacePtr ptrWorkspace, GeoDatabase::ITransactionPtr ptrTransaction,
                               Geometry::ISpatialReferencePtr ptrSpatRef) :
                        m_def(def), m_sNameKey(sNameKey)
                {
                    std::vector<COSMTableWriter::SField> vecFields = {
                            {"osm_id", GeoDatabase::dtInteger64}, {"osm_type", GeoDatabase::dtString}, {"name", GeoDatabase::dtString},
                            {"class", GeoDatabase::dtString}, {"type", GeoDatabase::dtString}};
                    for(size_t i = 0; i < def.vecFields.size(); ++i)
                        vecFields.push_back({def.vecFields[i].sName, def.vecFields[i].kind == OSMFieldText ? GeoDatabase::dtString : GeoDatabase::dtInteger64});

                    CommonLib::eShapeType shapeType = def.geometryType == OSMGeometryPoint ? CommonLib::shape_type_point :
                                                      def.geometryType == OSMGeometryLine ? CommonLib::shape_type_polyline : CommonLib::shape_type_polygon;
                    m_ptrWriter = std::make_shared<COSMTableWriter>(ptrWorkspace, ptrTransaction, sTableName, vecFields, shapeType, ptrSpatRef);
                }

                void Write(int64_t nId, char cType, const SOSMMatch& match, const COSMTags& tags, CommonLib::IGeoShapePtr ptrShape)
                {
                    m_ptrWriter->SetInt64(0, nId);
                    m_ptrWriter->SetText(1, std::string(1, cType));
                    const char* pszName = m_sNameKey.empty() ? nullptr : tags.Get(m_sNameKey.c_str());
                    if(!pszName || !*pszName)
                        pszName = tags.Get("name");
                    if(pszName && *pszName)
                        m_ptrWriter->SetText(2, pszName);
                    m_ptrWriter->SetText(3, match.sKey);
                    m_ptrWriter->SetText(4, match.sValue);

                    for(size_t i = 0; i < m_def.vecFields.size(); ++i)
                    {
                        const SOSMFieldDef& field = m_def.vecFields[i];
                        const char* pszValue = tags.Get(field.sKey.c_str());
                        int nField = 5 + (int)i;
                        switch(field.kind)
                        {
                            case OSMFieldText:
                                if(pszValue && *pszValue)
                                    m_ptrWriter->SetText(nField, pszValue);
                                break;
                            case OSMFieldInteger:
                            {
                                int64_t nValue = 0;
                                if(ParseInteger(pszValue, nValue))
                                    m_ptrWriter->SetInt64(nField, nValue);
                                break;
                            }
                            case OSMFieldBool:
                                m_ptrWriter->SetInt64(nField, IsYes(pszValue) ? 1 : 0);
                                break;
                            case OSMFieldOneway:
                                m_ptrWriter->SetInt64(nField, IsYes(pszValue) ? 1 :
                                        (pszValue && (strcmp(pszValue, "-1") == 0 || strcmp(pszValue, "reverse") == 0)) ? -1 : 0);
                                break;
                        }
                    }
                    m_ptrWriter->SetShape(ptrShape);
                    m_ptrWriter->Insert();
                }

                COSMTableWriterPtr Writer() const { return m_ptrWriter; }

            private:
                const SOSMLayerDef& m_def;
                std::string         m_sNameKey;
                COSMTableWriterPtr  m_ptrWriter;
            };

            typedef std::shared_ptr<CFeatureWriter> CFeatureWriterPtr;

            // a layer of the schema in the current run
            struct SLayerRun
            {
                IOSMLayerPtr      ptrLayer;     // null - removed from the OSM map
                bool              bSelected = false;   // converted now, earlier or enabled: the tags of its objects go to the tags table
                CFeatureWriterPtr ptrWriter;    // converted now
            };

            // a table of the current run
            struct STableRun
            {
                IOSMTablePtr      ptrTable;
                bool              bSelected = false;
                COSMTableWriterPtr ptrWriter;
                COSMTableWriterPtr ptrMembers;  // routes
            };

            // a multipolygon of a converted layer, built after the ways are read
            struct SMultipolygon
            {
                int64_t              nId;
                int                  nLayer;
                SOSMMatch            match;
                TTagList             tags;
                std::vector<int64_t> vecWays;
            };

            // the state of one conversion run (Convert / ConvertDataset) shared by the passes
            class CRun
            {
            public:
                CRun(const COSMSchema& schema, const SOSMConvertSettings& settings, const COSMProjection& projection,
                     IProgressUpdaterPtr ptrProgress, Display::ITrackCancelPtr ptrCancel) :
                        schema(schema), settings(settings), projection(projection), ptrProgress(ptrProgress), ptrCancel(ptrCancel),
                        vecLayers(schema.Layers().size())
                {}

                bool Step()
                {
                    if(++nObjects % ProgressStep)
                        return true;
                    progress.nFeatures = nFeatures;
                    if(ptrProgress.get())
                        ptrProgress->UpdateStatusInfo(progress);
                    return !ptrCancel.get() || ptrCancel->Continue();
                }

                void Report(eOSMStage stage, const std::string& sMessage)
                {
                    progress.stage = stage;
                    progress.sMessage = sMessage;
                    progress.nFeatures = nFeatures;
                    if(ptrProgress.get())
                        ptrProgress->UpdateStatusInfo(progress);
                }

                void CheckCancel() const
                {
                    if(ptrCancel.get() && !ptrCancel->Continue())
                        throw CommonLib::CExcBase("OSM conversion is canceled");
                }

                // the object belongs to a selected dataset: its tags are written to the tags table
                bool IsSelectedLayer(const SOSMMatch& match) const
                {
                    if(!match.IsValid())
                        return false;
                    const SLayerRun& layer = vecLayers[match.nLayer];
                    return layer.bSelected && layer.ptrLayer->IsValueEnabled(match.sKey, match.sValue);
                }

                static bool IsSelectedTable(const STableRun& table, const char* pszKey, const char* pszValue)
                {
                    return pszValue && table.bSelected && table.ptrTable->IsValueEnabled(pszKey, pszValue);
                }

                // the feature goes to a table of this run
                CFeatureWriter* LayerWriter(const SOSMMatch& match) const
                {
                    if(!match.IsValid())
                        return nullptr;
                    const SLayerRun& layer = vecLayers[match.nLayer];
                    if(!layer.ptrWriter.get() || !layer.ptrLayer->IsValueEnabled(match.sKey, match.sValue))
                        return nullptr;
                    return layer.ptrWriter.get();
                }

                void WriteTags(char cType, int64_t nId, const COSMTags& tags, const readosm_tag* pTags)
                {
                    if(!tagsTable.ptrWriter.get())
                        return;
                    for(int i = 0; i < tags.Count(); ++i)
                    {
                        const char* pszKey = pTags[i].key ? pTags[i].key : "";
                        if(!tagsTable.ptrTable->IsValueEnabled("key", pszKey))
                            continue;
                        COSMTableWriter& writer = *tagsTable.ptrWriter;
                        writer.SetText(0, std::string(1, cType));
                        writer.SetInt64(1, nId);
                        writer.SetText(2, pszKey);
                        writer.SetText(3, pTags[i].value ? pTags[i].value : "");
                        writer.Insert();
                        ++nFeatures;
                    }
                }

                bool Project(int64_t nRef, CommonLib::GisXYPoint& pt)
                {
                    double dLon, dLat;
                    if(!pNodes->Get(nRef, dLon, dLat))
                    {
                        ++nMissingNodes;
                        return false;
                    }
                    projection.Project(dLon, dLat, pt.x, pt.y);
                    return true;
                }

            public:
                const COSMSchema&           schema;
                const SOSMConvertSettings&  settings;
                const COSMProjection&       projection;
                IProgressUpdaterPtr         ptrProgress;
                Display::ITrackCancelPtr    ptrCancel;
                SOSMProgress                progress;
                uint64_t                    nObjects = 0;
                uint64_t                    nFeatures = 0;
                uint64_t                    nMissingNodes = 0;

                std::vector<SLayerRun>      vecLayers;     // by the schema index
                STableRun                   routes;
                STableRun                   restrictions;
                STableRun                   tagsTable;
                bool                        bLines = false;      // line / polygon layers are converted: the nodes are needed
                bool                        bPolygons = false;
                bool                        bPoints = false;

                COSMNodeStore*              pNodes = nullptr;
                bool                        bStoreNodes = false;
                std::vector<SMultipolygon>  vecMultipolygons;
                std::vector<int64_t>        vecMemberWays;  // sorted
                std::unique_ptr<COSMWayStore> ptrWays;
            };

            // pass 1: nodes (coordinates, points) and relations (routes, restrictions, multipolygons)
            class CNodesRelationsHandler : public IOSMReadHandler
            {
            public:
                explicit CNodesRelationsHandler(CRun& run) : m_run(run) {}

                virtual bool OnNode(const readosm_node& node)
                {
                    ++m_run.progress.nNodes;
                    if(node.longitude == READOSM_UNDEFINED || node.latitude == READOSM_UNDEFINED)
                        return m_run.Step();

                    if(m_run.bStoreNodes)
                        m_run.pNodes->Add(node.id, node.longitude, node.latitude);

                    if(node.tag_count > 0)
                    {
                        COSMTags tags(node.tags, node.tag_count);
                        SOSMMatch match = m_run.schema.MatchNode(tags);
                        if(CFeatureWriter* pWriter = m_run.LayerWriter(match))
                        {
                            double x, y;
                            m_run.projection.Project(node.longitude, node.latitude, x, y);
                            pWriter->Write(node.id, 'n', match, tags, COSMGeometry::CreatePoint(x, y));
                            ++m_run.nFeatures;
                        }
                        if(m_run.IsSelectedLayer(match))
                            m_run.WriteTags('n', node.id, tags, node.tags);
                    }
                    return m_run.Step();
                }

                virtual bool OnWay(const readosm_way&)
                {
                    return true;
                }

                virtual bool OnRelation(const readosm_relation& relation)
                {
                    ++m_run.progress.nRelations;
                    if(relation.tag_count == 0)
                        return m_run.Step();

                    COSMTags tags(relation.tags, relation.tag_count);
                    bool bSelected = false;

                    SOSMMatch match = m_run.schema.MatchRelation(tags);
                    if(m_run.LayerWriter(match))
                        AddMultipolygon(relation, match);
                    bSelected = m_run.IsSelectedLayer(match);

                    const char* pszRoute = COSMSchema::MatchRoute(tags);
                    if(pszRoute)
                    {
                        if(m_run.routes.ptrWriter.get() && m_run.routes.ptrTable->IsValueEnabled("route", pszRoute))
                            WriteRoute(relation, tags, pszRoute);
                        bSelected = bSelected || CRun::IsSelectedTable(m_run.routes, "route", pszRoute);
                    }

                    const char* pszRestriction = COSMSchema::MatchRestriction(tags);
                    if(pszRestriction)
                    {
                        if(m_run.restrictions.ptrWriter.get() && m_run.restrictions.ptrTable->IsValueEnabled("restriction", pszRestriction))
                            WriteRestriction(relation, tags, pszRestriction);
                        bSelected = bSelected || CRun::IsSelectedTable(m_run.restrictions, "restriction", pszRestriction);
                    }

                    if(bSelected)
                        m_run.WriteTags('r', relation.id, tags, relation.tags);
                    return m_run.Step();
                }

            private:
                void AddMultipolygon(const readosm_relation& relation, const SOSMMatch& match)
                {
                    SMultipolygon mp;
                    mp.nId = relation.id;
                    mp.nLayer = match.nLayer;
                    mp.match = match;
                    for(int i = 0; i < relation.tag_count; ++i)
                        mp.tags.push_back(std::make_pair(std::string(relation.tags[i].key ? relation.tags[i].key : ""),
                                                         std::string(relation.tags[i].value ? relation.tags[i].value : "")));
                    for(int i = 0; i < relation.member_count; ++i)
                    {
                        if(relation.members[i].member_type == READOSM_MEMBER_WAY)
                        {
                            mp.vecWays.push_back(relation.members[i].id);
                            m_run.vecMemberWays.push_back(relation.members[i].id);
                        }
                    }
                    if(!mp.vecWays.empty())
                        m_run.vecMultipolygons.push_back(std::move(mp));
                }

                static void SetText(COSMTableWriter& writer, const COSMTags& tags, int nField, const char* pszKey)
                {
                    const char* pszValue = tags.Get(pszKey);
                    if(pszValue && *pszValue)
                        writer.SetText(nField, pszValue);
                }

                void WriteRoute(const readosm_relation& relation, const COSMTags& tags, const char* pszRoute)
                {
                    // osm_id, route, name, ref, network, operator, colour, route_from, route_to
                    COSMTableWriter& writer = *m_run.routes.ptrWriter;
                    writer.SetInt64(0, relation.id);
                    writer.SetText(1, pszRoute);
                    const char* pszName = m_run.settings.sNameLanguage.empty() ? nullptr : tags.Get(("name:" + m_run.settings.sNameLanguage).c_str());
                    if(pszName && *pszName)
                        writer.SetText(2, pszName);
                    else
                        SetText(writer, tags, 2, "name");
                    SetText(writer, tags, 3, "ref");
                    SetText(writer, tags, 4, "network");
                    SetText(writer, tags, 5, "operator");
                    SetText(writer, tags, 6, "colour");
                    SetText(writer, tags, 7, "from");
                    SetText(writer, tags, 8, "to");
                    writer.Insert();
                    ++m_run.nFeatures;

                    // route_id, seq, member_type, member_id, role
                    COSMTableWriter& members = *m_run.routes.ptrMembers;
                    for(int i = 0; i < relation.member_count; ++i)
                    {
                        const readosm_member& member = relation.members[i];
                        members.SetInt64(0, relation.id);
                        members.SetInt64(1, i);
                        members.SetText(2, std::string(1, MemberType(member.member_type)));
                        members.SetInt64(3, member.id);
                        if(member.role && *member.role)
                            members.SetText(4, member.role);
                        members.Insert();
                    }
                }

                void WriteRestriction(const readosm_relation& relation, const COSMTags& tags, const char* pszRestriction)
                {
                    // osm_id, restriction, from_way, via_type, via_id, to_way, except_for
                    COSMTableWriter& writer = *m_run.restrictions.ptrWriter;
                    writer.SetInt64(0, relation.id);
                    writer.SetText(1, pszRestriction);
                    bool bVia = false;
                    for(int i = 0; i < relation.member_count; ++i)
                    {
                        const readosm_member& member = relation.members[i];
                        if(!member.role)
                            continue;
                        if(strcmp(member.role, "from") == 0 && member.member_type == READOSM_MEMBER_WAY)
                            writer.SetInt64(2, member.id);
                        else if(strcmp(member.role, "via") == 0 && !bVia)    // a chain of via ways: the first one
                        {
                            writer.SetText(3, std::string(1, MemberType(member.member_type)));
                            writer.SetInt64(4, member.id);
                            bVia = true;
                        }
                        else if(strcmp(member.role, "to") == 0 && member.member_type == READOSM_MEMBER_WAY)
                            writer.SetInt64(5, member.id);
                    }
                    SetText(writer, tags, 6, "except");
                    writer.Insert();
                    ++m_run.nFeatures;
                }

            private:
                CRun& m_run;
            };

            // pass 2: ways (lines, polygons, members of the multipolygons)
            class CWaysHandler : public IOSMReadHandler
            {
            public:
                explicit CWaysHandler(CRun& run) : m_run(run) {}

                virtual bool OnNode(const readosm_node&) { return true; }
                virtual bool OnRelation(const readosm_relation&) { return true; }

                virtual bool OnWay(const readosm_way& way)
                {
                    ++m_run.progress.nWays;
                    if(way.node_ref_count < 2)
                        return m_run.Step();

                    bool bMember = !m_run.vecMemberWays.empty() &&
                                   std::binary_search(m_run.vecMemberWays.begin(), m_run.vecMemberWays.end(), (int64_t)way.id);

                    SOSMMatch line, polygon;
                    CFeatureWriter* pLine = nullptr;
                    CFeatureWriter* pPolygon = nullptr;
                    COSMTags tags(way.tags, way.tag_count);
                    bool bClosed = way.node_ref_count >= 4 && way.node_refs[0] == way.node_refs[way.node_ref_count - 1];
                    if(way.tag_count > 0)
                    {
                        m_run.schema.MatchWay(tags, bClosed, line, polygon);
                        pLine = m_run.LayerWriter(line);
                        pPolygon = m_run.LayerWriter(polygon);
                    }

                    if(pLine || pPolygon || bMember)
                    {
                        m_points.clear();
                        for(int i = 0; i < way.node_ref_count; ++i)
                        {
                            CommonLib::GisXYPoint pt;
                            if(m_run.Project(way.node_refs[i], pt))
                                m_points.push_back(pt);
                        }

                        if(pLine && m_points.size() >= 2)
                        {
                            std::vector<TOSMPoints> parts(1, m_points);
                            CommonLib::IGeoShapePtr ptrShape = COSMGeometry::CreatePolyline(parts);
                            if(ptrShape.get())
                            {
                                pLine->Write(way.id, 'w', line, tags, ptrShape);
                                ++m_run.nFeatures;
                            }
                        }

                        // the ring is closed if the nodes are (a missing node of an extract can make it open)
                        bool bRing = m_points.size() >= 4 && m_points.front().x == m_points.back().x && m_points.front().y == m_points.back().y;
                        if(pPolygon && bRing)
                        {
                            std::vector<TOSMPoints> rings(1, m_points);
                            std::vector<bool> outer(1, true);
                            CommonLib::IGeoShapePtr ptrShape = COSMGeometry::CreatePolygon(rings, outer);
                            if(ptrShape.get())
                            {
                                pPolygon->Write(way.id, 'w', polygon, tags, ptrShape);
                                ++m_run.nFeatures;
                            }
                        }

                        if(bMember && m_points.size() >= 2)
                            m_run.ptrWays->Add(way.id, m_points);
                    }

                    if(way.tag_count > 0 && (m_run.IsSelectedLayer(line) || m_run.IsSelectedLayer(polygon)))
                        m_run.WriteTags('w', way.id, tags, way.tags);
                    return m_run.Step();
                }

            private:
                CRun&      m_run;
                TOSMPoints m_points;
            };

            void Read(const std::string& sPath, IOSMReadHandler& handler, int nObjects)
            {
                if(!COSMReader::Read(sPath, handler, nObjects))
                    throw CommonLib::CExcBase("OSM conversion is canceled");
            }
        }

        COSMConvertSession::COSMConvertSession(const COSMSchema& schema, const SOSMConvertSettings& settings, IOSMMapPtr ptrOSMMap,
                                               Cartography::IMapPtr ptrMap, GeoDatabase::IDatabaseWorkspacePtr ptrWorkspace) :
                m_schema(schema), m_settings(settings), m_ptrOSMMap(ptrOSMMap), m_ptrMap(ptrMap), m_ptrWorkspace(ptrWorkspace),
                m_projection(settings.bWebMercator)
        {
            if(!m_ptrOSMMap.get())
                throw CommonLib::CExcBase("OSM convert session: the OSM map is null");
            if(!m_ptrWorkspace.get())
                throw CommonLib::CExcBase("OSM convert session: the workspace is null");
        }

        COSMConvertSession::~COSMConvertSession()
        {
            try
            {
                Close();
            }
            catch (...)
            {
            }
        }

        void COSMConvertSession::Close()
        {
            m_ptrNodes.reset();
        }

        std::string COSMConvertSession::DatasetKey(const IOSMDatasetPtr& ptrDataset) const
        {
            bool bLayer = std::dynamic_pointer_cast<IOSMLayer>(ptrDataset).get() != nullptr;
            return (bLayer ? LayerPrefix : TablePrefix) + ptrDataset->GetName();
        }

        bool COSMConvertSession::IsConverted(IOSMDatasetPtr ptrDataset) const
        {
            return ptrDataset.get() && m_setConverted.count(DatasetKey(ptrDataset)) > 0;
        }

        // the datasets without features (by ReadMap) are skipped, ConvertDataset creates also an empty table
        void COSMConvertSession::Convert(IProgressUpdaterPtr ptrProgress, Display::ITrackCancelPtr ptrCancel)
        {
            std::vector<IOSMDatasetPtr> vecDatasets;
            for(int i = 0; i < m_ptrOSMMap->GetLayerCount(); ++i)
            {
                IOSMLayerPtr ptrLayer = m_ptrOSMMap->GetLayer(i);
                if(ptrLayer->GetEnabled() && ptrLayer->GetFeatureCount() > 0 && !IsConverted(ptrLayer))
                    vecDatasets.push_back(ptrLayer);
            }
            for(int i = 0; i < m_ptrOSMMap->GetTableCount(); ++i)
            {
                IOSMTablePtr ptrTable = m_ptrOSMMap->GetTable(i);
                if(ptrTable->GetEnabled() && ptrTable->GetFeatureCount() > 0 && !IsConverted(ptrTable))
                    vecDatasets.push_back(ptrTable);
            }
            ConvertDatasets(vecDatasets, ptrProgress, ptrCancel);
        }

        void COSMConvertSession::ConvertDataset(IOSMDatasetPtr ptrDataset, IProgressUpdaterPtr ptrProgress, Display::ITrackCancelPtr ptrCancel)
        {
            if(!ptrDataset.get())
                throw CommonLib::CExcBase("OSM convert session: the dataset is null");
            if(IsConverted(ptrDataset))
                return;
            ConvertDatasets(std::vector<IOSMDatasetPtr>(1, ptrDataset), ptrProgress, ptrCancel);
        }

        void COSMConvertSession::ConvertDatasets(const std::vector<IOSMDatasetPtr>& vecDatasets, IProgressUpdaterPtr ptrProgress, Display::ITrackCancelPtr ptrCancel)
        {
            if(vecDatasets.empty())
                return;

            GeoDatabase::ITransactionPtr ptrTransaction;
            try
            {
                CRun run(m_schema, m_settings, m_projection, ptrProgress, ptrCancel);
                Geometry::ISpatialReferencePtr ptrSpatRef = m_projection.CreateSpatialReference();
                std::string sNameKey = m_settings.sNameLanguage.empty() ? std::string() : "name:" + m_settings.sNameLanguage;

                // the layers of the schema: which are in the OSM map, selected (for the tags table), converted now
                for(size_t i = 0; i < m_schema.Layers().size(); ++i)
                {
                    SLayerRun& layer = run.vecLayers[i];
                    layer.ptrLayer = m_ptrOSMMap->FindLayer(m_schema.Layers()[i].sName);
                    layer.bSelected = layer.ptrLayer.get() && (layer.ptrLayer->GetEnabled() || IsConverted(layer.ptrLayer));
                }
                for(int i = 0; i < m_ptrOSMMap->GetTableCount(); ++i)
                {
                    IOSMTablePtr ptrTable = m_ptrOSMMap->GetTable(i);
                    STableRun& table = ptrTable->GetTableType() == OSMTableRoutes ? run.routes :
                                       ptrTable->GetTableType() == OSMTableRestrictions ? run.restrictions : run.tagsTable;
                    table.ptrTable = ptrTable;
                    table.bSelected = ptrTable->GetEnabled() || IsConverted(ptrTable);
                }

                ptrTransaction = m_ptrWorkspace->StartTransaction(GeoDatabase::ttModify);

                std::vector<IOSMLayerPtr> vecRunLayers;
                for(size_t d = 0; d < vecDatasets.size(); ++d)
                {
                    IOSMDatasetPtr ptrDataset = vecDatasets[d];
                    if(IOSMLayerPtr ptrLayer = std::dynamic_pointer_cast<IOSMLayer>(ptrDataset))
                    {
                        int nIndex = m_schema.FindLayerIndex(ptrLayer->GetName());
                        if(nIndex < 0)
                            throw CommonLib::CExcBase("The layer {0} isn't in the schema", ptrLayer->GetName());
                        const SOSMLayerDef& def = m_schema.Layers()[nIndex];
                        SLayerRun& layer = run.vecLayers[nIndex];
                        layer.ptrLayer = ptrLayer;
                        layer.bSelected = true;
                        layer.ptrWriter = std::make_shared<CFeatureWriter>(def, ptrLayer->GetTableName(), sNameKey, m_ptrWorkspace, ptrTransaction, ptrSpatRef);
                        run.bLines = run.bLines || def.geometryType != OSMGeometryPoint;
                        run.bPolygons = run.bPolygons || def.geometryType == OSMGeometryPolygon;
                        run.bPoints = run.bPoints || def.geometryType == OSMGeometryPoint;
                        vecRunLayers.push_back(ptrLayer);
                    }
                    else if(IOSMTablePtr ptrTable = std::dynamic_pointer_cast<IOSMTable>(ptrDataset))
                    {
                        typedef COSMTableWriter::SField F;
                        const std::string& sName = ptrTable->GetTableName();
                        switch(ptrTable->GetTableType())
                        {
                            case OSMTableRoutes:
                                run.routes.ptrTable = ptrTable;
                                run.routes.bSelected = true;
                                run.routes.ptrWriter = std::make_shared<COSMTableWriter>(m_ptrWorkspace, ptrTransaction, sName, std::vector<F>{
                                        {"osm_id", GeoDatabase::dtInteger64}, {"route", GeoDatabase::dtString}, {"name", GeoDatabase::dtString},
                                        {"ref", GeoDatabase::dtString}, {"network", GeoDatabase::dtString}, {"operator", GeoDatabase::dtString},
                                        {"colour", GeoDatabase::dtString}, {"route_from", GeoDatabase::dtString}, {"route_to", GeoDatabase::dtString}});
                                run.routes.ptrMembers = std::make_shared<COSMTableWriter>(m_ptrWorkspace, ptrTransaction, sName + "_members", std::vector<F>{
                                        {"route_id", GeoDatabase::dtInteger64}, {"seq", GeoDatabase::dtInteger64}, {"member_type", GeoDatabase::dtString},
                                        {"member_id", GeoDatabase::dtInteger64}, {"role", GeoDatabase::dtString}});
                                break;
                            case OSMTableRestrictions:
                                run.restrictions.ptrTable = ptrTable;
                                run.restrictions.bSelected = true;
                                run.restrictions.ptrWriter = std::make_shared<COSMTableWriter>(m_ptrWorkspace, ptrTransaction, sName, std::vector<F>{
                                        {"osm_id", GeoDatabase::dtInteger64}, {"restriction", GeoDatabase::dtString}, {"from_way", GeoDatabase::dtInteger64},
                                        {"via_type", GeoDatabase::dtString}, {"via_id", GeoDatabase::dtInteger64}, {"to_way", GeoDatabase::dtInteger64},
                                        {"except_for", GeoDatabase::dtString}});
                                break;
                            default:
                                run.tagsTable.ptrTable = ptrTable;
                                run.tagsTable.bSelected = true;
                                run.tagsTable.ptrWriter = std::make_shared<COSMTableWriter>(m_ptrWorkspace, ptrTransaction, sName, std::vector<F>{
                                        {"osm_type", GeoDatabase::dtString}, {"osm_id", GeoDatabase::dtInteger64},
                                        {"key", GeoDatabase::dtString}, {"value", GeoDatabase::dtString}});
                                break;
                        }
                    }
                }

                // the tags table also needs the ways when it is converted
                bool bWays = run.bLines || run.tagsTable.ptrWriter.get();

                // pass 1: nodes and relations
                if(run.bLines && !HasNodeStore())
                {
                    std::string sTempDir = m_settings.sTempDir;
                    m_ptrNodes.reset(new COSMNodeStore(sTempDir, (uint64_t)m_settings.nNodeCacheMB * 1024 * 1024));
                    run.bStoreNodes = true;
                }
                run.pNodes = m_ptrNodes.get();

                run.Report(OSMStageNodes, "Reading nodes and relations");
                {
                    CNodesRelationsHandler handler(run);
                    // the nodes are skipped when only the relation tables are converted
                    bool bNodes = run.bStoreNodes || run.bPoints || run.tagsTable.ptrWriter.get();
                    Read(m_ptrOSMMap->GetPath(), handler, (bNodes ? COSMReader::ReadNodes : 0) | COSMReader::ReadRelations);
                }

                if(run.bStoreNodes)
                {
                    run.Report(OSMStageSortNodes, "Indexing node coordinates");
                    m_ptrNodes->Finish([&run](uint64_t nNodes)
                                       {
                                           run.progress.nNodes = nNodes;
                                           if(run.ptrProgress.get())
                                               run.ptrProgress->UpdateStatusInfo(run.progress);
                                           return !run.ptrCancel.get() || run.ptrCancel->Continue();
                                       });
                    run.CheckCancel();
                }

                // pass 2: ways
                if(bWays)
                {
                    std::sort(run.vecMemberWays.begin(), run.vecMemberWays.end());
                    run.vecMemberWays.erase(std::unique(run.vecMemberWays.begin(), run.vecMemberWays.end()), run.vecMemberWays.end());
                    if(!run.vecMemberWays.empty())
                    {
                        uint64_t nWayMemory = (std::max)((uint64_t)16, (uint64_t)m_settings.nNodeCacheMB / 4) * 1024 * 1024;
                        run.ptrWays.reset(new COSMWayStore(m_settings.sTempDir, nWayMemory));
                    }

                    run.Report(OSMStageWays, "Reading ways");
                    CWaysHandler handler(run);
                    Read(m_ptrOSMMap->GetPath(), handler, COSMReader::ReadWays);
                }

                // multipolygons
                if(!run.vecMultipolygons.empty())
                {
                    run.Report(OSMStageMultipolygons, "Building multipolygons");
                    std::vector<TOSMPoints> ways;
                    std::vector<TOSMPoints> rings;
                    std::vector<bool> outer;
                    for(size_t i = 0; i < run.vecMultipolygons.size(); ++i)
                    {
                        SMultipolygon& mp = run.vecMultipolygons[i];
                        ways.clear();
                        for(size_t w = 0; w < mp.vecWays.size(); ++w)
                        {
                            ways.push_back(TOSMPoints());
                            if(!run.ptrWays->Get(mp.vecWays[w], ways.back()))
                                ways.pop_back();
                        }

                        if(COSMGeometry::BuildMultipolygon(ways, rings, outer))
                        {
                            CommonLib::IGeoShapePtr ptrShape = COSMGeometry::CreatePolygon(rings, outer);
                            if(ptrShape.get())
                            {
                                CStoredTags tags(mp.tags);
                                run.vecLayers[mp.nLayer].ptrWriter->Write(mp.nId, 'r', mp.match, tags.Tags(), ptrShape);
                                ++run.nFeatures;
                            }
                        }
                        TTagList().swap(mp.tags);
                        if(!run.Step())
                            throw CommonLib::CExcBase("OSM conversion is canceled");
                    }
                }

                run.Report(OSMStageFinish, "Saving tables");
                std::vector<GeoDatabase::ITablePtr> vecTables(vecRunLayers.size());
                for(size_t i = 0; i < vecRunLayers.size(); ++i)
                {
                    SLayerRun& layer = run.vecLayers[m_schema.FindLayerIndex(vecRunLayers[i]->GetName())];
                    layer.ptrWriter->Writer()->Finish();
                    vecTables[i] = layer.ptrWriter->Writer()->GetTable();
                }
                for(STableRun* pTable : {&run.routes, &run.restrictions, &run.tagsTable})
                {
                    if(pTable->ptrWriter.get())
                        pTable->ptrWriter->Finish();
                    if(pTable->ptrMembers.get())
                        pTable->ptrMembers->Finish();
                }

                ptrTransaction->Commit();
                ptrTransaction.reset();

                for(size_t d = 0; d < vecDatasets.size(); ++d)
                    m_setConverted.insert(DatasetKey(vecDatasets[d]));

                if(m_ptrMap.get() && m_settings.bAddLayersToMap)
                {
                    for(size_t i = 0; i < vecRunLayers.size(); ++i)
                        AddMapLayer(*vecRunLayers[i], vecTables[i]);
                }

                run.Report(OSMStageFinish, "Done");
            }
            catch (std::exception& exc)
            {
                if(ptrTransaction.get())
                {
                    try
                    {
                        ptrTransaction->Rollback();
                    }
                    catch (...)
                    {
                    }
                }
                CommonLib::CExcBase::RegenExc("Failed to convert OSM map {0}", m_ptrOSMMap->GetPath(), exc);
                throw;
            }
        }

        void COSMConvertSession::AddMapLayer(const IOSMLayer& osmLayer, GeoDatabase::ITablePtr ptrTable)
        {
            if(!m_ptrMap->GetSpatialReference().get())
            {
                m_ptrMap->SetSpatialReference(m_projection.CreateSpatialReference());
                m_ptrMap->SetMapUnits(m_projection.GetUnits());
            }
            if(!m_ptrMap->GetBackgroundSymbol().get())
                m_ptrMap->SetBackgroundSymbol(COSMMapStyle::CreateBackground());

            Cartography::IFeatureLayerPtr ptrLayer = COSMMapStyle::CreateLayer(osmLayer, ptrTable);
            int nRank = COSMMapStyle::LayerRank(osmLayer.GetName());

            // under the first added layer with a higher rank (index 0 is drawn first)
            Cartography::ILayersPtr ptrLayers = m_ptrMap->GetLayers();
            int nInsert = -1;
            for(int i = 0; i < ptrLayers->GetLayerCount() && nInsert < 0; ++i)
            {
                Cartography::ILayerPtr ptrMapLayer = ptrLayers->GetLayer(i);
                for(size_t j = 0; j < m_vecMapLayers.size(); ++j)
                {
                    if(m_vecMapLayers[j].second == ptrMapLayer && m_vecMapLayers[j].first > nRank)
                    {
                        nInsert = i;
                        break;
                    }
                }
            }

            ptrLayers->InsertLayer(ptrLayer, nInsert);
            m_vecMapLayers.push_back(std::make_pair(nRank, Cartography::ILayerPtr(ptrLayer)));
        }
    }
}
