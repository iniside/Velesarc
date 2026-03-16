// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcUtilityTraceProvider.h"

const FName FArcUtilityTraceProvider::ProviderName(TEXT("ArcUtilityTraceProvider"));

void FArcUtilityTraceProvider::OnRequestCompleted(uint64 InstanceId, FArcUtilityTraceRequestRecord&& Record)
{
	Record.InstanceId = InstanceId;
	TArray<FArcUtilityTraceRequestRecord>& Requests = Records.FindOrAdd(InstanceId);
	Requests.Add(MoveTemp(Record));
}

const TArray<FArcUtilityTraceRequestRecord>* FArcUtilityTraceProvider::GetRequestsForInstance(uint64 InstanceId) const
{
	return Records.Find(InstanceId);
}

void FArcUtilityTraceProvider::GetCompletedRequestsAtTime(double Time, TArray<const FArcUtilityTraceRequestRecord*>& OutRequests) const
{
	for (const auto& Pair : Records)
	{
		for (const FArcUtilityTraceRequestRecord& Record : Pair.Value)
		{
			if ((Record.Timestamp - 0.3) <= Time && (Record.EndTimestamp + 0.3) >= Time)
			{
				OutRequests.Add(&Record);
			}
		}
	}
}

bool FArcUtilityTraceProvider::HasDataForInstance(uint64 InstanceId) const
{
	const TArray<FArcUtilityTraceRequestRecord>* Requests = Records.Find(InstanceId);
	return Requests && Requests->Num() > 0;
}

void FArcUtilityTraceProvider::SetInstanceDisplayName(uint64 InstanceId, const FString& Name)
{
	if (!InstanceDisplayNames.Contains(InstanceId))
	{
		InstanceDisplayNames.Add(InstanceId, Name);
	}
}

const FString& FArcUtilityTraceProvider::GetInstanceDisplayName(uint64 InstanceId) const
{
	static const FString EmptyString;
	const FString* Found = InstanceDisplayNames.Find(InstanceId);
	return Found ? *Found : EmptyString;
}
