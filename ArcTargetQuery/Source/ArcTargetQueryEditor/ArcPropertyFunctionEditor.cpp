// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcPropertyFunctionEditor.h"

#include "ArcTQSBindings.h"
#include "ArcTQSPropertyFunction.h"
#include "IDetailChildrenBuilder.h"
#include "IPropertyAccessEditor.h"
#include "IStructureDataProvider.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "PropertyBindingBindingCollectionOwner.h"
#include "PropertyBindingDataView.h"
#include "PropertyHandle.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "ArcPropertyFunctionEditor"

namespace ArcPropertyFunctionEditorPrivate
{
	constexpr int32 MaxDescriptionDepth = 4;

	FString DescribeFunction(const IPropertyBindingBindingCollectionOwner& Owner, FConstStructView FunctionView, int32 Depth);

	FString GetEndpointName(const IPropertyBindingBindingCollectionOwner& Owner, const FGuid& ID)
	{
		TInstancedStruct<FPropertyBindingBindableStructDescriptor> Descriptor;
		return Owner.GetBindableStructByID(ID, Descriptor)
			? Descriptor.Get().Name.ToString()
			: FName(NAME_None).ToString();
	}

	FString DescribeSource(const IPropertyBindingBindingCollectionOwner& Owner, const FPropertyBindingBinding& Binding, const int32 Depth)
	{
		if (Binding.GetPropertyFunctionNode().IsValid())
		{
			return Depth < MaxDescriptionDepth
				? DescribeFunction(Owner, Binding.GetPropertyFunctionNode(), Depth + 1)
				: FString(TEXT("..."));
		}
		const FString Endpoint = GetEndpointName(Owner, Binding.GetSourcePath().GetStructID());
		const FString Path = Binding.GetSourcePath().ToString();
		return Path.IsEmpty() ? Endpoint : Endpoint + TEXT(".") + Path;
	}

	FString DescribeFunction(const IPropertyBindingBindingCollectionOwner& Owner, const FConstStructView FunctionView, const int32 Depth)
	{
		const FArcTQSPropertyFunction* Function = FunctionView.GetPtr<const FArcTQSPropertyFunction>();
		const FPropertyBindingBindingCollection* Bindings = Owner.GetEditorPropertyBindings();
		if (!Function || !Bindings)
		{
			return FString();
		}
		TArray<FString> Inputs;
		Bindings->ForEachBinding([&Owner, Function, Depth, &Inputs](const FPropertyBindingBinding& Binding)
		{
			if (Binding.GetTargetPath().GetStructID() == Function->ID)
			{
				Inputs.Add(FString::Printf(TEXT("%s: %s"),
					*Binding.GetTargetPath().ToString(), *DescribeSource(Owner, Binding, Depth)));
			}
		});
		const FString Name = FunctionView.GetScriptStruct()->GetDisplayNameText().ToString();
		return Inputs.IsEmpty() ? Name : FString::Printf(TEXT("%s(%s)"), *Name, *FString::Join(Inputs, TEXT(", ")));
	}

	const IPropertyBindingBindingCollectionOwner* GetBindingsOwner(UObject* Object)
	{
		return Cast<IPropertyBindingBindingCollectionOwner>(Object);
	}

	UObject* FindOwnerObject(const IPropertyHandle& PropertyHandle, const UClass* OwnerClass)
	{
		TArray<UObject*> OuterObjects;
		PropertyHandle.GetOuterObjects(OuterObjects);
		if (OuterObjects.Num() != 1 || !OuterObjects[0] || !OwnerClass || !OuterObjects[0]->IsA(OwnerClass))
		{
			return nullptr;
		}
		return Cast<IPropertyBindingBindingCollectionOwner>(OuterObjects[0]) ? OuterObjects[0] : nullptr;
	}

	FStructView FindBoundFunction(UObject* OwnerObject, const FPropertyBindingPath& TargetPath)
	{
		FStructView Result;
		IPropertyBindingBindingCollectionOwner* Owner = Cast<IPropertyBindingBindingCollectionOwner>(OwnerObject);
		FPropertyBindingBindingCollection* Bindings = Owner ? Owner->GetEditorPropertyBindings() : nullptr;
		if (!Bindings || TargetPath.IsPathEmpty())
		{
			return Result;
		}
		Bindings->VisitMutableBindings([&TargetPath, &Result](FPropertyBindingBinding& Binding)
		{
			if (Binding.GetTargetPath() != TargetPath)
			{
				return FPropertyBindingBindingCollection::EVisitResult::Continue;
			}
			const FStructView Node = Binding.GetMutablePropertyFunctionNode();
			if (Node.GetPtr<FArcTQSPropertyFunction>())
			{
				Result = Node;
			}
			return FPropertyBindingBindingCollection::EVisitResult::Break;
		});
		return Result;
	}

