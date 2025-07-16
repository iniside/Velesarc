/**
 * This file is part of ArcX.
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or –
 * as soon as they will be approved by the European Commission – later versions
 * of the EUPL (the "License");
 *
 * You may not use this work except in compliance with the License.
 * You may get a copy of the License at:
 *
 * https://eupl.eu/
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 */

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"
#include "ArcDeveloperSettings.generated.h"

class UArcExperienceDefinition;

/**
 * Developer settings / editor cheats
 */
UCLASS(config = EditorPerProjectUserSettings)
class ARCCORE_API UArcDeveloperSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UArcDeveloperSettings();

	//~UDeveloperSettings interface
	virtual FName GetCategoryName() const override;

	//~End of UDeveloperSettings interface

public:
	// The experience override to use for Play in Editor (if not set, the default for the
	// world settings of the open map will be used)
	UPROPERTY(EditDefaultsOnly
		, BlueprintReadOnly
		, config
		, Category = Arc
		, meta = (AllowedTypes = "ArcExperienceDefinition"))
	FPrimaryAssetId ExperienceOverride;

	UPROPERTY(BlueprintReadOnly
		, config
		, Category = Arc)
	bool bOverrideBotCount = false;

	UPROPERTY(EditDefaultsOnly
		, BlueprintReadOnly
		, config
		, Category = Arc
		, meta = (EditCondition = bOverrideBotCount))
	int32 OverrideNumPlayerBotsToSpawn = 0;

	// Do the full game flow when playing in the editor, or skip 'waiting for player' /
	// etc... game phases?
	UPROPERTY(EditDefaultsOnly
		, BlueprintReadOnly
		, config
		, Category = Arc)
	bool bTestFullGameFlowInPIE = false;

	UPROPERTY(EditDefaultsOnly
		, BlueprintReadOnly
		, config
		, Category = Arc
		, meta = (Categories = "Input,Platform.Trait"))
	FGameplayTagContainer AdditionalPlatformTraitsToEnable;

	UPROPERTY(EditDefaultsOnly
		, BlueprintReadOnly
		, config
		, Category = Arc
		, meta = (Categories = "Input,Platform.Trait"))
	FGameplayTagContainer AdditionalPlatformTraitsToSuppress;

	UPROPERTY(EditDefaultsOnly
		, BlueprintReadOnly
		, config
		, Category = Arc
		, meta = (GetOptions = GetKnownPlatformIds))
	FName PretendPlatform;

#if WITH_EDITOR

private:
	// The last pretend platform we applied
	FName LastAppliedPretendPlatform;

private:
	void ApplySettings();

	void ChangeActivePretendPlatform(FName NewPlatformName);
#endif

public:
	//~UObject interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	virtual void PostReloadConfig(FProperty* PropertyThatWasLoaded) override;

	virtual void PostInitProperties() override;
#endif
	//~End of UObject interface

	UFUNCTION()
	TArray<FName> GetKnownPlatformIds() const;

#if WITH_EDITOR

public:
	// Called by the editor engine to let us pop reminder notifications when cheats are
	// active
	void OnPlayInEditorStarted() const;

#endif
};
