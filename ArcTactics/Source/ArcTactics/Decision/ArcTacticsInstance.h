// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcTQSPropertyFunction.h"
#include "Decision/ArcTacticsCompiledData.h"
#include "Decision/ArcTacticsNode.h"
#include "Mass/ArcTacticsOption.h"
#include "PropertyBindingDataView.h"
#include "StructUtils/InstancedStructContainer.h"
#include "StructUtils/PropertyBag.h"

#include "ArcTacticsInstance.generated.h"

class FReferenceCollector;

UENUM()
enum class EArcTacticsDecisionStatus : uint8
{
	Idle,
	Running,
	Succeeded,
	Failed,
	Cancelled
};

USTRUCT()
struct ARCTACTICS_API FArcTacticsInstanceData
{
	GENERATED_BODY()

	bool Initialize(const FArcTacticsCompiledData& CompiledData, FArcTacticsContextData* InSharedContext);
	void Reset();
	bool IsCompatible(const FArcTacticsCompiledData& CompiledData) const;
	static int32 GetBranchIndex(const FArcTacticsCompiledData& CompiledData, const FArcTacticsOption& Option);

	FPropertyBindingDataView GetMutableDataView(FArcTacticsDataHandle Handle);
	FPropertyBindingDataView GetDataView(FArcTacticsDataHandle Handle) const;
	FPropertyBindingDataView GetScopedDataView(
		const FArcTacticsCompiledData& CompiledData,
		FArcTacticsDataHandle Handle,
		const FArcTacticsOption& Scope) const;
	bool ApplyBindings(
		const FArcTacticsCompiledData& CompiledData,
		FPropertyBindingIndex16 BatchIndex,
		FPropertyBindingDataView TargetView,
		const FArcTQSFunctionContext& FunctionContext,
		const FArcTacticsOption* Scope = nullptr);
	bool ResetNodeData(const FArcTacticsCompiledData& CompiledData, FArcTacticsNodeHandle NodeHandle);
	bool ResetForDecision(const FArcTacticsCompiledData& CompiledData, const FArcTQSFunctionContext& FunctionContext);

	void AddReferencedObjects(FReferenceCollector& Collector);

	UPROPERTY()
	uint32 CompiledRevision = 0;

	UPROPERTY()
	int32 GlobalTaskIndex = INDEX_NONE;

	UPROPERTY()
	bool bGlobalTaskRunning = false;

	UPROPERTY()
	FArcTacticsDecisionSettingsParameters DecisionSettings;

	UPROPERTY()
	FInstancedPropertyBag GlobalParameters;

	UPROPERTY()
	TArray<FArcTacticsBranchParameters> BranchParameters;

	UPROPERTY()
	TArray<FInstancedPropertyBag> BranchParameterBags;

	UPROPERTY()
	FInstancedStructContainer NodeInstanceData;

	UPROPERTY()
	FInstancedStructContainer FunctionInstanceData;

	UPROPERTY()
	FName Diagnostic;

	FArcTacticsContextData* SharedContext = nullptr;

private:
	bool EvaluateFunctions(
		const FArcTacticsCompiledData& CompiledData,
		const FPropertyBindingCopyInfoBatch& Batch,
		const FArcTQSFunctionContext& FunctionContext,
		const FArcTacticsOption* Scope);
};