	FStructView FindBoundFunction(const TSharedPtr<IPropertyHandle>& PropertyHandle, const UClass* OwnerClass, FPropertyBindingPath& OutTargetPath)
	{
		if (!PropertyHandle.IsValid() || !PropertyHandle->IsValidHandle())
		{
			return FStructView();
		}
		UObject* OwnerObject = FindOwnerObject(*PropertyHandle, OwnerClass);
		if (!OwnerObject)
		{
			return FStructView();
		}
		UE::PropertyBinding::MakeStructPropertyPathFromPropertyHandle(PropertyHandle, OutTargetPath);
		return FindBoundFunction(OwnerObject, OutTargetPath);
	}

	class FPropertyFunctionNodeProvider final : public IStructureDataProvider
	{
	public:
		FPropertyFunctionNodeProvider(UObject& InOwner, FPropertyBindingPath InTargetPath, const UScriptStruct* InFunctionStruct)
			: Owner(&InOwner)
			, TargetPath(MoveTemp(InTargetPath))
			, FunctionStruct(InFunctionStruct)
		{
		}

		virtual bool IsValid() const override
		{
			const FStructView Node = FindBoundFunction(Owner.Get(), TargetPath);
			return Node.IsValid() && Node.GetScriptStruct() == FunctionStruct.Get();
		}

		virtual const UStruct* GetBaseStructure() const override
		{
			return FunctionStruct.Get();
		}

		virtual void GetInstances(TArray<TSharedPtr<FStructOnScope>>& OutInstances, const UStruct* ExpectedBaseStructure) const override
		{
			if (!ExpectedBaseStructure)
			{
				return;
			}
			const FStructView Node = FindBoundFunction(Owner.Get(), TargetPath);
			if (Node.IsValid() && Node.GetScriptStruct()->IsChildOf(ExpectedBaseStructure))
			{
				OutInstances.Add(MakeShared<FStructOnScope>(Node.GetScriptStruct(), Node.GetMemory()));
			}
		}

	private:
		TWeakObjectPtr<UObject> Owner;
		FPropertyBindingPath TargetPath;
		TWeakObjectPtr<const UScriptStruct> FunctionStruct;
	};
}

const UScriptStruct* ArcPropertyFunctionEditor::GetFunctionStruct(const FPropertyBindingBindableStructDescriptor& Descriptor)
{
	const UScriptStruct* Struct = Cast<UScriptStruct>(Descriptor.Struct);
	return Struct && Struct->IsChildOf(FArcTQSPropertyFunction::StaticStruct()) ? Struct : nullptr;
}

bool ArcPropertyFunctionEditor::CanAcceptFunctionProperty(const FProperty& Property, const TConstArrayView<FBindingChainElement> BindingChain)
{
	return BindingChain.Num() > 1 || ArcTQS::IsFunctionOutputProperty(Property);
}

FString ArcPropertyFunctionEditor::DescribeFunction(const IPropertyBindingBindingCollectionOwner& Owner, const FConstStructView FunctionView)
{
	return ArcPropertyFunctionEditorPrivate::DescribeFunction(Owner, FunctionView, 0);
}

bool ArcPropertyFunctionEditor::UpdateFunctionContextStruct(
	const TConstStructView<FPropertyBindingBindableStructDescriptor> StructDesc,
	FBindingContextStruct& InOutContextStruct,
	TMap<FString, FText>& InOutSectionNames)
{
	if (!GetFunctionStruct(StructDesc.Get()))
	{
		return false;
	}
	const FString SectionKey(TEXT("Functions"));
	if (const FText* SectionText = InOutSectionNames.Find(SectionKey))
	{
		InOutContextStruct.Section = *SectionText;
		return true;
	}
	InOutContextStruct.Section = InOutSectionNames.Add(SectionKey, LOCTEXT("FunctionsSection", "Functions"));
	return true;
}

FArcPropertyFunctionCachedBindingData::FArcPropertyFunctionCachedBindingData(
	IPropertyBindingBindingCollectionOwner* InBindingsOwner,
	const FPropertyBindingPath& InTargetPath,
	const TSharedPtr<const IPropertyHandle>& InPropertyHandle,
	const TConstArrayView<TInstancedStruct<FPropertyBindingBindableStructDescriptor>> InAccessibleStructs)
	: UE::PropertyBinding::FCachedBindingData(InBindingsOwner, InTargetPath, InPropertyHandle, InAccessibleStructs)
{
}

