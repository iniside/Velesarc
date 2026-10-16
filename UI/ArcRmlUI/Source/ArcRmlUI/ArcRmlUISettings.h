// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"

#include "ArcRmlUISettings.generated.h"

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "ArcRmlUI"))
class ARCRMLUI_API UArcRmlUISettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(Config, EditAnywhere, Category = "Fonts", meta = (ConfigRestartRequired = true))
	TMap<FName, FString> GenericFontFamilies;

	UPROPERTY(Config, EditAnywhere, Category = "Fonts", meta = (ConfigRestartRequired = true))
	FString FallbackFontFamily;

	UPROPERTY(Config, EditAnywhere, Category = "Layers", meta = (Categories = "UI.Layer"))
	TArray<FGameplayTag> LayerOrder;

	UPROPERTY(Config, EditAnywhere, Category = "Transitions", meta = (ClampMin = "0", Units = "s"))
	float LeaveTimeoutSeconds = 0.0f;
};
