// Copyright Lukasz Baran. All Rights Reserved.

#include "TargetQuery/Debugger/ArcTQSTrace.h"
#if WITH_ARCTQS_TRACE


#include "TargetQuery/ArcTQSQueryInstance.h"
#include "TargetQuery/ArcTQSStep.h"
#include "TargetQuery/ArcTQSGenerator.h"
#include "TargetQuery/ArcTQSQuerySubsystem.h"
#include "ObjectTrace.h"
#include "Serialization/BufferArchive.h"
#include "Trace/Trace.inl"

DEFINE_LOG_CATEGORY_STATIC(LogArcTQSTrace, Log, All);

UE_TRACE_CHANNEL_DEFINE(ArcTQSDebugChannel)

namespace { TMap<int32, uint64> GQueryTraceInstanceIds; }

// --- Event definitions ---

UE_TRACE_EVENT_BEGIN(ArcTQSDebugger, QueryStartedEvent)
	UE_TRACE_EVENT_FIELD(uint64, Cycle)
	UE_TRACE_EVENT_FIELD(uint64, InstanceId)
	UE_TRACE_EVENT_FIELD(int32, QueryId)
	UE_TRACE_EVENT_FIELD(float, QuerierLocX)
	UE_TRACE_EVENT_FIELD(float, QuerierLocY)
	UE_TRACE_EVENT_FIELD(float, QuerierLocZ)
	UE_TRACE_EVENT_FIELD(float, QuerierFwdX)
	UE_TRACE_EVENT_FIELD(float, QuerierFwdY)
	UE_TRACE_EVENT_FIELD(float, QuerierFwdZ)
	UE_TRACE_EVENT_FIELD(UE::Trace::WideString, GeneratorName)
	UE_TRACE_EVENT_FIELD(int32, ItemCount)
	UE_TRACE_EVENT_FIELD(uint8[], ContextLocationsBlob)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN(ArcTQSDebugger, StepCompletedEvent)
	UE_TRACE_EVENT_FIELD(uint64, Cycle)
	UE_TRACE_EVENT_FIELD(int32, QueryId)
	UE_TRACE_EVENT_FIELD(int32, StepIndex)
	UE_TRACE_EVENT_FIELD(UE::Trace::WideString, StepName)
	UE_TRACE_EVENT_FIELD(uint8, StepKind) // 0=Filter, 1=Score
	UE_TRACE_EVENT_FIELD(float, Weight)
	UE_TRACE_EVENT_FIELD(int32, ValidRemaining)
	UE_TRACE_EVENT_FIELD(uint8[], ItemScoresBlob) // compact per-item: {int32 Index, float Score, uint8 bValid}
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN(ArcTQSDebugger, QueryCompletedEvent)
	UE_TRACE_EVENT_FIELD(uint64, Cycle)
	UE_TRACE_EVENT_FIELD(uint64, InstanceId)
	UE_TRACE_EVENT_FIELD(int32, QueryId)
	UE_TRACE_EVENT_FIELD(uint8, Status) // EArcTQSQueryStatus
	UE_TRACE_EVENT_FIELD(uint8, SelectionMode) // EArcTQSSelectionMode
	UE_TRACE_EVENT_FIELD(float, ExecutionTimeMs)
	UE_TRACE_EVENT_FIELD(uint8[], ItemsBlob) // {uint8 TargetType, float LocX/Y/Z, float Score, uint8 bValid, uint64 EntityHandle}
	UE_TRACE_EVENT_FIELD(uint8[], ResultIndicesBlob) // int32[]
UE_TRACE_EVENT_END()

// --- Helper: get display name from instanced struct ---

static FString GetStructDisplayName(const FInstancedStruct& Struct)
{
	if (const UScriptStruct* ScriptStruct = Struct.GetScriptStruct())
	{
		const FString DisplayName = ScriptStruct->GetMetaData(TEXT("DisplayName"));
		return DisplayName.IsEmpty() ? ScriptStruct->GetName() : DisplayName;
	}
	return TEXT("Unknown");
}

// --- Helper: resolve subsystem from query context ---

static UArcTQSQuerySubsystem* GetSubsystemFromQuery(const FArcTQSQueryInstance& Query)
{
	if (UWorld* World = Query.QueryContext.World.Get())
	{
		return World->GetSubsystem<UArcTQSQuerySubsystem>();
	}
	return nullptr;
}

