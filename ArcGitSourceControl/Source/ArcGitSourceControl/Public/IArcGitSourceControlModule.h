#pragma once

#include "Modules/ModuleManager.h"

class IArcGitSourceControlModule : public IModuleInterface
{
public:
    static inline IArcGitSourceControlModule& Get()
    {
        return FModuleManager::LoadModuleChecked<IArcGitSourceControlModule>("ArcGitSourceControl");
    }

    static inline bool IsAvailable()
    {
        return FModuleManager::Get().IsModuleLoaded("ArcGitSourceControl");
    }
};
