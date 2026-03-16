// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Items/ArcItemScalableFloat.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Input/SComboButton.h"

struct FArcScalableCurveFloat;

class SArcScalableCurveFloatWidget : public SCompoundWidget
{
public:
	DECLARE_DELEGATE_ThreeParams(FOnScalableCurveFloatChanged, FProperty* /*SelectedProperty*/, UScriptStruct* /*OwnerStruct*/, EArcScalableType /*Type*/);

	SLATE_BEGIN_ARGS(SArcScalableCurveFloatWidget)
		: _DefaultProperty(nullptr)
		, _DefaultOwnerStruct(nullptr)
	{}
		SLATE_ARGUMENT(FProperty*, DefaultProperty)
		SLATE_ARGUMENT(UScriptStruct*, DefaultOwnerStruct)
		SLATE_EVENT(FOnScalableCurveFloatChanged, OnSelectionChanged)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	TSharedRef<SWidget> GeneratePickerContent();
	FText GetSelectedValueAsString() const;

	void OnPropertyPicked(FProperty* InProperty, UScriptStruct* InOwnerStruct, EArcScalableType InType);

	FOnScalableCurveFloatChanged OnSelectionChanged;

	FProperty* SelectedProperty;
	UScriptStruct* SelectedOwnerStruct;

	TSharedPtr<SComboButton> ComboButton;
};
