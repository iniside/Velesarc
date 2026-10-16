// Copyright Lukasz Baran. All Rights Reserved.

#include "Decision/ArcTacticsNode.h"

#include "Actions/ArcTacticsAction.h"
#include "ArcTQSBindings.h"
#include "ArcTQSPropertyFunction.h"
#include "ArcTQSQueryReference.h"
#include "UObject/UnrealType.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcTacticsNode)

#if WITH_EDITOR
namespace ArcTacticsNodePrivate
{
	bool IsFunctionProperty(const FProperty& Property)
	{
		const UStruct* Owner = Property.GetOwnerStruct();
		return Owner && Owner->IsChildOf(FArcTQSPropertyFunction::StaticStruct());
	}
}

EArcTacticsPropertyRole ArcTacticsNode::ResolvePropertyRole(const FProperty& Property)
{
	if (ArcTacticsNodePrivate::IsFunctionProperty(Property))
	{
		if (ArcTQS::IsFunctionOutputProperty(Property))
		{
			return EArcTacticsPropertyRole::Output;
		}
		return Property.HasAnyPropertyFlags(CPF_Edit) && !Property.HasAnyPropertyFlags(CPF_EditConst)
			? EArcTacticsPropertyRole::Input
			: EArcTacticsPropertyRole::Invalid;
	}
	const bool bInput = Property.HasMetaData(TEXT("ArcTacticsInput"));
	const bool bOutput = Property.HasMetaData(TEXT("ArcTacticsOutput"));
	const bool bParameter = Property.HasMetaData(TEXT("ArcTacticsParameter"));
	const int32 RoleCount = static_cast<int32>(bInput)
		+ static_cast<int32>(bOutput)
		+ static_cast<int32>(bParameter);
	if (RoleCount != 1)
	{
		return EArcTacticsPropertyRole::Invalid;
	}
	if (bInput)
	{
		return EArcTacticsPropertyRole::Input;
	}
	if (bOutput)
	{
		return EArcTacticsPropertyRole::Output;
	}
	return EArcTacticsPropertyRole::Parameter;
}

bool ArcTacticsNode::IsRequiredInput(const FProperty& Property)
{
	if (ResolvePropertyRole(Property) != EArcTacticsPropertyRole::Input)
	{
		return false;
	}
	return ArcTacticsNodePrivate::IsFunctionProperty(Property)
		? ArcTQS::IsRequiredBinding(Property)
		: !Property.HasMetaData(TEXT("ArcTacticsOptional"));
}

void ArcTacticsNode::ForEachQueryReference(
	const UStruct& Struct,
	void* Memory,
	const TFunctionRef<void(FArcTQSQueryReference& Reference)> Function)
{
	if (!Memory)
	{
		return;
	}
	for (TFieldIterator<FProperty> PropertyIt(&Struct); PropertyIt; ++PropertyIt)
	{
		const FProperty* Property = *PropertyIt;
		if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
		{
			for (int32 Index = 0; Index < Property->ArrayDim; ++Index)
			{
				void* Value = StructProperty->ContainerPtrToValuePtr<void>(Memory, Index);
				if (StructProperty->Struct == FArcTQSQueryReference::StaticStruct())
				{
					Function(*static_cast<FArcTQSQueryReference*>(Value));
				}
				else
				{
					ForEachQueryReference(*StructProperty->Struct, Value, Function);
				}
			}
		}
		else if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
		{
			const FStructProperty* InnerStruct = CastField<FStructProperty>(ArrayProperty->Inner);
			if (!InnerStruct)
			{
				continue;
			}
			FScriptArrayHelper Helper(ArrayProperty, ArrayProperty->ContainerPtrToValuePtr<void>(Memory));
			for (int32 Index = 0; Index < Helper.Num(); ++Index)
			{
				void* Element = Helper.GetRawPtr(Index);
				if (InnerStruct->Struct == FArcTQSQueryReference::StaticStruct())
				{
					Function(*static_cast<FArcTQSQueryReference*>(Element));
				}
				else
				{
					ForEachQueryReference(*InnerStruct->Struct, Element, Function);
				}
			}
		}
	}
}
#endif

const UScriptStruct* FArcTacticsNode::GetInstanceDataType() const
{
	return FArcTacticsEmptyInstanceData::StaticStruct();
}

