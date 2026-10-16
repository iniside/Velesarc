// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"

#include "ArcBuilderSettings.generated.h"

class UArcTargetingPreset;

/**
 * Central Project Settings entry for ArcBuilder. Replaces the subsystem's raw
 * UPROPERTY(Config) fields so builder tunables are editable from Project Settings
 * instead of hand-editing DefaultGame.ini. Future builder tunables belong here.
 *
 * Stored in DefaultGame.ini under [/Script/ArcBuilder.ArcBuilderSettings].
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Arc Builder"))
class ARCBUILDER_API UArcBuilderSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UArcBuilderSettings();

	//~UDeveloperSettings interface
	virtual FName GetCategoryName() const override
	{
		return FName(TEXT("Arc Game"));
	}

#if WITH_EDITOR
	virtual FText GetSectionText() const override;
#endif
	//~End of UDeveloperSettings interface

	/** Fallback placement targeting preset used when a building definition has no PlacementTargetingPreset. */
	UPROPERTY(EditAnywhere, Config, Category = "Placement", meta = (DisplayName = "Default Placement Preset"))
	TSoftObjectPtr<UArcTargetingPreset> DefaultPlacementPreset;
};
