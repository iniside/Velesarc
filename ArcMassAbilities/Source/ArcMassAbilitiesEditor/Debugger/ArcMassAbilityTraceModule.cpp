// Copyright Lukasz Baran. All Rights Reserved.

#include "Debugger/ArcMassAbilityTraceModule.h"
#include "Debugger/ArcMassAbilityTraceProvider.h"
#include "Debugger/ArcMassAbilityTraceAnalyzer.h"

DEFINE_LOG_CATEGORY(LogArcMABTraceModule);

void FArcMassAbilityTraceModule::GetModuleInfo(TraceServices::FModuleInfo& OutModuleInfo)
{
	static const FName ModuleName(TEXT("ArcMassAbilityModule"));
	OutModuleInfo.Name        = ModuleName;
	OutModuleInfo.DisplayName = TEXT("ArcMassAbility");
	UE_LOGF(LogArcMABTraceModule, Log,"[ArcMAB TraceModule] GetModuleInfo called");
}

void FArcMassAbilityTraceModule::OnAnalysisBegin(TraceServices::IAnalysisSession& InSession)
{
	UE_LOGF(LogArcMABTraceModule, Log,"[ArcMAB TraceModule] OnAnalysisBegin — creating provider + analyzer");

	TSharedPtr<FArcMassAbilityTraceProvider> Provider = MakeShared<FArcMassAbilityTraceProvider>();
	InSession.AddProvider(FArcMassAbilityTraceProvider::ProviderName, Provider);
	InSession.AddAnalyzer(new FArcMassAbilityTraceAnalyzer(InSession, *Provider));
}

void FArcMassAbilityTraceModule::GetLoggers(TArray<const TCHAR*>& OutLoggers)
{
	OutLoggers.Add(TEXT("ArcMassAbilityDebugger"));
	UE_LOGF(LogArcMABTraceModule, Log,"[ArcMAB TraceModule] GetLoggers — registered 'ArcMassAbilityDebugger'");
}
