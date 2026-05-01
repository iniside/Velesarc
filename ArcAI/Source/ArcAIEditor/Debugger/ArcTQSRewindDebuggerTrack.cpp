// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSRewindDebuggerTrack.h"
#include "ArcTQSTraceProvider.h"
#include "ArcTQSTraceTypes.h"
#include "ArcTQSTypes.h"  // for FArcTQSQueryContext::StaticStruct()
#include "IRewindDebugger.h"
#include "Widgets/Text/SMultiLineEditableText.h"
#include "Widgets/Layout/SScrollBox.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcTQSTrack, Log, All);

// --- Track Creator ---

FName FArcTQSRewindDebuggerTrackCreator::GetTargetTypeNameInternal() const
{
	return FArcTQSQueryContext::StaticStruct()->GetFName();
}

FName FArcTQSRewindDebuggerTrackCreator::GetNameInternal() const
{
	static const FName Name(TEXT("ArcTQSQueryTrack"));
	return Name;
}

void FArcTQSRewindDebuggerTrackCreator::GetTrackTypesInternal(
	TArray<RewindDebugger::FRewindDebuggerTrackType>& Types) const
{
	Types.Add({FName(TEXT("ArcTQSQueryTrack")), NSLOCTEXT("ArcTQS", "TrackName", "TQS Queries")});
}

bool FArcTQSRewindDebuggerTrackCreator::HasDebugInfoInternal(
	const RewindDebugger::FObjectId& InObjectId) const
{
	const IRewindDebugger* Debugger = IRewindDebugger::Instance();
	if (!Debugger || !Debugger->GetAnalysisSession())
	{
		return false;
	}

	const FArcTQSTraceProvider* Provider = Debugger->GetAnalysisSession()->ReadProvider<FArcTQSTraceProvider>(
		FArcTQSTraceProvider::ProviderName);

	return Provider && Provider->HasDataForInstance(InObjectId.GetMainId());
}

TSharedPtr<RewindDebugger::FRewindDebuggerTrack> FArcTQSRewindDebuggerTrackCreator::CreateTrackInternal(
	const RewindDebugger::FObjectId& InObjectId) const
{
	return MakeShared<FArcTQSRewindDebuggerTrack>(InObjectId);
}

// --- Track ---

static FLinearColor StatusToTimelineColor(uint8 Status)
{
	switch (static_cast<EArcTQSQueryStatus>(Status))
	{
	case EArcTQSQueryStatus::Completed: return FLinearColor::Green;
	case EArcTQSQueryStatus::Failed:    return FLinearColor(1.0f, 0.2f, 0.2f);
	case EArcTQSQueryStatus::Aborted:   return FLinearColor(0.6f, 0.6f, 0.6f);
	default:                            return FLinearColor(0.3f, 0.6f, 1.0f);
	}
}

FArcTQSRewindDebuggerTrack::FArcTQSRewindDebuggerTrack(const RewindDebugger::FObjectId& InObjectId)
	: ObjectId(InObjectId)
{
	EventData = MakeShared<SEventTimelineView::FTimelineEventData>();
}

FName FArcTQSRewindDebuggerTrack::GetNameInternal() const
{
	static const FName Name(TEXT("ArcTQSQueryTrack"));
	return Name;
}

FText FArcTQSRewindDebuggerTrack::GetDisplayNameInternal() const
{
	const IRewindDebugger* Debugger = IRewindDebugger::Instance();
	if (Debugger && Debugger->GetAnalysisSession())
	{
		const FArcTQSTraceProvider* Provider = Debugger->GetAnalysisSession()->ReadProvider<FArcTQSTraceProvider>(
			FArcTQSTraceProvider::ProviderName);
		if (Provider)
		{
			const FString& Name = Provider->GetInstanceDisplayName(ObjectId.GetMainId());
			if (!Name.IsEmpty())
			{
				return FText::FromString(Name);
			}
		}
	}
	return NSLOCTEXT("ArcTQS", "TrackDisplayName", "TQS Queries");
}

FSlateIcon FArcTQSRewindDebuggerTrack::GetIconInternal()
{
	return FSlateIcon("EditorStyle", "ClassIcon.Default");
}

uint64 FArcTQSRewindDebuggerTrack::GetObjectIdInternal() const
{
	return ObjectId.GetMainId();
}

bool FArcTQSRewindDebuggerTrack::HasDebugDataInternal() const
{
	return CachedQueries != nullptr && CachedQueries->Num() > 0;
}

