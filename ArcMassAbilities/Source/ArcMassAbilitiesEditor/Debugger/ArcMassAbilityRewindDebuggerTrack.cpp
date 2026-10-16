// Copyright Lukasz Baran. All Rights Reserved.

#include "Debugger/ArcMassAbilityRewindDebuggerTrack.h"
#include "Debugger/ArcMassAbilityTraceProvider.h"
#include "Debugger/ArcMassAbilityTraceTypes.h"
#include "Debugger/ArcMassAbilityEntityContext.h"
#include "Events/ArcAbilityEventPayloads.h"
#include "IRewindDebugger.h"
#include "Widgets/Text/SMultiLineEditableText.h"
#include "Widgets/Layout/SScrollBox.h"

DEFINE_LOG_CATEGORY(LogArcMABTrack);

// ---------------------------------------------------------------------------
// Track Creator
// ---------------------------------------------------------------------------

FName FArcMassAbilityRewindDebuggerTrackCreator::GetTargetTypeNameInternal() const
{
	return FArcMassAbilityEntityContext::StaticStruct()->GetFName();
}

FName FArcMassAbilityRewindDebuggerTrackCreator::GetNameInternal() const
{
	static const FName Name(TEXT("ArcMassAbilityTrack"));
	return Name;
}

void FArcMassAbilityRewindDebuggerTrackCreator::GetTrackTypesInternal(
	TArray<RewindDebugger::FRewindDebuggerTrackType>& Types) const
{
	Types.Add({FName(TEXT("ArcMassAbilityTrack")), NSLOCTEXT("ArcMAB", "TrackName", "Mass Ability Events")});
}

bool FArcMassAbilityRewindDebuggerTrackCreator::HasDebugInfoInternal(
	const RewindDebugger::FObjectId& InObjectId) const
{
	const IRewindDebugger* Debugger = IRewindDebugger::Instance();
	if (!Debugger || !Debugger->GetAnalysisSession())
	{
		return false;
	}

	const FArcMassAbilityTraceProvider* Provider =
		Debugger->GetAnalysisSession()->ReadProvider<FArcMassAbilityTraceProvider>(
			FArcMassAbilityTraceProvider::ProviderName);

	if (!Provider)
	{
		return false;
	}

	return Provider->GetRecordsForInstance(InObjectId.GetMainId()) != nullptr;
}

TSharedPtr<RewindDebugger::FRewindDebuggerTrack>
FArcMassAbilityRewindDebuggerTrackCreator::CreateTrackInternal(
	const RewindDebugger::FObjectId& InObjectId) const
{
	return MakeShared<FArcMassAbilityRewindDebuggerTrack>(InObjectId);
}

// ---------------------------------------------------------------------------
// Track
// ---------------------------------------------------------------------------

FArcMassAbilityRewindDebuggerTrack::FArcMassAbilityRewindDebuggerTrack(
	const RewindDebugger::FObjectId& InObjectId)
	: ObjectId(InObjectId)
{
	EventData = MakeShared<SEventTimelineView::FTimelineEventData>();
}

FName FArcMassAbilityRewindDebuggerTrack::GetNameInternal() const
{
	static const FName Name(TEXT("ArcMassAbilityTrack"));
	return Name;
}

FText FArcMassAbilityRewindDebuggerTrack::GetDisplayNameInternal() const
{
	if (CachedRecord)
	{
		return FText::FromString(
			FString::Printf(TEXT("Entity [%u:%u]"), CachedRecord->EntityIndex, CachedRecord->EntitySerial));
	}
	return NSLOCTEXT("ArcMAB", "TrackDisplayName", "Mass Ability Entity");
}

FSlateIcon FArcMassAbilityRewindDebuggerTrack::GetIconInternal()
{
	return FSlateIcon("EditorStyle", "ClassIcon.Default");
}

uint64 FArcMassAbilityRewindDebuggerTrack::GetObjectIdInternal() const
{
	return ObjectId.GetMainId();
}

