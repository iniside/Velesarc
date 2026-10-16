// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSBindings.h"

#include "ArcTQSPropertyFunction.h"
#include "UObject/UnrealType.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcTQSBindings)

FArcTQSBindableStructDescriptor& FArcTQSBindingCollection::AddBindableStruct(const FArcTQSBindableStructDescriptor& Descriptor)
{
	return BindableStructs.Add_GetRef(Descriptor);
}

FArcTQSBinding& FArcTQSBindingCollection::AddCompiledBinding(FArcTQSBinding&& Binding)
{
	return Bindings.Add_GetRef(MoveTemp(Binding));
}

const FArcTQSBindableStructDescriptor* FArcTQSBindingCollection::FindBindableStruct(const FArcTQSDataHandle Handle) const
{
	return BindableStructs.FindByPredicate([Handle](const FArcTQSBindableStructDescriptor& Descriptor)
	{
		return Descriptor.DataHandle == Handle;
	});
}

FArcTQSBindableStructDescriptor* FArcTQSBindingCollection::FindMutableBindableStruct(const FArcTQSDataHandle Handle)
{
	return BindableStructs.FindByPredicate([Handle](const FArcTQSBindableStructDescriptor& Descriptor)
	{
		return Descriptor.DataHandle == Handle;
	});
}

int32 FArcTQSBindingCollection::GetNumBindableStructDescriptors() const
{
	return BindableStructs.Num();
}

const FPropertyBindingBindableStructDescriptor* FArcTQSBindingCollection::GetBindableStructDescriptorFromHandle(
	const FConstStructView InSourceHandleView) const
{
	return FindBindableStruct(InSourceHandleView.Get<const FArcTQSDataHandle>());
}

int32 FArcTQSBindingCollection::GetNumBindings() const
{
	return Bindings.Num();
}

void FArcTQSBindingCollection::ForEachBinding(TFunctionRef<void(const FPropertyBindingBinding& Binding)> InFunction) const
{
	for (const FArcTQSBinding& Binding : Bindings)
	{
		InFunction(Binding);
	}
}

void FArcTQSBindingCollection::ForEachBinding(
	const FPropertyBindingIndex16 InBegin,
	const FPropertyBindingIndex16 InEnd,
	TFunctionRef<void(const FPropertyBindingBinding& Binding, const int32 BindingIndex)> InFunction) const
{
	check(InBegin.IsValid() && InEnd.IsValid());
	for (int32 Index = InBegin.Get(); Index < InEnd.Get(); ++Index)
	{
		InFunction(Bindings[Index], Index);
	}
}

void FArcTQSBindingCollection::ForEachMutableBinding(TFunctionRef<void(FPropertyBindingBinding& Binding)> InFunction)
{
	for (FArcTQSBinding& Binding : Bindings)
	{
		InFunction(Binding);
	}
}

void FArcTQSBindingCollection::VisitBindings(TFunctionRef<EVisitResult(const FPropertyBindingBinding& Binding)> InFunction) const
{
	for (const FArcTQSBinding& Binding : Bindings)
	{
		if (InFunction(Binding) == EVisitResult::Break)
		{
			break;
		}
	}
}

void FArcTQSBindingCollection::VisitBindings(
	const FPropertyBindingIndex16 InBegin,
	const FPropertyBindingIndex16 InEnd,
	TFunctionRef<EVisitResult(const FPropertyBindingBinding& Binding, const int32 BindingIndex)> InFunction) const
{
	check(InBegin.IsValid() && InEnd.IsValid());
	for (int32 Index = InBegin.Get(); Index < InEnd.Get(); ++Index)
	{
		if (InFunction(Bindings[Index], Index) == EVisitResult::Break)
		{
			break;
		}
	}
}

void FArcTQSBindingCollection::VisitMutableBindings(TFunctionRef<EVisitResult(FPropertyBindingBinding& Binding)> InFunction)
{
	for (FArcTQSBinding& Binding : Bindings)
	{
		if (InFunction(Binding) == EVisitResult::Break)
		{
			break;
		}
	}
}

void FArcTQSBindingCollection::VisitSourceStructDescriptorInternal(
	TFunctionRef<EVisitResult(const FPropertyBindingBindableStructDescriptor& Descriptor)> InFunction) const
{
	for (const FArcTQSBindableStructDescriptor& Descriptor : BindableStructs)
	{
		if (InFunction(Descriptor) == EVisitResult::Break)
		{
			break;
		}
	}
}

void FArcTQSBindingCollection::OnReset()
{
	BindableStructs.Reset();
	Bindings.Reset();
}

