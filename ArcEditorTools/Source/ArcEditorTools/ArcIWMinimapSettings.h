// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "ArcIWMinimapSettings.generated.h"

/**
 * Settings for the ArcIW Minimap editor tab.
 * Accessible via Project Settings > Plugins > ArcIW Minimap.
 */
UCLASS(config = EditorPerProjectUserSettings, defaultconfig, meta = (DisplayName = "ArcIW Minimap"))
class ARCEDITORTOOLS_API UArcIWMinimapSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return FName(TEXT("Plugins")); }

	static const UArcIWMinimapSettings* Get() { return GetDefault<UArcIWMinimapSettings>(); }

	/** Per-actor-class color overrides. Classes not in this map get a deterministic hash-based color. */
	UPROPERTY(EditAnywhere, config, Category = "ArcIW Minimap")
	TMap<TSoftClassPtr<AActor>, FLinearColor> ActorClassColors;

	/** Seconds between entity data refreshes. */
	UPROPERTY(EditAnywhere, config, Category = "ArcIW Minimap", meta = (ClampMin = "0.05", ClampMax = "10.0"))
	float RefreshInterval = 0.5f;

	/** Radius of entity dots in pixels. */
	UPROPERTY(EditAnywhere, config, Category = "ArcIW Minimap", meta = (ClampMin = "1.0", ClampMax = "20.0"))
	float EntityDotRadius = 3.0f;

	/** World-space distance between grid lines (cm). */
	UPROPERTY(EditAnywhere, config, Category = "ArcIW Minimap", meta = (ClampMin = "100.0"))
	float GridSpacing = 10000.0f;
};