bool FArcMassAbilityRewindDebuggerTrack::HasDebugDataInternal() const
{
	if (!CachedRecord)
	{
		return false;
	}
	return CachedRecord->AbilityActivated.Num() > 0
		|| CachedRecord->AbilityEnded.Num()      > 0
		|| CachedRecord->EffectApplied.Num()     > 0
		|| CachedRecord->EffectExecuted.Num()    > 0
		|| CachedRecord->AttributeChanged.Num()  > 0
		|| CachedRecord->TagCountChanged.Num()   > 0
		|| CachedRecord->CostFailed.Num()        > 0
		|| CachedRecord->CooldownFailed.Num()    > 0
		|| CachedRecord->SignalRaised.Num()      > 0;
}

// ---------------------------------------------------------------------------
// Color palette per event type
// ---------------------------------------------------------------------------

namespace ArcMABTrackColors
{
	static const FLinearColor AbilityActivated   = FLinearColor(0.30f, 0.90f, 0.40f); // green
	static const FLinearColor AbilityEndedOK     = FLinearColor(0.20f, 0.65f, 0.30f); // dark green
	static const FLinearColor AbilityEndedFail   = FLinearColor(0.90f, 0.40f, 0.20f); // orange-red
	static const FLinearColor EffectApplied      = FLinearColor(0.20f, 0.80f, 0.90f); // cyan
	static const FLinearColor EffectExecuted     = FLinearColor(0.30f, 0.55f, 0.95f); // blue
	static const FLinearColor AttributeChanged   = FLinearColor(0.95f, 0.30f, 0.85f); // magenta
	static const FLinearColor TagCountChanged    = FLinearColor(0.95f, 0.65f, 0.20f); // orange
	static const FLinearColor CostFailed         = FLinearColor(1.00f, 0.20f, 0.20f); // bright red
	static const FLinearColor CooldownFailed     = FLinearColor(0.85f, 0.10f, 0.10f); // dark red
	static const FLinearColor SignalRaised       = FLinearColor(0.60f, 0.60f, 0.60f); // gray
	static const FLinearColor AbilityWindow      = FLinearColor(0.30f, 0.80f, 0.40f, 0.35f); // translucent green
}

// ---------------------------------------------------------------------------
// Trace-time -> recording-time linear remap.  All engine timeline widgets
// operate in recording time; our provider stores trace time.  Mapping uses
// the current frame's IRewindDebugger ranges (see TQS for the same pattern).
// ---------------------------------------------------------------------------

namespace ArcMABTrackInternal
{
	struct FTimeRemap
	{
		double TraceLo = 0.0;
		double TraceSize = 0.0;
		double ViewLo = 0.0;
		double ViewSize = 0.0;

		double operator()(double TraceTime) const
		{
			if (TraceSize <= 0.0)
			{
				return 0.0;
			}
			return ViewLo + (TraceTime - TraceLo) * (ViewSize / TraceSize);
		}
	};

	static FTimeRemap MakeRemap(const IRewindDebugger& Debugger)
	{
		FTimeRemap Remap;
		const TRange<double>& TraceRange = Debugger.GetCurrentTraceRange();
		const TRange<double>& ViewRange  = Debugger.GetCurrentViewRange();
		Remap.TraceLo   = TraceRange.GetLowerBoundValue();
		Remap.TraceSize = TraceRange.Size<double>();
		Remap.ViewLo    = ViewRange.GetLowerBoundValue();
		Remap.ViewSize  = ViewRange.Size<double>();
		return Remap;
	}

	static void AddPoint(
		SEventTimelineView::FTimelineEventData& Out,
		const FTimeRemap& Remap,
		double TraceTime,
		const FLinearColor& Color,
		FText Description)
	{
		SEventTimelineView::FTimelineEventData::FEventPoint& Point = Out.EventPoints.AddDefaulted_GetRef();
		Point.Time        = Remap(TraceTime);
		Point.Color       = Color;
		Point.Description = MoveTemp(Description);
	}
}

