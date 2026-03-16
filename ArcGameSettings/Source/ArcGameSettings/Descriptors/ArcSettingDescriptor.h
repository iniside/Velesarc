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

#pragma once

#include "GameplayTagContainer.h"
#include "Model/ArcSettingsTypes.h"

#include "ArcSettingDescriptor.generated.h"

class UArcSettingsModel;

USTRUCT(BlueprintType)
struct ARCGAMESETTINGS_API FArcSettingOption
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString Value;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FText DisplayText;
};

USTRUCT(BlueprintType)
struct ARCGAMESETTINGS_API FArcSettingDescriptor
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FGameplayTag SettingTag;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FGameplayTag CategoryTag;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FText Description;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    EArcSettingStorage StorageType = EArcSettingStorage::Local;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName PropertyName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<FGameplayTag> DependsOn;
};

USTRUCT(BlueprintType)
struct ARCGAMESETTINGS_API FArcDiscreteSettingDescriptor : public FArcSettingDescriptor
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<FArcSettingOption> Options;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 DefaultIndex = 0;
};

USTRUCT(BlueprintType)
struct ARCGAMESETTINGS_API FArcScalarSettingDescriptor : public FArcSettingDescriptor
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    double MinValue = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    double MaxValue = 1.0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    double StepSize = 0.01;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    double DefaultValue = 0.5;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    EArcSettingDisplayFormat DisplayFormat = EArcSettingDisplayFormat::Percent;
};

USTRUCT(BlueprintType)
struct ARCGAMESETTINGS_API FArcActionSettingDescriptor : public FArcSettingDescriptor
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FText ActionText;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FGameplayTag ActionTag;
};
