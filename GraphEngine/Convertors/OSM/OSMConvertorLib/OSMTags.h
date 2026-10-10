#pragma once
#include "readosm/readosm.h"
#include <cstring>

namespace GraphEngine {
    namespace Convertors {

        // tags of an OSM object from readosm (the pointers are valid only in the readosm callback)
        class COSMTags
        {
        public:
            COSMTags(const readosm_tag* pTags, int nCount) : m_pTags(pTags), m_nCount(pTags ? nCount : 0) {}

            int Count() const { return m_nCount; }

            // value of the key, nullptr - no such tag
            const char* Get(const char* pszKey) const
            {
                for(int i = 0; i < m_nCount; ++i)
                {
                    if(m_pTags[i].key && strcmp(m_pTags[i].key, pszKey) == 0)
                        return m_pTags[i].value ? m_pTags[i].value : "";
                }
                return nullptr;
            }

            bool Is(const char* pszKey, const char* pszValue) const
            {
                const char* pszTagValue = Get(pszKey);
                return pszTagValue && strcmp(pszTagValue, pszValue) == 0;
            }

        private:
            const readosm_tag* m_pTags;
            int m_nCount;
        };
    }
}
