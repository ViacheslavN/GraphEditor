#include "OSMConvertor.h"
#include "OSMReader.h"
#include "OSMAnalyzer.h"
#include "OSMProgressThrottle.h"
#include "OSMConvertSession.h"
#include "OSMSplitter.h"
#include "OSMSplitExport.h"
#include "OSMGeometry.h"
#include "../../../GisGeometry/Envelope.h"
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <limits>
#include <unordered_map>

namespace GraphEngine {
    namespace Convertors {

        namespace
        {
            const uint64_t ProgressStep = 100000;

            // Analyze: one pass over the file, only the tags are classified (COSMAnalyzer, no node coordinates are kept)
            class CAnalyzeHandler : public IOSMReadHandler
            {
            public:
                CAnalyzeHandler(const COSMSchema& schema, COSMMapPtr ptrMap, IProgressUpdaterPtr ptrProgress, Display::ITrackCancelPtr ptrCancel) :
                        m_analyzer(schema, ptrMap), m_ptrMap(ptrMap), m_ptrProgress(ptrProgress), m_ptrCancel(ptrCancel)
                {
                    m_progress.stage = OSMStageAnalyze;
                    m_progress.nPass = 1;
                    m_progress.nPassCount = 1;
                    std::error_code ec;
                    uint64_t nFileSize = std::filesystem::file_size(std::filesystem::u8path(ptrMap->GetPath()), ec);
                    m_progress.nFileSize = ec ? 0 : nFileSize;
                }

                virtual bool OnNode(const readosm_node& node)
                {
                    m_analyzer.AddNode(node);
                    return Step();
                }

                virtual bool OnWay(const readosm_way& way)
                {
                    m_analyzer.AddWay(way);
                    return Step();
                }

                virtual bool OnRelation(const readosm_relation& relation)
                {
                    m_analyzer.AddRelation(relation);
                    return Step();
                }

                virtual void OnReaderStats(const SOSMReaderStats& stats)
                {
                    m_reader = stats;
                }

                // .pbf: a block is processed, sent once per second and at the end
                virtual bool OnProgress(uint64_t nBytesRead, uint64_t nBytesTotal)
                {
                    m_progress.nBytesRead = nBytesRead;
                    m_progress.nBytesTotal = nBytesTotal;
                    if(nBytesRead >= nBytesTotal || m_throttle.IsTime())
                        Send();
                    return !m_ptrCancel.get() || m_ptrCancel->Continue();
                }

                void Send()
                {
                    m_progress.nNodes = m_analyzer.NodeCount();
                    m_progress.nWays = m_analyzer.WayCount();
                    m_progress.nRelations = m_analyzer.RelationCount();
                    m_progress.dStageSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - m_start).count();
                    m_progress.vecItems.clear();
                    AddReaderItems(m_progress.vecItems, m_reader, m_ptrMap->GetPath());
                    if(m_ptrProgress.get())
                        m_ptrProgress->UpdateStatusInfo(m_progress);
                }

                void Finish()
                {
                    m_analyzer.Finish();

                    // the details of the analysis (the reading) as the finished stage
                    Send();
                    SOSMStageSummary summary;
                    summary.stage = OSMStageAnalyze;
                    summary.sMessage = "Analyzing the file";
                    summary.dSeconds = m_progress.dStageSeconds;
                    summary.vecItems = m_progress.vecItems;
                    m_progress.vecStages.push_back(summary);
                    m_progress.vecItems.clear();
                    m_progress.sMessage = "Done";
                    if(m_ptrProgress.get())
                        m_ptrProgress->UpdateStatusInfo(m_progress);
                }

            private:
                bool Step()
                {
                    if(++m_nObjects % ProgressStep)
                        return true;
                    // the cancel is checked every ProgressStep objects, the progress is sent once per second
                    if(m_throttle.IsTime())
                        Send();
                    return !m_ptrCancel.get() || m_ptrCancel->Continue();
                }

