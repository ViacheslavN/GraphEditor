#pragma once
#include "GeoDatabase.h"

namespace GraphEngine {
    namespace GeoDatabase {

        // Process wide registry of opened workspaces.
        // A saved table keeps only the id of its workspace, CDatasetLoader::LoadTable finds the workspace here.
        class CWorkspaceHolder
        {
        public:
            static void AddWorkspace(IWorkspacePtr ptrWorkspace);
            static void RemoveWorkspace(const CommonLib::CGuid& id);
            static IWorkspacePtr GetWorkspace(const CommonLib::CGuid& id);
            static std::vector<IWorkspacePtr> GetWorkspaces();
            static void Clear();
        };

    }
}
