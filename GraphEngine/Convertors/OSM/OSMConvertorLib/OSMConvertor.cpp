#include "OSMConvertor.h"
#include "OSMReader.h"
#include "OSMConvertSession.h"
#include "OSMGeometry.h"
#include "../../../GisGeometry/Envelope.h"
#include <limits>

namespace GraphEngine {
    namespace Convertors {

        namespace
        {
            const uint64_t ProgressStep = 100000;

            // ReadMap: one pass over the file, only the tags are classified (no node coordinates are kept)
            class CMapReadHandler : public IOSMReadHandler
            {
            public:
                CMapReadHandler(const COSMSchema& schema, COSMMapPtr ptrMap, IProgressUpdaterPtr ptrProgress, Display::ITrackCancelPtr ptrCancel) :
                        m_schema(schema), m_ptrMap(ptrMap), m_ptrProgress(ptrProgress), m_ptrCancel(ptrCancel)
                {
                    m_bounds.xMin = m_bounds.yMin = (std::numeric_limits<double>::max)();
                    m_bounds.xMax = m_bounds.yMax = -(std::numeric_limits<double>::max)();
                    m_progress.stage = OSMStageReadMap;
                    m_ptrRoutes = m_ptrMap->FindTableByType(OSMTableRoutes);
                    m_ptrRestrictions = m_ptrMap->FindTableByType(OSMTableRestrictions);
                    m_ptrTags = m_ptrMap->FindTableByType(OSMTableTags);
                    if(m_ptrTags.get())
                        m_ptrTagKey = m_ptrTags->Keys().Find(COSMSchema::TableKey(OSMTableTags));
                }

                virtual bool OnNode(const readosm_node& node)
                {
                    ++m_progress.nNodes;
                    CheckOrder(0, node.id);

                    if(node.longitude != READOSM_UNDEFINED && node.latitude != READOSM_UNDEFINED)
                    {
                        m_bounds.xMin = (std::min)(m_bounds.xMin, node.longitude);
                        m_bounds.xMax = (std::max)(m_bounds.xMax, node.longitude);
                        m_bounds.yMin = (std::min)(m_bounds.yMin, node.latitude);
                        m_bounds.yMax = (std::max)(m_bounds.yMax, node.latitude);
                    }

                    if(node.tag_count > 0 && Add(m_schema.MatchNode(COSMTags(node.tags, node.tag_count))))
                        AddTags(node.tags, node.tag_count);
                    return Step();
                }

                virtual bool OnWay(const readosm_way& way)
                {
                    ++m_progress.nWays;
                    CheckOrder(1, way.id);

                    if(way.tag_count > 0 && way.node_ref_count >= 2)
                    {
                        bool bClosed = way.node_ref_count >= 4 && way.node_refs[0] == way.node_refs[way.node_ref_count - 1];
                        SOSMMatch line, polygon;
                        m_schema.MatchWay(COSMTags(way.tags, way.tag_count), bClosed, line, polygon);
                        bool bLine = Add(line);
                        bool bPolygon = Add(polygon);
                        if(bLine || bPolygon)
                            AddTags(way.tags, way.tag_count);
                    }
                    return Step();
                }

                virtual bool OnRelation(const readosm_relation& relation)
                {
                    ++m_progress.nRelations;
                    CheckOrder(2, relation.id);

                    if(relation.tag_count > 0)
                    {
                        COSMTags tags(relation.tags, relation.tag_count);
                        bool bMatched = Add(m_schema.MatchRelation(tags));
                        const char* pszRoute = COSMSchema::MatchRoute(tags);
                        if(pszRoute && m_ptrRoutes.get())
                        {
                            m_ptrRoutes->AddFeature(COSMSchema::TableKey(OSMTableRoutes), pszRoute);
                            bMatched = true;
                        }
                        const char* pszRestriction = COSMSchema::MatchRestriction(tags);
                        if(pszRestriction && m_ptrRestrictions.get())
                        {
                            m_ptrRestrictions->AddFeature(COSMSchema::TableKey(OSMTableRestrictions), pszRestriction);
                            bMatched = true;
                        }
                        if(bMatched)
                            AddTags(relation.tags, relation.tag_count);
                    }
                    return Step();
                }

