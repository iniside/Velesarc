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

#include "ViewModels/ArcScalarSettingViewModel.h"

#include "Descriptors/ArcSettingDescriptor.h"
#include "Model/ArcSettingsModel.h"
#include "StructUtils/InstancedStruct.h"

#include "Internationalization/Text.h"
#include "Math/UnrealMathUtility.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcScalarSettingViewModel)

namespace ArcScalarSettingViewModel_Private
{
	static FText FormatValue(double InValue, double InNormalized, EArcSettingDisplayFormat InFormat)
	{
		switch (InFormat)
		{
			case EArcSettingDisplayFormat::Percent:
				return FText::AsPercent(InNormalized);
			case EArcSettingDisplayFormat::Integer:
				return FText::AsNumber(static_cast<int32>(InValue));
			case EArcSettingDisplayFormat::Float:
				return FText::AsNumber(InValue);
			case EArcSettingDisplayFormat::Custom:
			default:
				return FText::AsNumber(InValue);
		}
	}

	/** Snap InValue to the nearest multiple of StepSize within [InMin, InMax]. */
	static double SnapToStep(double InValue, double InMin, double InStepSize)
	{
		if (InStepSize <= 0.0)
		{
			return InValue;
		}
		const double Steps = FMath::RoundToDouble((InValue - InMin) / InStepSize);
		return InMin + Steps * InStepSize;
	}
} // namespace ArcScalarSettingViewModel_Private

void UArcScalarSettingViewModel::Refresh(const UArcSettingsModel& InModel)
{
	const FInstancedStruct* DescriptorStruct = InModel.GetDescriptor(SettingTag);
	if (!DescriptorStruct)
	{
		return;
	}

	const FArcScalarSettingDescriptor* Desc = DescriptorStruct->GetPtr<FArcScalarSettingDescriptor>();
	if (!Desc)
	{
		return;
	}

	UE_MVVM_SET_PROPERTY_VALUE(DisplayName, Desc->DisplayName);
	UE_MVVM_SET_PROPERTY_VALUE(Description, Desc->Description);
	UE_MVVM_SET_PROPERTY_VALUE(MinValue, Desc->MinValue);
	UE_MVVM_SET_PROPERTY_VALUE(MaxValue, Desc->MaxValue);
	UE_MVVM_SET_PROPERTY_VALUE(StepSize, Desc->StepSize);

	const double RawValue    = InModel.GetScalarValue(SettingTag);
	const double ClampedValue = FMath::Clamp(RawValue, Desc->MinValue, Desc->MaxValue);
	UE_MVVM_SET_PROPERTY_VALUE(Value, ClampedValue);

	const double Range = Desc->MaxValue - Desc->MinValue;
	const double NewNormalized = (Range > 0.0)
		? FMath::Clamp((ClampedValue - Desc->MinValue) / Range, 0.0, 1.0)
		: 0.0;
	UE_MVVM_SET_PROPERTY_VALUE(NormalizedValue, NewNormalized);

	const FText NewFormattedValue = ArcScalarSettingViewModel_Private::FormatValue(ClampedValue, NewNormalized, Desc->DisplayFormat);
	UE_MVVM_SET_PROPERTY_VALUE(FormattedValue, NewFormattedValue);

	const EArcSettingVisibility NewVisibility = InModel.GetVisibility(SettingTag);
	UE_MVVM_SET_PROPERTY_VALUE(Visibility, NewVisibility);

	const bool bNewCanReset = !FMath::IsNearlyEqual(ClampedValue, Desc->DefaultValue);
	UE_MVVM_SET_PROPERTY_VALUE(bCanReset, bNewCanReset);
}

void UArcScalarSettingViewModel::SetValue(double InValue, UArcSettingsModel* InModel)
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

	const FArcScalarSettingDescriptor* Desc = DescriptorStruct->GetPtr<FArcScalarSettingDescriptor>();
	if (!Desc)
	{
		return;
	}

	double Clamped = FMath::Clamp(InValue, Desc->MinValue, Desc->MaxValue);
	Clamped        = ArcScalarSettingViewModel_Private::SnapToStep(Clamped, Desc->MinValue, Desc->StepSize);
	Clamped        = FMath::Clamp(Clamped, Desc->MinValue, Desc->MaxValue);

	InModel->SetScalarValue(SettingTag, Clamped);
}

void UArcScalarSettingViewModel::SetNormalizedValue(double InNormalized, UArcSettingsModel* InModel)
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

	const FArcScalarSettingDescriptor* Desc = DescriptorStruct->GetPtr<FArcScalarSettingDescriptor>();
	if (!Desc)
	{
		return;
	}

	const double Clamped     = FMath::Clamp(InNormalized, 0.0, 1.0);
	const double ActualValue = Desc->MinValue + Clamped * (Desc->MaxValue - Desc->MinValue);
	SetValue(ActualValue, InModel);
}

void UArcScalarSettingViewModel::ResetToDefault(UArcSettingsModel* InModel)
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

	const FArcScalarSettingDescriptor* Desc = DescriptorStruct->GetPtr<FArcScalarSettingDescriptor>();
	if (!Desc)
	{
		return;
	}

	SetValue(Desc->DefaultValue, InModel);
}
