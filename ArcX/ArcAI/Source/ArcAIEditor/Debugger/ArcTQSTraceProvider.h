// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "ArcTQSTraceTypes.h"
#include "TraceServices/Model/AnalysisSession.h"

class FArcTQSTraceProvider : public TraceServices::IProvider
{
public:
	static const FName ProviderName;

	/** Add a fully-completed query record to this querier's instance. */
	void OnQueryCompleted(uint64 InstanceId, FArcTQSTraceQueryRecord&& Record);

	/** Get all queries for a given trace instance (one per querier entity/actor). */
	const TArray<FArcTQSTraceQueryRecord>* GetQueriesForInstance(uint64 InstanceId) const;

	/** Get all completed queries overlapping a given time (for viewport drawing). */
	void GetCompletedQueriesAtTime(double Time, TArray<const FArcTQSTraceQueryRecord*>& OutQueries) const;

	bool HasDataForInstance(uint64 InstanceId) const;
	bool HasData() const { return !Records.IsEmpty(); }

	/** Store a display name for an instance (first occurrence wins). */
	void SetInstanceDisplayName(uint64 InstanceId, const FString& Name);

	/** Get the display name for an instance, or empty string if not set. */
	const FString& GetInstanceDisplayName(uint64 InstanceId) const;

	/** Enumerate all stored records (diagnostic). */
	void EnumerateRecords(TFunctionRef<void(uint64 InstanceId, const TArray<FArcTQSTraceQueryRecord>&)> Callback) const
	{
		for (const auto& Pair : Records)
		{
			Callback(Pair.Key, Pair.Value);
		}
	}

private:
	TMap<uint64, TArray<FArcTQSTraceQueryRecord>> Records; // InstanceId (per-querier) -> all queries
	TMap<uint64, FString> InstanceDisplayNames; // InstanceId -> querier name
};
