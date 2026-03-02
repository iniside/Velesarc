// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSTraceAnalyzer.h"
#include "ArcTQSTraceProvider.h"
#include "TraceServices/Model/AnalysisSession.h"
#include "Serialization/MemoryReader.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcTQSAnalyzer, Log, All);

FArcTQSTraceAnalyzer::FArcTQSTraceAnalyzer(TraceServices::IAnalysisSession& InSession, FArcTQSTraceProvider& InProvider)
	: Session(InSession)
	, Provider(InProvider)
{
}

void FArcTQSTraceAnalyzer::OnAnalysisBegin(const FOnAnalysisContext& Context)
{
	auto& Builder = Context.InterfaceBuilder;
	Builder.RouteEvent(RouteId_QueryCompleted, "ArcTQSDebugger", "QueryCompletedEvent");
}

bool FArcTQSTraceAnalyzer::OnEvent(uint16 RouteId, EStyle Style, const FOnEventContext& Context)
{
	TraceServices::FAnalysisSessionEditScope _(Session);

	const auto& EventData = Context.EventData;
	const double Timestamp = Context.EventTime.AsSeconds(EventData.GetValue<uint64>("Cycle"));

	if (RouteId == RouteId_QueryCompleted)
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
		Record.GeneratedItemCount = EventData.GetValue<int32>("GeneratedItemCount");

		Record.Status = EventData.GetValue<uint8>("Status");
		Record.SelectionMode = EventData.GetValue<uint8>("SelectionMode");
		Record.ExecutionTimeMs = EventData.GetValue<float>("ExecutionTimeMs");

		// Timestamp: event time is the query end time. Compute start from execution time.
		Record.EndTimestamp = Timestamp;
		Record.Timestamp = Timestamp - static_cast<double>(Record.ExecutionTimeMs) / 1000.0;

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

		// Deserialize steps
		TArrayView<const uint8> StepsBlob = EventData.GetArrayView<uint8>("StepsBlob");
		if (StepsBlob.Num() > 0)
		{
			FMemoryReaderView StepsReader(MakeArrayView(StepsBlob.GetData(), StepsBlob.Num()));
			int32 StepCount = 0;
			StepsReader << StepCount;
			Record.Steps.Reserve(StepCount);
			for (int32 i = 0; i < StepCount; ++i)
			{
				FArcTQSTraceStepRecord Step;
				Step.StepIndex = i;
				StepsReader << Step.StepName;
				StepsReader << Step.StepKind;
				StepsReader << Step.Weight;
				Record.Steps.Add(MoveTemp(Step));
			}
		}

		// Deserialize final items
		TArrayView<const uint8> ItemsBlob = EventData.GetArrayView<uint8>("ItemsBlob");
		if (ItemsBlob.Num() > 0)
		{
			FMemoryReaderView Reader(MakeArrayView(ItemsBlob.GetData(), ItemsBlob.Num()));
			int32 ItemCount = 0;
			Reader << ItemCount;
			Record.FinalItems.Reserve(ItemCount);
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
				Record.FinalItems.Add(MoveTemp(Item));
			}
		}

		// Deserialize result indices
		TArrayView<const uint8> ResultBlob = EventData.GetArrayView<uint8>("ResultIndicesBlob");
		if (ResultBlob.Num() > 0)
		{
			FMemoryReaderView Reader(MakeArrayView(ResultBlob.GetData(), ResultBlob.Num()));
			int32 ResultCount = 0;
			Reader << ResultCount;
			Record.ResultIndices.Reserve(ResultCount);
			for (int32 i = 0; i < ResultCount; ++i)
			{
				int32 Idx;
				Reader << Idx;
				Record.ResultIndices.Add(Idx);
			}
		}

		const uint64 InstanceId = EventData.GetValue<uint64>("InstanceId");

		FString QuerierName;
		EventData.GetString("QuerierName", QuerierName);
		if (InstanceId != 0 && !QuerierName.IsEmpty())
		{
			Provider.SetInstanceDisplayName(InstanceId, QuerierName);
		}

		UE_LOG(LogArcTQSAnalyzer, Log, TEXT("[TQS Analyzer] QueryCompleted: QueryId=%d InstanceId=%llu Status=%d Items=%d Results=%d Time=[%.4f..%.4f]"),
			Record.QueryId, InstanceId, Record.Status, Record.FinalItems.Num(), Record.ResultIndices.Num(),
			Record.Timestamp, Record.EndTimestamp);

		Provider.OnQueryCompleted(InstanceId, MoveTemp(Record));
	}

	return true;
}
