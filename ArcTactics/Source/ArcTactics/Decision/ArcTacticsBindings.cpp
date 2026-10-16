// Copyright Lukasz Baran. All Rights Reserved.

#include "Decision/ArcTacticsBindings.h"

#include "ArcTQSBindings.h"
#include "Decision/ArcTacticsNode.h"
#include "PropertyBindingDataView.h"
#include "UObject/UnrealType.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcTacticsBindings)

bool ArcTacticsBindings::UpdatePathSegments(FPropertyBindingPath& Path, const FPropertyBindingDataView& DataView)
{
	if (Path.IsPathEmpty() || !DataView.IsValid() || Path.UpdateSegmentsFromValue(DataView))
	{
		return true;
	}
	return !Path.GetSegments().ContainsByPredicate([](const FPropertyBindingPathSegment& Segment) -> bool
	{
		return Segment.GetInstanceStruct() != nullptr;
	});
}

FArcTacticsBindableStructDescriptor& FArcTacticsBindingCollection::AddBindableStruct(
	const FArcTacticsBindableStructDescriptor& Descriptor)
{
	return BindableStructs.Add_GetRef(Descriptor);
}

FArcTacticsBinding& FArcTacticsBindingCollection::AddCompiledBinding(FArcTacticsBinding&& Binding)
{
	return Bindings.Add_GetRef(MoveTemp(Binding));
}

const FArcTacticsBindableStructDescriptor* FArcTacticsBindingCollection::FindBindableStruct(
	const FArcTacticsDataHandle Handle) const
{
	return BindableStructs.FindByPredicate([Handle](const FArcTacticsBindableStructDescriptor& Descriptor)
	{
		return Descriptor.DataHandle == Handle;
	});
}

FArcTacticsBindableStructDescriptor* FArcTacticsBindingCollection::FindMutableBindableStruct(
	const FArcTacticsDataHandle Handle)
{
	return BindableStructs.FindByPredicate([Handle](const FArcTacticsBindableStructDescriptor& Descriptor)
	{
		return Descriptor.DataHandle == Handle;
	});
}

int32 FArcTacticsBindingCollection::GetNumBindableStructDescriptors() const
{
	return BindableStructs.Num();
}

const FPropertyBindingBindableStructDescriptor* FArcTacticsBindingCollection::GetBindableStructDescriptorFromHandle(
	const FConstStructView InSourceHandleView) const
{
	return FindBindableStruct(InSourceHandleView.Get<const FArcTacticsDataHandle>());
}

int32 FArcTacticsBindingCollection::GetNumBindings() const
{
	return Bindings.Num();
}

void FArcTacticsBindingCollection::ForEachBinding(
	TFunctionRef<void(const FPropertyBindingBinding& Binding)> InFunction) const
{
	for (const FArcTacticsBinding& Binding : Bindings)
	{
		InFunction(Binding);
	}
}

