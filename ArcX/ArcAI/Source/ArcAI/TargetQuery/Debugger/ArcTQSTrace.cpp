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

namespace
{
	// Querier identity -> InstanceId (one TRACE_INSTANCE per unique querier)
	// Key is either FObjectTrace::GetObjectId(Actor) or packed entity handle
	TMap<uint64, uint64> GQuerierToInstanceId;
}

// --- Single event definition: all data emitted at query end ---

UE_TRACE_EVENT_BEGIN(ArcTQSDebugger, QueryCompletedEvent)
	UE_TRACE_EVENT_FIELD(uint64, Cycle)
	UE_TRACE_EVENT_FIELD(uint64, InstanceId)
	UE_TRACE_EVENT_FIELD(int32, QueryId)
	UE_TRACE_EVENT_FIELD(UE::Trace::WideString, QuerierName)
	UE_TRACE_EVENT_FIELD(float, QuerierLocX)
	UE_TRACE_EVENT_FIELD(float, QuerierLocY)
	UE_TRACE_EVENT_FIELD(float, QuerierLocZ)
	UE_TRACE_EVENT_FIELD(float, QuerierFwdX)
	UE_TRACE_EVENT_FIELD(float, QuerierFwdY)
	UE_TRACE_EVENT_FIELD(float, QuerierFwdZ)
	UE_TRACE_EVENT_FIELD(UE::Trace::WideString, GeneratorName)
	UE_TRACE_EVENT_FIELD(int32, GeneratedItemCount)
	UE_TRACE_EVENT_FIELD(uint8, Status)         // EArcTQSQueryStatus
	UE_TRACE_EVENT_FIELD(uint8, SelectionMode)  // EArcTQSSelectionMode
	UE_TRACE_EVENT_FIELD(float, ExecutionTimeMs)
	UE_TRACE_EVENT_FIELD(uint8[], ContextLocationsBlob)
	UE_TRACE_EVENT_FIELD(uint8[], StepsBlob)          // step definitions: {FString Name, uint8 Kind, float Weight}
	UE_TRACE_EVENT_FIELD(uint8[], ItemsBlob)           // final items: {uint8 Type, float X/Y/Z, float Score, uint8 Valid, uint64 Entity}
	UE_TRACE_EVENT_FIELD(uint8[], ResultIndicesBlob)   // int32[]
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

