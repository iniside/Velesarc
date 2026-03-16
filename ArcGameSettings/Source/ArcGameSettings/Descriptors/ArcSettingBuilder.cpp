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

#include "Descriptors/ArcSettingBuilder.h"

// NOTE: ArcSettingsModel.h will be created in Task 7.
#include "Model/ArcSettingsModel.h"

#include "StructUtils/InstancedStruct.h"

// ============================================================
// FArcDiscreteSettingBuilder
// ============================================================

FArcDiscreteSettingBuilder::FArcDiscreteSettingBuilder(UArcSettingsModel& InModel, FGameplayTag InSettingTag, FGameplayTag InCategoryTag)
	: Model(InModel)
{
	Descriptor.SettingTag = InSettingTag;
	Descriptor.CategoryTag = InCategoryTag;
}

FArcDiscreteSettingBuilder& FArcDiscreteSettingBuilder::Name(FText InName)
{
	Descriptor.DisplayName = MoveTemp(InName);
	return *this;
}

FArcDiscreteSettingBuilder& FArcDiscreteSettingBuilder::Desc(FText InDesc)
{
	Descriptor.Description = MoveTemp(InDesc);
	return *this;
}

FArcDiscreteSettingBuilder& FArcDiscreteSettingBuilder::Local(FName InPropertyName)
{
	Descriptor.StorageType = EArcSettingStorage::Local;
	Descriptor.PropertyName = InPropertyName;
	return *this;
}

FArcDiscreteSettingBuilder& FArcDiscreteSettingBuilder::Shared(FName InPropertyName)
{
	Descriptor.StorageType = EArcSettingStorage::Shared;
	Descriptor.PropertyName = InPropertyName;
	return *this;
}

FArcDiscreteSettingBuilder& FArcDiscreteSettingBuilder::Engine(FName InPropertyName)
{
	Descriptor.StorageType = EArcSettingStorage::Engine;
	Descriptor.PropertyName = InPropertyName;
	return *this;
}

FArcDiscreteSettingBuilder& FArcDiscreteSettingBuilder::DependsOn(TArray<FGameplayTag> InDependsOn)
{
	Descriptor.DependsOn = MoveTemp(InDependsOn);
	return *this;
}

FArcDiscreteSettingBuilder& FArcDiscreteSettingBuilder::EditCondition(TFunction<EArcSettingVisibility(const UArcSettingsModel&)> InCondition)
{
	Model.RegisterEditCondition(Descriptor.SettingTag, MoveTemp(InCondition));
	return *this;
}

FArcDiscreteSettingBuilder& FArcDiscreteSettingBuilder::Options(TArray<FArcSettingOption> InOptions)
{
	Descriptor.Options = MoveTemp(InOptions);
	return *this;
}

FArcDiscreteSettingBuilder& FArcDiscreteSettingBuilder::Default(int32 InDefaultIndex)
{
	Descriptor.DefaultIndex = InDefaultIndex;
	return *this;
}

FArcDiscreteSettingBuilder& FArcDiscreteSettingBuilder::Getter(TFunction<int32()> InGetter)
{
	PendingGetter = MoveTemp(InGetter);
	return *this;
}

FArcDiscreteSettingBuilder& FArcDiscreteSettingBuilder::Setter(TFunction<void(int32)> InSetter)
{
	PendingSetter = MoveTemp(InSetter);
	return *this;
}

FArcDiscreteSettingBuilder& FArcDiscreteSettingBuilder::DynamicOptions(TFunction<TArray<FArcSettingOption>()> InProvider)
{
	PendingDynamicOptions = MoveTemp(InProvider);
	return *this;
}

void FArcDiscreteSettingBuilder::Done()
{
	const FGameplayTag Tag = Descriptor.SettingTag;
	Model.RegisterSetting(FInstancedStruct::Make(MoveTemp(Descriptor)));
	if (PendingGetter) Model.RegisterDiscreteGetter(Tag, MoveTemp(PendingGetter));
	if (PendingSetter) Model.RegisterDiscreteSetter(Tag, MoveTemp(PendingSetter));
	if (PendingDynamicOptions) Model.RegisterDynamicOptionsProvider(Tag, MoveTemp(PendingDynamicOptions));
}

// ============================================================
// FArcScalarSettingBuilder
// ============================================================

FArcScalarSettingBuilder::FArcScalarSettingBuilder(UArcSettingsModel& InModel, FGameplayTag InSettingTag, FGameplayTag InCategoryTag)
	: Model(InModel)
{
	Descriptor.SettingTag = InSettingTag;
	Descriptor.CategoryTag = InCategoryTag;
}

FArcScalarSettingBuilder& FArcScalarSettingBuilder::Name(FText InName)
{
	Descriptor.DisplayName = MoveTemp(InName);
	return *this;
}

FArcScalarSettingBuilder& FArcScalarSettingBuilder::Desc(FText InDesc)
{
	Descriptor.Description = MoveTemp(InDesc);
	return *this;
}

FArcScalarSettingBuilder& FArcScalarSettingBuilder::Local(FName InPropertyName)
{
	Descriptor.StorageType = EArcSettingStorage::Local;
	Descriptor.PropertyName = InPropertyName;
	return *this;
}

