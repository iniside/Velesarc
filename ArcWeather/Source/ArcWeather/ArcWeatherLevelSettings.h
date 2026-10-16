// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "ArcWeatherLevelSettings.generated.h"

// Thresholds that map climate to the shared 0-3 rain, cold and heat levels.
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Arc Weather Levels"))
class ARCWEATHER_API UArcWeatherLevelSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return FName(TEXT("Arc Game")); }

	UPROPERTY(config, EditAnywhere, Category = "Cold", meta = (ToolTip = "Cold level 1 when temperature (C) is at or below this value."))
	float ColdLevel1 = 10.f;

	UPROPERTY(config, EditAnywhere, Category = "Cold", meta = (ToolTip = "Cold level 2 when temperature (C) is at or below this value."))
	float ColdLevel2 = 0.f;

	UPROPERTY(config, EditAnywhere, Category = "Cold", meta = (ToolTip = "Cold level 3 when temperature (C) is at or below this value."))
	float ColdLevel3 = -10.f;

	UPROPERTY(config, EditAnywhere, Category = "Heat", meta = (ToolTip = "Heat level 1 when temperature (C) is at or above this value."))
	float HeatLevel1 = 28.f;

	UPROPERTY(config, EditAnywhere, Category = "Heat", meta = (ToolTip = "Heat level 2 when temperature (C) is at or above this value."))
	float HeatLevel2 = 35.f;

	UPROPERTY(config, EditAnywhere, Category = "Heat", meta = (ToolTip = "Heat level 3 when temperature (C) is at or above this value."))
	float HeatLevel3 = 42.f;

	UPROPERTY(config, EditAnywhere, Category = "Rain", meta = (ToolTip = "Rain level 1 when humidity is at or above the cell humidity threshold plus this value."))
	float RainLevel1 = 0.f;

	UPROPERTY(config, EditAnywhere, Category = "Rain", meta = (ToolTip = "Rain level 2 when humidity is at or above the cell humidity threshold plus this value."))
	float RainLevel2 = 10.f;

	UPROPERTY(config, EditAnywhere, Category = "Rain", meta = (ToolTip = "Rain level 3 when humidity is at or above the cell humidity threshold plus this value."))
	float RainLevel3 = 20.f;
};