#if WITH_EDITOR
FPropertyBindingBinding* FArcTQSBindingCollection::AddBindingInternal(
	const FPropertyBindingPath& InSourcePath,
	const FPropertyBindingPath& InTargetPath)
{
	return &Bindings.Emplace_GetRef(InSourcePath, InTargetPath);
}

void FArcTQSBindingCollection::RemoveBindingsInternal(TFunctionRef<bool(FPropertyBindingBinding&)> InPredicate)
{
	Bindings.RemoveAllSwap(InPredicate);
}

bool FArcTQSBindingCollection::HasBindingInternal(TFunctionRef<bool(const FPropertyBindingBinding&)> InPredicate) const
{
	return Bindings.ContainsByPredicate(InPredicate);
}

const FPropertyBindingBinding* FArcTQSBindingCollection::FindBindingInternal(
	TFunctionRef<bool(const FPropertyBindingBinding&)> InPredicate) const
{
	return Bindings.FindByPredicate(InPredicate);
}
#endif

void FArcTQSCompiledData::Reset()
{
	Version = 0;
	Revision = 0;
	bIsValid = false;
	GeneratorBatch = FPropertyBindingIndex16();
	StepBatches.Reset();
	StepPerItemBatches.Reset();
	Functions.Reset();
	Bindings.Reset();
}

bool FArcTQSCompiledData::ResolvePaths(const UStruct* ParametersStruct)
{
	if (!bIsValid)
	{
		return false;
	}
	if (FArcTQSBindableStructDescriptor* ParametersDescriptor = Bindings.FindMutableBindableStruct(FArcTQSDataHandle(EArcTQSDataKind::Parameters)))
	{
		ParametersDescriptor->Struct = ParametersStruct;
	}
	for (const FArcTQSCompiledFunction& Function : Functions)
	{
		if (!Function.Function.GetPtr<FArcTQSPropertyFunction>())
		{
			bIsValid = false;
			return false;
		}
	}
	bIsValid = Bindings.ResolvePaths();
	return bIsValid;
}

bool FArcTQSCompiledData::IsExecutable() const
{
	return bIsValid
		&& Version == CurrentVersion
		&& Revision != 0
		&& Bindings.IsValid();
}

namespace ArcTQS
{
	bool IsBindingSourceAccessible(const EArcTQSDataKind Source, const EArcTQSDataKind Target)
	{
		const bool bQueryLevelSource = Source == EArcTQSDataKind::Parameters || Source == EArcTQSDataKind::Querier;
		switch (Target)
		{
		case EArcTQSDataKind::Generator:
			return bQueryLevelSource;
		case EArcTQSDataKind::Step:
			return bQueryLevelSource || Source == EArcTQSDataKind::Item;
		default:
			return false;
		}
	}

#if WITH_EDITOR
	bool IsLiteralOnlyProperty(const FProperty& Property)
	{
		return Property.HasMetaData(TEXT("ArcMemoryAccess"));
	}

	bool IsBindableProperty(const FProperty& Property)
	{
		return Property.HasAnyPropertyFlags(CPF_Edit) && !IsLiteralOnlyProperty(Property);
	}

	bool IsRequiredBinding(const FProperty& Property)
	{
		return Property.HasMetaData(TEXT("ArcTQSRequiredBinding"));
	}

	void ForEachMemoryKey(
		const UStruct& Struct,
		const void* Memory,
		const TFunctionRef<void(const FProperty& Property, FGameplayTag Key)> Function)
	{
		if (!Memory)
		{
			return;
		}
		for (TFieldIterator<FProperty> PropertyIt(&Struct); PropertyIt; ++PropertyIt)
		{
			const FProperty* Property = *PropertyIt;
			for (int32 ArrayIndex = 0; ArrayIndex < Property->ArrayDim; ++ArrayIndex)
			{
				const void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Memory, ArrayIndex);
				if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
				{
					if (StructProperty->Struct != FGameplayTag::StaticStruct())
					{
						ForEachMemoryKey(*StructProperty->Struct, ValuePtr, Function);
					}
					else if (IsLiteralOnlyProperty(*StructProperty))
					{
						Function(*StructProperty, *static_cast<const FGameplayTag*>(ValuePtr));
					}
				}
				else if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
				{
					const FStructProperty* InnerProperty = CastField<FStructProperty>(ArrayProperty->Inner);
					if (!InnerProperty || InnerProperty->Struct == FGameplayTag::StaticStruct())
					{
						continue;
					}
					FScriptArrayHelper ArrayHelper(ArrayProperty, ValuePtr);
					for (int32 ElementIndex = 0; ElementIndex < ArrayHelper.Num(); ++ElementIndex)
					{
						ForEachMemoryKey(*InnerProperty->Struct, ArrayHelper.GetRawPtr(ElementIndex), Function);
					}
				}
			}
		}
	}
#endif
}
