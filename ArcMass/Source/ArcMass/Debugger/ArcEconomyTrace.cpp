// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMass/Debugger/ArcEconomyTrace.h"

#if WITH_ARC_ECONOMY_TRACE

#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "HAL/CriticalSection.h"
#include "Misc/ScopeLock.h"
#include "ProfilingDebugging/CountersTrace.h"

// Gate only: economy data rides the engine Counters and Bookmark channels, which already ship analyzers and providers.
UE_TRACE_CHANNEL_DEFINE(ArcEconomyChannel)

namespace ArcEconomyTracePrivate_ArcEconomyTrace
{
	struct FCounterKey
	{
		FName BuildingType;
		FName Metric;

		bool operator==(const FCounterKey& Other) const
		{
			return BuildingType == Other.BuildingType && Metric == Other.Metric;
		}
	};

	uint32 GetTypeHash(const FCounterKey& Key)
	{
		return HashCombine(GetTypeHash(Key.BuildingType), GetTypeHash(Key.Metric));
	}

	struct FIntCounterEntry
	{
		uint16 CounterId = 0;
		int64 LastValue = 0;
		bool bHasValue = false;
		uint32 Epoch = 0;
	};

	struct FFloatCounterEntry
	{
		uint16 CounterId = 0;
		double LastValue = 0.0;
		bool bHasValue = false;
		uint32 Epoch = 0;
	};

	FCriticalSection GCounterCS;
	TMap<FCounterKey, FIntCounterEntry> GIntCounters;
	TMap<FCounterKey, FFloatCounterEntry> GFloatCounters;
	uint32 GTraceEpoch = 1;
	uint32 GTraceGuid[4] = {0, 0, 0, 0};

	// Each connection re-sends every counter spec, so ids and dedup state cached under the previous trace guid are stale.
	uint32 CurrentTraceEpoch()
	{
		uint32 SessionGuid[4] = {0, 0, 0, 0};
		uint32 TraceGuid[4] = {0, 0, 0, 0};
		if (!UE::Trace::IsTracingTo(SessionGuid, TraceGuid))
		{
			return GTraceEpoch;
		}

		bool bSameTrace = true;
		for (int32 Index = 0; Index < 4; ++Index)
		{
			bSameTrace &= TraceGuid[Index] == GTraceGuid[Index];
		}

		if (!bSameTrace)
		{
			for (int32 Index = 0; Index < 4; ++Index)
			{
				GTraceGuid[Index] = TraceGuid[Index];
			}
			++GTraceEpoch;
		}

		return GTraceEpoch;
	}

	uint16 ResolveCounterId(uint16 CachedId, const FCounterKey& Key, ETraceCounterType CounterType)
	{
		// OutputInitCounter returns 0 while CountersChannel is off, and OutputSetValue(0, ...) is a permanent no-op, so a cached zero is retried instead of kept.
		if (CachedId != 0)
		{
			return CachedId;
		}

		if (!UE_TRACE_CHANNELEXPR_IS_ENABLED(CountersChannel))
		{
			return 0;
		}

		const FString CounterName = FString::Printf(TEXT("Arc/Economy/%s/%s"),
			*Key.BuildingType.ToString(), *Key.Metric.ToString());

		return FCountersTrace::OutputInitCounter(*CounterName, CounterType, TraceCounterDisplayHint_None);
	}

} // namespace ArcEconomyTracePrivate_ArcEconomyTrace

namespace UE::ArcEconomyTrace
{
	void SetBuildingCounterInt(FName BuildingType, FName Metric, int64 Value)
	{
		using namespace ArcEconomyTracePrivate_ArcEconomyTrace;

		const FCounterKey Key{BuildingType, Metric};

		FScopeLock Lock(&GCounterCS);

		const uint32 Epoch = CurrentTraceEpoch();

		FIntCounterEntry& Entry = GIntCounters.FindOrAdd(Key);
		if (Entry.Epoch != Epoch)
		{
			Entry.Epoch = Epoch;
			Entry.CounterId = 0;
			Entry.bHasValue = false;
		}

		Entry.CounterId = ResolveCounterId(Entry.CounterId, Key, TraceCounterType_Int);
		if (Entry.CounterId == 0)
		{
			return;
		}

		if (Entry.bHasValue && Entry.LastValue == Value)
		{
			return;
		}

		Entry.LastValue = Value;
		Entry.bHasValue = true;

		FCountersTrace::OutputSetValue(Entry.CounterId, Value);
	}

	void SetBuildingCounterFloat(FName BuildingType, FName Metric, double Value)
	{
		using namespace ArcEconomyTracePrivate_ArcEconomyTrace;

		const FCounterKey Key{BuildingType, Metric};

		FScopeLock Lock(&GCounterCS);

		const uint32 Epoch = CurrentTraceEpoch();

		FFloatCounterEntry& Entry = GFloatCounters.FindOrAdd(Key);
		if (Entry.Epoch != Epoch)
		{
			Entry.Epoch = Epoch;
			Entry.CounterId = 0;
			Entry.bHasValue = false;
		}

		Entry.CounterId = ResolveCounterId(Entry.CounterId, Key, TraceCounterType_Float);
		if (Entry.CounterId == 0)
		{
			return;
		}

		if (Entry.bHasValue && Entry.LastValue == Value)
		{
			return;
		}

		Entry.LastValue = Value;
		Entry.bHasValue = true;

		FCountersTrace::OutputSetValue(Entry.CounterId, Value);
	}

} // namespace UE::ArcEconomyTrace

#endif // WITH_ARC_ECONOMY_TRACE
