#pragma once
#include "OSMConvertorLib.h"
#include "OSMSchema.h"
#include "OSMSplitFormat.h"
#include "../../../../CommonLib/xml/XMLDoc.h"

namespace GraphEngine {
    namespace Convertors {

        // osm_split.xml: the source file, the analyzed map, the symbols of the layers
        class COSMSplitMeta
        {
        public:
            // sNodeFile - the kept coordinates of all the nodes: nodes.flat / nodes.sorted, empty - none
            static void Save(const std::string& sMetaPath, const COSMMap& map, const std::string& sSourcePath, uint64_t nSourceSize,
                             const std::string& sNodeFile);

            void Load(const std::string& sMetaPath);
            COSMMapPtr GetMap() const { return m_ptrMap; }
            const std::string& GetSourcePath() const { return m_sSourcePath; }
            const std::string& GetNodeFile() const { return m_sNodeFile; }   // nodes.flat / nodes.sorted, empty - none
            // the saved feature layer of a layer (its symbols), null - none
            CommonLib::ISerializeObjPtr GetStyle(const std::string& sLayer) const;

        private:
            CommonLib::xml::CXMLDoc     m_doc;          // the styles are read from it
            CommonLib::ISerializeObjPtr m_ptrStyles;
            COSMMapPtr                  m_ptrMap;
            std::string                 m_sSourcePath;
            std::string                 m_sNodeFile;
        };

        // splits an OSM file into the split data (see OSMSplitFormat.h, IOSMConvertor::Split)
        class COSMSplitter
        {
        public:
            COSMSplitter(const COSMSchema& schema, const SOSMConvertSettings& settings);

            COSMMapPtr Split(const std::string& sOSMPath, const std::string& sFolder, bool bKeepNodeFile,
                             IProgressUpdaterPtr ptrProgress, Display::ITrackCancelPtr ptrCancel);

        private:
            // bTwoPasses - the nodes first, then the ways and the relations (an unsorted file)
            COSMMapPtr Run(const std::string& sOSMPath, const std::string& sFolder, bool bKeepNodeFile, bool bTwoPasses,
                           IProgressUpdaterPtr ptrProgress, Display::ITrackCancelPtr ptrCancel);

        private:
            COSMSchema          m_schema;
            SOSMConvertSettings m_settings;
        };
    }
}
