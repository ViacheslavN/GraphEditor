#include "WorkspaceHolder.h"

namespace GraphEngine {
    namespace GeoDatabase {

        namespace
        {
            std::mutex& HolderMutex()
            {
                static std::mutex mutex;
                return mutex;
            }

            std::map<CommonLib::CGuid, IWorkspacePtr>& HolderMap()
            {
                static std::map<CommonLib::CGuid, IWorkspacePtr> workspaces;
                return workspaces;
            }
        }

        void CWorkspaceHolder::AddWorkspace(IWorkspacePtr ptrWorkspace)
        {
            if(!ptrWorkspace.get())
                throw CommonLib::CExcBase("WorkspaceHolder: workspace is null");

            std::lock_guard<std::mutex> lock(HolderMutex());
            HolderMap()[ptrWorkspace->GetWorkspaceId()] = ptrWorkspace;
        }

        void CWorkspaceHolder::RemoveWorkspace(const CommonLib::CGuid& id)
        {
            std::lock_guard<std::mutex> lock(HolderMutex());
            HolderMap().erase(id);
        }

        IWorkspacePtr CWorkspaceHolder::GetWorkspace(const CommonLib::CGuid& id)
        {
            std::lock_guard<std::mutex> lock(HolderMutex());
            auto it = HolderMap().find(id);
            if(it == HolderMap().end())
                return IWorkspacePtr();

            return it->second;
        }

        std::vector<IWorkspacePtr> CWorkspaceHolder::GetWorkspaces()
        {
            std::lock_guard<std::mutex> lock(HolderMutex());
            std::vector<IWorkspacePtr> vecWorkspaces;
            for(auto it = HolderMap().begin(); it != HolderMap().end(); ++it)
                vecWorkspaces.push_back(it->second);

            return vecWorkspaces;
        }

        void CWorkspaceHolder::Clear()
        {
            std::lock_guard<std::mutex> lock(HolderMutex());
            HolderMap().clear();
        }

    }
}
