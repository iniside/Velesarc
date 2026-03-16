// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcUtilityTraceModule.h"
#include "ArcUtilityTraceProvider.h"
#include "ArcUtilityTraceAnalyzer.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcUtilityTraceModule, Log, All);

void FArcUtilityTraceModule::GetModuleInfo(TraceServices::FModuleInfo& OutModuleInfo)
{
	static const FName ModuleName(TEXT("ArcUtilityTrace"));
	OutModuleInfo.Name = ModuleName;
	OutModuleInfo.DisplayName = TEXT("Arc Utility AI");
	UE_LOG(LogArcUtilityTraceModule, Log, TEXT("[Utility TraceModule] GetModuleInfo called"));
}

void FArcUtilityTraceModule::OnAnalysisBegin(TraceServices::IAnalysisSession& InSession)
{
	UE_LOG(LogArcUtilityTraceModule, Log, TEXT("[Utility TraceModule] OnAnalysisBegin — creating provider + analyzer"));
	TSharedPtr<FArcUtilityTraceProvider> Provider = MakeShared<FArcUtilityTraceProvider>();
	InSession.AddProvider(FArcUtilityTraceProvider::ProviderName, Provider);
	InSession.AddAnalyzer(new FArcUtilityTraceAnalyzer(InSession, *Provider));
}

void FArcUtilityTraceModule::GetLoggers(TArray<const TCHAR*>& OutLoggers)
{
	OutLoggers.Add(TEXT("ArcUtilityDebugger"));
	UE_LOG(LogArcUtilityTraceModule, Log, TEXT("[Utility TraceModule] GetLoggers — registered 'ArcUtilityDebugger'"));
}
