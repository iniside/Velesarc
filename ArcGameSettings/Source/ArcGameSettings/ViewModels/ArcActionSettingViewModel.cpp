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

#include "ViewModels/ArcActionSettingViewModel.h"

#include "Descriptors/ArcSettingDescriptor.h"
#include "Model/ArcSettingsModel.h"
#include "StructUtils/InstancedStruct.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcActionSettingViewModel)

void UArcActionSettingViewModel::Refresh(const UArcSettingsModel& InModel)
{
	const FInstancedStruct* DescriptorStruct = InModel.GetDescriptor(SettingTag);
	if (!DescriptorStruct)
	{
		return;
	}

	const FArcActionSettingDescriptor* Desc = DescriptorStruct->GetPtr<FArcActionSettingDescriptor>();
	if (!Desc)
	{
		return;
	}

	UE_MVVM_SET_PROPERTY_VALUE(DisplayName, Desc->DisplayName);
	UE_MVVM_SET_PROPERTY_VALUE(Description, Desc->Description);
	UE_MVVM_SET_PROPERTY_VALUE(ActionText, Desc->ActionText);

	const EArcSettingVisibility NewVisibility = InModel.GetVisibility(SettingTag);
	UE_MVVM_SET_PROPERTY_VALUE(Visibility, NewVisibility);
}

void UArcActionSettingViewModel::ExecuteAction(UArcSettingsModel* InModel)
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

	const FArcActionSettingDescriptor* Desc = DescriptorStruct->GetPtr<FArcActionSettingDescriptor>();
	if (!Desc)
	{
		return;
	}

	InModel->OnActionExecuted.Broadcast(SettingTag, Desc->ActionTag);
}
