// Copyright Lukasz Baran. All Rights Reserved.

#include "UtilityAI/Debugger/ArcUtilityTrace.h"
#if WITH_ARCUTILITY_TRACE

#include "UtilityAI/ArcUtilityScoringInstance.h"
#include "UtilityAI/ArcUtilityConsideration.h"
#include "UtilityAI/ArcUtilityScoringSubsystem.h"
#include "ObjectTrace.h"
#include "Serialization/BufferArchive.h"
#include "Trace/Trace.inl"

DEFINE_LOG_CATEGORY_STATIC(LogArcUtilityTrace, Log, All);

UE_TRACE_CHANNEL_DEFINE(ArcUtilityDebugChannel)

namespace
{
	// Querier identity -> InstanceId (one TRACE_INSTANCE per unique querier)
	// Key is either FObjectTrace::GetObjectId(Actor) or packed entity handle
	TMap<uint64, uint64> GUtilityQuerierToInstanceId;
}

// --- Single event definition: all data emitted at request end ---

UE_TRACE_EVENT_BEGIN(ArcUtilityDebugger, RequestCompletedEvent)
	UE_TRACE_EVENT_FIELD(uint64, Cycle)
	UE_TRACE_EVENT_FIELD(uint64, InstanceId)
	UE_TRACE_EVENT_FIELD(int32, RequestId)
	UE_TRACE_EVENT_FIELD(UE::Trace::WideString, QuerierName)
	UE_TRACE_EVENT_FIELD(float, QuerierLocX)
	UE_TRACE_EVENT_FIELD(float, QuerierLocY)
	UE_TRACE_EVENT_FIELD(float, QuerierLocZ)
	UE_TRACE_EVENT_FIELD(float, QuerierFwdX)
	UE_TRACE_EVENT_FIELD(float, QuerierFwdY)
	UE_TRACE_EVENT_FIELD(float, QuerierFwdZ)
	UE_TRACE_EVENT_FIELD(int32, NumEntries)
	UE_TRACE_EVENT_FIELD(int32, NumTargets)
	UE_TRACE_EVENT_FIELD(uint8, Status)          // EArcUtilityScoringStatus
	UE_TRACE_EVENT_FIELD(uint8, SelectionMode)   // EArcUtilitySelectionMode
	UE_TRACE_EVENT_FIELD(float, MinScore)
	UE_TRACE_EVENT_FIELD(float, ExecutionTimeMs)
	UE_TRACE_EVENT_FIELD(int32, WinnerEntryIndex)
	UE_TRACE_EVENT_FIELD(int32, WinnerTargetIndex)
	UE_TRACE_EVENT_FIELD(float, WinnerScore)
	UE_TRACE_EVENT_FIELD(uint8[], TargetsBlob)
	UE_TRACE_EVENT_FIELD(uint8[], ConsiderationDetailsBlob)
	UE_TRACE_EVENT_FIELD(uint8[], ScoredPairsBlob)
UE_TRACE_EVENT_END()

// --- Helpers ---

static FString GetStructDisplayName(const FInstancedStruct& Struct)
{
	if (const UScriptStruct* ScriptStruct = Struct.GetScriptStruct())
	{
		const FString DisplayName = ScriptStruct->GetMetaData(TEXT("DisplayName"));
		return DisplayName.IsEmpty() ? ScriptStruct->GetName() : DisplayName;
	}
	return TEXT("Unknown");
}

static UArcUtilityScoringSubsystem* GetSubsystemFromInstance(const FArcUtilityScoringInstance& Instance)
{
	if (UWorld* World = Instance.Context.World.Get())
	{
		return World->GetSubsystem<UArcUtilityScoringSubsystem>();
	}
	return nullptr;
}

