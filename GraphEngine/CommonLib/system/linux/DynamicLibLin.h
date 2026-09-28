#pragma once

namespace CommonLib {
    namespace system {
        namespace lin {

            class CDynamicLibraryLin
            {
            public:
                CDynamicLibraryLin(const std::string& path);
                CDynamicLibraryLin(const std::wstring& path);
                ~CDynamicLibraryLin();

                CDynamicLibraryLin(const CDynamicLibraryLin&) = delete;
                CDynamicLibraryLin& operator=(const CDynamicLibraryLin&) = delete;

                void*  GetProcAddr(const std::string& proc_name);
                void*  GetProcAddr(const std::wstring& proc_name);

                template <typename TFunkPtrType>
                void GetProcAddrEx(const std::string& procName, TFunkPtrType& result)
                {
                    result = (TFunkPtrType)GetProcAddr(procName);
                }

                template <typename TFunkPtrType>
                void GetProcAddrEx(const std::wstring& procName, TFunkPtrType& result)
                {
                    result = (TFunkPtrType)GetProcAddr(procName);
                }
            private:
                void LoadLib(const std::string& path);
            private:
                void* m_handle;
            };


        }
        typedef lin::CDynamicLibraryLin TDynamicLib;
        typedef std::shared_ptr<TDynamicLib> TDynamicLibPtr;
    }
}