bool FArcTQSRewindDebuggerTrack::UpdateInternal()
{
	const IRewindDebugger* Debugger = IRewindDebugger::Instance();
	if (!Debugger || !Debugger->GetAnalysisSession())
	{
		CachedQueries = nullptr;
		return false;
	}

	const FArcTQSTraceProvider* Provider = Debugger->GetAnalysisSession()->ReadProvider<FArcTQSTraceProvider>(
		FArcTQSTraceProvider::ProviderName);
	if (!Provider)
	{
		CachedQueries = nullptr;
		return false;
	}

	CachedQueries = Provider->GetQueriesForInstance(ObjectId.GetMainId());

	// Convert profiler-time timestamps to recording time using the linear mapping
	// between the two time bases (all engine tracks use recording time + GetCurrentViewRange).
	const TRange<double>& TraceRange = Debugger->GetCurrentTraceRange();
	const TRange<double>& ViewRange = Debugger->GetCurrentViewRange();
	const double TraceSize = TraceRange.Size<double>();
	const double ViewSize = ViewRange.Size<double>();

	auto ProfileToRecording = [&](double ProfileTime) -> double
	{
		if (TraceSize <= 0.0)
		{
			return 0.0;
		}
		return ViewRange.GetLowerBoundValue() + (ProfileTime - TraceRange.GetLowerBoundValue()) * (ViewSize / TraceSize);
	};

	const int32 PrevNumPoints = EventData->Points.Num();
	const int32 PrevNumWindows = EventData->Windows.Num();

	EventData->Points.SetNum(0, EAllowShrinking::No);
	EventData->Windows.SetNum(0, EAllowShrinking::No);

	if (CachedQueries)
	{
		for (const FArcTQSTraceQueryRecord& Q : *CachedQueries)
		{
			SEventTimelineView::FTimelineEventData::EventWindow& Window = EventData->Windows.AddDefaulted_GetRef();
			Window.TimeStart = ProfileToRecording(Q.Timestamp);
			Window.TimeEnd = ProfileToRecording(Q.EndTimestamp);
			Window.Color = StatusToTimelineColor(Q.Status);
			Window.Description = FText::FromString(FString::Printf(TEXT("#%d %s"), Q.QueryId, *Q.GeneratorName));
		}
	}

	return (PrevNumPoints != EventData->Points.Num() || PrevNumWindows != EventData->Windows.Num());
}

TSharedPtr<SWidget> FArcTQSRewindDebuggerTrack::GetTimelineViewInternal()
{
	TWeakPtr<SEventTimelineView::FTimelineEventData> WeakEventData = EventData;
	return SNew(SEventTimelineView)
		.ViewRange_Lambda([]() { return IRewindDebugger::Instance()->GetCurrentViewRange(); })
		.EventData_Lambda([WeakEventData]()
		{
			return WeakEventData.Pin();
		});
}

static void AppendQueryText(FString& Text, const FArcTQSTraceQueryRecord& Q)
{
	const TCHAR* StatusStr = TEXT("?");
	switch (static_cast<EArcTQSQueryStatus>(Q.Status))
	{
	case EArcTQSQueryStatus::Pending:    StatusStr = TEXT("Pending"); break;
	case EArcTQSQueryStatus::Generating: StatusStr = TEXT("Generating"); break;
	case EArcTQSQueryStatus::Processing: StatusStr = TEXT("Processing"); break;
	case EArcTQSQueryStatus::Selecting:  StatusStr = TEXT("Selecting"); break;
	case EArcTQSQueryStatus::Completed:  StatusStr = TEXT("Completed"); break;
	case EArcTQSQueryStatus::Failed:     StatusStr = TEXT("Failed"); break;
	case EArcTQSQueryStatus::Aborted:    StatusStr = TEXT("Aborted"); break;
	}

	Text += FString::Printf(TEXT("Query #%d  |  %s  |  %s (%d items)  |  %.2fms\n"),
		Q.QueryId, StatusStr, *Q.GeneratorName, Q.GeneratedItemCount, Q.ExecutionTimeMs);

	// Steps (definitions only)
	for (const FArcTQSTraceStepRecord& Step : Q.Steps)
	{
		const TCHAR* KindStr = Step.StepKind == 0 ? TEXT("Filter") : TEXT("Score");
		if (Step.StepKind == 1)
		{
			Text += FString::Printf(TEXT("  Step %d: %s [%s, w=%.2f]\n"),
				Step.StepIndex, *Step.StepName, KindStr, Step.Weight);
		}
		else
		{
			Text += FString::Printf(TEXT("  Step %d: %s [%s]\n"),
				Step.StepIndex, *Step.StepName, KindStr);
		}
	}

	// Final items summary
	int32 ValidCount = 0;
	for (const FArcTQSTraceItemRecord& Item : Q.FinalItems)
	{
		if (Item.bValid) ++ValidCount;
	}
	Text += FString::Printf(TEXT("  Items: %d total, %d valid\n"), Q.FinalItems.Num(), ValidCount);

	// Results
	for (int32 i = 0; i < Q.ResultIndices.Num(); ++i)
	{
		const int32 Idx = Q.ResultIndices[i];
		if (Q.FinalItems.IsValidIndex(Idx))
		{
			const FArcTQSTraceItemRecord& Item = Q.FinalItems[Idx];
			Text += FString::Printf(TEXT("  Result #%d: Score=%.4f  Loc=(%.0f, %.0f, %.0f)\n"),
				i, Item.Score, Item.Location.X, Item.Location.Y, Item.Location.Z);
		}
	}
}

TSharedPtr<SWidget> FArcTQSRewindDebuggerTrack::GetDetailsViewInternal()
{
	auto BuildDetailsText = [this]() -> FText
	{
		if (!CachedQueries || CachedQueries->Num() == 0)
		{
			return FText::FromString(TEXT("No TQS query data."));
		}

		FString Text;
		Text += FString::Printf(TEXT("%d queries recorded\n\n"), CachedQueries->Num());

		for (const FArcTQSTraceQueryRecord& Q : *CachedQueries)
		{
			AppendQueryText(Text, Q);
			Text += TEXT("\n");
		}

		return FText::FromString(Text);
	};

	return SNew(SScrollBox)
		+ SScrollBox::Slot()
		[
			SNew(SMultiLineEditableText)
			.IsReadOnly(true)
			.AutoWrapText(true)
			.Text_Lambda(BuildDetailsText)
		];
}