namespace UE::ArcTQSTrace
{

void OutputQueryStartedEvent(const FArcTQSQueryInstance& Query)
{
	UArcTQSQuerySubsystem* Subsystem = GetSubsystemFromQuery(Query);

	// Register with ObjectTrace for Rewind Debugger discovery.
	// Parent to the SUBSYSTEM so the TQS track appears when the subsystem is
	// selected in the Rewind Debugger dropdown. Each query gets its own track.
	// We do NOT call TRACE_INSTANCE_LIFETIME_END — queries are short-lived and
	// a zero-duration lifetime would fail the overlap check against the view range.
	uint64 TraceInstanceId = 0;
#if OBJECT_TRACE_ENABLED
	if (Subsystem)
	{
		TRACE_OBJECT(Subsystem);

		const uint64 OuterId = FObjectTrace::GetObjectId(Subsystem);
		TraceInstanceId = FObjectTrace::AllocateInstanceId();

		GQueryTraceInstanceIds.Add(Query.QueryId, TraceInstanceId);

		// Include querier actor name for identification in the track tree
		const AActor* QuerierActor = Query.QueryContext.QuerierActor.Get();
		const FString ActorName = QuerierActor ? QuerierActor->GetName() : TEXT("Unknown");

		TRACE_INSTANCE(Subsystem, TraceInstanceId, OuterId,
			FArcTQSQueryContext::StaticStruct(),
			FString::Printf(TEXT("TQS Query %d (%s)"), Query.QueryId, *ActorName));

		UE_LOG(LogArcTQSTrace, Log, TEXT("[TQS Trace] QueryStarted: QueryId=%d InstanceId=%llu OuterId=%llu Subsystem=%s Querier=%s"),
			Query.QueryId, TraceInstanceId, OuterId, *Subsystem->GetName(), *ActorName);
	}
	else
	{
		UE_LOG(LogArcTQSTrace, Warning, TEXT("[TQS Trace] QueryStarted: Subsystem is null! QueryId=%d"), Query.QueryId);
	}
#else
	UE_LOG(LogArcTQSTrace, Warning, TEXT("[TQS Trace] OBJECT_TRACE_ENABLED is 0 — TRACE_INSTANCE skipped"));
#endif

	const FString GenName = GetStructDisplayName(Query.Generator);

	// Serialize context locations
	FBufferArchive CtxArchive;
	int32 CtxCount = Query.QueryContext.ContextLocations.Num();
	CtxArchive << CtxCount;
	for (const FVector& Loc : Query.QueryContext.ContextLocations)
	{
		float X = static_cast<float>(Loc.X), Y = static_cast<float>(Loc.Y), Z = static_cast<float>(Loc.Z);
		CtxArchive << X << Y << Z;
	}

	UE_TRACE_LOG(ArcTQSDebugger, QueryStartedEvent, ArcTQSDebugChannel)
		<< QueryStartedEvent.Cycle(FPlatformTime::Cycles64())
		<< QueryStartedEvent.InstanceId(TraceInstanceId)
		<< QueryStartedEvent.QueryId(Query.QueryId)
		<< QueryStartedEvent.QuerierLocX(static_cast<float>(Query.QueryContext.QuerierLocation.X))
		<< QueryStartedEvent.QuerierLocY(static_cast<float>(Query.QueryContext.QuerierLocation.Y))
		<< QueryStartedEvent.QuerierLocZ(static_cast<float>(Query.QueryContext.QuerierLocation.Z))
		<< QueryStartedEvent.QuerierFwdX(static_cast<float>(Query.QueryContext.QuerierForward.X))
		<< QueryStartedEvent.QuerierFwdY(static_cast<float>(Query.QueryContext.QuerierForward.Y))
		<< QueryStartedEvent.QuerierFwdZ(static_cast<float>(Query.QueryContext.QuerierForward.Z))
		<< QueryStartedEvent.GeneratorName(*GenName, GenName.Len())
		<< QueryStartedEvent.ItemCount(Query.Items.Num())
		<< QueryStartedEvent.ContextLocationsBlob(CtxArchive.GetData(), CtxArchive.Num());
}

void OutputStepCompletedEvent(const FArcTQSQueryInstance& Query, int32 StepIndex)
{
	if (!Query.Steps.IsValidIndex(StepIndex))
	{
		return;
	}

	const FString StepName = GetStructDisplayName(Query.Steps[StepIndex]);
	const FArcTQSStep* Step = Query.Steps[StepIndex].GetPtr<FArcTQSStep>();
	const uint8 StepKind = Step ? static_cast<uint8>(Step->StepType) : 0;
	const float Weight = Step ? Step->Weight : 1.0f;

	// Serialize per-item snapshot: count prefix + per-item data
	FBufferArchive ItemsArchive;
	int32 ItemCount = Query.Items.Num();
	ItemsArchive << ItemCount;
	int32 ValidCount = 0;
	for (int32 i = 0; i < ItemCount; ++i)
	{
		const FArcTQSTargetItem& Item = Query.Items[i];
		int32 Idx = i;
		float Score = Item.Score;
		uint8 Valid = Item.bValid ? 1 : 0;
		ItemsArchive << Idx << Score << Valid;
		if (Item.bValid) ++ValidCount;
	}

	UE_TRACE_LOG(ArcTQSDebugger, StepCompletedEvent, ArcTQSDebugChannel)
		<< StepCompletedEvent.Cycle(FPlatformTime::Cycles64())
		<< StepCompletedEvent.QueryId(Query.QueryId)
		<< StepCompletedEvent.StepIndex(StepIndex)
		<< StepCompletedEvent.StepName(*StepName, StepName.Len())
		<< StepCompletedEvent.StepKind(StepKind)
		<< StepCompletedEvent.Weight(Weight)
		<< StepCompletedEvent.ValidRemaining(ValidCount)
		<< StepCompletedEvent.ItemScoresBlob(ItemsArchive.GetData(), ItemsArchive.Num());
}

void OutputQueryCompletedEvent(const FArcTQSQueryInstance& Query)
{
	// Serialize all items
	FBufferArchive ItemsArchive;
	int32 ItemCount = Query.Items.Num();
	ItemsArchive << ItemCount;
	for (const FArcTQSTargetItem& Item : Query.Items)
	{
		uint8 Type = static_cast<uint8>(Item.TargetType);
		float X = static_cast<float>(Item.Location.X);
		float Y = static_cast<float>(Item.Location.Y);
		float Z = static_cast<float>(Item.Location.Z);
		float Score = Item.Score;
		uint8 Valid = Item.bValid ? 1 : 0;
		uint64 EntHandle = static_cast<uint64>(Item.EntityHandle.Index)
			| (static_cast<uint64>(Item.EntityHandle.SerialNumber) << 32);
		ItemsArchive << Type << X << Y << Z << Score << Valid << EntHandle;
	}

	// Serialize result indices
	FBufferArchive ResultArchive;
	int32 ResultCount = Query.Results.Num();
	ResultArchive << ResultCount;
	for (const FArcTQSTargetItem& Result : Query.Results)
	{
		int32 FoundIndex = INDEX_NONE;
		for (int32 i = 0; i < Query.Items.Num(); ++i)
		{
			if (Query.Items[i].Location.Equals(Result.Location, 1.0f) &&
				Query.Items[i].TargetType == Result.TargetType)
			{
				FoundIndex = i;
				break;
			}
		}
		ResultArchive << FoundIndex;
	}

	// Retrieve trace instance ID for the completed event.
	// NOTE: We intentionally do NOT call TRACE_INSTANCE_LIFETIME_END here.
	// TQS queries complete within a single frame, producing a near-zero lifetime
	// that fails the Rewind Debugger's view-range overlap check. By leaving the
	// lifetime open-ended, the instance remains visible at any scrub position.
	uint64 TraceInstanceId = 0;
#if OBJECT_TRACE_ENABLED
	if (const uint64* Found = GQueryTraceInstanceIds.Find(Query.QueryId))
	{
		TraceInstanceId = *Found;
		GQueryTraceInstanceIds.Remove(Query.QueryId);
	}
#endif

	UE_TRACE_LOG(ArcTQSDebugger, QueryCompletedEvent, ArcTQSDebugChannel)
		<< QueryCompletedEvent.Cycle(FPlatformTime::Cycles64())
		<< QueryCompletedEvent.InstanceId(TraceInstanceId)
		<< QueryCompletedEvent.QueryId(Query.QueryId)
		<< QueryCompletedEvent.Status(static_cast<uint8>(Query.Status))
		<< QueryCompletedEvent.SelectionMode(static_cast<uint8>(Query.SelectionMode))
		<< QueryCompletedEvent.ExecutionTimeMs(static_cast<float>(Query.TotalExecutionTime * 1000.0))
		<< QueryCompletedEvent.ItemsBlob(ItemsArchive.GetData(), ItemsArchive.Num())
		<< QueryCompletedEvent.ResultIndicesBlob(ResultArchive.GetData(), ResultArchive.Num());
}

} // UE::ArcTQSTrace

#endif // WITH_ARCTQS_TRACE
