// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "ArcUtilityTraceTypes.h"
#include "TraceServices/Model/AnalysisSession.h"

class FArcUtilityTraceProvider : public TraceServices::IProvider
{
public:
	static const FName ProviderName;

	void OnRequestCompleted(uint64 InstanceId, FArcUtilityTraceRequestRecord&& Record);
	const TArray<FArcUtilityTraceRequestRecord>* GetRequestsForInstance(uint64 InstanceId) const;
	void GetCompletedRequestsAtTime(double Time, TArray<const FArcUtilityTraceRequestRecord*>& OutRequests) const;
	bool HasDataForInstance(uint64 InstanceId) const;
	bool HasData() const { return !Records.IsEmpty(); }
	void SetInstanceDisplayName(uint64 InstanceId, const FString& Name);
	const FString& GetInstanceDisplayName(uint64 InstanceId) const;

private:
	TMap<uint64, TArray<FArcUtilityTraceRequestRecord>> Records;
	TMap<uint64, FString> InstanceDisplayNames;
};
