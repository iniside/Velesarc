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

#include "Engine/DeveloperSettingsBackedByCVars.h"
#include "GameplayTagContainer.h"
#include "LyraPlatformEmulationSettings.generated.h"

struct FPropertyChangedEvent;

/**
 * Platform emulation settings
 */
UCLASS(config=EditorPerProjectUserSettings, MinimalAPI)
class ULyraPlatformEmulationSettings : public UDeveloperSettingsBackedByCVars
{
	GENERATED_BODY()

public:
	ULyraPlatformEmulationSettings();

	//~UDeveloperSettings interface
	virtual FName GetCategoryName() const override;
	//~End of UDeveloperSettings interface

	ARCCORE_API FName GetPretendBaseDeviceProfile() const;
	ARCCORE_API FName GetPretendPlatformName() const;

private:
	UPROPERTY(EditAnywhere, config, Category=PlatformEmulation, meta=(Categories="Input,Platform.Trait"))
	FGameplayTagContainer AdditionalPlatformTraitsToEnable;

	UPROPERTY(EditAnywhere, config, Category=PlatformEmulation, meta=(Categories="Input,Platform.Trait"))
	FGameplayTagContainer AdditionalPlatformTraitsToSuppress;

	UPROPERTY(EditAnywhere, config, Category=PlatformEmulation, meta=(GetOptions=GetKnownPlatformIds))
	FName PretendPlatform;

	// The base device profile to pretend we are using when emulating device-specific device profiles applied from ULyraSettingsLocal
	UPROPERTY(EditAnywhere, config, Category=PlatformEmulation, meta=(GetOptions=GetKnownDeviceProfiles, EditCondition=bApplyDeviceProfilesInPIE))
	FName PretendBaseDeviceProfile;

	// Do we apply desktop-style frame rate settings in PIE?
	// (frame rate limits are an engine-wide setting so it's not always desirable to have enabled in the editor)
	// You may also want to disable the editor preference "Use Less CPU when in Background" if testing background frame rate limits
	UPROPERTY(EditAnywhere, config, Category=PlatformEmulation, meta=(ConsoleVariable="Lyra.Settings.ApplyFrameRateSettingsInPIE"))
	bool bApplyFrameRateSettingsInPIE = false;

	// Do we apply front-end specific performance options in PIE?
	// Most engine performance/scalability settings they drive are global, so if one PIE window
	// is in the front-end and the other is in-game one will win and the other gets stuck with those settings
	UPROPERTY(EditAnywhere, config, Category=PlatformEmulation, meta=(ConsoleVariable="Lyra.Settings.ApplyFrontEndPerformanceOptionsInPIE"))
	bool bApplyFrontEndPerformanceOptionsInPIE = false;

	// Should we apply experience/platform emulated device profiles in PIE?
	UPROPERTY(EditAnywhere, config, Category=PlatformEmulation, meta=(InlineEditConditionToggle, ConsoleVariable="Lyra.Settings.ApplyDeviceProfilesInPIE"))
	bool bApplyDeviceProfilesInPIE = false;

#if WITH_EDITOR
public:
	// Called by the editor engine to let us pop reminder notifications when cheats are active
	ARCCORE_API void OnPlayInEditorStarted() const;

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

private:
	UFUNCTION()
	TArray<FName> GetKnownPlatformIds() const;

	UFUNCTION()
	TArray<FName> GetKnownDeviceProfiles() const;

	void PickReasonableBaseDeviceProfile();
};