FArcScalarSettingBuilder& FArcScalarSettingBuilder::Shared(FName InPropertyName)
{
	Descriptor.StorageType = EArcSettingStorage::Shared;
	Descriptor.PropertyName = InPropertyName;
	return *this;
}

FArcScalarSettingBuilder& FArcScalarSettingBuilder::Engine(FName InPropertyName)
{
	Descriptor.StorageType = EArcSettingStorage::Engine;
	Descriptor.PropertyName = InPropertyName;
	return *this;
}

FArcScalarSettingBuilder& FArcScalarSettingBuilder::DependsOn(TArray<FGameplayTag> InDependsOn)
{
	Descriptor.DependsOn = MoveTemp(InDependsOn);
	return *this;
}

FArcScalarSettingBuilder& FArcScalarSettingBuilder::EditCondition(TFunction<EArcSettingVisibility(const UArcSettingsModel&)> InCondition)
{
	Model.RegisterEditCondition(Descriptor.SettingTag, MoveTemp(InCondition));
	return *this;
}

FArcScalarSettingBuilder& FArcScalarSettingBuilder::Range(double Min, double Max, double Step)
{
	Descriptor.MinValue = Min;
	Descriptor.MaxValue = Max;
	Descriptor.StepSize = Step;
	return *this;
}

FArcScalarSettingBuilder& FArcScalarSettingBuilder::Default(double InDefaultValue)
{
	Descriptor.DefaultValue = InDefaultValue;
	return *this;
}

FArcScalarSettingBuilder& FArcScalarSettingBuilder::Format(EArcSettingDisplayFormat InFormat)
{
	Descriptor.DisplayFormat = InFormat;
	return *this;
}

FArcScalarSettingBuilder& FArcScalarSettingBuilder::Getter(TFunction<double()> InGetter)
{
	PendingGetter = MoveTemp(InGetter);
	return *this;
}

FArcScalarSettingBuilder& FArcScalarSettingBuilder::Setter(TFunction<void(double)> InSetter)
{
	PendingSetter = MoveTemp(InSetter);
	return *this;
}

void FArcScalarSettingBuilder::Done()
{
	const FGameplayTag Tag = Descriptor.SettingTag;
	Model.RegisterSetting(FInstancedStruct::Make(MoveTemp(Descriptor)));
	if (PendingGetter) Model.RegisterScalarGetter(Tag, MoveTemp(PendingGetter));
	if (PendingSetter) Model.RegisterScalarSetter(Tag, MoveTemp(PendingSetter));
}

// ============================================================
// FArcActionSettingBuilder
// ============================================================

FArcActionSettingBuilder::FArcActionSettingBuilder(UArcSettingsModel& InModel, FGameplayTag InSettingTag, FGameplayTag InCategoryTag)
	: Model(InModel)
{
	Descriptor.SettingTag = InSettingTag;
	Descriptor.CategoryTag = InCategoryTag;
}

FArcActionSettingBuilder& FArcActionSettingBuilder::Name(FText InName)
{
	Descriptor.DisplayName = MoveTemp(InName);
	return *this;
}

FArcActionSettingBuilder& FArcActionSettingBuilder::Desc(FText InDesc)
{
	Descriptor.Description = MoveTemp(InDesc);
	return *this;
}

FArcActionSettingBuilder& FArcActionSettingBuilder::Local(FName InPropertyName)
{
	Descriptor.StorageType = EArcSettingStorage::Local;
	Descriptor.PropertyName = InPropertyName;
	return *this;
}

FArcActionSettingBuilder& FArcActionSettingBuilder::Shared(FName InPropertyName)
{
	Descriptor.StorageType = EArcSettingStorage::Shared;
	Descriptor.PropertyName = InPropertyName;
	return *this;
}

FArcActionSettingBuilder& FArcActionSettingBuilder::Engine(FName InPropertyName)
{
	Descriptor.StorageType = EArcSettingStorage::Engine;
	Descriptor.PropertyName = InPropertyName;
	return *this;
}

FArcActionSettingBuilder& FArcActionSettingBuilder::DependsOn(TArray<FGameplayTag> InDependsOn)
{
	Descriptor.DependsOn = MoveTemp(InDependsOn);
	return *this;
}

FArcActionSettingBuilder& FArcActionSettingBuilder::EditCondition(TFunction<EArcSettingVisibility(const UArcSettingsModel&)> InCondition)
{
	Model.RegisterEditCondition(Descriptor.SettingTag, MoveTemp(InCondition));
	return *this;
}

FArcActionSettingBuilder& FArcActionSettingBuilder::ActionText(FText InActionText)
{
	Descriptor.ActionText = MoveTemp(InActionText);
	return *this;
}

FArcActionSettingBuilder& FArcActionSettingBuilder::ActionTag(FGameplayTag InActionTag)
{
	Descriptor.ActionTag = InActionTag;
	return *this;
}

void FArcActionSettingBuilder::Done()
{
	Model.RegisterSetting(FInstancedStruct::Make(MoveTemp(Descriptor)));
}
