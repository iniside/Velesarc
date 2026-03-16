/**
 * This file is part of Velesarc
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

#include "LyraSettingValueDiscrete_OverallQuality.h"

#include "Engine/Engine.h"
#include "GameFramework/GameUserSettings.h"
#include "SettingsLyra/LyraSettingsLocal.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraSettingValueDiscrete_OverallQuality)

#define LOCTEXT_NAMESPACE "LyraSettings"

ULyraSettingValueDiscrete_OverallQuality::ULyraSettingValueDiscrete_OverallQuality()
{
}

void ULyraSettingValueDiscrete_OverallQuality::OnInitialized()
{
	Super::OnInitialized();

	ULyraSettingsLocal* UserSettings = ULyraSettingsLocal::Get();
	const int32 MaxQualityLevel = UserSettings->GetMaxSupportedOverallQualityLevel();

	auto AddOptionIfPossible = [&](int Index, FText&& Value) { if ((MaxQualityLevel < 0) || (Index <= MaxQualityLevel)) { Options.Add(Value); }};

	AddOptionIfPossible(0, LOCTEXT("VideoQualityOverall_Low", "Low"));
	AddOptionIfPossible(1, LOCTEXT("VideoQualityOverall_Medium", "Medium"));
	AddOptionIfPossible(2, LOCTEXT("VideoQualityOverall_High", "High"));
	AddOptionIfPossible(3, LOCTEXT("VideoQualityOverall_Epic", "Epic"));

	OptionsWithCustom = Options;
	OptionsWithCustom.Add(LOCTEXT("VideoQualityOverall_Custom", "Custom"));

	const int32 LowestQualityWithFrameRateLimit = UserSettings->GetLowestQualityWithFrameRateLimit();
	if (Options.IsValidIndex(LowestQualityWithFrameRateLimit))
	{
		SetWarningRichText(FText::Format(LOCTEXT("OverallQuality_Mobile_ImpactsFramerate", "<strong>Note: Changing the Quality setting to {0} or higher might limit your framerate.</>"), Options[LowestQualityWithFrameRateLimit]));
	}
}

void ULyraSettingValueDiscrete_OverallQuality::StoreInitial()
{
}

void ULyraSettingValueDiscrete_OverallQuality::ResetToDefault()
{
}

void ULyraSettingValueDiscrete_OverallQuality::RestoreToInitial()
{
}

void ULyraSettingValueDiscrete_OverallQuality::SetDiscreteOptionByIndex(int32 Index)
{
	UGameUserSettings* UserSettings = CastChecked<UGameUserSettings>(GEngine->GetGameUserSettings());

	if (Index == GetCustomOptionIndex())
	{
		// Leave everything as is we're in a custom setup.
	}
	else
	{
		// Low / Medium / High / Epic
		UserSettings->SetOverallScalabilityLevel(Index);
	}

	NotifySettingChanged(EGameSettingChangeReason::Change);
}

int32 ULyraSettingValueDiscrete_OverallQuality::GetDiscreteOptionIndex() const
{
	const int32 OverallQualityLevel = GetOverallQualityLevel();
	if (OverallQualityLevel == INDEX_NONE)
	{
		return GetCustomOptionIndex();
	}

	return OverallQualityLevel;
}

TArray<FText> ULyraSettingValueDiscrete_OverallQuality::GetDiscreteOptions() const
{
	const int32 OverallQualityLevel = GetOverallQualityLevel();
	if (OverallQualityLevel == INDEX_NONE)
	{
		return OptionsWithCustom;
	}
	else
	{
		return Options;
	}
}

int32 ULyraSettingValueDiscrete_OverallQuality::GetCustomOptionIndex() const
{
	return OptionsWithCustom.Num() - 1;
}

int32 ULyraSettingValueDiscrete_OverallQuality::GetOverallQualityLevel() const
{
	const UGameUserSettings* UserSettings = CastChecked<const UGameUserSettings>(GEngine->GetGameUserSettings());
	return UserSettings->GetOverallScalabilityLevel();
}

#undef LOCTEXT_NAMESPACE
