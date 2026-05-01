// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"

class IPropertyHandle;

class FArcScopedModifierCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	virtual void CustomizeHeader(
		TSharedRef<IPropertyHandle> StructPropertyHandle,
		FDetailWidgetRow& HeaderRow,
		IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;

	virtual void CustomizeChildren(
		TSharedRef<IPropertyHandle> StructPropertyHandle,
		IDetailChildrenBuilder& StructBuilder,
		IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;

private:
	struct FCaptureOption
	{
		int32 Index;
		FText DisplayName;
	};

	void RebuildCaptureOptions(TSharedRef<IPropertyHandle> StructPropertyHandle);
	FText GetSelectedCaptureText() const;
	void OnCaptureSelected(int32 NewIndex);

	TSharedPtr<IPropertyHandle> CaptureIndexHandle;
	TArray<FCaptureOption> CaptureOptions;
	TArray<TSharedPtr<FCaptureOption>> ComboItems;
};
