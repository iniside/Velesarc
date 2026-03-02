// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "ArcTQSTraceTypes.h"
#include "TraceServices/Model/AnalysisSession.h"

class FArcTQSTraceProvider : public TraceServices::IProvider
{
public:
	static const FName ProviderName;

	void OnQueryStarted(uint64 InstanceId, int32 QueryId, double Timestamp, FArcTQSTraceQueryRecord&& Record);
	void OnStepCompleted(int32 QueryId, FArcTQSTraceStepRecord&& StepRecord);
	void OnQueryCompleted(int32 QueryId, uint64 InstanceId, uint8 Status, uint8 SelectionMode,
		float ExecutionTimeMs, TArray<FArcTQSTraceItemRecord>&& Items, TArray<int32>&& ResultIndices, double EndTimestamp);

	const FArcTQSTraceQueryRecord* GetQueryByInstanceId(uint64 InstanceId) const;
	void GetCompletedQueriesAtTime(double Time, TArray<const FArcTQSTraceQueryRecord*>& OutQueries) const;
	bool HasDataForInstance(uint64 InstanceId) const;
	bool HasData() const { return !Records.IsEmpty(); }

	/** Enumerate all stored records (diagnostic). */
	void EnumerateRecords(TFunctionRef<void(uint64 InstanceId, const FArcTQSTraceQueryRecord&)> Callback) const
	{
		for (const auto& Pair : Records)
		{
			Callback(Pair.Key, Pair.Value);
		}
	}

private:
	TMap<uint64, FArcTQSTraceQueryRecord> Records; // InstanceId -> Record
	TMap<int32, uint64> ActiveQueryToInstance; // QueryId -> InstanceId (transient routing for StepCompleted)
};
