// Copyright Lukasz Baran. All Rights Reserved.

#include "Decision/ArcTacticsInstance.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcTacticsInstance)

bool FArcTacticsInstanceData::Initialize(
	const FArcTacticsCompiledData& CompiledData,
	FArcTacticsContextData* InSharedContext)
{
	Reset();
	if (!CompiledData.IsExecutable()
		|| CompiledData.DefaultInstanceData.Num() != CompiledData.Nodes.Num())
	{
		return false;
	}

	CompiledRevision = CompiledData.Revision;
	SharedContext = InSharedContext;
	DecisionSettings = CompiledData.DefaultDecisionSettings;
	GlobalParameters = CompiledData.DefaultGlobalParameters;
	NodeInstanceData = CompiledData.DefaultInstanceData;
	TArray<FConstStructView> FunctionDefaults;
	FunctionDefaults.Reserve(CompiledData.Functions.Num());
	for (const FArcTacticsCompiledFunction& Function : CompiledData.Functions)
	{
		FunctionDefaults.Add(FConstStructView(Function.Function));
	}
	FunctionInstanceData = FunctionDefaults;
	BranchParameters.Reserve(CompiledData.Actions.Num() + CompiledData.Selectors.Num());
	BranchParameterBags.Reserve(CompiledData.Actions.Num() + CompiledData.Selectors.Num());
	for (const FArcTacticsCompiledAction& Action : CompiledData.Actions)
	{
		BranchParameters.Add(Action.DefaultParameters);
		BranchParameterBags.Add(Action.DefaultParameterBag);
	}
	for (const FArcTacticsCompiledSelector& Selector : CompiledData.Selectors)
	{
		BranchParameters.Add(Selector.DefaultParameters);
		BranchParameterBags.Add(Selector.DefaultParameterBag);
	}
	return true;
}

void FArcTacticsInstanceData::Reset()
{
	CompiledRevision = 0;
	GlobalTaskIndex = INDEX_NONE;
	bGlobalTaskRunning = false;
	DecisionSettings = FArcTacticsDecisionSettingsParameters();
	GlobalParameters.Reset();
	BranchParameters.Reset();
	BranchParameterBags.Reset();
	NodeInstanceData.Reset();
	FunctionInstanceData.Reset();
	Diagnostic = NAME_None;
	SharedContext = nullptr;
}

bool FArcTacticsInstanceData::IsCompatible(const FArcTacticsCompiledData& CompiledData) const
{
	if (!CompiledData.IsExecutable()
		|| CompiledRevision != CompiledData.Revision
		|| NodeInstanceData.Num() != CompiledData.Nodes.Num()
		|| BranchParameters.Num() != CompiledData.Actions.Num() + CompiledData.Selectors.Num()
		|| BranchParameterBags.Num() != BranchParameters.Num()
		|| FunctionInstanceData.Num() != CompiledData.Functions.Num())
	{
		return false;
	}
	for (int32 FunctionIndex = 0; FunctionIndex < CompiledData.Functions.Num(); ++FunctionIndex)
	{
		if (FunctionInstanceData[FunctionIndex].GetScriptStruct()
			!= CompiledData.Functions[FunctionIndex].Function.GetScriptStruct())
		{
			return false;
		}
	}
	return true;
}

int32 FArcTacticsInstanceData::GetBranchIndex(
	const FArcTacticsCompiledData& CompiledData,
	const FArcTacticsOption& Option)
{
	return Option.bFromSelector
		? CompiledData.Actions.Num() + Option.SourceIndex
		: Option.SourceIndex;
}

