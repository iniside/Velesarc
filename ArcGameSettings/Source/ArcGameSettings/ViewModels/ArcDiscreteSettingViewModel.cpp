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

#include "ViewModels/ArcDiscreteSettingViewModel.h"

#include "Descriptors/ArcSettingDescriptor.h"
#include "Model/ArcSettingsModel.h"
#include "StructUtils/InstancedStruct.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcDiscreteSettingViewModel)

void UArcDiscreteSettingViewModel::Refresh(const UArcSettingsModel& InModel)
{
	const FInstancedStruct* DescriptorStruct = InModel.GetDescriptor(SettingTag);
	if (!DescriptorStruct)
	{
		return;
	}

	const FArcDiscreteSettingDescriptor* Desc = DescriptorStruct->GetPtr<FArcDiscreteSettingDescriptor>();
	if (!Desc)
	{
		return;
	}

	UE_MVVM_SET_PROPERTY_VALUE(DisplayName, Desc->DisplayName);
	UE_MVVM_SET_PROPERTY_VALUE(Description, Desc->Description);

	const TArray<FArcSettingOption> ResolvedOptions = InModel.GetOptionsForSetting(SettingTag);
	TArray<FText> NewOptionNames;
	NewOptionNames.Reserve(ResolvedOptions.Num());
	for (const FArcSettingOption& Option : ResolvedOptions)
	{
		NewOptionNames.Add(Option.DisplayText);
	}
	UE_MVVM_SET_PROPERTY_VALUE(OptionNames, NewOptionNames);

	const int32 NewOptionCount = ResolvedOptions.Num();
	UE_MVVM_SET_PROPERTY_VALUE(OptionCount, NewOptionCount);

	const int32 NewIndex = InModel.GetDiscreteIndex(SettingTag);
	UE_MVVM_SET_PROPERTY_VALUE(CurrentIndex, NewIndex);

	FText NewCurrentOptionText;
	if (ResolvedOptions.IsValidIndex(NewIndex))
	{
		NewCurrentOptionText = ResolvedOptions[NewIndex].DisplayText;
	}
	UE_MVVM_SET_PROPERTY_VALUE(CurrentOptionText, NewCurrentOptionText);

	const EArcSettingVisibility NewVisibility = InModel.GetVisibility(SettingTag);
	UE_MVVM_SET_PROPERTY_VALUE(Visibility, NewVisibility);

	const bool bNewCanReset = (NewIndex != Desc->DefaultIndex);
	UE_MVVM_SET_PROPERTY_VALUE(bCanReset, bNewCanReset);
}

void UArcDiscreteSettingViewModel::SetCurrentIndex(int32 InIndex, UArcSettingsModel* InModel)
{
	if (!InModel)
	{
		return;
	}
	InModel->SetDiscreteIndex(SettingTag, InIndex);
}

void UArcDiscreteSettingViewModel::IncrementOption(UArcSettingsModel* InModel)
{
	if (!InModel)
	{
		return;
	}

	const int32 Count = OptionCount;
	if (Count <= 0)
	{
		return;
	}

	const int32 NewIndex = (CurrentIndex + 1) % Count;
	InModel->SetDiscreteIndex(SettingTag, NewIndex);
}

void UArcDiscreteSettingViewModel::DecrementOption(UArcSettingsModel* InModel)
{
	if (!InModel)
	{
		return;
	}

	const int32 Count = OptionCount;
	if (Count <= 0)
	{
		return;
	}

	const int32 NewIndex = (CurrentIndex - 1 + Count) % Count;
	InModel->SetDiscreteIndex(SettingTag, NewIndex);
}

void UArcDiscreteSettingViewModel::ResetToDefault(UArcSettingsModel* InModel)
{
	if (!InModel)
	{
		return;
	}

	const FInstancedStruct* DescriptorStruct = InModel->GetDescriptor(SettingTag);
	if (!DescriptorStruct)
	{
		return;
	}

	const FArcDiscreteSettingDescriptor* Desc = DescriptorStruct->GetPtr<FArcDiscreteSettingDescriptor>();
	if (!Desc)
	{
		return;
	}

	InModel->SetDiscreteIndex(SettingTag, Desc->DefaultIndex);
}
