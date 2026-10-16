// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSBindingExtension.h"

#include "ArcPropertyFunctionEditor.h"
#include "ArcTQSBindings.h"
#include "ArcTQSQueryDefinition.h"
#include "IPropertyAccessEditor.h"
#include "PropertyHandle.h"

namespace ArcTQSBindingExtensionPrivate
{
	class FCachedBindingData final : public FArcPropertyFunctionCachedBindingData
	{
	public:
		FCachedBindingData(
			IPropertyBindingBindingCollectionOwner* InBindingsOwner,
			const FPropertyBindingPath& InTargetPath,
			const TSharedPtr<const IPropertyHandle>& InPropertyHandle,
			const TConstArrayView<TInstancedStruct<FPropertyBindingBindableStructDescriptor>> InAccessibleStructs)
			: FArcPropertyFunctionCachedBindingData(InBindingsOwner, InTargetPath, InPropertyHandle, InAccessibleStructs)
		{
		}

		virtual bool CanBindToContextStructInternal(const UStruct* InStruct, const int32 InStructIndex) override
		{
			return false;
		}

		virtual bool CanAcceptPropertyOrChildrenInternal(
			const FProperty& InProperty,
			const TConstArrayView<FBindingChainElement> InBindingChain) override
		{
			const TConstArrayView<TInstancedStruct<FPropertyBindingBindableStructDescriptor>> Sources = GetAccessibleStructs();
			if (InBindingChain.IsEmpty() || !Sources.IsValidIndex(InBindingChain[0].ArrayIndex))
			{
				return false;
			}
			if (GetChainFunctionStruct(InBindingChain))
			{
				return ArcPropertyFunctionEditor::CanAcceptFunctionProperty(InProperty, InBindingChain);
			}
			return true;
		}

		virtual bool AddBindingInternal(
			const TConstStructView<FPropertyBindingBindableStructDescriptor> InDescriptor,
			FPropertyBindingPath& InOutSourcePath,
			const FPropertyBindingPath& InTargetPath) override
		{
			if (!Cast<UArcTQSQueryDefinition>(GetOwner()))
			{
				return false;
			}
			return AddFunctionBinding(InDescriptor, InOutSourcePath, InTargetPath);
		}

	protected:
		virtual FPropertyBindingBinding& AddFunctionBindingToCollection(
			const FConstStructView FunctionNode,
			const FPropertyBindingPath& InSourcePath,
			const FPropertyBindingPath& InTargetPath) override
		{
			return CastChecked<UArcTQSQueryDefinition>(GetOwner())->EditorBindings.AddCompiledBinding(
				FArcTQSBinding(FunctionNode, InSourcePath, InTargetPath));
		}
	};
}

bool FArcTQSBindingExtension::IsPropertyExtendable(const UClass* InObjectClass, const IPropertyHandle& InPropertyHandle) const
{
	TArray<UObject*> OuterObjects;
	InPropertyHandle.GetOuterObjects(OuterObjects);
	if (OuterObjects.Num() != 1 || !Cast<UArcTQSQueryDefinition>(OuterObjects[0]))
	{
		return false;
	}
	return FPropertyBindingExtension::IsPropertyExtendable(InObjectClass, InPropertyHandle);
}

TSharedPtr<UE::PropertyBinding::FCachedBindingData> FArcTQSBindingExtension::CreateCachedBindingData(
	IPropertyBindingBindingCollectionOwner* InBindingsOwner,
	const FPropertyBindingPath& InTargetPath,
	const TSharedPtr<IPropertyHandle>& InPropertyHandle,
	const TConstArrayView<TInstancedStruct<FPropertyBindingBindableStructDescriptor>> InAccessibleStructs) const
{
	return MakeShared<ArcTQSBindingExtensionPrivate::FCachedBindingData>(
		InBindingsOwner, InTargetPath, InPropertyHandle, InAccessibleStructs);
}

bool FArcTQSBindingExtension::CanBindToProperty(
	const FPropertyBindingPath& InTargetPath,
	const IPropertyHandle& InPropertyHandle) const
{
	const FProperty* Property = InPropertyHandle.GetProperty();
	if (!Property || !ArcTQS::IsBindableProperty(*Property))
	{
		return false;
	}
	for (TSharedPtr<IPropertyHandle> Parent = InPropertyHandle.GetParentHandle(); Parent.IsValid(); Parent = Parent->GetParentHandle())
	{
		const FProperty* ParentProperty = Parent->GetProperty();
		if (ParentProperty && ArcTQS::IsLiteralOnlyProperty(*ParentProperty))
		{
			return false;
		}
	}
	return true;
}

void FArcTQSBindingExtension::UpdateContextStruct(
	const TConstStructView<FPropertyBindingBindableStructDescriptor> InStructDesc,
	FBindingContextStruct& InOutContextStruct,
	TMap<FString, FText>& InOutSectionNames) const
{
	ArcPropertyFunctionEditor::UpdateFunctionContextStruct(InStructDesc, InOutContextStruct, InOutSectionNames);
}