void FArcTacticsBindingCollection::ForEachBinding(
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

void FArcTacticsBindingCollection::ForEachMutableBinding(
	TFunctionRef<void(FPropertyBindingBinding& Binding)> InFunction)
{
	for (FArcTacticsBinding& Binding : Bindings)
	{
		InFunction(Binding);
	}
}

void FArcTacticsBindingCollection::VisitBindings(
	TFunctionRef<EVisitResult(const FPropertyBindingBinding& Binding)> InFunction) const
{
	for (const FArcTacticsBinding& Binding : Bindings)
	{
		if (InFunction(Binding) == EVisitResult::Break)
		{
			break;
		}
	}
}

void FArcTacticsBindingCollection::VisitBindings(
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

void FArcTacticsBindingCollection::VisitMutableBindings(
	TFunctionRef<EVisitResult(FPropertyBindingBinding& Binding)> InFunction)
{
	for (FArcTacticsBinding& Binding : Bindings)
	{
		if (InFunction(Binding) == EVisitResult::Break)
		{
			break;
		}
	}
}

#if WITH_EDITOR
EArcTacticsBindingValidationResult FArcTacticsBindingCollection::ValidateEditorBinding(
	const FArcTacticsBinding& Binding,
	const UStruct* SourceStruct,
	const FArcTacticsBindingEndpoint& SourceEndpoint,
	const UStruct* TargetStruct,
	const FArcTacticsBindingEndpoint& TargetEndpoint,
	const bool bSourceAccessible,
	FString* OutPathError)
{
	if (!SourceStruct || !TargetStruct || !SourceEndpoint.IsValid() || !TargetEndpoint.IsValid())
	{
		return EArcTacticsBindingValidationResult::InvalidTarget;
	}
	if (!bSourceAccessible)
	{
		return EArcTacticsBindingValidationResult::InvalidScope;
	}

	TArray<FPropertyBindingPathIndirection> SourceIndirections;
	TArray<FPropertyBindingPathIndirection> TargetIndirections;
	FString PathError;
	if (!Binding.GetSourcePath().ResolveIndirections(SourceStruct, SourceIndirections, &PathError, true))
	{
		if (OutPathError)
		{
			*OutPathError = MoveTemp(PathError);
		}
		return EArcTacticsBindingValidationResult::InvalidSourcePath;
	}
	if (!Binding.GetTargetPath().ResolveIndirections(TargetStruct, TargetIndirections, &PathError, true))
	{
		if (OutPathError)
		{
			*OutPathError = MoveTemp(PathError);
		}
		return EArcTacticsBindingValidationResult::InvalidTargetPath;
	}

	const FProperty* SourceRootProperty = SourceIndirections.IsEmpty()
		? nullptr
		: SourceIndirections[0].GetProperty();
	const FProperty* TargetRootProperty = TargetIndirections.IsEmpty()
		? nullptr
		: TargetIndirections[0].GetProperty();
	if (Binding.IsOutputBinding())
	{
		if ((SourceEndpoint.Kind != EArcTacticsBindingEndpointKind::GlobalParameters
				&& SourceEndpoint.Kind != EArcTacticsBindingEndpointKind::BranchParameterBag)
			|| TargetEndpoint.Kind != EArcTacticsBindingEndpointKind::Node)
		{
			return EArcTacticsBindingValidationResult::InvalidOutputTarget;
		}
		if (!SourceRootProperty || !TargetRootProperty)
		{
			return !SourceRootProperty
				? EArcTacticsBindingValidationResult::InvalidSourcePath
				: EArcTacticsBindingValidationResult::InvalidTargetPath;
		}
		const EArcTacticsPropertyRole TargetRole = ArcTacticsNode::ResolvePropertyRole(*TargetRootProperty);
		if (TargetRole == EArcTacticsPropertyRole::Invalid)
		{
			return EArcTacticsBindingValidationResult::InvalidRole;
		}
		if (TargetRole != EArcTacticsPropertyRole::Output)
		{
			return EArcTacticsBindingValidationResult::InvalidOutputTarget;
		}
	}
	else
	{
		const bool bSourcePropertyBag = SourceEndpoint.Kind == EArcTacticsBindingEndpointKind::GlobalParameters
			|| SourceEndpoint.Kind == EArcTacticsBindingEndpointKind::BranchParameterBag;
		if (!bSourcePropertyBag)
		{
			if (!SourceRootProperty)
			{
				return EArcTacticsBindingValidationResult::InvalidSourcePath;
			}
			const EArcTacticsPropertyRole SourceRole = ArcTacticsNode::ResolvePropertyRole(*SourceRootProperty);
			if (SourceRole == EArcTacticsPropertyRole::Invalid)
			{
				return EArcTacticsBindingValidationResult::InvalidRole;
			}
			if ((SourceEndpoint.Kind == EArcTacticsBindingEndpointKind::Node
					|| SourceEndpoint.Kind == EArcTacticsBindingEndpointKind::Context
					|| SourceEndpoint.Kind == EArcTacticsBindingEndpointKind::Function)
				&& SourceRole != EArcTacticsPropertyRole::Output)
			{
				return EArcTacticsBindingValidationResult::SourceNotOutput;
			}
		}

		const bool bParameterBagTarget = TargetEndpoint.Kind == EArcTacticsBindingEndpointKind::GlobalParameters
			|| TargetEndpoint.Kind == EArcTacticsBindingEndpointKind::BranchParameterBag;
		const bool bValidForwardTarget = bParameterBagTarget
			|| TargetEndpoint.Kind == EArcTacticsBindingEndpointKind::Node
			|| TargetEndpoint.Kind == EArcTacticsBindingEndpointKind::DecisionSettings
			|| TargetEndpoint.Kind == EArcTacticsBindingEndpointKind::BranchParameters
			|| TargetEndpoint.Kind == EArcTacticsBindingEndpointKind::Function;
		if (!bValidForwardTarget)
		{
			return EArcTacticsBindingValidationResult::InvalidTarget;
		}
		const bool bLiteralOnlyTarget = TargetIndirections.ContainsByPredicate(
			[](const FPropertyBindingPathIndirection& Indirection)
		{
			const FProperty* Property = Indirection.GetProperty();
			return Property && ArcTQS::IsLiteralOnlyProperty(*Property);
		});
		if (bLiteralOnlyTarget)
		{
			return EArcTacticsBindingValidationResult::LiteralOnlyTarget;
		}
		if (!bParameterBagTarget)
		{
			if (!TargetRootProperty)
			{
				return EArcTacticsBindingValidationResult::InvalidTargetPath;
			}
			const EArcTacticsPropertyRole TargetRole = ArcTacticsNode::ResolvePropertyRole(*TargetRootProperty);
			if (TargetRole == EArcTacticsPropertyRole::Invalid)
			{
				return EArcTacticsBindingValidationResult::InvalidRole;
			}
			if (TargetRole != EArcTacticsPropertyRole::Input
				&& TargetRole != EArcTacticsPropertyRole::Parameter)
			{
				return EArcTacticsBindingValidationResult::TargetNotInput;
			}
		}
	}

	const FPropertyBindingPathIndirection SourceLeaf = SourceIndirections.IsEmpty()
		? FPropertyBindingPathIndirection(SourceStruct)
		: SourceIndirections.Last();
	const FPropertyBindingPathIndirection TargetLeaf = TargetIndirections.IsEmpty()
		? FPropertyBindingPathIndirection(TargetStruct)
		: TargetIndirections.Last();
	FPropertyBindingCopyInfo CopyInfo;
	return ResolveBindingCopyInfo(Binding, SourceLeaf, TargetLeaf, CopyInfo)
		? EArcTacticsBindingValidationResult::Valid
		: EArcTacticsBindingValidationResult::IncompatibleTypes;
}
#endif

bool FArcTacticsBindingCollection::ResolveBindingCopyInfo(
	const FPropertyBindingBinding& InResolvedBinding,
	const FPropertyBindingPathIndirection& InBindingSourceLeafIndirection,
	const FPropertyBindingPathIndirection& InBindingTargetLeafIndirection,
	FPropertyBindingCopyInfo& OutCopyInfo)
{
	OutCopyInfo.bCopyFromTargetToSource =
		static_cast<const FArcTacticsBinding&>(InResolvedBinding).IsOutputBinding();
	return Super::ResolveBindingCopyInfo(
		InResolvedBinding,
		InBindingSourceLeafIndirection,
		InBindingTargetLeafIndirection,
		OutCopyInfo);
}

void FArcTacticsBindingCollection::VisitSourceStructDescriptorInternal(
	TFunctionRef<EVisitResult(const FPropertyBindingBindableStructDescriptor& Descriptor)> InFunction) const
{
	for (const FArcTacticsBindableStructDescriptor& Descriptor : BindableStructs)
	{
		if (InFunction(Descriptor) == EVisitResult::Break)
		{
			break;
		}
	}
}

void FArcTacticsBindingCollection::OnReset()
{
	BindableStructs.Reset();
	Bindings.Reset();
}

#if WITH_EDITOR
FPropertyBindingBinding* FArcTacticsBindingCollection::AddBindingInternal(
	const FPropertyBindingPath& InSourcePath,
	const FPropertyBindingPath& InTargetPath)
{
	return &Bindings.Emplace_GetRef(InSourcePath, InTargetPath);
}

void FArcTacticsBindingCollection::RemoveBindingsInternal(
	TFunctionRef<bool(FPropertyBindingBinding&)> InPredicate)
{
	Bindings.RemoveAllSwap(InPredicate);
}

bool FArcTacticsBindingCollection::HasBindingInternal(
	TFunctionRef<bool(const FPropertyBindingBinding&)> InPredicate) const
{
	return Bindings.ContainsByPredicate(InPredicate);
}

const FPropertyBindingBinding* FArcTacticsBindingCollection::FindBindingInternal(
	TFunctionRef<bool(const FPropertyBindingBinding&)> InPredicate) const
{
	return Bindings.FindByPredicate(InPredicate);
}
#endif
