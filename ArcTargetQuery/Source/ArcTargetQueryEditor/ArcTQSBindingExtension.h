// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "PropertyBindingExtension.h"

class FArcTQSBindingExtension final : public FPropertyBindingExtension
{
public:
	virtual bool IsPropertyExtendable(const UClass* InObjectClass, const IPropertyHandle& InPropertyHandle) const override;

protected:
	virtual TSharedPtr<UE::PropertyBinding::FCachedBindingData> CreateCachedBindingData(
		IPropertyBindingBindingCollectionOwner* InBindingsOwner,
		const FPropertyBindingPath& InTargetPath,
		const TSharedPtr<IPropertyHandle>& InPropertyHandle,
		TConstArrayView<TInstancedStruct<FPropertyBindingBindableStructDescriptor>> InAccessibleStructs) const override;
	virtual bool CanBindToProperty(
		const FPropertyBindingPath& InTargetPath,
		const IPropertyHandle& InPropertyHandle) const override;
	virtual void UpdateContextStruct(
		TConstStructView<FPropertyBindingBindableStructDescriptor> InStructDesc,
		FBindingContextStruct& InOutContextStruct,
		TMap<FString, FText>& InOutSectionNames) const override;
};
