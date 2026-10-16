// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Misc/Build.h"
#include "ProfilingDebugging/MiscTrace.h"
#include "Trace/Config.h"

#if !UE_BUILD_SHIPPING && UE_TRACE_ENABLED
#define WITH_ARC_ECONOMY_TRACE 1
#else
#define WITH_ARC_ECONOMY_TRACE 0
#endif

#if WITH_ARC_ECONOMY_TRACE

#include "Trace/Detail/Channel.h"
#include "Trace/Detail/Channel.inl"
#include "Trace/Trace.h"
#include "UObject/NameTypes.h"

UE_TRACE_CHANNEL_EXTERN(ArcEconomyChannel, ARCMASS_API)

namespace UE::ArcEconomyTrace
{
	ARCMASS_API void SetBuildingCounterInt(FName BuildingType, FName Metric, int64 Value);
	ARCMASS_API void SetBuildingCounterFloat(FName BuildingType, FName Metric, double Value);

} // namespace UE::ArcEconomyTrace

#define TRACE_ARC_ECONOMY_BOOKMARK(LiteralText) \
	if (UE_TRACE_CHANNELEXPR_IS_ENABLED(ArcEconomyChannel)) \
	{ TRACE_BOOKMARK(TEXT("Arc/Economy/") LiteralText); }

#define TRACE_ARC_ECONOMY_BOOKMARKF(LiteralFormat, ...) \
	if (UE_TRACE_CHANNELEXPR_IS_ENABLED(ArcEconomyChannel)) \
	{ TRACE_BOOKMARK(TEXT("Arc/Economy/") LiteralFormat, ##__VA_ARGS__); }

#define TRACE_ARC_ECONOMY_BOOKMARK_STR(TextExpr) \
	if (UE_TRACE_CHANNELEXPR_IS_ENABLED(ArcEconomyChannel)) \
	{ TRACE_BOOKMARK(TEXT("Arc/Economy/%s"), *(TextExpr)); }

#define TRACE_ARC_ECONOMY_COUNTER_INT(BuildingType, Metric, Value) \
	if (UE_TRACE_CHANNELEXPR_IS_ENABLED(ArcEconomyChannel)) \
	{ UE::ArcEconomyTrace::SetBuildingCounterInt(BuildingType, Metric, Value); }

#define TRACE_ARC_ECONOMY_COUNTER_FLOAT(BuildingType, Metric, Value) \
	if (UE_TRACE_CHANNELEXPR_IS_ENABLED(ArcEconomyChannel)) \
	{ UE::ArcEconomyTrace::SetBuildingCounterFloat(BuildingType, Metric, Value); }

#else // WITH_ARC_ECONOMY_TRACE

#define TRACE_ARC_ECONOMY_BOOKMARK(LiteralText)                         (void)0
#define TRACE_ARC_ECONOMY_BOOKMARKF(LiteralFormat, ...)                 (void)0
#define TRACE_ARC_ECONOMY_BOOKMARK_STR(TextExpr)                        (void)0
#define TRACE_ARC_ECONOMY_COUNTER_INT(BuildingType, Metric, Value)      (void)0
#define TRACE_ARC_ECONOMY_COUNTER_FLOAT(BuildingType, Metric, Value)    (void)0

#endif // WITH_ARC_ECONOMY_TRACE