void OutputQueryCompletedEvent(const FArcTQSQueryInstance& Query)
{
	UArcTQSQuerySubsystem* Subsystem = GetSubsystemFromQuery(Query);

	// --- TRACE_INSTANCE: one per unique querier, parented to subsystem ---
	uint64 TraceInstanceId = 0;
	FString QuerierNameStr;

#if OBJECT_TRACE_ENABLED
	if (Subsystem)
	{
		TRACE_OBJECT(Subsystem);
		const uint64 OuterId = FObjectTrace::GetObjectId(Subsystem);

		const AActor* QuerierActor = Query.QueryContext.QuerierActor.Get();
		uint64 QuerierKey = 0;
		if (QuerierActor)
		{
			QuerierKey = FObjectTrace::GetObjectId(QuerierActor);
			QuerierNameStr = QuerierActor->GetName();
		}
		else if (Query.QueryContext.QuerierEntity.IsSet())
		{
			const FMassEntityHandle& E = Query.QueryContext.QuerierEntity;
			QuerierKey = static_cast<uint64>(E.Index) | (static_cast<uint64>(E.SerialNumber) << 32);
			QuerierNameStr = FString::Printf(TEXT("Entity [%d:%d]"), E.Index, E.SerialNumber);
		}

		if (QuerierKey != 0)
		{
			if (const uint64* Existing = GQuerierToInstanceId.Find(QuerierKey))
			{
				TraceInstanceId = *Existing;
			}
			else
			{
				TraceInstanceId = FObjectTrace::AllocateInstanceId();
				GQuerierToInstanceId.Add(QuerierKey, TraceInstanceId);

				TRACE_INSTANCE(Subsystem, TraceInstanceId, OuterId,
					FArcTQSQueryContext::StaticStruct(),
					QuerierNameStr);
			}
		}
	}
#else
	// Compute querier name even without OBJECT_TRACE
	if (const AActor* QuerierActor = Query.QueryContext.QuerierActor.Get())
	{
		QuerierNameStr = QuerierActor->GetName();
	}
	else if (Query.QueryContext.QuerierEntity.IsSet())
	{
		const FMassEntityHandle& E = Query.QueryContext.QuerierEntity;
		QuerierNameStr = FString::Printf(TEXT("Entity [%d:%d]"), E.Index, E.SerialNumber);
	}
#endif

	// --- Serialize blobs ---

	const FString GenName = GetStructDisplayName(Query.Generator);

	// Context locations
	FBufferArchive CtxArchive;
	{
		int32 CtxCount = Query.QueryContext.ContextLocations.Num();
		CtxArchive << CtxCount;
		for (const FVector& Loc : Query.QueryContext.ContextLocations)
		{
			float X = static_cast<float>(Loc.X), Y = static_cast<float>(Loc.Y), Z = static_cast<float>(Loc.Z);
			CtxArchive << X << Y << Z;
		}
	}

	// Steps (definitions only — name, kind, weight)
	FBufferArchive StepsArchive;
	{
		int32 StepCount = Query.Steps.Num();
		StepsArchive << StepCount;
		for (int32 i = 0; i < StepCount; ++i)
		{
			FString StepName = GetStructDisplayName(Query.Steps[i]);
			const FArcTQSStep* Step = Query.Steps[i].GetPtr<FArcTQSStep>();
			uint8 StepKind = Step ? static_cast<uint8>(Step->StepType) : 0;
			float Weight = Step ? Step->Weight : 1.0f;
			StepsArchive << StepName << StepKind << Weight;
		}
	}

	// Final items
	FBufferArchive ItemsArchive;
	{
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
	}

	// Result indices
	FBufferArchive ResultArchive;
	{
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
	}

	// --- Emit single event ---

	UE_TRACE_LOG(ArcTQSDebugger, QueryCompletedEvent, ArcTQSDebugChannel)
		<< QueryCompletedEvent.Cycle(FPlatformTime::Cycles64())
		<< QueryCompletedEvent.InstanceId(TraceInstanceId)
		<< QueryCompletedEvent.QueryId(Query.QueryId)
		<< QueryCompletedEvent.QuerierName(*QuerierNameStr, QuerierNameStr.Len())
		<< QueryCompletedEvent.QuerierLocX(static_cast<float>(Query.QueryContext.QuerierLocation.X))
		<< QueryCompletedEvent.QuerierLocY(static_cast<float>(Query.QueryContext.QuerierLocation.Y))
		<< QueryCompletedEvent.QuerierLocZ(static_cast<float>(Query.QueryContext.QuerierLocation.Z))
		<< QueryCompletedEvent.QuerierFwdX(static_cast<float>(Query.QueryContext.QuerierForward.X))
		<< QueryCompletedEvent.QuerierFwdY(static_cast<float>(Query.QueryContext.QuerierForward.Y))
		<< QueryCompletedEvent.QuerierFwdZ(static_cast<float>(Query.QueryContext.QuerierForward.Z))
		<< QueryCompletedEvent.GeneratorName(*GenName, GenName.Len())
		<< QueryCompletedEvent.GeneratedItemCount(Query.Items.Num())
		<< QueryCompletedEvent.Status(static_cast<uint8>(Query.Status))
		<< QueryCompletedEvent.SelectionMode(static_cast<uint8>(Query.SelectionMode))
		<< QueryCompletedEvent.ExecutionTimeMs(static_cast<float>(Query.TotalExecutionTime * 1000.0))
		<< QueryCompletedEvent.ContextLocationsBlob(CtxArchive.GetData(), CtxArchive.Num())
		<< QueryCompletedEvent.StepsBlob(StepsArchive.GetData(), StepsArchive.Num())
		<< QueryCompletedEvent.ItemsBlob(ItemsArchive.GetData(), ItemsArchive.Num())
		<< QueryCompletedEvent.ResultIndicesBlob(ResultArchive.GetData(), ResultArchive.Num());

	UE_LOG(LogArcTQSTrace, Log, TEXT("[TQS Trace] QueryCompleted: QueryId=%d InstanceId=%llu Querier=%s Status=%d Items=%d Results=%d %.2fms"),
		Query.QueryId, TraceInstanceId, *QuerierNameStr, static_cast<int32>(Query.Status),
		Query.Items.Num(), Query.Results.Num(), Query.TotalExecutionTime * 1000.0);
}

} // UE::ArcTQSTrace

#endif // WITH_ARCTQS_TRACE
