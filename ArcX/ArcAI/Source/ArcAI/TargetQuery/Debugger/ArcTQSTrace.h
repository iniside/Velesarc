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
	/** Emit after generator phase completes. Also registers query with ObjectTrace. */
	void OutputQueryStartedEvent(const FArcTQSQueryInstance& Query);

	/** Emit after each filter/score step completes for a query. */
	void OutputStepCompletedEvent(const FArcTQSQueryInstance& Query, int32 StepIndex);

	/** Emit when query finishes (completed/failed/aborted). Also ends instance lifetime. */
	void OutputQueryCompletedEvent(const FArcTQSQueryInstance& Query);

} // UE::ArcTQSTrace

#define TRACE_ARCTQS_QUERY_STARTED(Query) \
	if (UE_TRACE_CHANNELEXPR_IS_ENABLED(ArcTQSDebugChannel)) \
	{ UE::ArcTQSTrace::OutputQueryStartedEvent(Query); }

#define TRACE_ARCTQS_STEP_COMPLETED(Query, StepIndex) \
	if (UE_TRACE_CHANNELEXPR_IS_ENABLED(ArcTQSDebugChannel)) \
	{ UE::ArcTQSTrace::OutputStepCompletedEvent(Query, StepIndex); }

#define TRACE_ARCTQS_QUERY_COMPLETED(Query) \
	if (UE_TRACE_CHANNELEXPR_IS_ENABLED(ArcTQSDebugChannel)) \
	{ UE::ArcTQSTrace::OutputQueryCompletedEvent(Query); }

#else // WITH_ARCTQS_TRACE

#define TRACE_ARCTQS_QUERY_STARTED(...)
#define TRACE_ARCTQS_STEP_COMPLETED(...)
#define TRACE_ARCTQS_QUERY_COMPLETED(...)

#endif // WITH_ARCTQS_TRACE