FText FArcTacticsNode::GetDisplayName() const
{
	return NSLOCTEXT("ArcTactics", "TacticsNode", "Tactics Node");
}

EArcTacticsNodeRole FArcTacticsNode::GetRole() const
{
	return EArcTacticsNodeRole::Invalid;
}

#if WITH_EDITOR
FName FArcTacticsNode::GetIconName() const
{
	return NAME_None;
}

FColor FArcTacticsNode::GetIconColor() const
{
	return FColor::White;
}

FText FArcTacticsNode::GetDescription(
	const FGuid&,
	FConstStructView,
	const IPropertyBindingBindingCollectionOwner&,
	const bool) const
{
	return FText::GetEmpty();
}
#endif

EArcTacticsNodeRole FArcTacticsActionNode::GetRole() const
{
	return EArcTacticsNodeRole::Action;
}

EArcTacticsActionStatus FArcTacticsActionNode::Enter(FArcTacticsActionContext&, FStructView) const
{
	return EArcTacticsActionStatus::Failed;
}

EArcTacticsActionStatus FArcTacticsActionNode::Tick(FArcTacticsActionContext&, FStructView) const
{
	return EArcTacticsActionStatus::Failed;
}

void FArcTacticsActionNode::Cancel(
	FArcTacticsActionContext&,
	FStructView,
	EArcTacticsActionCancelMode) const
{
}

void FArcTacticsActionNode::OnAsyncQueryCompleted(
	FArcTacticsActionContext&,
	FStructView,
	const FArcTacticsAsyncRequestToken&,
	FArcTQSQueryInstance&) const
{
}

EArcTacticsActionTickLane FArcTacticsActionNode::GetTickLane(const FArcTacticsActionView&) const
{
	return EArcTacticsActionTickLane::None;
}

EArcTacticsExecutionRequirement FArcTacticsActionNode::GetRequirements(FConstStructView) const
{
	return EArcTacticsExecutionRequirement::None;
}

bool FArcTacticsActionNode::BlocksDecision(const FArcTacticsActionView&) const
{
	return false;
}

double FArcTacticsActionNode::GetNextWakeTime(const FArcTacticsActionView&) const
{
	return 0.0;
}

int32 FArcTacticsActionNode::GetCurrentStepIndex(const FArcTacticsActionView&) const
{
	return INDEX_NONE;
}

EArcTacticsNodeRole FArcTacticsStepNode::GetRole() const
{
	return EArcTacticsNodeRole::Step;
}

bool FArcTacticsStepNode::Enter(FArcTacticsStepContext&, FStructView) const
{
	return false;
}

EArcTacticsActionStatus FArcTacticsStepNode::Tick(FArcTacticsStepContext&, FStructView) const
{
	return EArcTacticsActionStatus::Failed;
}

void FArcTacticsStepNode::Cancel(
	FArcTacticsStepContext&,
	FStructView,
	EArcTacticsActionCancelMode) const
{
}

void FArcTacticsStepNode::OnAsyncQueryCompleted(
	FArcTacticsStepContext&,
	FStructView,
	const FArcTacticsAsyncRequestToken&,
	FArcTQSQueryInstance&) const
{
}

EArcTacticsActionTickLane FArcTacticsStepNode::GetTickLane(FConstStructView) const
{
	return EArcTacticsActionTickLane::None;
}

EArcTacticsExecutionRequirement FArcTacticsStepNode::GetRequirements(FConstStructView) const
{
	return EArcTacticsExecutionRequirement::None;
}

bool FArcTacticsStepNode::BlocksDecision(FConstStructView) const
{
	return false;
}

double FArcTacticsStepNode::GetNextWakeTime(FConstStructView) const
{
	return 0.0;
}

EArcTacticsNodeRole FArcTacticsActiveStepNode::GetRole() const
{
	return EArcTacticsNodeRole::ActiveStep;
}

bool FArcTacticsActiveStepNode::Enter(FArcTacticsActionContext&, FStructView) const
{
	return false;
}

void FArcTacticsActiveStepNode::Exit(FArcTacticsActionContext&, FStructView) const
{
}

EArcTacticsExecutionRequirement FArcTacticsActiveStepNode::GetRequirements(FConstStructView) const
{
	return EArcTacticsExecutionRequirement::None;
}
