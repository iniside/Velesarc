// Copyright Lukasz Baran. All Rights Reserved.

#include "Actions/ArcTacticsAction.h"

#include "Decision/ArcTacticsCompiledData.h"
#include "Decision/ArcTacticsInstance.h"
#include "Mass/ArcTacticsOption.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcTacticsAction)

namespace ArcTacticsActionPrivate
{
	uint32 AdvanceNonZero(uint32& Value)
	{
		++Value;
		if (Value == 0)
		{
			++Value;
		}
		return Value;
	}

	TConstArrayView<FArcTacticsNodeHandle> GetBranchSteps(
		const FArcTacticsCompiledData& CompiledData,
		const FArcTacticsOption& Option,
		const bool bActiveSteps)
	{
		if (Option.bFromSelector)
		{
			if (!CompiledData.Selectors.IsValidIndex(Option.SourceIndex)
				|| CompiledData.Selectors[Option.SourceIndex].Action != Option.Action)
			{
				return TConstArrayView<FArcTacticsNodeHandle>();
			}
			const FArcTacticsCompiledSelector& Selector = CompiledData.Selectors[Option.SourceIndex];
			return bActiveSteps ? Selector.ActiveSteps : Selector.Steps;
		}
		if (!CompiledData.Actions.IsValidIndex(Option.SourceIndex)
			|| CompiledData.Actions[Option.SourceIndex].Action != Option.Action)
		{
			return TConstArrayView<FArcTacticsNodeHandle>();
		}
		const FArcTacticsCompiledAction& Action = CompiledData.Actions[Option.SourceIndex];
		return bActiveSteps ? Action.ActiveSteps : Action.Steps;
	}

	FName GetNodeName(const FArcTacticsCompiledData* CompiledData, const FArcTacticsNodeHandle Handle)
	{
		const int32 NodeIndex = Handle.Get();
		return CompiledData && CompiledData->Nodes.IsValidIndex(NodeIndex)
			? CompiledData->Nodes[NodeIndex].Name
			: NAME_None;
	}

	bool ApplyNodeBindings(
		const FArcTacticsCompiledData* CompiledData,
		FArcTacticsOption* Option,
		FArcTacticsInstanceData& BindingInstance,
		const FArcTQSFunctionContext& FunctionContext,
		const FArcTacticsNodeHandle Handle,
		const FStructView NodeData,
		const bool bOutputs)
	{
		const int32 NodeIndex = Handle.Get();
		return CompiledData && CompiledData->Nodes.IsValidIndex(NodeIndex)
			&& CompiledData->DefaultInstanceData.IsValidIndex(NodeIndex)
			&& NodeData.IsValid()
			&& NodeData.GetScriptStruct() == CompiledData->DefaultInstanceData[NodeIndex].GetScriptStruct()
			&& BindingInstance.ApplyBindings(
				*CompiledData,
				bOutputs ? CompiledData->Nodes[NodeIndex].OutputBindingsBatch : CompiledData->Nodes[NodeIndex].BindingsBatch,
				FPropertyBindingDataView(NodeData),
				FunctionContext,
				Option);
	}
}

FArcTacticsStepAccess::FArcTacticsStepAccess(
	const FArcTacticsCompiledData& InCompiledData,
	FArcTacticsOption& InOption,
	FArcTacticsInstanceData* InBindingInstance,
	const FArcTQSFunctionContext& InFunctionContext)
	: CompiledData(&InCompiledData)
	, Option(&InOption)
	, BindingInstance(InBindingInstance)
	, FunctionContext(InFunctionContext)
	, Handles(ArcTacticsActionPrivate::GetBranchSteps(InCompiledData, InOption, false))
{
}

int32 FArcTacticsStepAccess::Num() const
{
	return Handles.Num();
}

FArcTacticsNodeHandle FArcTacticsStepAccess::GetHandle(const int32 StepIndex) const
{
	return Handles.IsValidIndex(StepIndex) ? Handles[StepIndex] : FArcTacticsNodeHandle();
}

const FArcTacticsStepNode* FArcTacticsStepAccess::GetNode(const int32 StepIndex) const
{
	const int32 NodeIndex = GetHandle(StepIndex).Get();
	return CompiledData && CompiledData->Nodes.IsValidIndex(NodeIndex)
		? CompiledData->Nodes[NodeIndex].Node.GetPtr<FArcTacticsStepNode>()
		: nullptr;
}

FName FArcTacticsStepAccess::GetName(const int32 StepIndex) const
{
	return ArcTacticsActionPrivate::GetNodeName(CompiledData, GetHandle(StepIndex));
}

FStructView FArcTacticsStepAccess::GetData(const int32 StepIndex) const
{
	return Option && Handles.IsValidIndex(StepIndex) && Option->StepData.IsValidIndex(StepIndex)
		? Option->StepData[StepIndex]
		: FStructView();
}

bool FArcTacticsStepAccess::ApplyStepInputs(const int32 StepIndex) const
{
	return ApplyStepBindings(StepIndex, false);
}

