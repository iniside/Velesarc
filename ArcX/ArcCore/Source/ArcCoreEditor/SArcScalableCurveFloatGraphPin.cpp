// Copyright Lukasz Baran. All Rights Reserved.

#include "SArcScalableCurveFloatGraphPin.h"
#include "SArcScalableCurveFloatWidget.h"

#include "Items/ArcItemScalableFloat.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "ArcScalableCurveFloatPin"

void SArcScalableCurveFloatGraphPin::Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj)
{
	SGraphPin::Construct(SGraphPin::FArguments(), InGraphPinObj);
}

TSharedRef<SWidget> SArcScalableCurveFloatGraphPin::GetDefaultValueWidget()
{
	FArcScalableCurveFloat DefaultValue;

	// Parse current default value
	const FString DefaultString = GraphPinObj->GetDefaultAsString();
	if (!DefaultString.IsEmpty())
	{
		UScriptStruct* PinStruct = FArcScalableCurveFloat::StaticStruct();
		PinStruct->ImportText(*DefaultString, &DefaultValue, nullptr, EPropertyPortFlags::PPF_SerializedAsImportText, GError, PinStruct->GetName(), true);
	}

	FProperty* CurrentProperty = nullptr;
	UScriptStruct* CurrentOwner = const_cast<UScriptStruct*>(DefaultValue.GetOwnerType());

	// Try to resolve the current property from field paths
	// The widget will show "None" if we can't resolve it

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SArcScalableCurveFloatWidget)
			.OnSelectionChanged(this, &SArcScalableCurveFloatGraphPin::OnSelectionChanged)
			.DefaultProperty(CurrentProperty)
			.DefaultOwnerStruct(CurrentOwner)
			.Visibility(this, &SGraphPin::GetDefaultValueVisibility)
			.IsEnabled(this, &SArcScalableCurveFloatGraphPin::GetDefaultValueIsEnabled)
		];
}

void SArcScalableCurveFloatGraphPin::OnSelectionChanged(FProperty* SelectedProperty, UScriptStruct* OwnerStruct, EArcScalableType Type)
{
	FString FinalValue;

	if (SelectedProperty && OwnerStruct)
	{
		FArcScalableCurveFloat NewValue;

		if (Type == EArcScalableType::Scalable)
		{
			NewValue = FArcScalableCurveFloat(CastField<FStructProperty>(SelectedProperty), OwnerStruct);
		}
		else if (Type == EArcScalableType::Curve)
		{
			NewValue = FArcScalableCurveFloat(CastField<FObjectProperty>(SelectedProperty), OwnerStruct);
		}

		FArcScalableCurveFloat::StaticStruct()->ExportText(FinalValue, &NewValue, &NewValue, nullptr, EPropertyPortFlags::PPF_SerializedAsImportText, nullptr);
	}

	if (FinalValue != GraphPinObj->GetDefaultAsString())
	{
		const FScopedTransaction Transaction(LOCTEXT("ChangePin", "Change Pin Value"));
		GraphPinObj->Modify();
		GraphPinObj->GetSchema()->TrySetDefaultValue(*GraphPinObj, FinalValue);
	}
}

#undef LOCTEXT_NAMESPACE
