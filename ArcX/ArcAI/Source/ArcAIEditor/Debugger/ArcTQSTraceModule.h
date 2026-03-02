// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "TraceServices/ModuleService.h"

class FArcTQSTraceModule : public TraceServices::IModule
{
public:
	virtual void GetModuleInfo(TraceServices::FModuleInfo& OutModuleInfo) override;
	virtual void OnAnalysisBegin(TraceServices::IAnalysisSession& InSession) override;
	virtual void GetLoggers(TArray<const TCHAR*>& OutLoggers) override;
};
