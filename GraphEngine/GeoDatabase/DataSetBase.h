#pragma once

#include "GeoDatabase.h"

namespace GraphEngine {
    namespace GeoDatabase {

        template< class I>
        class IDataSetBase : public I
        {
        public:
            IDataSetBase(eDatasetType datasetType,  std::string sDatasetName,  std::string sDatasetViewName) :
                    m_DatasetType(dtUndefined)
                    ,m_sDatasetName(sDatasetName)
                    ,m_sDatasetViewName(sDatasetViewName)

            {

            }

            virtual ~IDataSetBase()
            {

            }

            virtual eDatasetType  GetDatasetType() const
            {
                return m_DatasetType;
            }

            const std::string&   GetDatasetName() const
            {
                return m_sDatasetName;
            }

            const  std::string&   GetDatasetViewName() const
            {
                return m_sDatasetViewName;
            }

        protected:

            eDatasetType m_DatasetType;
            std::string m_sDatasetName;
            std::string m_sDatasetViewName;
        };


        }
    }