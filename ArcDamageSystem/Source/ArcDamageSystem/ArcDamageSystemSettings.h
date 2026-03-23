// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "ArcDamageSystemSettings.generated.h"

/**
 * Settings for ArcDamageSystem. Configurable in Project Settings > Plugins > Arc Damage System.
 */
UCLASS(config = ArcDamageSystem, defaultconfig, meta = (DisplayName = "Arc Damage System"))
class ARCDAMAGESYSTEM_API UArcDamageSystemSettings : public UDeveloperSettings
{
	GENERATED_BODY()
};