bool FArcMassAbilityRewindDebuggerTrack::UpdateInternal()
{
	const IRewindDebugger* Debugger = IRewindDebugger::Instance();
	if (!Debugger || !Debugger->GetAnalysisSession())
	{
		CachedRecord = nullptr;
		return false;
	}

	const FArcMassAbilityTraceProvider* Provider =
		Debugger->GetAnalysisSession()->ReadProvider<FArcMassAbilityTraceProvider>(
			FArcMassAbilityTraceProvider::ProviderName);
	if (!Provider)
	{
		CachedRecord = nullptr;
		return false;
	}

	CachedRecord = Provider->GetRecordsForInstance(ObjectId.GetMainId());

	UE_LOGF(LogArcMABTrack, Verbose,"[ArcMAB Track] UpdateInternal InstanceId=%llu Record=%ls",
		ObjectId.GetMainId(), CachedRecord ? TEXT("found") : TEXT("null"));

	// Rebuild timeline geometry every update.  Cheap — small N per entity.
	const int32 PrevPoints  = EventData->EventPoints.Num();
	const int32 PrevWindows = EventData->EventWindows.Num();

	EventData->EventPoints.SetNum(0,  EAllowShrinking::No);
	EventData->EventWindows.SetNum(0, EAllowShrinking::No);

	if (CachedRecord)
	{
		const ArcMABTrackInternal::FTimeRemap Remap = ArcMABTrackInternal::MakeRemap(*Debugger);
		const FArcMABEntityRecord& R = *CachedRecord;

		// Points: every event type renders as a coloured point marker.
		for (const FArcMABAbilityActivatedRecord& Ev : R.AbilityActivated)
		{
			ArcMABTrackInternal::AddPoint(*EventData, Remap, Ev.Time,
				ArcMABTrackColors::AbilityActivated,
				FText::FromString(FString::Printf(TEXT("Activate %s"), *Ev.AbilityName)));
		}
		for (const FArcMABAbilityEndedRecord& Ev : R.AbilityEnded)
		{
			const FLinearColor& Color = (Ev.Reason == static_cast<uint8>(EArcAbilityEndReason::Completed))
				? ArcMABTrackColors::AbilityEndedOK
				: ArcMABTrackColors::AbilityEndedFail;
			const TCHAR* ReasonStr =
				(Ev.Reason == static_cast<uint8>(EArcAbilityEndReason::Completed)) ? TEXT("OK")
				: (Ev.Reason == static_cast<uint8>(EArcAbilityEndReason::Cancelled)) ? TEXT("Cancel")
				: (Ev.Reason == static_cast<uint8>(EArcAbilityEndReason::Interrupted)) ? TEXT("Interrupt")
				: TEXT("Fail");
			const FString Label = Ev.Reason == static_cast<uint8>(EArcAbilityEndReason::Interrupted)
				? FString::Printf(TEXT("End %s (%s by [%u:%u])"), *Ev.AbilityName, ReasonStr, Ev.InstigatorIndex, Ev.InstigatorSerial)
				: FString::Printf(TEXT("End %s (%s)"), *Ev.AbilityName, ReasonStr);
			ArcMABTrackInternal::AddPoint(*EventData, Remap, Ev.Time, Color, FText::FromString(Label));
		}
		for (const FArcMABEffectAppliedRecord& Ev : R.EffectApplied)
		{
			ArcMABTrackInternal::AddPoint(*EventData, Remap, Ev.Time,
				ArcMABTrackColors::EffectApplied,
				FText::FromString(FString::Printf(TEXT("FX+ %s"), *Ev.EffectName)));
		}
		for (const FArcMABEffectExecutedRecord& Ev : R.EffectExecuted)
		{
			ArcMABTrackInternal::AddPoint(*EventData, Remap, Ev.Time,
				ArcMABTrackColors::EffectExecuted,
				FText::FromString(FString::Printf(TEXT("FX* %s"), *Ev.EffectName)));
		}
		for (const FArcMABAttributeChangedRecord& Ev : R.AttributeChanged)
		{
			ArcMABTrackInternal::AddPoint(*EventData, Remap, Ev.Time,
				ArcMABTrackColors::AttributeChanged,
				FText::FromString(FString::Printf(TEXT("%s %.2f→%.2f"), *Ev.AttributeName, Ev.OldValue, Ev.NewValue)));
		}
		for (const FArcMABTagCountChangedRecord& Ev : R.TagCountChanged)
		{
			ArcMABTrackInternal::AddPoint(*EventData, Remap, Ev.Time,
				ArcMABTrackColors::TagCountChanged,
				FText::FromString(FString::Printf(TEXT("%s %d→%d"), *Ev.TagName, Ev.OldCount, Ev.NewCount)));
		}
		for (const FArcMABCostFailedRecord& Ev : R.CostFailed)
		{
			ArcMABTrackInternal::AddPoint(*EventData, Remap, Ev.Time,
				ArcMABTrackColors::CostFailed,
				FText::FromString(FString::Printf(TEXT("Cost! %s"), *Ev.AbilityName)));
		}
		for (const FArcMABCooldownFailedRecord& Ev : R.CooldownFailed)
		{
			ArcMABTrackInternal::AddPoint(*EventData, Remap, Ev.Time,
				ArcMABTrackColors::CooldownFailed,
				FText::FromString(FString::Printf(TEXT("CD! %s"), *Ev.AbilityName)));
		}
		for (const FArcMABSignalRaisedRecord& Ev : R.SignalRaised)
		{
			ArcMABTrackInternal::AddPoint(*EventData, Remap, Ev.Time,
				ArcMABTrackColors::SignalRaised,
				FText::FromString(FString::Printf(TEXT("Sig %s"), *Ev.SignalName)));
		}

		// Windows: pair each AbilityActivated with the next AbilityEnded carrying
		// the same AbilityObjId (or matching name as fallback).  Unmatched
		// activations get an open-ended window to the latest record timestamp.
		double LatestTraceTime = R.AbilityEnded.Num() > 0 ? R.AbilityEnded.Last().Time : 0.0;
		for (const FArcMABAbilityActivatedRecord& Act : R.AbilityActivated)
		{
			if (Act.Time > LatestTraceTime) { LatestTraceTime = Act.Time; }
		}

		for (const FArcMABAbilityActivatedRecord& Act : R.AbilityActivated)
		{
			double EndTime = LatestTraceTime;
			for (const FArcMABAbilityEndedRecord& End : R.AbilityEnded)
			{
				if (End.Time >= Act.Time
					&& (End.AbilityObjId == Act.AbilityObjId || End.AbilityName == Act.AbilityName))
				{
					EndTime = End.Time;
					break;
				}
			}

			SEventTimelineView::FTimelineEventData::FEventWindow& W = EventData->EventWindows.AddDefaulted_GetRef();
			W.TimeStart   = Remap(Act.Time);
			W.TimeEnd     = Remap(EndTime);
			W.Color       = ArcMABTrackColors::AbilityWindow;
			W.Description = FText::FromString(Act.AbilityName);
		}
	}

	return (PrevPoints != EventData->EventPoints.Num() || PrevWindows != EventData->EventWindows.Num());
}

