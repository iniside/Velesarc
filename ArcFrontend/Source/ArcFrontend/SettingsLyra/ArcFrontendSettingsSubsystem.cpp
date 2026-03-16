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

#include "SettingsLyra/ArcFrontendSettingsSubsystem.h"
#include "Model/ArcSettingsModel.h"

// Forward-declare the registration trampolines
extern void RegisterAudioSettings(UArcSettingsModel& Model);
extern void RegisterGameplaySettings(UArcSettingsModel& Model);
extern void ArcSettingsRegistration_Video_Register(UArcSettingsModel& Model);
extern void ArcSettingsRegistration_Input_Register(UArcSettingsModel& Model);

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcFrontendSettingsSubsystem)

void UArcFrontendSettingsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    Collection.InitializeDependency<UArcSettingsModel>();

    UArcSettingsModel* Model = GetGameInstance()->GetSubsystem<UArcSettingsModel>();
    if (!Model) return;

    RegisterAudioSettings(*Model);
    ArcSettingsRegistration_Video_Register(*Model);
    RegisterGameplaySettings(*Model);
    ArcSettingsRegistration_Input_Register(*Model);
}
