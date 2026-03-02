// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Trace/Analyzer.h"

namespace TraceServices { class IAnalysisSession; }
class FArcUtilityTraceProvider;

class FArcUtilityTraceAnalyzer : public UE::Trace::IAnalyzer
{
public:
	FArcUtilityTraceAnalyzer(TraceServices::IAnalysisSession& InSession, FArcUtilityTraceProvider& InProvider);

	virtual void OnAnalysisBegin(const FOnAnalysisContext& Context) override;
	virtual bool OnEvent(uint16 RouteId, EStyle Style, const FOnEventContext& Context) override;

private:
	enum : uint16
	{
		RouteId_RequestCompleted
	};

	TraceServices::IAnalysisSession& Session;
	FArcUtilityTraceProvider& Provider;
};
