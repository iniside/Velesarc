// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSTraceAnalyzer.h"
#include "ArcTQSTraceProvider.h"
#include "TraceServices/Model/AnalysisSession.h"
#include "Serialization/MemoryReader.h" // for FMemoryReaderView

DEFINE_LOG_CATEGORY_STATIC(LogArcTQSAnalyzer, Log, All);

FArcTQSTraceAnalyzer::FArcTQSTraceAnalyzer(TraceServices::IAnalysisSession& InSession, FArcTQSTraceProvider& InProvider)
	: Session(InSession)
	, Provider(InProvider)
{
	UE_LOG(LogArcTQSAnalyzer, Log, TEXT("[TQS Analyzer] Constructor called"));
}

void FArcTQSTraceAnalyzer::OnAnalysisBegin(const FOnAnalysisContext& Context)
{
	UE_LOG(LogArcTQSAnalyzer, Log, TEXT("[TQS Analyzer] OnAnalysisBegin — routing 3 events from 'ArcTQSDebugger'"));
	auto& Builder = Context.InterfaceBuilder;
	Builder.RouteEvent(RouteId_QueryStarted, "ArcTQSDebugger", "QueryStartedEvent");
	Builder.RouteEvent(RouteId_StepCompleted, "ArcTQSDebugger", "StepCompletedEvent");
	Builder.RouteEvent(RouteId_QueryCompleted, "ArcTQSDebugger", "QueryCompletedEvent");
}

