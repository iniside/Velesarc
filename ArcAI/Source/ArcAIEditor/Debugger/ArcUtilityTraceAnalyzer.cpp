// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcUtilityTraceAnalyzer.h"
#include "ArcUtilityTraceProvider.h"
#include "TraceServices/Model/AnalysisSession.h"
#include "Serialization/MemoryReader.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcUtilityAnalyzer, Log, All);

FArcUtilityTraceAnalyzer::FArcUtilityTraceAnalyzer(TraceServices::IAnalysisSession& InSession, FArcUtilityTraceProvider& InProvider)
	: Session(InSession)
	, Provider(InProvider)
{
}

void FArcUtilityTraceAnalyzer::OnAnalysisBegin(const FOnAnalysisContext& Context)
{
	auto& Builder = Context.InterfaceBuilder;
	Builder.RouteEvent(RouteId_RequestCompleted, "ArcUtilityDebugger", "RequestCompletedEvent");
}

bool FArcUtilityTraceAnalyzer::OnEvent(uint16 RouteId, EStyle Style, const FOnEventContext& Context)
{
	TraceServices::FAnalysisSessionEditScope _(Session);

	const auto& EventData = Context.EventData;
	const double Timestamp = Context.EventTime.AsSeconds(EventData.GetValue<uint64>("Cycle"));

	if (RouteId == RouteId_RequestCompleted)
	{
		FArcUtilityTraceRequestRecord Record;
		Record.RequestId = EventData.GetValue<int32>("RequestId");
		Record.QuerierLocation = FVector(
			EventData.GetValue<float>("QuerierLocX"),
			EventData.GetValue<float>("QuerierLocY"),
			EventData.GetValue<float>("QuerierLocZ"));
		Record.QuerierForward = FVector(
			EventData.GetValue<float>("QuerierFwdX"),
			EventData.GetValue<float>("QuerierFwdY"),
			EventData.GetValue<float>("QuerierFwdZ"));
		Record.NumEntries = EventData.GetValue<int32>("NumEntries");
		Record.NumTargets = EventData.GetValue<int32>("NumTargets");
		Record.Status = EventData.GetValue<uint8>("Status");
		Record.SelectionMode = EventData.GetValue<uint8>("SelectionMode");
		Record.MinScore = EventData.GetValue<float>("MinScore");
		Record.ExecutionTimeMs = EventData.GetValue<float>("ExecutionTimeMs");
		Record.WinnerEntryIndex = EventData.GetValue<int32>("WinnerEntryIndex");
		Record.WinnerTargetIndex = EventData.GetValue<int32>("WinnerTargetIndex");
		Record.WinnerScore = EventData.GetValue<float>("WinnerScore");

		Record.EndTimestamp = Timestamp;
		Record.Timestamp = Timestamp - static_cast<double>(Record.ExecutionTimeMs) / 1000.0;

		// Deserialize targets
		TArrayView<const uint8> TargetsBlob = EventData.GetArrayView<uint8>("TargetsBlob");
		if (TargetsBlob.Num() > 0)
		{
			FMemoryReaderView Reader(MakeArrayView(TargetsBlob.GetData(), TargetsBlob.Num()));
			int32 Count = 0;
			Reader << Count;
			Record.Targets.Reserve(Count);
			for (int32 i = 0; i < Count; ++i)
			{
				FArcUtilityTraceTargetRecord Target;
				Reader << Target.TargetType;
				float X, Y, Z;
				Reader << X << Y << Z;
				Target.Location = FVector(X, Y, Z);
				Record.Targets.Add(MoveTemp(Target));
			}
		}

		// Deserialize consideration details
		TArrayView<const uint8> ConsiderationBlob = EventData.GetArrayView<uint8>("ConsiderationDetailsBlob");
		if (ConsiderationBlob.Num() > 0)
		{
			FMemoryReaderView Reader(MakeArrayView(ConsiderationBlob.GetData(), ConsiderationBlob.Num()));
			int32 Count = 0;
			Reader << Count;
			Record.ConsiderationSnapshots.Reserve(Count);
			for (int32 i = 0; i < Count; ++i)
			{
				FArcUtilityTraceConsiderationSnapshot Snap;
				Reader << Snap.EntryIndex << Snap.TargetIndex << Snap.ConsiderationIndex;
				Reader << Snap.ConsiderationName;
				Reader << Snap.RawScore << Snap.CurvedScore << Snap.CompensatedScore;
				Record.ConsiderationSnapshots.Add(MoveTemp(Snap));
			}
		}

		// Deserialize scored pairs
		TArrayView<const uint8> PairsBlob = EventData.GetArrayView<uint8>("ScoredPairsBlob");
		if (PairsBlob.Num() > 0)
		{
			FMemoryReaderView Reader(MakeArrayView(PairsBlob.GetData(), PairsBlob.Num()));
			int32 Count = 0;
			Reader << Count;
			Record.ScoredPairs.Reserve(Count);
			for (int32 i = 0; i < Count; ++i)
			{
				FArcUtilityTraceScoredPair Pair;
				Reader << Pair.EntryIndex << Pair.TargetIndex << Pair.Score;
				Record.ScoredPairs.Add(MoveTemp(Pair));
			}
		}

		const uint64 InstanceId = EventData.GetValue<uint64>("InstanceId");

		FString QuerierName;
		EventData.GetString("QuerierName", QuerierName);
		if (InstanceId != 0 && !QuerierName.IsEmpty())
		{
			Provider.SetInstanceDisplayName(InstanceId, QuerierName);
		}

		UE_LOG(LogArcUtilityAnalyzer, Log, TEXT("[Utility Analyzer] RequestCompleted: Req=%d InstanceId=%llu Status=%d Entries=%d Targets=%d Time=[%.4f..%.4f]"),
			Record.RequestId, InstanceId, Record.Status, Record.NumEntries, Record.NumTargets,
			Record.Timestamp, Record.EndTimestamp);

		Provider.OnRequestCompleted(InstanceId, MoveTemp(Record));
	}

	return true;
}
