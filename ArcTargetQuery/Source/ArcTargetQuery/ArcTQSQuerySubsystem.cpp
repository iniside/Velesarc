// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSQuerySubsystem.h"
#include "ArcTQSQueryDefinition.h"
#include "ArcTQSGenerator.h"
#include "ArcTargetQueryLogs.h"
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

void UArcTQSQuerySubsystem::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	Super::AddReferencedObjects(InThis, Collector);
	for (const TSharedPtr<FArcTQSQueryInstance>& Query : CastChecked<UArcTQSQuerySubsystem>(InThis)->RunningQueries)
	{
		if (Query)
		{
			Query->Parameters.AddStructReferencedObjects(Collector);
			for (FInstancedStruct& Function : Query->Functions)
			{
				Function.AddStructReferencedObjects(Collector);
			}
		}
	}
}

int32 UArcTQSQuerySubsystem::RunQuery(
	const FArcTQSQueryReference& Reference,
	const FArcTQSQueryContext& Context,
	FArcTQSQueryFinished OnFinished,
	int32 Priority,
	const FArcTQSSelectionOverride* Override)
{
	EArcTQSCreateFailure Failure = EArcTQSCreateFailure::None;
	TSharedPtr<FArcTQSQueryInstance> Instance = ArcTQS::CreateQueryInstance(Reference, Context, Priority, Override, Failure);
	if (!Instance)
	{
		const UArcTQSQueryDefinition* Definition = Reference.GetDefinition();
		if (Failure == EArcTQSCreateFailure::NotExecutable)
		{
			if (!LoggedNotExecutable.Contains(FObjectKey(Definition)))
			{
				LoggedNotExecutable.Add(FObjectKey(Definition));
				UE_LOGF(LogArcTQS, Warning, "Refusing query '%ls': the definition is not compiled or failed to compile.", *Definition->GetPathName());
			}
		}
		else if (Failure == EArcTQSCreateFailure::LayoutMismatch)
		{
			if (!LoggedLayoutMismatch.Contains(FObjectKey(Definition)))
			{
				LoggedLayoutMismatch.Add(FObjectKey(Definition));
				UE_LOGF(LogArcTQS, Warning, "Refusing query '%ls': the reference parameters do not match the definition's parameter layout.", *Definition->GetPathName());
			}
		}
		return INDEX_NONE;
	}

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
	ArcTQS::FillResult(CompletedQuery, Data.Result);
	Data.Timestamp = FPlatformTime::Seconds();
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