namespace UE::ArcUtilityTrace
{

void OutputRequestCompletedEvent(const FArcUtilityScoringInstance& Instance)
{
	UArcUtilityScoringSubsystem* Subsystem = GetSubsystemFromInstance(Instance);

	// --- TRACE_INSTANCE: one per unique querier, parented to subsystem ---
	uint64 TraceInstanceId = 0;
	FString QuerierNameStr;

#if OBJECT_TRACE_ENABLED
	if (Subsystem)
	{
		TRACE_OBJECT(Subsystem);
		const uint64 OuterId = FObjectTrace::GetObjectId(Subsystem);

		const AActor* QuerierActor = Instance.Context.QuerierActor.Get();
		uint64 QuerierKey = 0;
		if (QuerierActor)
		{
			QuerierKey = FObjectTrace::GetObjectId(QuerierActor);
			QuerierNameStr = QuerierActor->GetName();
		}
		else if (Instance.Context.QuerierEntity.IsSet())
		{
			const FMassEntityHandle& E = Instance.Context.QuerierEntity;
			QuerierKey = static_cast<uint64>(E.Index) | (static_cast<uint64>(E.SerialNumber) << 32);
			QuerierNameStr = FString::Printf(TEXT("Entity [%d:%d]"), E.Index, E.SerialNumber);
		}

		if (QuerierKey != 0)
		{
			if (const uint64* Existing = GUtilityQuerierToInstanceId.Find(QuerierKey))
			{
				TraceInstanceId = *Existing;
			}
			else
			{
				TraceInstanceId = FObjectTrace::AllocateInstanceId();
				GUtilityQuerierToInstanceId.Add(QuerierKey, TraceInstanceId);

				TRACE_INSTANCE(Subsystem, TraceInstanceId, OuterId,
					FArcUtilityContext::StaticStruct(),
					QuerierNameStr);
			}
		}
	}
#else
	// Compute querier name even without OBJECT_TRACE
	if (const AActor* QuerierActor = Instance.Context.QuerierActor.Get())
	{
		QuerierNameStr = QuerierActor->GetName();
	}
	else if (Instance.Context.QuerierEntity.IsSet())
	{
		const FMassEntityHandle& E = Instance.Context.QuerierEntity;
		QuerierNameStr = FString::Printf(TEXT("Entity [%d:%d]"), E.Index, E.SerialNumber);
	}
#endif

	// --- Serialize blobs ---

	// Targets: count + per-target {uint8 Type, float X, float Y, float Z}
	FBufferArchive TargetsArchive;
	{
		int32 TargetCount = Instance.Targets.Num();
		TargetsArchive << TargetCount;
		for (const FArcUtilityTarget& Target : Instance.Targets)
		{
			uint8 Type = static_cast<uint8>(Target.TargetType);
			float X = static_cast<float>(Target.Location.X);
			float Y = static_cast<float>(Target.Location.Y);
			float Z = static_cast<float>(Target.Location.Z);
			TargetsArchive << Type << X << Y << Z;
		}
	}

	// Consideration snapshots: count + per-snapshot {int32 EntryIdx, int32 TargetIdx, int32 ConsiderationIdx, FString ConsiderationName, float RawScore, float CurvedScore, float CompensatedScore}
	FBufferArchive ConsiderationArchive;
	{
		int32 SnapshotCount = Instance.ConsiderationSnapshots.Num();
		ConsiderationArchive << SnapshotCount;
		for (const auto& Snap : Instance.ConsiderationSnapshots)
		{
			int32 EntryIdx = Snap.EntryIndex;
			int32 TargetIdx = Snap.TargetIndex;
			int32 ConsiderationIdx = Snap.ConsiderationIndex;
			FString ConsiderationName = Snap.ConsiderationName;
			float RawScore = Snap.RawScore;
			float CurvedScore = Snap.CurvedScore;
			float CompensatedScore = Snap.CompensatedScore;
			ConsiderationArchive << EntryIdx << TargetIdx << ConsiderationIdx << ConsiderationName << RawScore << CurvedScore << CompensatedScore;
		}
	}

	// Scored pairs: count + per-pair {int32 EntryIdx, int32 TargetIdx, float Score}
	FBufferArchive PairsArchive;
	{
		int32 PairCount = Instance.ScoredPairs.Num();
		PairsArchive << PairCount;
		for (const auto& Pair : Instance.ScoredPairs)
		{
			int32 EntryIdx = Pair.EntryIndex;
			int32 TargetIdx = Pair.TargetIndex;
			float Score = Pair.Score;
			PairsArchive << EntryIdx << TargetIdx << Score;
		}
	}

	// Compute WinnerTargetIndex from ScoredPairs matching the WinningEntryIndex
	int32 WinnerTargetIndex = INDEX_NONE;
	if (Instance.Result.bSuccess)
	{
		float BestScore = -1.0f;
		for (const auto& Pair : Instance.ScoredPairs)
		{
			if (Pair.EntryIndex == Instance.Result.WinningEntryIndex && Pair.Score > BestScore)
			{
				BestScore = Pair.Score;
				WinnerTargetIndex = Pair.TargetIndex;
			}
		}
	}

	// --- Emit single event ---

	UE_TRACE_LOG(ArcUtilityDebugger, RequestCompletedEvent, ArcUtilityDebugChannel)
		<< RequestCompletedEvent.Cycle(FPlatformTime::Cycles64())
		<< RequestCompletedEvent.InstanceId(TraceInstanceId)
		<< RequestCompletedEvent.RequestId(Instance.RequestId)
		<< RequestCompletedEvent.QuerierName(*QuerierNameStr, QuerierNameStr.Len())
		<< RequestCompletedEvent.QuerierLocX(static_cast<float>(Instance.Context.QuerierLocation.X))
		<< RequestCompletedEvent.QuerierLocY(static_cast<float>(Instance.Context.QuerierLocation.Y))
		<< RequestCompletedEvent.QuerierLocZ(static_cast<float>(Instance.Context.QuerierLocation.Z))
		<< RequestCompletedEvent.QuerierFwdX(static_cast<float>(Instance.Context.QuerierForward.X))
		<< RequestCompletedEvent.QuerierFwdY(static_cast<float>(Instance.Context.QuerierForward.Y))
		<< RequestCompletedEvent.QuerierFwdZ(static_cast<float>(Instance.Context.QuerierForward.Z))
		<< RequestCompletedEvent.NumEntries(Instance.Entries.Num())
		<< RequestCompletedEvent.NumTargets(Instance.Targets.Num())
		<< RequestCompletedEvent.Status(static_cast<uint8>(Instance.Status))
		<< RequestCompletedEvent.SelectionMode(static_cast<uint8>(Instance.SelectionMode))
		<< RequestCompletedEvent.MinScore(Instance.MinScore)
		<< RequestCompletedEvent.ExecutionTimeMs(static_cast<float>(Instance.TotalExecutionTime * 1000.0))
		<< RequestCompletedEvent.WinnerEntryIndex(Instance.Result.WinningEntryIndex)
		<< RequestCompletedEvent.WinnerTargetIndex(WinnerTargetIndex)
		<< RequestCompletedEvent.WinnerScore(Instance.Result.Score)
		<< RequestCompletedEvent.TargetsBlob(TargetsArchive.GetData(), TargetsArchive.Num())
		<< RequestCompletedEvent.ConsiderationDetailsBlob(ConsiderationArchive.GetData(), ConsiderationArchive.Num())
		<< RequestCompletedEvent.ScoredPairsBlob(PairsArchive.GetData(), PairsArchive.Num());

	UE_LOG(LogArcUtilityTrace, Log, TEXT("[Utility Trace] RequestCompleted: RequestId=%d InstanceId=%llu Querier=%s Status=%d Entries=%d Targets=%d Winner=%d Score=%.3f %.2fms"),
		Instance.RequestId, TraceInstanceId, *QuerierNameStr, static_cast<int32>(Instance.Status),
		Instance.Entries.Num(), Instance.Targets.Num(), Instance.Result.WinningEntryIndex, Instance.Result.Score,
		Instance.TotalExecutionTime * 1000.0);
}

} // UE::ArcUtilityTrace

#endif // WITH_ARCUTILITY_TRACE