FPropertyBindingDataView FArcTacticsInstanceData::GetMutableDataView(const FArcTacticsDataHandle Handle)
{
	if (Handle.GetKind() == EArcTacticsBindingEndpointKind::Context)
	{
		return SharedContext
			? FPropertyBindingDataView(FArcTacticsContextData::StaticStruct(), SharedContext)
			: FPropertyBindingDataView();
	}
	if (Handle.GetKind() == EArcTacticsBindingEndpointKind::DecisionSettings)
	{
		return FPropertyBindingDataView(FArcTacticsDecisionSettingsParameters::StaticStruct(), &DecisionSettings);
	}
	if (Handle.GetKind() == EArcTacticsBindingEndpointKind::GlobalParameters)
	{
		return FPropertyBindingDataView(GlobalParameters.GetMutableValue());
	}
	if (Handle.GetKind() == EArcTacticsBindingEndpointKind::BranchParameters
		&& BranchParameters.IsValidIndex(Handle.GetIndex()))
	{
		return FPropertyBindingDataView(
			FArcTacticsBranchParameters::StaticStruct(), &BranchParameters[Handle.GetIndex()]);
	}
	if (Handle.GetKind() == EArcTacticsBindingEndpointKind::BranchParameterBag
		&& BranchParameterBags.IsValidIndex(Handle.GetIndex()))
	{
		return FPropertyBindingDataView(BranchParameterBags[Handle.GetIndex()].GetMutableValue());
	}
	if (Handle.GetKind() == EArcTacticsBindingEndpointKind::Node && NodeInstanceData.IsValidIndex(Handle.GetIndex()))
	{
		return FPropertyBindingDataView(NodeInstanceData[Handle.GetIndex()]);
	}
	if (Handle.GetKind() == EArcTacticsBindingEndpointKind::Function
		&& FunctionInstanceData.IsValidIndex(Handle.GetIndex()))
	{
		return FPropertyBindingDataView(FunctionInstanceData[Handle.GetIndex()]);
	}
	return FPropertyBindingDataView();
}

FPropertyBindingDataView FArcTacticsInstanceData::GetDataView(const FArcTacticsDataHandle Handle) const
{
	return const_cast<FArcTacticsInstanceData*>(this)->GetMutableDataView(Handle);
}

FPropertyBindingDataView FArcTacticsInstanceData::GetScopedDataView(
	const FArcTacticsCompiledData& CompiledData,
	const FArcTacticsDataHandle Handle,
	const FArcTacticsOption& Scope) const
{
	if (Handle.GetKind() == EArcTacticsBindingEndpointKind::GlobalParameters)
	{
		return FPropertyBindingDataView(const_cast<FInstancedPropertyBag&>(Scope.GlobalParameters).GetMutableValue());
	}
	const int32 BranchIndex = GetBranchIndex(CompiledData, Scope);
	if (Handle.GetKind() == EArcTacticsBindingEndpointKind::BranchParameters
		&& Handle.GetIndex() == BranchIndex)
	{
		return FPropertyBindingDataView(
			FArcTacticsBranchParameters::StaticStruct(),
			const_cast<FArcTacticsBranchParameters*>(&Scope.Parameters));
	}
	if (Handle.GetKind() == EArcTacticsBindingEndpointKind::BranchParameterBag
		&& Handle.GetIndex() == BranchIndex)
	{
		return FPropertyBindingDataView(const_cast<FInstancedPropertyBag&>(Scope.ParameterBag).GetMutableValue());
	}
	if (Handle.GetKind() == EArcTacticsBindingEndpointKind::Node
		&& Scope.CapturedNodeData.IsValidIndex(Handle.GetIndex()))
	{
		return FPropertyBindingDataView(
			const_cast<FInstancedStructContainer&>(Scope.CapturedNodeData)[Handle.GetIndex()]);
	}
	if (Handle.GetKind() == EArcTacticsBindingEndpointKind::BranchParameters
		|| Handle.GetKind() == EArcTacticsBindingEndpointKind::BranchParameterBag
		|| Handle.GetKind() == EArcTacticsBindingEndpointKind::Node)
	{
		return FPropertyBindingDataView();
	}
	return GetDataView(Handle);
}

bool FArcTacticsInstanceData::ApplyBindings(
	const FArcTacticsCompiledData& CompiledData,
	const FPropertyBindingIndex16 BatchIndex,
	const FPropertyBindingDataView TargetView,
	const FArcTQSFunctionContext& FunctionContext,
	const FArcTacticsOption* Scope)
{
	if (!BatchIndex.IsValid())
	{
		return true;
	}
	if (!TargetView.IsValid())
	{
		return false;
	}
	const FPropertyBindingCopyInfoBatch& Batch = CompiledData.Bindings.GetBatch(BatchIndex);
	if (!EvaluateFunctions(CompiledData, Batch, FunctionContext, Scope))
	{
		return false;
	}
	const TConstArrayView<FPropertyBindingCopyInfo> Copies = CompiledData.Bindings.GetBatchCopies(Batch);
	const int32 BindingBegin = Batch.BindingsBegin.Get();
	if (Batch.BindingsEnd.Get() - BindingBegin != Copies.Num())
	{
		return false;
	}
	for (int32 CopyIndex = 0; CopyIndex < Copies.Num(); ++CopyIndex)
	{
		const FArcTacticsBinding& Binding = CompiledData.Bindings.GetBinding(BindingBegin + CopyIndex);
		const FPropertyBindingDataView SourceView = Scope
			? GetScopedDataView(CompiledData, Binding.GetSourceDataHandle(), *Scope)
			: GetDataView(Binding.GetSourceDataHandle());
		if (!SourceView.IsValid()
			|| !CompiledData.Bindings.CopyProperty(Copies[CopyIndex], SourceView, TargetView))
		{
			return false;
		}
	}
	return true;
}