UE::PropertyBinding::FCachedBindingData::FPinInfo FArcPropertyFunctionCachedBindingData::GetPinInfoForProperty(
	const TNotNull<const FProperty*> InProperty,
	const FPropertyBindingDataView InTargetDataView) const
{
	FPinInfo Info = UE::PropertyBinding::FCachedBindingData::GetPinInfoForProperty(InProperty, InTargetDataView);
	Info.IconBrush = FBlueprintEditorUtils::GetIconFromPin(Info.Type, true);
	return Info;
}

void FArcPropertyFunctionCachedBindingData::GetSourceDataViewForNewBinding(
	TNotNull<IPropertyBindingBindingCollectionOwner*> InBindingsOwner,
	const TConstStructView<FPropertyBindingBindableStructDescriptor> InDescriptor,
	FPropertyBindingDataView& OutSourceDataView)
{
	if (const UScriptStruct* FunctionStruct = ArcPropertyFunctionEditor::GetFunctionStruct(InDescriptor.Get()))
	{
		OutSourceDataView = FPropertyBindingDataView(FunctionStruct, nullptr);
		return;
	}
	UE::PropertyBinding::FCachedBindingData::GetSourceDataViewForNewBinding(InBindingsOwner, InDescriptor, OutSourceDataView);
}

void FArcPropertyFunctionCachedBindingData::UpdateSourcePropertyPath(
	const TConstStructView<FPropertyBindingBindableStructDescriptor> InDescriptor,
	const FPropertyBindingPath& InSourcePath,
	FString& OutString)
{
	if (ArcPropertyFunctionEditor::GetFunctionStruct(InDescriptor.Get()))
	{
		OutString = InSourcePath.ToString(INDEX_NONE, nullptr, nullptr, false, 1);
	}
}

bool FArcPropertyFunctionCachedBindingData::GetPropertyFunctionText(const FConstStructView InPropertyFunctionStructView, FText& OutText) const
{
	const IPropertyBindingBindingCollectionOwner* Owner = ArcPropertyFunctionEditorPrivate::GetBindingsOwner(GetOwner());
	if (!Owner || !InPropertyFunctionStructView.GetPtr<const FArcTQSPropertyFunction>())
	{
		return false;
	}
	OutText = FText::FormatNamed(GetFormatableText(), TEXT("SourceStruct"),
		FText::FromString(ArcPropertyFunctionEditor::DescribeFunction(*Owner, InPropertyFunctionStructView)));
	return true;
}

bool FArcPropertyFunctionCachedBindingData::GetPropertyFunctionTooltipText(const FConstStructView InPropertyFunctionStructView, FText& OutText) const
{
	const IPropertyBindingBindingCollectionOwner* Owner = ArcPropertyFunctionEditorPrivate::GetBindingsOwner(GetOwner());
	if (!Owner || !InPropertyFunctionStructView.GetPtr<const FArcTQSPropertyFunction>())
	{
		return false;
	}
	OutText = FText::FormatNamed(GetFormatableTooltipText(), TEXT("SourceStruct"),
		FText::FromString(ArcPropertyFunctionEditor::DescribeFunction(*Owner, InPropertyFunctionStructView)));
	return true;
}

bool FArcPropertyFunctionCachedBindingData::GetPropertyFunctionIconColor(const FConstStructView InPropertyFunctionStructView, FLinearColor& OutColor) const
{
	const FProperty* OutputProperty = ArcTQS::FindFunctionOutputProperty(InPropertyFunctionStructView.GetScriptStruct());
	if (!OutputProperty)
	{
		return false;
	}
	OutColor = GetPinInfoForProperty(OutputProperty, FPropertyBindingDataView()).IconColor;
	return true;
}

bool FArcPropertyFunctionCachedBindingData::GetPropertyFunctionImage(const FConstStructView InPropertyFunctionStructView, const FSlateBrush*& OutImage) const
{
	if (!InPropertyFunctionStructView.GetPtr<const FArcTQSPropertyFunction>())
	{
		return false;
	}
	OutImage = FAppStyle::GetBrush(TEXT("GraphEditor.Function_16x"));
	return true;
}

const UScriptStruct* FArcPropertyFunctionCachedBindingData::GetChainFunctionStruct(const TConstArrayView<FBindingChainElement> InBindingChain) const
{
	const TConstArrayView<TInstancedStruct<FPropertyBindingBindableStructDescriptor>> Sources = GetAccessibleStructs();
	if (InBindingChain.IsEmpty() || !Sources.IsValidIndex(InBindingChain[0].ArrayIndex))
	{
		return nullptr;
	}
	return ArcPropertyFunctionEditor::GetFunctionStruct(Sources[InBindingChain[0].ArrayIndex].Get());
}

