#pragma once

#include "Modules/ModuleManager.h"

class IArcLoreSourceControlModule : public IModuleInterface
{
public:
    static inline IArcLoreSourceControlModule& Get()
    {
        return FModuleManager::LoadModuleChecked<IArcLoreSourceControlModule>("ArcLore");
    }

    static inline bool IsAvailable()
    {
        return FModuleManager::Get().IsModuleLoaded("ArcLore");
    }
};
