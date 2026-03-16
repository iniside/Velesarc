// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSTraceModule.h"
#include "ArcTQSTraceProvider.h"
#include "ArcTQSTraceAnalyzer.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcTQSTraceModule, Log, All);

void FArcTQSTraceModule::GetModuleInfo(TraceServices::FModuleInfo& OutModuleInfo)
{
	static const FName ModuleName(TEXT("ArcTQSTrace"));
	OutModuleInfo.Name = ModuleName;
	OutModuleInfo.DisplayName = TEXT("Arc TQS");
	UE_LOG(LogArcTQSTraceModule, Log, TEXT("[TQS TraceModule] GetModuleInfo called"));
}

void FArcTQSTraceModule::OnAnalysisBegin(TraceServices::IAnalysisSession& InSession)
{
	UE_LOG(LogArcTQSTraceModule, Log, TEXT("[TQS TraceModule] OnAnalysisBegin — creating provider + analyzer"));
	TSharedPtr<FArcTQSTraceProvider> Provider = MakeShared<FArcTQSTraceProvider>();
	InSession.AddProvider(FArcTQSTraceProvider::ProviderName, Provider);
	InSession.AddAnalyzer(new FArcTQSTraceAnalyzer(InSession, *Provider));
}

void FArcTQSTraceModule::GetLoggers(TArray<const TCHAR*>& OutLoggers)
{
	OutLoggers.Add(TEXT("ArcTQSDebugger"));
	UE_LOG(LogArcTQSTraceModule, Log, TEXT("[TQS TraceModule] GetLoggers — registered 'ArcTQSDebugger'"));
}
