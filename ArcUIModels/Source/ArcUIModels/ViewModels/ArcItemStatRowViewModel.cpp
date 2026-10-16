// Copyright Lukasz Baran. All Rights Reserved.

#include "ViewModels/ArcItemStatRowViewModel.h"

#include "Display/ArcStatDisplayRegistry.h"
#include "ItemFragments/ArcItemFragment_ItemStats.h"

#define LOCTEXT_NAMESPACE "ArcItemStatRowViewModel"

namespace ArcItemStatRowVM_Private
{
	// Humanize a raw property name into a display label when no registry entry exists.
	// e.g. "FireRate" -> "Fire Rate". A documented fallback, not an error.
	FText HumanizePropertyName(FName PropertyName)
	{
		if (PropertyName.IsNone())
		{
			return FText::GetEmpty();
		}
		return FText::FromString(FName::NameToDisplayString(PropertyName.ToString(), false));
	}
}

void UArcItemStatRowViewModel::RefreshFrom(const FArcItemAttributeStat& Stat, const UArcStatDisplayRegistry* Reg)
{
	FText NewLabel;
	FText Suffix;

	const FArcStatDisplayData* Display = (Reg != nullptr) ? Reg->FindDisplay(Stat.Stat) : nullptr;
	if (Display != nullptr && !Display->DisplayName.IsEmpty())
	{
		NewLabel = Display->DisplayName;
		Suffix = Display->Suffix;
	}
	else
	{
		NewLabel = ArcItemStatRowVM_Private::HumanizePropertyName(Stat.Stat.PropertyName);
	}

	const float RawValue = Stat.Value.GetValue();

	FText NewValue;
	if (Suffix.IsEmpty())
	{
		NewValue = FText::AsNumber(RawValue);
	}
	else
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("Value"), FText::AsNumber(RawValue));
		Args.Add(TEXT("Suffix"), Suffix);
		NewValue = FText::Format(LOCTEXT("StatValueFmt", "{Value}{Suffix}"), Args);
	}

	UE_MVVM_SET_PROPERTY_VALUE(Label, NewLabel);
	UE_MVVM_SET_PROPERTY_VALUE(Value, NewValue);
}

#undef LOCTEXT_NAMESPACE
