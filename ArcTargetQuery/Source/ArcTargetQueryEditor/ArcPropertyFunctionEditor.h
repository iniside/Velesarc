// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "IDetailPropertyChildrenCustomizationHandler.h"
#include "PropertyBindingExtension.h"
#include "Templates/SubclassOf.h"

struct FBindingChainElement;
struct FBindingContextStruct;

namespace ArcPropertyFunctionEditor
{
	ARCTARGETQUERYEDITOR_API const UScriptStruct* GetFunctionStruct(const FPropertyBindingBindableStructDescriptor& Descriptor);
	ARCTARGETQUERYEDITOR_API bool CanAcceptFunctionProperty(const FProperty& Property, TConstArrayView<FBindingChainElement> BindingChain);
	ARCTARGETQUERYEDITOR_API FString DescribeFunction(const IPropertyBindingBindingCollectionOwner& Owner, FConstStructView FunctionView);
	ARCTARGETQUERYEDITOR_API bool UpdateFunctionContextStruct(
		TConstStructView<FPropertyBindingBindableStructDescriptor> StructDesc,
		FBindingContextStruct& InOutContextStruct,
		TMap<FString, FText>& InOutSectionNames);
}

class ARCTARGETQUERYEDITOR_API FArcPropertyFunctionCachedBindingData : public UE::PropertyBinding::FCachedBindingData
{
public:
	FArcPropertyFunctionCachedBindingData(
		IPropertyBindingBindingCollectionOwner* InBindingsOwner,
		const FPropertyBindingPath& InTargetPath,
		const TSharedPtr<const IPropertyHandle>& InPropertyHandle,
		TConstArrayView<TInstancedStruct<FPropertyBindingBindableStructDescriptor>> InAccessibleStructs);

	virtual FPinInfo GetPinInfoForProperty(TNotNull<const FProperty*> InProperty, FPropertyBindingDataView InTargetDataView) const override;
	virtual void GetSourceDataViewForNewBinding(
		TNotNull<IPropertyBindingBindingCollectionOwner*> InBindingsOwner,
		TConstStructView<FPropertyBindingBindableStructDescriptor> InDescriptor,
		FPropertyBindingDataView& OutSourceDataView) override;
	virtual void UpdateSourcePropertyPath(
		TConstStructView<FPropertyBindingBindableStructDescriptor> InDescriptor,
		const FPropertyBindingPath& InSourcePath,
		FString& OutString) override;
	virtual bool GetPropertyFunctionText(FConstStructView InPropertyFunctionStructView, FText& OutText) const override;
	virtual bool GetPropertyFunctionTooltipText(FConstStructView InPropertyFunctionStructView, FText& OutText) const override;
	virtual bool GetPropertyFunctionIconColor(FConstStructView InPropertyFunctionStructView, FLinearColor& OutColor) const override;
	virtual bool GetPropertyFunctionImage(FConstStructView InPropertyFunctionStructView, const FSlateBrush*& OutImage) const override;

protected:
	const UScriptStruct* GetChainFunctionStruct(TConstArrayView<FBindingChainElement> InBindingChain) const;
	bool AddFunctionBinding(
		TConstStructView<FPropertyBindingBindableStructDescriptor> InDescriptor,
		FPropertyBindingPath& InOutSourcePath,
		const FPropertyBindingPath& InTargetPath);
	virtual FPropertyBindingBinding& AddFunctionBindingToCollection(
		FConstStructView FunctionNode,
		const FPropertyBindingPath& InSourcePath,
		const FPropertyBindingPath& InTargetPath) = 0;
};

class ARCTARGETQUERYEDITOR_API FArcPropertyFunctionChildrenCustomization final : public IDetailPropertyChildrenCustomizationHandler
{
public:
	explicit FArcPropertyFunctionChildrenCustomization(TSubclassOf<UObject> InOwnerClass);

	virtual bool ShouldCustomizeChildren(TSharedRef<IPropertyHandle> InPropertyHandle) override;
	virtual void CustomizeChildren(IDetailChildrenBuilder& ChildrenBuilder, TSharedPtr<IPropertyHandle> InPropertyHandle) override;

private:
	TSubclassOf<UObject> OwnerClass;
};