bool FArcTacticsStepAccess::PublishStepOutputs(const int32 StepIndex) const
{
	return ApplyStepBindings(StepIndex, true);
}

bool FArcTacticsStepAccess::ApplyStepBindings(const int32 StepIndex, const bool bOutputs) const
{
	if (!BindingInstance)
	{
		return false;
	}
	const bool bApplied = ArcTacticsActionPrivate::ApplyNodeBindings(
		CompiledData, Option, *BindingInstance, FunctionContext, GetHandle(StepIndex), GetData(StepIndex), bOutputs);
	if (!bApplied)
	{
		BindingInstance->Diagnostic = bOutputs ? TEXT("StepOutputBindingFailed") : TEXT("StepBindingFailed");
	}
	return bApplied;
}

FArcTacticsActiveStepAccess::FArcTacticsActiveStepAccess(
	const FArcTacticsCompiledData& InCompiledData,
	FArcTacticsOption& InOption,
	FArcTacticsInstanceData* InBindingInstance,
	const FArcTQSFunctionContext& InFunctionContext)
	: CompiledData(&InCompiledData)
	, Option(&InOption)
	, BindingInstance(InBindingInstance)
	, FunctionContext(InFunctionContext)
	, Handles(ArcTacticsActionPrivate::GetBranchSteps(InCompiledData, InOption, true))
{
}

int32 FArcTacticsActiveStepAccess::Num() const
{
	return Handles.Num();
}

FArcTacticsNodeHandle FArcTacticsActiveStepAccess::GetHandle(const int32 ActiveStepIndex) const
{
	return Handles.IsValidIndex(ActiveStepIndex) ? Handles[ActiveStepIndex] : FArcTacticsNodeHandle();
}

const FArcTacticsActiveStepNode* FArcTacticsActiveStepAccess::GetNode(const int32 ActiveStepIndex) const
{
	const int32 NodeIndex = GetHandle(ActiveStepIndex).Get();
	return CompiledData && CompiledData->Nodes.IsValidIndex(NodeIndex)
		? CompiledData->Nodes[NodeIndex].Node.GetPtr<FArcTacticsActiveStepNode>()
		: nullptr;
}

FName FArcTacticsActiveStepAccess::GetName(const int32 ActiveStepIndex) const
{
	return ArcTacticsActionPrivate::GetNodeName(CompiledData, GetHandle(ActiveStepIndex));
}

FStructView FArcTacticsActiveStepAccess::GetData(const int32 ActiveStepIndex) const
{
	return Option && Handles.IsValidIndex(ActiveStepIndex) && Option->ActiveStepData.IsValidIndex(ActiveStepIndex)
		? Option->ActiveStepData[ActiveStepIndex]
		: FStructView();
}

bool FArcTacticsActiveStepAccess::ApplyInputs(const int32 ActiveStepIndex) const
{
	if (!BindingInstance)
	{
		return false;
	}
	const bool bApplied = ArcTacticsActionPrivate::ApplyNodeBindings(
		CompiledData, Option, *BindingInstance, FunctionContext, GetHandle(ActiveStepIndex), GetData(ActiveStepIndex), false);
	if (!bApplied)
	{
		BindingInstance->Diagnostic = TEXT("ActiveStepBindingFailed");
	}
	return bApplied;
}

FArcTacticsActionView::FArcTacticsActionView(
	const FArcTacticsCompiledData& CompiledData,
	FArcTacticsOption& Option)
	: ActionData(Option.ActionData.GetScriptStruct(), Option.ActionData.GetMutableMemory())
	, Steps(CompiledData, Option, nullptr, FArcTQSFunctionContext())
	, ActiveSteps(CompiledData, Option, nullptr, FArcTQSFunctionContext())
{
}

FArcTacticsStepContext::FArcTacticsStepContext(
	FArcTacticsActionContext& InAction,
	uint32& InRequestGeneration,
	uint32& InRequestDecisionGeneration)
	: Action(InAction)
	, RequestGeneration(InRequestGeneration)
	, RequestDecisionGeneration(InRequestDecisionGeneration)
{
}

FArcTacticsAsyncRequestToken FArcTacticsStepContext::BeginRequest()
{
	ArcTacticsActionPrivate::AdvanceNonZero(RequestGeneration);
	RequestDecisionGeneration = Action.RequestToken.DecisionGeneration;
	FArcTacticsAsyncRequestToken Token = Action.RequestToken;
	Token.RequestGeneration = RequestGeneration;
	return Token;
}

void FArcTacticsStepContext::InvalidateRequests()
{
	ArcTacticsActionPrivate::AdvanceNonZero(RequestGeneration);
}

bool FArcTacticsStepContext::IsCurrentRequest(const FArcTacticsAsyncRequestToken& Token) const
{
	return Token.InstanceSubsystem == Action.RequestToken.InstanceSubsystem
		&& Token.Entity == Action.Entity
		&& Token.InstanceHandle == Action.RequestToken.InstanceHandle
		&& Token.DecisionGeneration == RequestDecisionGeneration
		&& Token.ActionSerial == Action.RequestToken.ActionSerial
		&& Token.RequestGeneration == RequestGeneration;
}
