// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Trace/Analyzer.h"

namespace TraceServices { class IAnalysisSession; }
class FArcTQSTraceProvider;

class FArcTQSTraceAnalyzer : public UE::Trace::IAnalyzer
{
public:
	FArcTQSTraceAnalyzer(TraceServices::IAnalysisSession& InSession, FArcTQSTraceProvider& InProvider);

	virtual void OnAnalysisBegin(const FOnAnalysisContext& Context) override;
	virtual bool OnEvent(uint16 RouteId, EStyle Style, const FOnEventContext& Context) override;

private:
	enum : uint16
	{
		RouteId_QueryCompleted
	};

	TraceServices::IAnalysisSession& Session;
	FArcTQSTraceProvider& Provider;
};