                void Finish()
                {
                    m_ptrMap->SetCounts(m_progress.nNodes, m_progress.nWays, m_progress.nRelations);
                    m_ptrMap->SetSorted(m_bSorted);
                    m_ptrMap->SetScanned(true);
                    if(m_bounds.xMin <= m_bounds.xMax)
                    {
                        m_bounds.type = CommonLib::bbox_type_normal;
                        m_ptrMap->SetBounds(m_bounds);
                    }
                    m_ptrMap->SortValues();

                    m_progress.sMessage = "Done";
                    if(m_ptrProgress.get())
                        m_ptrProgress->UpdateStatusInfo(m_progress);
                }

            private:
                bool Add(const SOSMMatch& match)
                {
                    if(!match.IsValid())
                        return false;
                    m_ptrMap->GetLayerImpl(match.nLayer)->AddFeature(match.sKey, match.sValue);
                    return true;
                }

                // tags table: the number of records (tags), its key "key" counts the objects having an OSM key
                void AddTags(const readosm_tag* pTags, int nCount)
                {
                    if(!m_ptrTagKey.get())
                        return;
                    m_ptrTags->AddCount(nCount);
                    for(int i = 0; i < nCount; ++i)
                        m_ptrTagKey->AddFeature(pTags[i].key ? pTags[i].key : "");
                }

                // nodes, ways, relations, every type sorted by id
                void CheckOrder(int nType, int64_t nId)
                {
                    if(!m_bSorted)
                        return;
                    if(nType < m_nLastType || (nType == m_nLastType && nId <= m_nLastId))
                        m_bSorted = false;
                    m_nLastType = nType;
                    m_nLastId = nId;
                }

                bool Step()
                {
                    if(++m_nObjects % ProgressStep)
                        return true;

                    if(m_ptrProgress.get())
                        m_ptrProgress->UpdateStatusInfo(m_progress);
                    return !m_ptrCancel.get() || m_ptrCancel->Continue();
                }

            private:
                const COSMSchema&        m_schema;
                COSMMapPtr               m_ptrMap;
                COSMTablePtr             m_ptrRoutes;
                COSMTablePtr             m_ptrRestrictions;
                COSMTablePtr             m_ptrTags;
                COSMTagKeyPtr            m_ptrTagKey;
                IProgressUpdaterPtr      m_ptrProgress;
                Display::ITrackCancelPtr m_ptrCancel;
                SOSMProgress             m_progress;
                CommonLib::bbox          m_bounds;
                uint64_t                 m_nObjects = 0;
                bool                     m_bSorted = true;
                int                      m_nLastType = 0;
                int64_t                  m_nLastId = (std::numeric_limits<int64_t>::min)();
            };
        }

        COSMConvertor::COSMConvertor()
        {

        }

        COSMConvertor::COSMConvertor(const SOSMConvertSettings& settings, const COSMSchema& schema) : m_schema(schema), m_settings(settings)
        {

        }

        COSMConvertor::~COSMConvertor()
        {

        }

        IOSMMapPtr COSMConvertor::ReadMap(const std::string& path, IProgressUpdaterPtr ptrProgress, Display::ITrackCancelPtr ptrCancel)
        {
            try
            {
                if(COSMMap::FormatByPath(path) == OSMFormatUnknown)
                    throw CommonLib::CExcBase("Unknown OSM file format, expected .osm or .pbf");

                COSMMapPtr ptrMap = m_schema.CreateMap();
                ptrMap->SetPath(path);

                CMapReadHandler handler(m_schema, ptrMap, ptrProgress, ptrCancel);
                if(!COSMReader::Read(path, handler))
                    throw CommonLib::CExcBase("Reading is canceled");

                handler.Finish();
                return ptrMap;
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to read OSM map {0}", path, exc);
                throw;
            }
        }

        IOSMMapPtr COSMConvertor::CreateMap(const std::string& path)
        {
            if(COSMMap::FormatByPath(path) == OSMFormatUnknown)
                throw CommonLib::CExcBase("Unknown OSM file format {0}, expected .osm or .pbf", path);

            COSMMapPtr ptrMap = m_schema.CreateMap();
            ptrMap->SetPath(path);
            return ptrMap;
        }

