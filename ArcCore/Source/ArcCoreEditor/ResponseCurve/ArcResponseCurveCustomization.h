// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"

class IPropertyHandle;
struct FArcResponseCurve;

class FArcResponseCurveCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle,
		FDetailWidgetRow& HeaderRow,
		IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;

	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle,
		IDetailChildrenBuilder& StructBuilder,
		IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;

private:
	const FArcResponseCurve* GetFirstCurve() const;
	TSharedRef<SWidget> BuildPresetMenu();
	void ApplyPreset(int32 PresetIndex);
	FText GetPresetButtonText() const;
	bool IsEditable() const;

	TSharedPtr<IPropertyHandle> StructHandle;
	TSharedPtr<IPropertyHandle> TypeHandle;
	TSharedPtr<IPropertyHandle> SlopeHandle;
	TSharedPtr<IPropertyHandle> ExponentHandle;
	TSharedPtr<IPropertyHandle> XShiftHandle;
	TSharedPtr<IPropertyHandle> YShiftHandle;
	TSharedPtr<IPropertyHandle> CurveInfoHandle;
};
