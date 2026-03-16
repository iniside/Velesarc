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
#include "Descriptors/ArcSettingDescriptor.h"
#include "Model/ArcSettingsTypes.h"

class UArcSettingsModel;

/**
 * Fluent builder for FArcDiscreteSettingDescriptor.
 * Construct via UArcSettingsModel::Discrete(). Call Done() to register.
 */
class ARCGAMESETTINGS_API FArcDiscreteSettingBuilder
{
public:
	FArcDiscreteSettingBuilder(UArcSettingsModel& InModel, FGameplayTag InSettingTag, FGameplayTag InCategoryTag);

	FArcDiscreteSettingBuilder& Name(FText InName);
	FArcDiscreteSettingBuilder& Desc(FText InDesc);

	/** Set storage type to Local and assign property name. */
	FArcDiscreteSettingBuilder& Local(FName InPropertyName);
	/** Set storage type to Shared and assign property name. */
	FArcDiscreteSettingBuilder& Shared(FName InPropertyName);
	/** Set storage type to Engine and assign property name. */
	FArcDiscreteSettingBuilder& Engine(FName InPropertyName);

	FArcDiscreteSettingBuilder& DependsOn(TArray<FGameplayTag> InDependsOn);
	FArcDiscreteSettingBuilder& EditCondition(TFunction<EArcSettingVisibility(const UArcSettingsModel&)> InCondition);

	FArcDiscreteSettingBuilder& Options(TArray<FArcSettingOption> InOptions);
	FArcDiscreteSettingBuilder& Default(int32 InDefaultIndex);

	FArcDiscreteSettingBuilder& Getter(TFunction<int32()> InGetter);
	FArcDiscreteSettingBuilder& Setter(TFunction<void(int32)> InSetter);
	FArcDiscreteSettingBuilder& DynamicOptions(TFunction<TArray<FArcSettingOption>()> InProvider);

	void Done();

private:
	UArcSettingsModel& Model;
	FArcDiscreteSettingDescriptor Descriptor;

	TFunction<int32()> PendingGetter;
	TFunction<void(int32)> PendingSetter;
	TFunction<TArray<FArcSettingOption>()> PendingDynamicOptions;
};

/**
 * Fluent builder for FArcScalarSettingDescriptor.
 * Construct via UArcSettingsModel::Scalar(). Call Done() to register.
 */
class ARCGAMESETTINGS_API FArcScalarSettingBuilder
{
public:
	FArcScalarSettingBuilder(UArcSettingsModel& InModel, FGameplayTag InSettingTag, FGameplayTag InCategoryTag);

	FArcScalarSettingBuilder& Name(FText InName);
	FArcScalarSettingBuilder& Desc(FText InDesc);

	/** Set storage type to Local and assign property name. */
	FArcScalarSettingBuilder& Local(FName InPropertyName);
	/** Set storage type to Shared and assign property name. */
	FArcScalarSettingBuilder& Shared(FName InPropertyName);
	/** Set storage type to Engine and assign property name. */
	FArcScalarSettingBuilder& Engine(FName InPropertyName);

	FArcScalarSettingBuilder& DependsOn(TArray<FGameplayTag> InDependsOn);
	FArcScalarSettingBuilder& EditCondition(TFunction<EArcSettingVisibility(const UArcSettingsModel&)> InCondition);

	FArcScalarSettingBuilder& Range(double Min, double Max, double Step);
	FArcScalarSettingBuilder& Default(double InDefaultValue);
	FArcScalarSettingBuilder& Format(EArcSettingDisplayFormat InFormat);

	FArcScalarSettingBuilder& Getter(TFunction<double()> InGetter);
	FArcScalarSettingBuilder& Setter(TFunction<void(double)> InSetter);

	void Done();

private:
	UArcSettingsModel& Model;
	FArcScalarSettingDescriptor Descriptor;

	TFunction<double()> PendingGetter;
	TFunction<void(double)> PendingSetter;
};

/**
 * Fluent builder for FArcActionSettingDescriptor.
 * Construct via UArcSettingsModel::Action(). Call Done() to register.
 */
class ARCGAMESETTINGS_API FArcActionSettingBuilder
{
public:
	FArcActionSettingBuilder(UArcSettingsModel& InModel, FGameplayTag InSettingTag, FGameplayTag InCategoryTag);

	FArcActionSettingBuilder& Name(FText InName);
	FArcActionSettingBuilder& Desc(FText InDesc);

	/** Set storage type to Local and assign property name. */
	FArcActionSettingBuilder& Local(FName InPropertyName);
	/** Set storage type to Shared and assign property name. */
	FArcActionSettingBuilder& Shared(FName InPropertyName);
	/** Set storage type to Engine and assign property name. */
	FArcActionSettingBuilder& Engine(FName InPropertyName);

	FArcActionSettingBuilder& DependsOn(TArray<FGameplayTag> InDependsOn);
	FArcActionSettingBuilder& EditCondition(TFunction<EArcSettingVisibility(const UArcSettingsModel&)> InCondition);

	FArcActionSettingBuilder& ActionText(FText InActionText);
	FArcActionSettingBuilder& ActionTag(FGameplayTag InActionTag);

	void Done();

private:
	UArcSettingsModel& Model;
	FArcActionSettingDescriptor Descriptor;
};
