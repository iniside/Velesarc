// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EdGraphSchema_K2.h"
#include "EdGraphUtilities.h"
#include "SGraphPin.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

#include "Items/ArcItemScalableFloat.h"

class SArcScalableCurveFloatGraphPin : public SGraphPin
{
public:
	SLATE_BEGIN_ARGS(SArcScalableCurveFloatGraphPin) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj);

	virtual TSharedRef<SWidget> GetDefaultValueWidget() override;

	void OnSelectionChanged(FProperty* SelectedProperty, UScriptStruct* OwnerStruct, EArcScalableType Type);

private:
	bool GetDefaultValueIsEnabled() const
	{
		return !GraphPinObj->bDefaultValueIsReadOnly;
	}
};

class FArcScalableCurveFloatPinFactory : public FGraphPanelPinFactory
{
	virtual TSharedPtr<class SGraphPin> CreatePin(class UEdGraphPin* InPin) const override
	{
		if (InPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Struct
			&& InPin->PinType.PinSubCategoryObject == FArcScalableCurveFloat::StaticStruct())
		{
			return SNew(SArcScalableCurveFloatGraphPin, InPin);
		}
		return nullptr;
	}
};