            private:
                COSMAnalyzer             m_analyzer;
                COSMMapPtr               m_ptrMap;
                IProgressUpdaterPtr      m_ptrProgress;
                Display::ITrackCancelPtr m_ptrCancel;
                SOSMProgress             m_progress;
                uint64_t                 m_nObjects = 0;
                COSMProgressThrottle     m_throttle;
                SOSMReaderStats          m_reader;
                std::chrono::steady_clock::time_point m_start = std::chrono::steady_clock::now();
            };
        }

        std::string COSMConvertor::FormatReport(const SOSMProgress& progress, const std::string& sResult)
        {
            auto time = [](double dSeconds)
            {
                int n = (int)(dSeconds + 0.5);
                char sz[32];
                snprintf(sz, sizeof(sz), "%02d:%02d:%02d", n / 3600, (n / 60) % 60, n % 60);
                return std::string(sz);
            };
            auto bytes = [](uint64_t nBytes)
            {
                char sz[32];
                if(nBytes == 0)
                    return std::string();
                if(nBytes >= 1024ull * 1024 * 1024)
                    snprintf(sz, sizeof(sz), "%.2f GB", nBytes / (1024. * 1024 * 1024));
                else
                    snprintf(sz, sizeof(sz), "%.1f MB", nBytes / (1024. * 1024));
                return std::string(sz);
            };

            // the finished stages and (a canceled / failed task) the stage which was running: its last details
            std::vector<SOSMStageSummary> stages = progress.vecStages;
            if(!progress.vecItems.empty() || (progress.dStageSeconds > 0 && progress.sMessage != "Done"))
            {
                SOSMStageSummary current;
                current.stage = progress.stage;
                current.sMessage = progress.sMessage + " (not finished)";
                current.dSeconds = progress.dStageSeconds;
                current.vecItems = progress.vecItems;
                stages.push_back(current);
            }

            std::string sText;
            if(!sResult.empty())
                sText += "Result: " + sResult + "\n";
            double dTotal = 0.;
            for(const SOSMStageSummary& stage : stages)
                dTotal += stage.dSeconds;
            sText += "Total: " + time(dTotal) + "\n";
            char szCounts[256];
            snprintf(szCounts, sizeof(szCounts), "Nodes: %llu, ways: %llu, relations: %llu, written features / records: %llu\n",
                     (unsigned long long)progress.nNodes, (unsigned long long)progress.nWays, (unsigned long long)progress.nRelations,
                     (unsigned long long)progress.nFeatures);
            sText += szCounts;
            for(const SOSMStageSummary& stage : stages)
            {
                char szLine[256];
                snprintf(szLine, sizeof(szLine), "\n%s: %s (%.0f%%)\n", stage.sMessage.c_str(), time(stage.dSeconds).c_str(),
                         dTotal > 0 ? 100. * stage.dSeconds / dTotal : 0.);
                sText += szLine;
                for(const SOSMProgressItem& item : stage.vecItems)
                {
                    // name, count, data (MB/s of the stage), busy time (% of the stage), info
                    std::string sCount = item.nCount ? std::to_string(item.nCount) : std::string();
                    std::string sData = bytes(item.nBytes);
                    if(item.nBytes && stage.dSeconds > 0)
                    {
                        char szRate[32];
                        snprintf(szRate, sizeof(szRate), " (%.0f MB/s)", item.nBytes / (1024. * 1024) / stage.dSeconds);
                        sData += szRate;
                    }
                    char szBusy[64] = "";
                    if(item.dSeconds > 0)
                        snprintf(szBusy, sizeof(szBusy), "%.1f s%s %3.0f%%", item.dSeconds, item.bThreads ? " (threads)" : "",
                                 stage.dSeconds > 0 ? 100. * item.dSeconds / stage.dSeconds : 0.);
                    snprintf(szLine, sizeof(szLine), "  %-34s %16s %22s %24s  ", item.sName.c_str(), sCount.c_str(), sData.c_str(), szBusy);
                    sText += szLine + item.sInfo + "\n";
                }
            }
            return sText;
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

        IOSMMapPtr COSMConvertor::Analyze(const std::string& path, IProgressUpdaterPtr ptrProgress, Display::ITrackCancelPtr ptrCancel)
        {
            try
            {
                eOSMFileFormat format = COSMMap::FormatByPath(path);
                if(format == OSMFormatUnknown)
                    throw CommonLib::CExcBase("Unknown OSM file format, expected .osm, .pbf or osm_split.xml");
                // the split data are analyzed already
                if(format == OSMFormatSplit)
                {
                    COSMSplitMeta meta;
                    meta.Load(path);
                    return meta.GetMap();
                }

                COSMMapPtr ptrMap = m_schema.CreateMap();
                ptrMap->SetPath(path);

                CAnalyzeHandler handler(m_schema, ptrMap, ptrProgress, ptrCancel);
                if(!COSMReader::Read(path, handler, COSMReader::ReadAll, m_settings.nThreads))
                    throw CommonLib::CExcBase("Analysis is canceled");

                handler.Finish();
                return ptrMap;
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to analyze OSM file {0}", path, exc);
                throw;
            }
        }

        IOSMMapPtr COSMConvertor::CreateMap(const std::string& path)
        {
            eOSMFileFormat format = COSMMap::FormatByPath(path);
            if(format == OSMFormatUnknown)
                throw CommonLib::CExcBase("Unknown OSM file format {0}, expected .osm, .pbf or osm_split.xml", path);
            if(format == OSMFormatSplit)
            {
                COSMSplitMeta meta;
                meta.Load(path);
                return meta.GetMap();
            }

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

        void COSMConvertor::Convert(IOSMMapPtr ptrOSMMap, const CLayerColumnExcludeList& excludeList, Cartography::IMapPtr ptrMap,
                                    GeoDatabase::IDatabaseWorkspacePtr ptrDstWorkspace, IProgressUpdaterPtr ptrProgress, Display::ITrackCancelPtr ptrCancel)
        {
            // the split data: exported from their files, an OSM file: converted in passes over it
            if(ptrOSMMap.get() && ptrOSMMap->GetFormat() == OSMFormatSplit)
            {
                COSMSplitExport splitExport(m_schema, m_settings, ptrOSMMap, excludeList, ptrMap, ptrDstWorkspace);
                splitExport.Convert(ptrProgress, ptrCancel);
                return;
            }
            COSMConvertSession session(m_schema, m_settings, ptrOSMMap, excludeList, ptrMap, ptrDstWorkspace);
            session.Convert(ptrProgress, ptrCancel);
        }

        IOSMMapPtr COSMConvertor::Split(const std::string& path, const std::string& sFolder, bool bKeepNodeFile,
                                        IProgressUpdaterPtr ptrProgress, Display::ITrackCancelPtr ptrCancel)
        {
            COSMSplitter splitter(m_schema, m_settings);
            return splitter.Split(path, sFolder, bKeepNodeFile, ptrProgress, ptrCancel);
        }

        void COSMConvertor::ConvertFile(const std::string& path, eOSMFileFormat format, Cartography::IMapPtr ptrMap,
                                        GeoDatabase::IDatabaseWorkspacePtr ptrDstWorkspace, IProgressUpdaterPtr ptrProgress, Display::ITrackCancelPtr ptrCancel)
        {
            if(COSMMap::FormatByPath(path) != format)
                throw CommonLib::CExcBase("Wrong OSM file {0}: expected the {1} suffix", path, format == OSMFormatPBF ? ".pbf" : ".osm");

            Convert(CreateMap(path), CLayerColumnExcludeList(), ptrMap, ptrDstWorkspace, ptrProgress, ptrCancel);
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
