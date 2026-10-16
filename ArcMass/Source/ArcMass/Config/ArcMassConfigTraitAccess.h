// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "MassEntityConfigAsset.h"
#include "MassEntityTraitBase.h"

#if WITH_EDITORONLY_DATA

namespace ArcMassConfig
{

/** Access the protected Traits array on FMassEntityConfig via reflection. Returns nullptr if the property is not found. */
ARCMASS_API TArray<TObjectPtr<UMassEntityTraitBase>>* GetMutableTraitsArray(FMassEntityConfig& Config);

} // namespace ArcMassConfig

#endif // WITH_EDITORONLY_DATA
