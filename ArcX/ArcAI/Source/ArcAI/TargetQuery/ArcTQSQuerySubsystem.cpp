// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSQuerySubsystem.h"
#include "ArcTQSQueryDefinition.h"
#include "ArcTQSGenerator.h"
#include "HAL/PlatformTime.h"

DECLARE_STATS_GROUP(TEXT("ArcTQS"), STATGROUP_ArcTQS, STATCAT_Advanced);
DECLARE_CYCLE_STAT(TEXT("ArcTQS Tick"), STAT_ArcTQSTick, STATGROUP_ArcTQS);

void UArcTQSQuerySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UArcTQSQuerySubsystem::Deinitialize()
{
	// Abort all running queries
	for (const TSharedPtr<FArcTQSQueryInstance>& Query : RunningQueries)
	{
		if (Query)
		{
			Query->Abort();
		}
	}
	RunningQueries.Empty();

	Super::Deinitialize();
}

bool UArcTQSQuerySubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UArcTQSQuerySubsystem::Tick(float DeltaTime)
{
	SCOPE_CYCLE_COUNTER(STAT_ArcTQSTick);

	if (RunningQueries.IsEmpty())
	{
		return;
	}

	const double FrameDeadline = FPlatformTime::Seconds() + static_cast<double>(MaxAllowedTestingTime);

	// Sort by priority (higher first), stable to preserve FIFO within same priority
	RunningQueries.StableSort([](const TSharedPtr<FArcTQSQueryInstance>& A, const TSharedPtr<FArcTQSQueryInstance>& B)
	{
		return A->Priority > B->Priority;
	});

	int32 Index = 0;
	while (Index < RunningQueries.Num() && FPlatformTime::Seconds() < FrameDeadline)
	{
		FArcTQSQueryInstance* Query = RunningQueries[Index].Get();
		if (!Query || Query->Status == EArcTQSQueryStatus::Aborted)
		{
			RunningQueries.RemoveAtSwap(Index);
			continue;
		}

		const bool bCompleted = Query->ExecuteStep(FrameDeadline);

		if (bCompleted)
		{
			// Fire callback — caller consumes results here
			if (Query->OnCompleted)
			{
				Query->OnCompleted(*Query);
			}

			RunningQueries.RemoveAtSwap(Index);
			// Don't increment Index since we swapped
		}
		else
		{
			++Index;
		}
	}
}

TStatId UArcTQSQuerySubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UArcTQSQuerySubsystem, STATGROUP_ArcTQS);
}

int32 UArcTQSQuerySubsystem::RunQuery(
	const UArcTQSQueryDefinition* Definition,
	const FArcTQSQueryContext& Context,
	FArcTQSQueryFinished OnFinished,
	int32 Priority)
{
	if (!Definition || !Definition->Generator.IsValid())
	{
		return INDEX_NONE;
	}

	if (!Definition->Generator.GetPtr<FArcTQSGenerator>())
	{
		return INDEX_NONE;
	}

	TSharedPtr<FArcTQSQueryInstance> Instance = MakeShared<FArcTQSQueryInstance>();

	// Copy data from the definition into the instance
	Instance->Generator = Definition->Generator;
	Instance->Steps = Definition->Steps;
	Instance->SelectionMode = Definition->SelectionMode;
	Instance->TopN = Definition->TopN;
	Instance->MinPassingScore = Definition->MinPassingScore;

	Instance->QueryContext = Context;
	Instance->Priority = Priority;
	Instance->StartTime = FPlatformTime::Seconds();
	Instance->RequestingEntity = Context.QuerierEntity;

	return SubmitInstance(MoveTemp(Instance), MoveTemp(OnFinished));
}

int32 UArcTQSQuerySubsystem::RunQuery(
	FInstancedStruct&& InGenerator,
	TArray<FInstancedStruct>&& InSteps,
	EArcTQSSelectionMode InSelectionMode,
	int32 InTopN,
	float InMinPassingScore,
	const FArcTQSQueryContext& Context,
	FArcTQSQueryFinished OnFinished,
	int32 Priority)
{
	if (!InGenerator.IsValid() || !InGenerator.GetPtr<FArcTQSGenerator>())
	{
		return INDEX_NONE;
	}

	TSharedPtr<FArcTQSQueryInstance> Instance = MakeShared<FArcTQSQueryInstance>();

	Instance->Generator = MoveTemp(InGenerator);
	Instance->Steps = MoveTemp(InSteps);
	Instance->SelectionMode = InSelectionMode;
	Instance->TopN = InTopN;
	Instance->MinPassingScore = InMinPassingScore;

	Instance->QueryContext = Context;
	Instance->Priority = Priority;
	Instance->StartTime = FPlatformTime::Seconds();
	Instance->RequestingEntity = Context.QuerierEntity;

	return SubmitInstance(MoveTemp(Instance), MoveTemp(OnFinished));
}

int32 UArcTQSQuerySubsystem::SubmitInstance(TSharedPtr<FArcTQSQueryInstance> Instance, FArcTQSQueryFinished OnFinished)
{
	Instance->QueryId = NextQueryId++;

	if (OnFinished.IsBound())
	{
		Instance->OnCompleted = [OnFinished](FArcTQSQueryInstance& CompletedQuery)
		{
			OnFinished.Execute(CompletedQuery);
		};
	}

	const int32 QueryId = Instance->QueryId;
	RunningQueries.Add(MoveTemp(Instance));
	return QueryId;
}

bool UArcTQSQuerySubsystem::AbortQuery(int32 QueryId)
{
	for (int32 i = 0; i < RunningQueries.Num(); ++i)
	{
		if (RunningQueries[i] && RunningQueries[i]->QueryId == QueryId)
		{
			RunningQueries[i]->Abort();
			RunningQueries.RemoveAtSwap(i);
			return true;
		}
	}
	return false;
}

bool UArcTQSQuerySubsystem::IsQueryRunning(int32 QueryId) const
{
	for (const TSharedPtr<FArcTQSQueryInstance>& Query : RunningQueries)
	{
		if (Query && Query->QueryId == QueryId)
		{
			return true;
		}
	}
	return false;
}
