// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

// Mirror the engine pattern: trace in non-shipping builds
#if !UE_BUILD_SHIPPING
#define WITH_ARCUTILITY_TRACE 1
#else
#define WITH_ARCUTILITY_TRACE 0
#endif

#if WITH_ARCUTILITY_TRACE

#include "Trace/Trace.h"
#include "HAL/PlatformTime.h"

struct FArcUtilityScoringInstance;

UE_TRACE_CHANNEL_EXTERN(ArcUtilityDebugChannel, ARCAI_API)

namespace UE::ArcUtilityTrace
{
	/** Emit when scoring request finishes (completed/failed/aborted). All data is available at this point. */
	void OutputRequestCompletedEvent(const FArcUtilityScoringInstance& Instance);

} // UE::ArcUtilityTrace

#define TRACE_ARCUTILITY_REQUEST_COMPLETED(Instance) \
	if (UE_TRACE_CHANNELEXPR_IS_ENABLED(ArcUtilityDebugChannel)) \
	{ UE::ArcUtilityTrace::OutputRequestCompletedEvent(Instance); }

#else // WITH_ARCUTILITY_TRACE

#define TRACE_ARCUTILITY_REQUEST_COMPLETED(...)

#endif // WITH_ARCUTILITY_TRACE
