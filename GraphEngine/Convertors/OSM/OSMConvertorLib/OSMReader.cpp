#include "OSMReader.h"
#include <exception>

namespace GraphEngine {
    namespace Convertors {

        namespace
        {
            struct SReadContext
            {
                IOSMReadHandler* pHandler = nullptr;
                std::exception_ptr ptrException;
                bool bStopped = false;
            };

            template<class TObject, bool (IOSMReadHandler::*Method)(const TObject&)>
            int Callback(const void* pUserData, const TObject* pObject)
            {
                SReadContext* pContext = (SReadContext*)pUserData;
                try
                {
                    if((pContext->pHandler->*Method)(*pObject))
                        return READOSM_OK;
                    pContext->bStopped = true;
                }
                catch (...)
                {
                    pContext->ptrException = std::current_exception();
                }
                return READOSM_ABORT;
            }

            // closes the handle on any exit
            class CHandleGuard
            {
            public:
                CHandleGuard() : m_pHandle(nullptr) {}
                ~CHandleGuard() { if(m_pHandle) readosm_close(m_pHandle); }
                const void* m_pHandle;
            };
        }

        std::string COSMReader::ErrorText(int nCode)
        {
            switch(nCode)
            {
                case READOSM_OK:                      return "no error";
                case READOSM_INVALID_SUFFIX:          return "the file must have the .osm or .pbf suffix";
                case READOSM_FILE_NOT_FOUND:          return "file not found";
                case READOSM_NULL_HANDLE:             return "null handle";
                case READOSM_INVALID_HANDLE:          return "invalid handle";
                case READOSM_INSUFFICIENT_MEMORY:     return "insufficient memory";
                case READOSM_CREATE_XML_PARSER_ERROR: return "can't create the XML parser";
                case READOSM_READ_ERROR:              return "read error";
                case READOSM_XML_ERROR:               return "XML error";
                case READOSM_INVALID_PBF_HEADER:      return "invalid PBF header";
                case READOSM_UNZIP_ERROR:             return "unzip error";
                case READOSM_ABORT:                   return "aborted";
                default:                              return "error " + std::to_string(nCode);
            }
        }

        bool COSMReader::Read(const std::string& sPath, IOSMReadHandler& handler, int nObjects)
        {
            CHandleGuard guard;
            int nRet = readosm_open(sPath.c_str(), &guard.m_pHandle);
            if(nRet != READOSM_OK)
                throw CommonLib::CExcBase("Failed to open OSM file {0}: {1}", sPath, ErrorText(nRet));

            SReadContext context;
            context.pHandler = &handler;
            nRet = readosm_parse(guard.m_pHandle, &context,
                                 (nObjects & ReadNodes) ? &Callback<readosm_node, &IOSMReadHandler::OnNode> : nullptr,
                                 (nObjects & ReadWays) ? &Callback<readosm_way, &IOSMReadHandler::OnWay> : nullptr,
                                 (nObjects & ReadRelations) ? &Callback<readosm_relation, &IOSMReadHandler::OnRelation> : nullptr);

            if(context.ptrException)
                std::rethrow_exception(context.ptrException);
            if(context.bStopped)
                return false;
            if(nRet != READOSM_OK)
                throw CommonLib::CExcBase("Failed to read OSM file {0}: {1}", sPath, ErrorText(nRet));
            return true;
        }
    }
}
