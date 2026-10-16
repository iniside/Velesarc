// Copyright Lukasz Baran. All Rights Reserved.

#include "Debugger/ArcMassAbilityTraceAnalyzer.h"
#include "Debugger/ArcMassAbilityTraceProvider.h"
#include "Events/ArcAbilityEventPayloads.h"
#include "TraceServices/Model/AnalysisSession.h"

DEFINE_LOG_CATEGORY(LogArcMABAnalyzer);

namespace ArcMassAbilityTraceAnalyzerInternal
{
	static const TCHAR* LoggerName = TEXT("ArcMassAbilityDebugger");

	static const TCHAR* AbilityEndReasonLabel(uint8 Reason)
	{
		switch (static_cast<EArcAbilityEndReason>(Reason))
		{
		case EArcAbilityEndReason::Completed:
			return TEXT("Completed");
		case EArcAbilityEndReason::Cancelled:
			return TEXT("Cancelled");
		case EArcAbilityEndReason::Failed:
			return TEXT("Failed");
		case EArcAbilityEndReason::Interrupted:
			return TEXT("Interrupted");
		}
		return TEXT("Unknown");
	}
}

FArcMassAbilityTraceAnalyzer::FArcMassAbilityTraceAnalyzer(TraceServices::IAnalysisSession& InSession, FArcMassAbilityTraceProvider& InProvider)
	: Session(InSession)
	, Provider(InProvider)
{
}

void FArcMassAbilityTraceAnalyzer::OnAnalysisBegin(const FOnAnalysisContext& Context)
{
	UE::Trace::IAnalyzer::FInterfaceBuilder& Builder = Context.InterfaceBuilder;

	Builder.RouteEvent(RouteId_AttributeChanged, "ArcMassAbilityDebugger", "AttributeChanged");
	Builder.RouteEvent(RouteId_AbilityActivated, "ArcMassAbilityDebugger", "AbilityActivated");
	Builder.RouteEvent(RouteId_EffectApplied,    "ArcMassAbilityDebugger", "EffectApplied");
	Builder.RouteEvent(RouteId_EffectExecuted,   "ArcMassAbilityDebugger", "EffectExecuted");
	Builder.RouteEvent(RouteId_TagCountChanged,  "ArcMassAbilityDebugger", "TagCountChanged");
	Builder.RouteEvent(RouteId_AbilityEnded,     "ArcMassAbilityDebugger", "AbilityEnded");
	Builder.RouteEvent(RouteId_CostFailed,       "ArcMassAbilityDebugger", "CostFailed");
	Builder.RouteEvent(RouteId_CooldownFailed,   "ArcMassAbilityDebugger", "CooldownFailed");
	Builder.RouteEvent(RouteId_SignalRaised,     "ArcMassAbilityDebugger", "SignalRaised");

	UE_LOGF(LogArcMABAnalyzer, Log,"[ArcMAB Analyzer] OnAnalysisBegin — 9 routes registered for logger 'ArcMassAbilityDebugger'");
}

