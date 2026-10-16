// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Trace/Analyzer.h"

DECLARE_LOG_CATEGORY_EXTERN(LogArcMABAnalyzer, Log, All);

namespace TraceServices { class IAnalysisSession; }
class FArcMassAbilityTraceProvider;

class FArcMassAbilityTraceAnalyzer : public UE::Trace::IAnalyzer
{
public:
	FArcMassAbilityTraceAnalyzer(TraceServices::IAnalysisSession& InSession, FArcMassAbilityTraceProvider& InProvider);

	virtual void OnAnalysisBegin(const FOnAnalysisContext& Context) override;
	virtual bool OnEvent(uint16 RouteId, EStyle Style, const FOnEventContext& Context) override;

private:
	enum : uint16
	{
		RouteId_AttributeChanged,
		RouteId_AbilityActivated,
		RouteId_EffectApplied,
		RouteId_EffectExecuted,
		RouteId_TagCountChanged,
		RouteId_AbilityEnded,
		RouteId_CostFailed,
		RouteId_CooldownFailed,
		RouteId_SignalRaised,
	};

	TraceServices::IAnalysisSession& Session;
	FArcMassAbilityTraceProvider&    Provider;
	uint64                           EventCount = 0;
};
