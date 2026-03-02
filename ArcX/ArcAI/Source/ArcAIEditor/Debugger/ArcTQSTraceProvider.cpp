// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSTraceProvider.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcTQSProvider, Log, All);

const FName FArcTQSTraceProvider::ProviderName(TEXT("ArcTQSTraceProvider"));

void FArcTQSTraceProvider::OnQueryStarted(uint64 InstanceId, int32 QueryId, double Timestamp, FArcTQSTraceQueryRecord&& Record)
{
	Record.InstanceId = InstanceId;
	Record.Timestamp = Timestamp;
	Records.Add(InstanceId, MoveTemp(Record));
	ActiveQueryToInstance.Add(QueryId, InstanceId);

	UE_LOG(LogArcTQSProvider, Log, TEXT("[TQS Provider] OnQueryStarted: InstanceId=%llu QueryId=%d Timestamp=%.4f TotalRecords=%d"),
		InstanceId, QueryId, Timestamp, Records.Num());
}

void FArcTQSTraceProvider::OnStepCompleted(int32 QueryId, FArcTQSTraceStepRecord&& StepRecord)
{
	if (const uint64* InstId = ActiveQueryToInstance.Find(QueryId))
	{
		if (FArcTQSTraceQueryRecord* Record = Records.Find(*InstId))
		{
			Record->Steps.Add(MoveTemp(StepRecord));
		}
	}
}

void FArcTQSTraceProvider::OnQueryCompleted(int32 QueryId, uint64 InstanceId, uint8 Status,
	uint8 SelectionMode, float ExecutionTimeMs,
	TArray<FArcTQSTraceItemRecord>&& Items, TArray<int32>&& ResultIndices, double EndTimestamp)
{
	// Try InstanceId first, fall back to QueryId mapping
	FArcTQSTraceQueryRecord* Record = Records.Find(InstanceId);
	if (!Record)
	{
		if (const uint64* InstId = ActiveQueryToInstance.Find(QueryId))
		{
			Record = Records.Find(*InstId);
		}
	}

	if (Record)
	{
		Record->Status = Status;
		Record->SelectionMode = SelectionMode;
		Record->ExecutionTimeMs = ExecutionTimeMs;
		Record->FinalItems = MoveTemp(Items);
		Record->ResultIndices = MoveTemp(ResultIndices);
		Record->EndTimestamp = EndTimestamp;
		Record->bCompleted = true;

		UE_LOG(LogArcTQSProvider, Log, TEXT("[TQS Provider] OnQueryCompleted: InstanceId=%llu QueryId=%d Status=%d Items=%d Results=%d Time=[%.4f..%.4f]"),
			Record->InstanceId, QueryId, Status, Record->FinalItems.Num(), Record->ResultIndices.Num(),
			Record->Timestamp, Record->EndTimestamp);
	}
	else
	{
		UE_LOG(LogArcTQSProvider, Warning, TEXT("[TQS Provider] OnQueryCompleted: Record NOT FOUND for QueryId=%d InstanceId=%llu"),
			QueryId, InstanceId);
	}
	ActiveQueryToInstance.Remove(QueryId);
}

const FArcTQSTraceQueryRecord* FArcTQSTraceProvider::GetQueryByInstanceId(uint64 InstanceId) const
{
	return Records.Find(InstanceId);
}

void FArcTQSTraceProvider::GetCompletedQueriesAtTime(double Time, TArray<const FArcTQSTraceQueryRecord*>& OutQueries) const
{
	for (const auto& Pair : Records)
	{
		const FArcTQSTraceQueryRecord& Record = Pair.Value;
		if (Record.bCompleted && (Record.Timestamp - 1.f) <= Time && (Record.EndTimestamp+1.f) >= Time)
		{
			OutQueries.Add(&Record);
		}
	}
}

bool FArcTQSTraceProvider::HasDataForInstance(uint64 InstanceId) const
{
	return Records.Contains(InstanceId);
}