bool FArcMassAbilityTraceAnalyzer::OnEvent(uint16 RouteId, EStyle Style, const FOnEventContext& Context)
{
	TraceServices::FAnalysisSessionEditScope _(Session);

	const UE::Trace::IAnalyzer::FEventData& EventData = Context.EventData;
	const double     Timestamp  = Context.EventTime.AsSeconds(EventData.GetValue<uint64>("Cycle"));
	const uint64     InstanceId = EventData.GetValue<uint64>("InstanceId");
	const uint32     EntityIndex  = EventData.GetValue<uint32>("EntityIndex");
	const uint32     EntitySerial = EventData.GetValue<uint32>("EntitySerial");

	++EventCount;
	if (EventCount % 1000 == 0)
	{
		UE_LOGF(LogArcMABAnalyzer, Verbose,"[ArcMAB Analyzer] Events processed: %llu", EventCount);
	}

	switch (RouteId)
	{
	case RouteId_AttributeChanged:
		{
			FArcMABAttributeChangedRecord Record;
			Record.Time         = Timestamp;
			Record.EntityIndex  = EntityIndex;
			Record.EntitySerial = EntitySerial;
			Record.OldValue     = EventData.GetValue<float>("OldValue");
			Record.NewValue     = EventData.GetValue<float>("NewValue");
			FString AttrName;
			EventData.GetString("AttributeName", AttrName);
			Record.AttributeName = MoveTemp(AttrName);

			UE_LOGF(LogArcMABAnalyzer, Verbose,"[ArcMAB Analyzer] AttributeChanged: InstanceId=%llu Entity=[%u:%u] Attr='%ls' %.2f->%.2f",
				InstanceId, EntityIndex, EntitySerial, *Record.AttributeName, Record.OldValue, Record.NewValue);

			Provider.AppendAttributeChanged(InstanceId, MoveTemp(Record));
			break;
		}

	case RouteId_AbilityActivated:
		{
			FArcMABAbilityActivatedRecord Record;
			Record.Time         = Timestamp;
			Record.EntityIndex  = EntityIndex;
			Record.EntitySerial = EntitySerial;
			Record.AbilityObjId = EventData.GetValue<uint64>("AbilityObjId");
			FString AbilityName;
			EventData.GetString("AbilityName", AbilityName);
			Record.AbilityName = MoveTemp(AbilityName);

			UE_LOGF(LogArcMABAnalyzer, Verbose,"[ArcMAB Analyzer] AbilityActivated: InstanceId=%llu Entity=[%u:%u] Ability='%ls'",
				InstanceId, EntityIndex, EntitySerial, *Record.AbilityName);

			Provider.AppendAbilityActivated(InstanceId, MoveTemp(Record));
			break;
		}

	case RouteId_EffectApplied:
		{
			FArcMABEffectAppliedRecord Record;
			Record.Time         = Timestamp;
			Record.EntityIndex  = EntityIndex;
			Record.EntitySerial = EntitySerial;
			Record.EffectObjId  = EventData.GetValue<uint64>("EffectObjId");
			FString EffectName;
			EventData.GetString("EffectName", EffectName);
			Record.EffectName = MoveTemp(EffectName);

			UE_LOGF(LogArcMABAnalyzer, Verbose,"[ArcMAB Analyzer] EffectApplied: InstanceId=%llu Entity=[%u:%u] Effect='%ls'",
				InstanceId, EntityIndex, EntitySerial, *Record.EffectName);

			Provider.AppendEffectApplied(InstanceId, MoveTemp(Record));
			break;
		}

	case RouteId_EffectExecuted:
		{
			FArcMABEffectExecutedRecord Record;
			Record.Time         = Timestamp;
			Record.EntityIndex  = EntityIndex;
			Record.EntitySerial = EntitySerial;
			Record.EffectObjId  = EventData.GetValue<uint64>("EffectObjId");
			FString EffectName;
			EventData.GetString("EffectName", EffectName);
			Record.EffectName = MoveTemp(EffectName);

			UE_LOGF(LogArcMABAnalyzer, Verbose,"[ArcMAB Analyzer] EffectExecuted: InstanceId=%llu Entity=[%u:%u] Effect='%ls'",
				InstanceId, EntityIndex, EntitySerial, *Record.EffectName);

			Provider.AppendEffectExecuted(InstanceId, MoveTemp(Record));
			break;
		}

	case RouteId_TagCountChanged:
		{
			FArcMABTagCountChangedRecord Record;
			Record.Time         = Timestamp;
			Record.EntityIndex  = EntityIndex;
			Record.EntitySerial = EntitySerial;
			Record.OldCount     = EventData.GetValue<int32>("OldCount");
			Record.NewCount     = EventData.GetValue<int32>("NewCount");
			FString TagName;
			EventData.GetString("TagName", TagName);
			Record.TagName = MoveTemp(TagName);

			UE_LOGF(LogArcMABAnalyzer, Verbose,"[ArcMAB Analyzer] TagCountChanged: InstanceId=%llu Entity=[%u:%u] Tag='%ls' %d->%d",
				InstanceId, EntityIndex, EntitySerial, *Record.TagName, Record.OldCount, Record.NewCount);

			Provider.AppendTagCountChanged(InstanceId, MoveTemp(Record));
			break;
		}

	case RouteId_AbilityEnded:
		{
			FArcMABAbilityEndedRecord Record;
			Record.Time         = Timestamp;
			Record.EntityIndex  = EntityIndex;
			Record.EntitySerial = EntitySerial;
			Record.AbilityObjId = EventData.GetValue<uint64>("AbilityObjId");
			Record.Reason       = EventData.GetValue<uint8>("Reason");
			Record.InstigatorIndex  = EventData.GetValue<uint32>("InstigatorIndex");
			Record.InstigatorSerial = EventData.GetValue<uint32>("InstigatorSerial");
			FString AbilityName;
			EventData.GetString("AbilityName", AbilityName);
			Record.AbilityName = MoveTemp(AbilityName);

			UE_LOGF(LogArcMABAnalyzer, Verbose,"[ArcMAB Analyzer] AbilityEnded: InstanceId=%llu Entity=[%u:%u] Ability='%ls' Reason=%ls Instigator=[%u:%u]",
				InstanceId, EntityIndex, EntitySerial, *Record.AbilityName,
				ArcMassAbilityTraceAnalyzerInternal::AbilityEndReasonLabel(Record.Reason),
				Record.InstigatorIndex, Record.InstigatorSerial);

			Provider.AppendAbilityEnded(InstanceId, MoveTemp(Record));
			break;
		}

	case RouteId_CostFailed:
		{
			FArcMABCostFailedRecord Record;
			Record.Time         = Timestamp;
			Record.EntityIndex  = EntityIndex;
			Record.EntitySerial = EntitySerial;
			Record.AbilityObjId = EventData.GetValue<uint64>("AbilityObjId");
			Record.Reason       = EventData.GetValue<uint8>("Reason");
			Record.MissingAmount = EventData.GetValue<float>("MissingAmount");
			FString AbilityName;
			EventData.GetString("AbilityName", AbilityName);
			Record.AbilityName = MoveTemp(AbilityName);

			UE_LOGF(LogArcMABAnalyzer, Verbose,"[ArcMAB Analyzer] CostFailed: InstanceId=%llu Entity=[%u:%u] Ability='%ls' Reason=%u Missing=%.4f",
				InstanceId, EntityIndex, EntitySerial, *Record.AbilityName, Record.Reason, Record.MissingAmount);

			Provider.AppendCostFailed(InstanceId, MoveTemp(Record));
			break;
		}

	case RouteId_CooldownFailed:
		{
			FArcMABCooldownFailedRecord Record;
			Record.Time         = Timestamp;
			Record.EntityIndex  = EntityIndex;
			Record.EntitySerial = EntitySerial;
			Record.AbilityObjId = EventData.GetValue<uint64>("AbilityObjId");
			Record.RemainingMs  = EventData.GetValue<float>("RemainingMs");
			FString AbilityName;
			EventData.GetString("AbilityName", AbilityName);
			Record.AbilityName = MoveTemp(AbilityName);

			UE_LOGF(LogArcMABAnalyzer, Verbose,"[ArcMAB Analyzer] CooldownFailed: InstanceId=%llu Entity=[%u:%u] Ability='%ls' RemainingMs=%.1f",
				InstanceId, EntityIndex, EntitySerial, *Record.AbilityName, Record.RemainingMs);

			Provider.AppendCooldownFailed(InstanceId, MoveTemp(Record));
			break;
		}

	case RouteId_SignalRaised:
		{
			FArcMABSignalRaisedRecord Record;
			Record.Time         = Timestamp;
			Record.EntityIndex  = EntityIndex;
			Record.EntitySerial = EntitySerial;
			FString SignalName;
			EventData.GetString("SignalName", SignalName);
			Record.SignalName = MoveTemp(SignalName);

			UE_LOGF(LogArcMABAnalyzer, Verbose,"[ArcMAB Analyzer] SignalRaised: InstanceId=%llu Entity=[%u:%u] Signal='%ls'",
				InstanceId, EntityIndex, EntitySerial, *Record.SignalName);

			Provider.AppendSignalRaised(InstanceId, MoveTemp(Record));
			break;
		}

	default:
		UE_LOGF(LogArcMABAnalyzer, Warning,"[ArcMAB Analyzer] Unknown RouteId=%u — event ignored", RouteId);
		break;
	}

	return true;
}
