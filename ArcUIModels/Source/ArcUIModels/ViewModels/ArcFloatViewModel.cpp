// Copyright Lukasz Baran. All Rights Reserved.

#include "ViewModels/ArcFloatViewModel.h"

void UArcFloatViewModel::SetValue(float InValue)
{
	UE_MVVM_SET_PROPERTY_VALUE(Value, InValue);
	RecalculateNormalized();
}

void UArcFloatViewModel::SetMinValue(float InMinValue)
{
	UE_MVVM_SET_PROPERTY_VALUE(MinValue, InMinValue);
	RecalculateNormalized();
}

void UArcFloatViewModel::SetMaxValue(float InMaxValue)
{
	UE_MVVM_SET_PROPERTY_VALUE(MaxValue, InMaxValue);
	RecalculateNormalized();
}

void UArcFloatViewModel::SetLabel(FText InLabel)
{
	UE_MVVM_SET_PROPERTY_VALUE(Label, InLabel);
}

void UArcFloatViewModel::RecalculateNormalized()
{
	float Range = MaxValue - MinValue;
	float NewNormalized = (Range > UE_KINDA_SMALL_NUMBER)
		? FMath::Clamp((Value - MinValue) / Range, 0.f, 1.f)
		: 0.f;
	UE_MVVM_SET_PROPERTY_VALUE(NormalizedValue, NewNormalized);
}
