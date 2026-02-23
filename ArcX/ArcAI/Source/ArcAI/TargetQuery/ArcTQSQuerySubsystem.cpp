// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSQuerySubsystem.h"
#include "ArcTQSQueryDefinition.h"
#include "ArcTQSGenerator.h"
#include "HAL/PlatformTime.h"

DECLARE_STATS_GROUP(TEXT("ArcTQS"), STATGROUP_ArcTQS, STATCAT_Advanced);
DECLARE_CYCLE_STAT(TEXT("ArcTQS Tick"), STAT_ArcTQSTick, STATGROUP_ArcTQS);

UArcTQSQuerySubsystem::UArcTQSQuerySubsystem()
{
}

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
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE || WorldType == EWorldType::Editor;
}

void UArcTQSQuerySubsystem::Tick(float DeltaTime)
{
	SCOPE_CYCLE_COUNTER(STAT_ArcTQSTick);

#if !UE_BUILD_SHIPPING
	CleanupDebugData();
#endif

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
#if !UE_BUILD_SHIPPING
			StoreDebugData(*Query);
#endif

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
	Instance->ContextProvider = Definition->ContextProvider;
	Instance->Generator = Definition->Generator;
	Instance->Steps = Definition->Steps;
	Instance->SelectionMode = Definition->SelectionMode;
	Instance->TopN = Definition->TopN;
	Instance->MinPassingScore = Definition->MinPassingScore;
	Instance->TopPercent = Definition->TopPercent;

	Instance->QueryContext = Context;
	Instance->Priority = Priority;
	Instance->StartTime = FPlatformTime::Seconds();
	Instance->RequestingEntity = Context.QuerierEntity;

	return SubmitInstance(MoveTemp(Instance), MoveTemp(OnFinished));
}

int32 UArcTQSQuerySubsystem::RunQuery(
	FInstancedStruct&& InContextProvider,
	FInstancedStruct&& InGenerator,
	TArray<FInstancedStruct>&& InSteps,
	EArcTQSSelectionMode InSelectionMode,
	int32 InTopN,
	float InMinPassingScore,
	float InTopPercent,
	const FArcTQSQueryContext& Context,
	FArcTQSQueryFinished OnFinished,
	int32 Priority)
{
	if (!InGenerator.IsValid() || !InGenerator.GetPtr<FArcTQSGenerator>())
	{
		return INDEX_NONE;
	}

	TSharedPtr<FArcTQSQueryInstance> Instance = MakeShared<FArcTQSQueryInstance>();

	Instance->ContextProvider = MoveTemp(InContextProvider);
	Instance->Generator = MoveTemp(InGenerator);
	Instance->Steps = MoveTemp(InSteps);
	Instance->SelectionMode = InSelectionMode;
	Instance->TopN = InTopN;
	Instance->MinPassingScore = InMinPassingScore;
	Instance->TopPercent = InTopPercent;

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

const FArcTQSDebugQueryData* UArcTQSQuerySubsystem::GetDebugData(FMassEntityHandle Entity) const
{
#if !UE_BUILD_SHIPPING
	return DebugQueryData.Find(Entity);
#else
	return nullptr;
#endif
}

#if !UE_BUILD_SHIPPING
void UArcTQSQuerySubsystem::StoreDebugData(const FArcTQSQueryInstance& CompletedQuery)
{
	if (!CompletedQuery.RequestingEntity.IsSet())
	{
		return;
	}

	FArcTQSDebugQueryData& Data = DebugQueryData.FindOrAdd(CompletedQuery.RequestingEntity);
	Data.AllItems = CompletedQuery.Items;
	Data.Results = CompletedQuery.Results;
	Data.QuerierLocation = CompletedQuery.QueryContext.QuerierLocation;
	Data.ContextLocations = CompletedQuery.QueryContext.ContextLocations;
	Data.ExecutionTimeMs = CompletedQuery.TotalExecutionTime * 1000.0;
	Data.Status = CompletedQuery.Status;
	Data.SelectionMode = CompletedQuery.SelectionMode;
	Data.TotalGenerated = CompletedQuery.Items.Num();
	Data.Timestamp = FPlatformTime::Seconds();

	int32 ValidCount = 0;
	for (const FArcTQSTargetItem& Item : CompletedQuery.Items)
	{
		if (Item.bValid)
		{
			++ValidCount;
		}
	}
	Data.TotalValid = ValidCount;
}

void UArcTQSQuerySubsystem::CleanupDebugData()
{
	const double Now = FPlatformTime::Seconds();
	for (auto It = DebugQueryData.CreateIterator(); It; ++It)
	{
		if (Now - It->Value.Timestamp > DebugDataExpiryTime)
		{
			It.RemoveCurrent();
		}
	}
}
#endif
