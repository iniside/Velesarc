// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"
#include "Effects/ArcAttributeCapture.h"

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
		FArcAttributeCaptureDef Def;
	};

	void RebuildCaptureOptions(TSharedRef<IPropertyHandle> StructPropertyHandle);
	FText GetSelectedCaptureText() const;
	FText GetSummaryText() const;
	void OnCaptureSelected(const FArcAttributeCaptureDef& NewDef);

	TSharedPtr<IPropertyHandle> CaptureDefHandle;            // FArcScopedModifier::CaptureDef
	TSharedPtr<IPropertyHandle> CaptureDef_FragTypeHandle;   // CaptureDef.Attribute.FragmentType
	TSharedPtr<IPropertyHandle> CaptureDef_PropNameHandle;   // CaptureDef.Attribute.PropertyName
	TSharedPtr<IPropertyHandle> CaptureDef_SourceHandle;     // CaptureDef.CaptureSource
	TSharedPtr<IPropertyHandle> CaptureDef_SnapshotHandle;   // CaptureDef.bSnapshot
	TSharedPtr<IPropertyHandle> OperationHandle;
	TSharedPtr<IPropertyHandle> TypeHandle;
	TSharedPtr<IPropertyHandle> MagnitudeHandle;
	TSharedPtr<IPropertyHandle> SetByCallerTagHandle;
	TSharedPtr<IPropertyHandle> CustomMagnitudeHandle;
	TArray<FCaptureOption> CaptureOptions;
	TArray<TSharedPtr<FCaptureOption>> ComboItems;
};
