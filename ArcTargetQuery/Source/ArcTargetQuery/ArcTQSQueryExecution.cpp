// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSQueryExecution.h"
#include "ArcTQSQueryDefinition.h"
#include "ArcTQSQueryReference.h"
#include "HAL/PlatformTime.h"

TSharedPtr<FArcTQSQueryInstance> ArcTQS::CreateQueryInstance(
	const FArcTQSQueryReference& Reference,
	const FArcTQSQueryContext& Context,
	const int32 Priority,
	const FArcTQSSelectionOverride* Override,
	EArcTQSCreateFailure& OutFailure)
{
	const UArcTQSQueryDefinition* Definition = Reference.GetDefinition();
	if (!Definition)
	{
		OutFailure = EArcTQSCreateFailure::NoDefinition;
		return nullptr;
	}
	if (!Definition->IsExecutable())
	{
		OutFailure = EArcTQSCreateFailure::NotExecutable;
		return nullptr;
	}
	if (!Reference.HasMatchingLayout())
	{
		OutFailure = EArcTQSCreateFailure::LayoutMismatch;
		return nullptr;
	}

	TSharedPtr<FArcTQSQueryInstance> Instance = MakeShared<FArcTQSQueryInstance>();

	Instance->Definition = Definition;
	Instance->CompiledRevision = Definition->CompiledData.Revision;
	Instance->Parameters = Reference.GetParameters();
	Instance->Functions.Reserve(Definition->CompiledData.Functions.Num());
	for (const FArcTQSCompiledFunction& CompiledFunction : Definition->CompiledData.Functions)
	{
		Instance->Functions.Add(CompiledFunction.Function);
	}
	Instance->Generator = Definition->Generator;
	Instance->Steps = Definition->Steps;
	Instance->SelectionMode = Override ? Override->SelectionMode : Definition->SelectionMode;
	Instance->TopN = Definition->TopN;
	Instance->MinPassingScore = Override ? Override->MinPassingScore : Definition->MinPassingScore;
	Instance->TopPercent = Definition->TopPercent;

	Instance->QueryContext = Context;
	Instance->Priority = Priority;
	Instance->StartTime = FPlatformTime::Seconds();
	Instance->RequestingEntity = Context.QuerierEntity;
	Instance->RandomStream.GenerateNewSeed();

	OutFailure = EArcTQSCreateFailure::None;
	return Instance;
}

EArcTQSCreateFailure ArcTQS::RunQuerySync(
	const FArcTQSQueryReference& Reference,
	const FArcTQSQueryContext& Context,
	const FArcTQSRunOptions& Options,
	FArcTQSQueryResult& OutResult)
{
	OutResult = FArcTQSQueryResult();

	EArcTQSCreateFailure Failure = EArcTQSCreateFailure::None;
	const TSharedPtr<FArcTQSQueryInstance> Instance = CreateQueryInstance(Reference, Context, 0, nullptr, Failure);
	if (!Instance)
	{
		return Failure;
	}

	Instance->RandomStream.Initialize(Options.Seed);
	Instance->UnavailableRequirements = Options.Unavailable;
	Instance->bEmitTrace = Options.bEmitTrace;

	while (!Instance->ExecuteStep(TNumericLimits<double>::Max()))
	{
	}

	FillResult(*Instance, OutResult);
	return EArcTQSCreateFailure::None;
}

void ArcTQS::FillResult(const FArcTQSQueryInstance& Instance, FArcTQSQueryResult& OutResult)
{
	OutResult.Items = Instance.Items;
	OutResult.ResultIndices = Instance.ResultIndices;
	OutResult.QuerierLocation = Instance.QueryContext.QuerierLocation;
	OutResult.Status = Instance.Status;
	OutResult.SelectionMode = Instance.SelectionMode;
	OutResult.TotalGenerated = Instance.Items.Num();
	OutResult.TotalValid = 0;
	for (const FArcTQSTargetItem& Item : Instance.Items)
	{
		if (Item.bValid)
		{
			++OutResult.TotalValid;
		}
	}
	OutResult.ExecutionTimeMs = Instance.TotalExecutionTime * 1000.0;

#if !UE_BUILD_SHIPPING
	OutResult.FilteredByStep = Instance.FilteredByStep;
	OutResult.Steps = Instance.StepResults;
	OutResult.bGeneratorLevelOnly = Instance.bGeneratorLevelOnly;
#endif
}
