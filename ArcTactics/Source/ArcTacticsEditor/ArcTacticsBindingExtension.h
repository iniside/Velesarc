// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "PropertyBindingExtension.h"

class UArcTacticsConfig;

class FArcTacticsBindingExtension final : public FPropertyBindingExtension
{
protected:
	virtual TSharedPtr<UE::PropertyBinding::FCachedBindingData> CreateCachedBindingData(
		IPropertyBindingBindingCollectionOwner* InBindingsOwner,
		const FPropertyBindingPath& InTargetPath,
		const TSharedPtr<IPropertyHandle>& InPropertyHandle,
		TConstArrayView<TInstancedStruct<FPropertyBindingBindableStructDescriptor>> InAccessibleStructs) const override;
	virtual bool CanBindToProperty(
		const FPropertyBindingPath& InTargetPath,
		const IPropertyHandle& InPropertyHandle) const override;
	virtual bool GetPromotionToParameterOverrideInternal(
		const FProperty& InProperty,
		bool& bOutOverride) const override;
	virtual void UpdateContextStruct(
		TConstStructView<FPropertyBindingBindableStructDescriptor> InStructDesc,
		FBindingContextStruct& InOutContextStruct,
		TMap<FString, FText>& InOutSectionNames) const override;
	virtual void CustomizeDetailWidgetRow(
		FDetailWidgetRow& InWidgetRow,
		const IDetailLayoutBuilder& InDetailBuilder,
		IPropertyBindingBindingCollectionOwner* InBindingsOwner,
		TSharedPtr<IPropertyHandle> InPropertyHandle,
		const FPropertyBindingPath& InTargetPath,
		TSharedPtr<UE::PropertyBinding::FCachedBindingData> InCachedBindingData) const override;
	virtual void CustomizePropertyBindingWidgetArgs(
		FPropertyBindingWidgetArgs& Args,
		IPropertyBindingBindingCollectionOwner* InBindingsOwner,
		const FPropertyBindingPath& InTargetPath,
		TSharedPtr<UE::PropertyBinding::FCachedBindingData> InCachedBindingData) override;
	virtual bool CanBindToArrayElements() const override { return true; }
};
