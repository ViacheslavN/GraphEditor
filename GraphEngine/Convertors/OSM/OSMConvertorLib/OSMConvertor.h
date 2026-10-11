#pragma once
#include "OSMConvertorLib.h"
#include "OSMSchema.h"

namespace GraphEngine {
    namespace Convertors {

        class COSMConvertor : public IOSMConvertor
        {
        public:
            COSMConvertor();
            explicit COSMConvertor(const SOSMConvertSettings& settings, const COSMSchema& schema = COSMSchema());
            virtual ~COSMConvertor();

            virtual IOSMMapPtr Analyze(const std::string& path, IProgressUpdaterPtr ptrProgress, Display::ITrackCancelPtr ptrCancel);
            virtual IOSMMapPtr CreateMap(const std::string& path);
            virtual GeometryCompression::SShapeCompressParams GetCompressParams(IOSMMapPtr ptrOSMMap) const;

            // the extent of the map in the output coordinate system (see GetCompressParams)
            static CommonLib::bbox OutputExtent(const SOSMConvertSettings& settings, IOSMMapPtr ptrOSMMap);
            static GeometryCompression::SShapeCompressParams CompressParams(const SOSMConvertSettings& settings, IOSMMapPtr ptrOSMMap);

            virtual void Convert(IOSMMapPtr ptrOSMMap, const CLayerColumnExcludeList& excludeList, Cartography::IMapPtr ptrMap,
                GeoDatabase::IDatabaseWorkspacePtr ptrDstWorkspace, IProgressUpdaterPtr ptrProgress, Display::ITrackCancelPtr ptrCancel);

            virtual IOSMMapPtr Split(const std::string& path, const std::string& sFolder, bool bKeepNodeFile,
                                     IProgressUpdaterPtr ptrProgress, Display::ITrackCancelPtr ptrCancel);

            virtual void ConvertFromXML(const std::string& path, Cartography::IMapPtr ptrMap ,
                GeoDatabase::IDatabaseWorkspacePtr ptrDstWorkspace, IProgressUpdaterPtr ptrProgress, Display::ITrackCancelPtr ptrCancel);

            virtual void ConvertFromPBF(const std::string& path, Cartography::IMapPtr ptrMap ,
                GeoDatabase::IDatabaseWorkspacePtr ptrDstWorkspace, IProgressUpdaterPtr ptrProgress, Display::ITrackCancelPtr ptrCancel);

            // the stages of a task with their items (SOSMProgress::vecStages, the last details of a stage which didn't finish)
            // as text, for a log; sResult - the first line ("done", "canceled", the error)
            static std::string FormatReport(const SOSMProgress& progress, const std::string& sResult = std::string());

            const COSMSchema& GetSchema() const { return m_schema; }
            const SOSMConvertSettings& GetSettings() const { return m_settings; }
            void SetSettings(const SOSMConvertSettings& settings) { m_settings = settings; }

        private:
            void ConvertFile(const std::string& path, eOSMFileFormat format, Cartography::IMapPtr ptrMap,
                GeoDatabase::IDatabaseWorkspacePtr ptrDstWorkspace, IProgressUpdaterPtr ptrProgress, Display::ITrackCancelPtr ptrCancel);

        private:
            COSMSchema          m_schema;
            SOSMConvertSettings m_settings;
        };
    }
}