bool FArcTacticsInstanceData::EvaluateFunctions(
	const FArcTacticsCompiledData& CompiledData,
	const FPropertyBindingCopyInfoBatch& Batch,
	const FArcTQSFunctionContext& FunctionContext,
	const FArcTacticsOption* Scope)
{
	if (!Batch.PropertyFunctionsBegin.IsValid() || !Batch.PropertyFunctionsEnd.IsValid())
	{
		return true;
	}
	for (int32 FunctionIndex = Batch.PropertyFunctionsBegin.Get(); FunctionIndex < Batch.PropertyFunctionsEnd.Get(); ++FunctionIndex)
	{
		if (!CompiledData.Functions.IsValidIndex(FunctionIndex) || !FunctionInstanceData.IsValidIndex(FunctionIndex))
		{
			return false;
		}
		const FArcTacticsCompiledFunction& CompiledFunction = CompiledData.Functions[FunctionIndex];
		FStructView Function = FunctionInstanceData[FunctionIndex];
		const UScriptStruct* FunctionStruct = CompiledFunction.Function.GetScriptStruct();
		if (!FunctionStruct || Function.GetScriptStruct() != FunctionStruct)
		{
			return false;
		}
		FunctionStruct->CopyScriptStruct(Function.GetMemory(), CompiledFunction.Function.GetMemory());
		if (!ApplyBindings(CompiledData, CompiledFunction.InputBatch, FPropertyBindingDataView(Function), FunctionContext, Scope))
		{
			return false;
		}
		Function.Get<FArcTQSPropertyFunction>().Execute(FunctionContext);
	}
	return true;
}

bool FArcTacticsInstanceData::ResetNodeData(
	const FArcTacticsCompiledData& CompiledData,
	const FArcTacticsNodeHandle NodeHandle)
{
	const int32 NodeIndex = NodeHandle.Get();
	if (!NodeInstanceData.IsValidIndex(NodeIndex)
		|| !CompiledData.DefaultInstanceData.IsValidIndex(NodeIndex))
	{
		return false;
	}
	FStructView Target = NodeInstanceData[NodeIndex];
	const FConstStructView Source = CompiledData.DefaultInstanceData[NodeIndex];
	if (Target.GetScriptStruct() != Source.GetScriptStruct())
	{
		return false;
	}
	Target.GetScriptStruct()->CopyScriptStruct(Target.GetMemory(), Source.GetMemory());
	return true;
}

bool FArcTacticsInstanceData::ResetForDecision(
	const FArcTacticsCompiledData& CompiledData,
	const FArcTQSFunctionContext& FunctionContext)
{
	GlobalTaskIndex = 0;
	bGlobalTaskRunning = false;
	Diagnostic = NAME_None;
	DecisionSettings = CompiledData.DefaultDecisionSettings;
	GlobalParameters = CompiledData.DefaultGlobalParameters;
	if (!ApplyBindings(
		CompiledData,
		CompiledData.GlobalInitializationBindingsBatch,
		GetMutableDataView(FArcTacticsDataHandle(EArcTacticsBindingEndpointKind::GlobalParameters)),
		FunctionContext))
	{
		Diagnostic = TEXT("GlobalInitializationBindingFailed");
		return false;
	}

	for (int32 NodeIndex = 0; NodeIndex < CompiledData.Nodes.Num(); ++NodeIndex)
	{
		if (!ResetNodeData(CompiledData, FArcTacticsNodeHandle(NodeIndex)))
		{
			Diagnostic = TEXT("NodeResetFailed");
			return false;
		}
	}
	return true;
}

void FArcTacticsInstanceData::AddReferencedObjects(FReferenceCollector& Collector)
{
	NodeInstanceData.AddStructReferencedObjects(Collector);
	FunctionInstanceData.AddStructReferencedObjects(Collector);
	GlobalParameters.AddStructReferencedObjects(Collector);
	for (FInstancedPropertyBag& ParameterBag : BranchParameterBags)
	{
		ParameterBag.AddStructReferencedObjects(Collector);
	}
}
