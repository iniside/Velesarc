// Copyright Lukasz Baran. All Rights Reserved.

#include "UtilityAI/ArcUtilityScoringSubsystem.h"
#include "HAL/PlatformTime.h"
#include "UtilityAI/Debugger/ArcUtilityTrace.h"

DECLARE_STATS_GROUP(TEXT("ArcUtility"), STATGROUP_ArcUtility, STATCAT_Advanced);
DECLARE_CYCLE_STAT(TEXT("ArcUtility Tick"), STAT_ArcUtilityTick, STATGROUP_ArcUtility);

UArcUtilityScoringSubsystem::UArcUtilityScoringSubsystem()
{
}

void UArcUtilityScoringSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UArcUtilityScoringSubsystem::Deinitialize()
{
	for (const TSharedPtr<FArcUtilityScoringInstance>& Instance : RunningRequests)
	{
		if (Instance)
		{
			Instance->Abort();
		}
	}
	RunningRequests.Empty();
	Super::Deinitialize();
}

bool UArcUtilityScoringSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE || WorldType == EWorldType::Editor;
}

void UArcUtilityScoringSubsystem::Tick(float DeltaTime)
{
	SCOPE_CYCLE_COUNTER(STAT_ArcUtilityTick);

#if !UE_BUILD_SHIPPING
	CleanupDebugData();
#endif

	if (RunningRequests.IsEmpty())
	{
		return;
	}

	const double FrameDeadline = FPlatformTime::Seconds() + static_cast<double>(MaxAllowedTestingTime);

	// Sort by priority (higher first), stable to preserve FIFO within same priority
	RunningRequests.StableSort([](const TSharedPtr<FArcUtilityScoringInstance>& A, const TSharedPtr<FArcUtilityScoringInstance>& B)
	{
		return A->Priority > B->Priority;
	});

	int32 Index = 0;
	while (Index < RunningRequests.Num() && FPlatformTime::Seconds() < FrameDeadline)
	{
		FArcUtilityScoringInstance* Instance = RunningRequests[Index].Get();
		if (!Instance || Instance->Status == EArcUtilityScoringStatus::Aborted)
		{
			RunningRequests.RemoveAtSwap(Index);
			continue;
		}

		const bool bCompleted = Instance->ExecuteStep(FrameDeadline);

		if (bCompleted)
		{
			TRACE_ARCUTILITY_REQUEST_COMPLETED(*Instance);

#if !UE_BUILD_SHIPPING
			StoreDebugData(*Instance);
#endif

			if (Instance->OnCompleted)
			{
				Instance->OnCompleted(*Instance);
			}

			RunningRequests.RemoveAtSwap(Index);
		}
		else
		{
			++Index;
		}
	}
}

TStatId UArcUtilityScoringSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UArcUtilityScoringSubsystem, STATGROUP_ArcUtility);
}

int32 UArcUtilityScoringSubsystem::SubmitRequest(
	TArray<FArcUtilityEntry>&& InEntries,
	TArray<FArcUtilityTarget>&& InTargets,
	const FArcUtilityContext& InContext,
	EArcUtilitySelectionMode InSelectionMode,
	float InMinScore,
	float InTopPercent,
	FArcUtilityScoringFinished OnFinished,
	int32 Priority)
{
	if (InEntries.IsEmpty() || InTargets.IsEmpty())
	{
		return INDEX_NONE;
	}

	TSharedPtr<FArcUtilityScoringInstance> Instance = MakeShared<FArcUtilityScoringInstance>();
	Instance->Entries = MoveTemp(InEntries);
	Instance->Targets = MoveTemp(InTargets);
	Instance->Context = InContext;
	Instance->SelectionMode = InSelectionMode;
	Instance->MinScore = InMinScore;
	Instance->TopPercent = InTopPercent;
	Instance->Priority = Priority;
	Instance->StartTime = FPlatformTime::Seconds();
	Instance->RequestingEntity = InContext.QuerierEntity;
	Instance->RequestId = NextRequestId++;

	if (OnFinished.IsBound())
	{
		Instance->OnCompleted = [OnFinished](FArcUtilityScoringInstance& Completed)
		{
			if (OnFinished.IsBound())
			{
				OnFinished.Execute(Completed);
			}
		};
	}

	const int32 Id = Instance->RequestId;
	RunningRequests.Add(MoveTemp(Instance));
	return Id;
}

bool UArcUtilityScoringSubsystem::AbortRequest(int32 RequestId)
{
	for (int32 i = 0; i < RunningRequests.Num(); ++i)
	{
		if (RunningRequests[i] && RunningRequests[i]->RequestId == RequestId)
		{
			RunningRequests[i]->Abort();
			RunningRequests.RemoveAtSwap(i);
			return true;
		}
	}
	return false;
}

bool UArcUtilityScoringSubsystem::IsRequestRunning(int32 RequestId) const
{
	for (const TSharedPtr<FArcUtilityScoringInstance>& Instance : RunningRequests)
	{
		if (Instance && Instance->RequestId == RequestId)
		{
			return true;
		}
	}
	return false;
}

#if !UE_BUILD_SHIPPING
const FArcUtilityDebugData* UArcUtilityScoringSubsystem::GetDebugData(FMassEntityHandle Entity) const
{
	return DebugData.Find(Entity);
}

void UArcUtilityScoringSubsystem::StoreDebugData(const FArcUtilityScoringInstance& Instance)
{
	if (!Instance.RequestingEntity.IsSet())
	{
		return;
	}

	FArcUtilityDebugData& Data = DebugData.FindOrAdd(Instance.RequestingEntity);
	Data.AllPairs = Instance.ScoredPairs;
	Data.Result = Instance.Result;
	Data.QuerierLocation = Instance.Context.QuerierLocation;
	Data.ExecutionTimeMs = Instance.TotalExecutionTime * 1000.0;
	Data.Status = Instance.Status;
	Data.SelectionMode = Instance.SelectionMode;
	Data.NumEntries = Instance.Entries.Num();
	Data.NumTargets = Instance.Targets.Num();
	Data.Timestamp = FPlatformTime::Seconds();
}

void UArcUtilityScoringSubsystem::CleanupDebugData()
{
	const double Now = FPlatformTime::Seconds();
	for (auto It = DebugData.CreateIterator(); It; ++It)
	{
		if (Now - It->Value.Timestamp > DebugDataExpiryTime)
		{
			It.RemoveCurrent();
		}
	}
}
#endif
