// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

// Mirror the engine pattern: trace in non-shipping builds
#if !UE_BUILD_SHIPPING
#define WITH_ARCTQS_TRACE 1
#else
#define WITH_ARCTQS_TRACE 0
#endif

#if WITH_ARCTQS_TRACE

#include "Trace/Trace.h"
#include "HAL/PlatformTime.h"

struct FArcTQSQueryInstance;

UE_TRACE_CHANNEL_EXTERN(ArcTQSDebugChannel, ARCAI_API)

namespace UE::ArcTQSTrace
{
	/** Emit when query finishes (completed/failed/aborted). All data is available at this point. */
	void OutputQueryCompletedEvent(const FArcTQSQueryInstance& Query);

} // UE::ArcTQSTrace

#define TRACE_ARCTQS_QUERY_COMPLETED(Query) \
	if (UE_TRACE_CHANNELEXPR_IS_ENABLED(ArcTQSDebugChannel)) \
	{ UE::ArcTQSTrace::OutputQueryCompletedEvent(Query); }

#else // WITH_ARCTQS_TRACE

#define TRACE_ARCTQS_QUERY_COMPLETED(...)

#endif // WITH_ARCTQS_TRACE