bool FArcTQSTraceAnalyzer::OnEvent(uint16 RouteId, EStyle Style, const FOnEventContext& Context)
{
	UE_LOG(LogArcTQSAnalyzer, Log, TEXT("[TQS Analyzer] OnEvent: RouteId=%d"), RouteId);
	TraceServices::FAnalysisSessionEditScope _(Session);

	const auto& EventData = Context.EventData;
	const double Timestamp = Context.EventTime.AsSeconds(EventData.GetValue<uint64>("Cycle"));

	if (RouteId == RouteId_QueryStarted)
	{
		FArcTQSTraceQueryRecord Record;
		Record.QueryId = EventData.GetValue<int32>("QueryId");
		Record.QuerierLocation = FVector(
			EventData.GetValue<float>("QuerierLocX"),
			EventData.GetValue<float>("QuerierLocY"),
			EventData.GetValue<float>("QuerierLocZ"));
		Record.QuerierForward = FVector(
			EventData.GetValue<float>("QuerierFwdX"),
			EventData.GetValue<float>("QuerierFwdY"),
			EventData.GetValue<float>("QuerierFwdZ"));

		FString GenName;
		EventData.GetString("GeneratorName", GenName);
		Record.GeneratorName = MoveTemp(GenName);

		Record.GeneratedItemCount = EventData.GetValue<int32>("ItemCount");

		// Deserialize context locations
		TArrayView<const uint8> CtxBlob = EventData.GetArrayView<uint8>("ContextLocationsBlob");
		if (CtxBlob.Num() > 0)
		{
			FMemoryReaderView CtxReader(MakeArrayView(CtxBlob.GetData(), CtxBlob.Num()));
			int32 CtxCount = 0;
			CtxReader << CtxCount;
			Record.ContextLocations.Reserve(CtxCount);
			for (int32 i = 0; i < CtxCount; ++i)
			{
				float X, Y, Z;
				CtxReader << X << Y << Z;
				Record.ContextLocations.Emplace(X, Y, Z);
			}
		}

		const uint64 InstanceId = EventData.GetValue<uint64>("InstanceId");
		UE_LOG(LogArcTQSAnalyzer, Log, TEXT("[TQS Analyzer] QueryStarted: QueryId=%d InstanceId=%llu Gen=%s Items=%d Timestamp=%.4f"),
			Record.QueryId, InstanceId, *Record.GeneratorName, Record.GeneratedItemCount, Timestamp);
		Provider.OnQueryStarted(InstanceId, Record.QueryId, Timestamp, MoveTemp(Record));
	}
	else if (RouteId == RouteId_StepCompleted)
	{
		FArcTQSTraceStepRecord StepRecord;
		StepRecord.StepIndex = EventData.GetValue<int32>("StepIndex");

		FString StepName;
		EventData.GetString("StepName", StepName);
		StepRecord.StepName = MoveTemp(StepName);

		StepRecord.StepKind = EventData.GetValue<uint8>("StepKind");
		StepRecord.Weight = EventData.GetValue<float>("Weight");
		StepRecord.ValidRemaining = EventData.GetValue<int32>("ValidRemaining");

		// Deserialize per-item snapshots
		TArrayView<const uint8> ItemsBlob = EventData.GetArrayView<uint8>("ItemScoresBlob");
		if (ItemsBlob.Num() > 0)
		{
			FMemoryReaderView Reader(MakeArrayView(ItemsBlob.GetData(), ItemsBlob.Num()));
			int32 SnapCount = 0;
			Reader << SnapCount;
			StepRecord.ItemSnapshots.Reserve(SnapCount);
			for (int32 j = 0; j < SnapCount; ++j)
			{
				FArcTQSTraceItemSnapshot Snap;
				Reader << Snap.ItemIndex;
				Reader << Snap.Score;
				uint8 Valid = 0;
				Reader << Valid;
				Snap.bValid = Valid != 0;
				StepRecord.ItemSnapshots.Add(MoveTemp(Snap));
			}
		}

		const int32 QueryId = EventData.GetValue<int32>("QueryId");
		Provider.OnStepCompleted(QueryId, MoveTemp(StepRecord));
	}
	else if (RouteId == RouteId_QueryCompleted)
	{
		const int32 QueryId = EventData.GetValue<int32>("QueryId");
		const uint64 InstanceId = EventData.GetValue<uint64>("InstanceId");
		const uint8 Status = EventData.GetValue<uint8>("Status");
		const uint8 SelectionMode = EventData.GetValue<uint8>("SelectionMode");
		const float ExecTimeMs = EventData.GetValue<float>("ExecutionTimeMs");

		// Deserialize final items
		TArray<FArcTQSTraceItemRecord> Items;
		TArrayView<const uint8> ItemsBlob = EventData.GetArrayView<uint8>("ItemsBlob");
		if (ItemsBlob.Num() > 0)
		{
			FMemoryReaderView Reader(MakeArrayView(ItemsBlob.GetData(), ItemsBlob.Num()));
			int32 ItemCount = 0;
			Reader << ItemCount;
			Items.Reserve(ItemCount);
			for (int32 i = 0; i < ItemCount; ++i)
			{
				FArcTQSTraceItemRecord Item;
				Reader << Item.TargetType;
				float X, Y, Z;
				Reader << X << Y << Z;
				Item.Location = FVector(X, Y, Z);
				Reader << Item.Score;
				uint8 Valid;
				Reader << Valid;
				Item.bValid = Valid != 0;
				Reader << Item.EntityHandle;
				Items.Add(MoveTemp(Item));
			}
		}

		// Deserialize result indices
		TArray<int32> ResultIndices;
		TArrayView<const uint8> ResultBlob = EventData.GetArrayView<uint8>("ResultIndicesBlob");
		if (ResultBlob.Num() > 0)
		{
			FMemoryReaderView Reader(MakeArrayView(ResultBlob.GetData(), ResultBlob.Num()));
			int32 ResultCount = 0;
			Reader << ResultCount;
			ResultIndices.Reserve(ResultCount);
			for (int32 i = 0; i < ResultCount; ++i)
			{
				int32 Idx;
				Reader << Idx;
				ResultIndices.Add(Idx);
			}
		}

		UE_LOG(LogArcTQSAnalyzer, Log, TEXT("[TQS Analyzer] QueryCompleted: QueryId=%d InstanceId=%llu Status=%d Items=%d Results=%d Timestamp=%.4f"),
			QueryId, InstanceId, Status, Items.Num(), ResultIndices.Num(), Timestamp);
		Provider.OnQueryCompleted(QueryId, InstanceId, Status, SelectionMode, ExecTimeMs,
			MoveTemp(Items), MoveTemp(ResultIndices), Timestamp);
	}

	return true;
}
