// Copyright Lukasz Baran. All Rights Reserved.

#include "NumericStepper/ArcNumericStepper.h"

void UArcNumericStepper::NativePreConstruct()
{
	Super::NativePreConstruct();

	Value = FMath::Clamp(Value, Min, Max);
	RefreshVisualState();
}

void UArcNumericStepper::NativeConstruct()
{
	Super::NativeConstruct();

	if (Button_Decrease)
	{
		Button_Decrease->OnClicked.AddUniqueDynamic(this, &UArcNumericStepper::HandleDecreaseClicked);
	}

	if (Button_Increase)
	{
		Button_Increase->OnClicked.AddUniqueDynamic(this, &UArcNumericStepper::HandleIncreaseClicked);
	}
}

void UArcNumericStepper::HandleDecreaseClicked()
{
	SetValue(Value - StepSize);
}

void UArcNumericStepper::HandleIncreaseClicked()
{
	SetValue(Value + StepSize);
}

void UArcNumericStepper::SetMin(int32 InMin)
{
	Min = InMin;
	const int32 ClampedValue = FMath::Clamp(Value, Min, Max);
	if (ClampedValue != Value)
	{
		Value = ClampedValue;
		RefreshVisualState();
		OnValueChanged.Broadcast(Value);
	}
	else
	{
		// Value unchanged, but button/icon enablement can still flip (e.g. Min raised to == Value).
		RefreshVisualState();
	}
}

void UArcNumericStepper::SetMax(int32 InMax)
{
	Max = InMax;
	const int32 ClampedValue = FMath::Clamp(Value, Min, Max);
	if (ClampedValue != Value)
	{
		Value = ClampedValue;
		RefreshVisualState();
		OnValueChanged.Broadcast(Value);
	}
	else
	{
		RefreshVisualState();
	}
}

void UArcNumericStepper::SetValue(int32 InValue)
{
	const int32 ClampedValue = FMath::Clamp(InValue, Min, Max);
	if (ClampedValue == Value)
	{
		return;
	}

	Value = ClampedValue;
	RefreshVisualState();
	OnValueChanged.Broadcast(Value);
}

void UArcNumericStepper::RefreshVisualState()
{
	if (TextBlock_Value)
	{
		TextBlock_Value->SetText(FText::AsNumber(Value));
	}

	if (Button_Decrease)
	{
		Button_Decrease->SetIsEnabled(Value > Min);
	}

	if (Button_Increase)
	{
		Button_Increase->SetIsEnabled(Value < Max);
	}

	if (Icon_Decrease)
	{
		Icon_Decrease->SetColorAndOpacity(Value > Min ? EnabledIconTint : DisabledIconOpacity);
	}

	if (Icon_Increase)
	{
		Icon_Increase->SetColorAndOpacity(Value < Max ? EnabledIconTint : DisabledIconOpacity);
	}
}
