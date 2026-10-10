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

            virtual IOSMMapPtr ReadMap(const std::string& path, IProgressUpdaterPtr ptrProgress, Display::ITrackCancelPtr ptrCancel);

            virtual IOSMConvertSessionPtr CreateSession(IOSMMapPtr ptrOSMMap, Cartography::IMapPtr ptrMap, GeoDatabase::IDatabaseWorkspacePtr ptrDstWorkspace);

            virtual void Convert(IOSMMapPtr ptrOSMMap, Cartography::IMapPtr ptrMap,
                GeoDatabase::IDatabaseWorkspacePtr ptrDstWorkspace, IProgressUpdaterPtr ptrProgress, Display::ITrackCancelPtr ptrCancel);

            virtual void ConvertFromXML(const std::string& path, Cartography::IMapPtr ptrMap ,
                GeoDatabase::IDatabaseWorkspacePtr ptrDstWorkspace, IProgressUpdaterPtr ptrProgress, Display::ITrackCancelPtr ptrCancel);

            virtual void ConvertFromPBF(const std::string& path, Cartography::IMapPtr ptrMap ,
                GeoDatabase::IDatabaseWorkspacePtr ptrDstWorkspace, IProgressUpdaterPtr ptrProgress, Display::ITrackCancelPtr ptrCancel);

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
