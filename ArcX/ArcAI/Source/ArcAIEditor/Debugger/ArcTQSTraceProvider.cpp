// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSTraceProvider.h"

const FName FArcTQSTraceProvider::ProviderName(TEXT("ArcTQSTraceProvider"));

void FArcTQSTraceProvider::OnQueryCompleted(uint64 InstanceId, FArcTQSTraceQueryRecord&& Record)
{
	Record.InstanceId = InstanceId;
	TArray<FArcTQSTraceQueryRecord>& Queries = Records.FindOrAdd(InstanceId);
	Queries.Add(MoveTemp(Record));
}

const TArray<FArcTQSTraceQueryRecord>* FArcTQSTraceProvider::GetQueriesForInstance(uint64 InstanceId) const
{
	return Records.Find(InstanceId);
}

void FArcTQSTraceProvider::GetCompletedQueriesAtTime(double Time, TArray<const FArcTQSTraceQueryRecord*>& OutQueries) const
{
	for (const auto& Pair : Records)
	{
		for (const FArcTQSTraceQueryRecord& Record : Pair.Value)
		{
			if ((Record.Timestamp - 0.3) <= Time && (Record.EndTimestamp + 0.3) >= Time)
			{
				OutQueries.Add(&Record);
			}
		}
	}
}

bool FArcTQSTraceProvider::HasDataForInstance(uint64 InstanceId) const
{
	const TArray<FArcTQSTraceQueryRecord>* Queries = Records.Find(InstanceId);
	return Queries && Queries->Num() > 0;
}

void FArcTQSTraceProvider::SetInstanceDisplayName(uint64 InstanceId, const FString& Name)
{
	if (!InstanceDisplayNames.Contains(InstanceId))
	{
		InstanceDisplayNames.Add(InstanceId, Name);
	}
}

const FString& FArcTQSTraceProvider::GetInstanceDisplayName(uint64 InstanceId) const
{
	static const FString EmptyString;
	const FString* Found = InstanceDisplayNames.Find(InstanceId);
	return Found ? *Found : EmptyString;
}
