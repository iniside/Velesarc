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

#include "LyraSettingAction_SafeZoneEditor.h"

#include "DataSource/GameSettingDataSource.h"
#include "Player/ArcLocalPlayerBase.h"
#include "SettingsLyra/LyraGameSettingRegistry.h"
#include "Widgets/Layout/SSafeZone.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraSettingAction_SafeZoneEditor)

class UGameSetting;

#define LOCTEXT_NAMESPACE "LyraSettings"

ULyraSettingAction_SafeZoneEditor::ULyraSettingAction_SafeZoneEditor()
{
	SafeZoneValueSetting = NewObject<ULyraSettingValueScalarDynamic_SafeZoneValue>();
	SafeZoneValueSetting->SetDevName(TEXT("SafeZoneValue"));
	SafeZoneValueSetting->SetDisplayName(LOCTEXT("SafeZoneValue_Name", "Safe Zone Value"));
	SafeZoneValueSetting->SetDescriptionRichText(LOCTEXT("SafeZoneValue_Description", "The safezone area percentage."));
	SafeZoneValueSetting->SetDefaultValue(0.0f);
	SafeZoneValueSetting->SetDynamicGetter(GET_LOCAL_SETTINGS_FUNCTION_PATH_LYRA(GetSafeZone));
	SafeZoneValueSetting->SetDynamicSetter(GET_LOCAL_SETTINGS_FUNCTION_PATH_LYRA(SetSafeZone));
	SafeZoneValueSetting->SetDisplayFormat([](double SourceValue, double NormalizedValue){ return FText::AsNumber(SourceValue); });
	SafeZoneValueSetting->SetSettingParent(this);
}

TArray<UGameSetting*> ULyraSettingAction_SafeZoneEditor::GetChildSettings()
{
	return { SafeZoneValueSetting };
}

void ULyraSettingValueScalarDynamic_SafeZoneValue::ResetToDefault()
{
	Super::ResetToDefault();
	SSafeZone::SetGlobalSafeZoneScale(TOptional<float>(DefaultValue.Get(0.0f)));
}

void ULyraSettingValueScalarDynamic_SafeZoneValue::RestoreToInitial()
{
	Super::RestoreToInitial();
	SSafeZone::SetGlobalSafeZoneScale(TOptional<float>(InitialValue));
}

#undef LOCTEXT_NAMESPACE