        CommonLib::bbox COSMConvertor::OutputExtent(const SOSMConvertSettings& settings, IOSMMapPtr ptrOSMMap)
        {
            // longitude / latitude: the bounds of the nodes or the world (Web Mercator stops at 85.0511 degrees)
            CommonLib::bbox lonLat;
            lonLat.type = CommonLib::bbox_type_normal;
            lonLat.xMin = -180.;
            lonLat.xMax = 180.;
            lonLat.yMin = settings.bWebMercator ? -85.0511287798 : -90.;
            lonLat.yMax = settings.bWebMercator ? 85.0511287798 : 90.;
            if(ptrOSMMap.get() && ptrOSMMap->IsScanned() && ptrOSMMap->GetBounds().type == CommonLib::bbox_type_normal)
                lonLat = ptrOSMMap->GetBounds();

            COSMProjection projection(settings.bWebMercator);
            CommonLib::bbox extent;
            extent.type = CommonLib::bbox_type_normal;
            projection.Project(lonLat.xMin, lonLat.yMin, extent.xMin, extent.yMin);
            projection.Project(lonLat.xMax, lonLat.yMax, extent.xMax, extent.yMax);
            return extent;
        }

        GeometryCompression::SShapeCompressParams COSMConvertor::CompressParams(const SOSMConvertSettings& settings, IOSMMapPtr ptrOSMMap)
        {
            CommonLib::bbox extent = OutputExtent(settings, ptrOSMMap);
            switch(settings.compression.scale)
            {
                case OSMCompressScaleManual:
                {
                    if(settings.compression.nManualScaleExponent < -22 || settings.compression.nManualScaleExponent > 22)
                        throw CommonLib::CExcBase("Wrong scale of the compressed coordinates 10^-{0}, expected -22..22", settings.compression.nManualScaleExponent);
                    GeometryCompression::SShapeCompressParams params;
                    params.nScaleExponent = settings.compression.nManualScaleExponent;
                    return params;
                }
                case OSMCompressScaleMaximum:
                    return GeometryCompression::MaxCompressParamsForExtent(extent);
                default:
                {
                    COSMProjection projection(settings.bWebMercator);
                    Geometry::CEnvelope envelope(extent, projection.CreateSpatialReference());
                    return envelope.GetCompressParams();
                }
            }
        }

        GeometryCompression::SShapeCompressParams COSMConvertor::GetCompressParams(IOSMMapPtr ptrOSMMap) const
        {
            return CompressParams(m_settings, ptrOSMMap);
        }

        IOSMConvertSessionPtr COSMConvertor::CreateSession(IOSMMapPtr ptrOSMMap, Cartography::IMapPtr ptrMap, GeoDatabase::IDatabaseWorkspacePtr ptrDstWorkspace)
        {
            return std::make_shared<COSMConvertSession>(m_schema, m_settings, ptrOSMMap, ptrMap, ptrDstWorkspace);
        }

        void COSMConvertor::Convert(IOSMMapPtr ptrOSMMap, Cartography::IMapPtr ptrMap,
                                    GeoDatabase::IDatabaseWorkspacePtr ptrDstWorkspace, IProgressUpdaterPtr ptrProgress, Display::ITrackCancelPtr ptrCancel)
        {
            IOSMConvertSessionPtr ptrSession = CreateSession(ptrOSMMap, ptrMap, ptrDstWorkspace);
            ptrSession->Convert(ptrProgress, ptrCancel);
            ptrSession->Close();
        }

        void COSMConvertor::ConvertFile(const std::string& path, eOSMFileFormat format, Cartography::IMapPtr ptrMap,
                                        GeoDatabase::IDatabaseWorkspacePtr ptrDstWorkspace, IProgressUpdaterPtr ptrProgress, Display::ITrackCancelPtr ptrCancel)
        {
            if(COSMMap::FormatByPath(path) != format)
                throw CommonLib::CExcBase("Wrong OSM file {0}: expected the {1} suffix", path, format == OSMFormatPBF ? ".pbf" : ".osm");

            Convert(CreateMap(path), ptrMap, ptrDstWorkspace, ptrProgress, ptrCancel);
        }

        void COSMConvertor::ConvertFromXML(const std::string& path, Cartography::IMapPtr ptrMap,
                                           GeoDatabase::IDatabaseWorkspacePtr ptrDstWorkspace, IProgressUpdaterPtr ptrProgress, Display::ITrackCancelPtr ptrCancel)
        {
            ConvertFile(path, OSMFormatXML, ptrMap, ptrDstWorkspace, ptrProgress, ptrCancel);
        }

        void COSMConvertor::ConvertFromPBF(const std::string& path, Cartography::IMapPtr ptrMap,
                                           GeoDatabase::IDatabaseWorkspacePtr ptrDstWorkspace, IProgressUpdaterPtr ptrProgress, Display::ITrackCancelPtr ptrCancel)
        {
            ConvertFile(path, OSMFormatPBF, ptrMap, ptrDstWorkspace, ptrProgress, ptrCancel);
        }
    }
}