bool FArcPropertyFunctionCachedBindingData::AddFunctionBinding(
	const TConstStructView<FPropertyBindingBindableStructDescriptor> InDescriptor,
	FPropertyBindingPath& InOutSourcePath,
	const FPropertyBindingPath& InTargetPath)
{
	const UScriptStruct* FunctionStruct = ArcPropertyFunctionEditor::GetFunctionStruct(InDescriptor.Get());
	IPropertyBindingBindingCollectionOwner* Owner = Cast<IPropertyBindingBindingCollectionOwner>(GetOwner());
	FPropertyBindingBindingCollection* Bindings = Owner ? Owner->GetEditorPropertyBindings() : nullptr;
	if (!FunctionStruct || !Bindings)
	{
		return false;
	}
	const FProperty* OutputProperty = ArcTQS::FindFunctionOutputProperty(FunctionStruct);
	if (!OutputProperty)
	{
		return true;
	}

	TArray<FPropertyBindingPathSegment> SourceSegments(InOutSourcePath.GetSegments());
	if (SourceSegments.IsEmpty())
	{
		SourceSegments.Emplace(OutputProperty->GetFName());
	}

	FInstancedStruct FunctionNode;
	FunctionNode.InitializeAs(FunctionStruct);
	FArcTQSPropertyFunction& Function = FunctionNode.GetMutable<FArcTQSPropertyFunction>();
	Function.ID = FGuid::NewGuid();
	InOutSourcePath = FPropertyBindingPath(Function.ID, SourceSegments);

	Bindings->RemoveBindings(InTargetPath, FPropertyBindingBindingCollection::ESearchMode::Exact);
	FPropertyBindingBinding& Binding = AddFunctionBindingToCollection(FConstStructView(FunctionNode), InOutSourcePath, InTargetPath);

	const FStructView BoundNode = Binding.GetMutablePropertyFunctionNode();
	Binding.GetMutableSourcePath().UpdateSegmentsFromValue(
		FPropertyBindingDataView(BoundNode.GetScriptStruct(), BoundNode.GetMemory()));
	FPropertyBindingDataView TargetDataView;
	if (Owner->GetBindingDataViewByID(InTargetPath.GetStructID(), TargetDataView))
	{
		Binding.GetMutableTargetPath().UpdateSegmentsFromValue(TargetDataView);
	}
	InOutSourcePath = Binding.GetSourcePath();
	return true;
}

FArcPropertyFunctionChildrenCustomization::FArcPropertyFunctionChildrenCustomization(const TSubclassOf<UObject> InOwnerClass)
	: OwnerClass(InOwnerClass)
{
}

bool FArcPropertyFunctionChildrenCustomization::ShouldCustomizeChildren(TSharedRef<IPropertyHandle> InPropertyHandle)
{
	FPropertyBindingPath TargetPath;
	return ArcPropertyFunctionEditorPrivate::FindBoundFunction(InPropertyHandle.ToSharedPtr(), OwnerClass.Get(), TargetPath).IsValid();
}

void FArcPropertyFunctionChildrenCustomization::CustomizeChildren(IDetailChildrenBuilder& ChildrenBuilder, TSharedPtr<IPropertyHandle> InPropertyHandle)
{
	using namespace ArcPropertyFunctionEditorPrivate;

	FPropertyBindingPath TargetPath;
	const FStructView Node = FindBoundFunction(InPropertyHandle, OwnerClass.Get(), TargetPath);
	UObject* OwnerObject = InPropertyHandle.IsValid() ? FindOwnerObject(*InPropertyHandle, OwnerClass.Get()) : nullptr;
	if (!Node.IsValid() || !OwnerObject)
	{
		return;
	}

	const FString FunctionID = LexToString(Node.Get<FArcTQSPropertyFunction>().ID);
	InPropertyHandle->RemoveChildren();
	const TArray<TSharedPtr<IPropertyHandle>> InputHandles = InPropertyHandle->AddChildStructure(
		MakeShared<FPropertyFunctionNodeProvider>(*OwnerObject, TargetPath, Node.GetScriptStruct()));
	for (const TSharedPtr<IPropertyHandle>& InputHandle : InputHandles)
	{
		const FProperty* InputProperty = InputHandle.IsValid() ? InputHandle->GetProperty() : nullptr;
		if (!InputProperty || ArcTQS::IsFunctionOutputProperty(*InputProperty))
		{
			continue;
		}
		if (!ArcTQS::IsLiteralOnlyProperty(*InputProperty))
		{
			InputHandle->SetInstanceMetaData(UE::PropertyBinding::MetaDataStructIDName, FunctionID);
		}
		ChildrenBuilder.AddProperty(InputHandle.ToSharedRef());
	}
}

#undef LOCTEXT_NAMESPACE