TSharedPtr<SWidget> FArcMassAbilityRewindDebuggerTrack::GetTimelineViewInternal()
{
	TWeakPtr<SEventTimelineView::FTimelineEventData> WeakEventData = EventData;
	return SNew(SEventTimelineView)
		.ViewRange_Lambda([]() { return IRewindDebugger::Instance()->GetCurrentViewRange(); })
		.EventData_Lambda([WeakEventData]() { return WeakEventData.Pin(); });
}

TSharedPtr<SWidget> FArcMassAbilityRewindDebuggerTrack::GetDetailsViewInternal()
{
	FText BodyText;
	if (!CachedRecord)
	{
		BodyText = FText::FromString(TEXT("No Mass Ability data for this entity."));
	}
	else
	{
		const FArcMABEntityRecord& R = *CachedRecord;
		FString Text;
		Text += FString::Printf(TEXT("Entity [%u:%u]  (InstanceId=%llu)\n\n"),
			R.EntityIndex, R.EntitySerial, R.InstanceId);

		Text += FString::Printf(TEXT("AbilityActivated : %d\n"), R.AbilityActivated.Num());
		for (const FArcMABAbilityActivatedRecord& Ev : R.AbilityActivated)
		{
			Text += FString::Printf(TEXT("  t=%.4f  %s\n"), Ev.Time, *Ev.AbilityName);
		}

		Text += FString::Printf(TEXT("AbilityEnded     : %d\n"), R.AbilityEnded.Num());
		for (const FArcMABAbilityEndedRecord& Ev : R.AbilityEnded)
		{
			Text += FString::Printf(TEXT("  t=%.4f  %s  reason=%u  instigator=[%u:%u]\n"), Ev.Time, *Ev.AbilityName, Ev.Reason, Ev.InstigatorIndex, Ev.InstigatorSerial);
		}

		Text += FString::Printf(TEXT("EffectApplied    : %d\n"), R.EffectApplied.Num());
		for (const FArcMABEffectAppliedRecord& Ev : R.EffectApplied)
		{
			Text += FString::Printf(TEXT("  t=%.4f  %s\n"), Ev.Time, *Ev.EffectName);
		}

		Text += FString::Printf(TEXT("EffectExecuted   : %d\n"), R.EffectExecuted.Num());
		for (const FArcMABEffectExecutedRecord& Ev : R.EffectExecuted)
		{
			Text += FString::Printf(TEXT("  t=%.4f  %s\n"), Ev.Time, *Ev.EffectName);
		}

		Text += FString::Printf(TEXT("AttributeChanged : %d\n"), R.AttributeChanged.Num());
		for (const FArcMABAttributeChangedRecord& Ev : R.AttributeChanged)
		{
			Text += FString::Printf(TEXT("  t=%.4f  %s  %.3f -> %.3f\n"),
				Ev.Time, *Ev.AttributeName, Ev.OldValue, Ev.NewValue);
		}

		Text += FString::Printf(TEXT("TagCountChanged  : %d\n"), R.TagCountChanged.Num());
		for (const FArcMABTagCountChangedRecord& Ev : R.TagCountChanged)
		{
			Text += FString::Printf(TEXT("  t=%.4f  %s  %d -> %d\n"),
				Ev.Time, *Ev.TagName, Ev.OldCount, Ev.NewCount);
		}

		Text += FString::Printf(TEXT("CostFailed       : %d\n"), R.CostFailed.Num());
		for (const FArcMABCostFailedRecord& Ev : R.CostFailed)
		{
			Text += FString::Printf(TEXT("  t=%.4f  %s  reason=%u  missing=%.4f\n"), Ev.Time, *Ev.AbilityName, Ev.Reason, Ev.MissingAmount);
		}

		Text += FString::Printf(TEXT("CooldownFailed   : %d\n"), R.CooldownFailed.Num());
		for (const FArcMABCooldownFailedRecord& Ev : R.CooldownFailed)
		{
			Text += FString::Printf(TEXT("  t=%.4f  %s  remaining=%.1fms\n"),
				Ev.Time, *Ev.AbilityName, Ev.RemainingMs);
		}

		Text += FString::Printf(TEXT("SignalRaised     : %d\n"), R.SignalRaised.Num());
		for (const FArcMABSignalRaisedRecord& Ev : R.SignalRaised)
		{
			Text += FString::Printf(TEXT("  t=%.4f  %s\n"), Ev.Time, *Ev.SignalName);
		}

		BodyText = FText::FromString(Text);
	}

	return SNew(SScrollBox)
		+ SScrollBox::Slot()
		[
			SNew(SMultiLineEditableText)
			.IsReadOnly(true)
			.AutoWrapText(false)
			.Text(BodyText)
		];
}
