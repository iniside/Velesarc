// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSRewindDebuggerTrack.h"
#include "ArcTQSTraceProvider.h"
#include "ArcTQSTraceTypes.h"
#include "TargetQuery/ArcTQSTypes.h"  // for FArcTQSQueryContext::StaticStruct()
#include "IRewindDebugger.h"
#include "Widgets/Text/SMultiLineEditableText.h"
#include "Widgets/Layout/SScrollBox.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcTQSTrack, Log, All);

// --- Track Creator ---

FName FArcTQSRewindDebuggerTrackCreator::GetTargetTypeNameInternal() const
{
	const FName TypeName = FArcTQSQueryContext::StaticStruct()->GetFName();
	UE_LOG(LogArcTQSTrack, Log, TEXT("[TQS Track] GetTargetTypeName: returning '%s'"), *TypeName.ToString());
	return TypeName;
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
		UE_LOG(LogArcTQSTrack, Warning, TEXT("[TQS Track] HasDebugInfo: No debugger or session for ObjectId=%llu"), InObjectId.GetMainId());
		return false;
	}

	const FArcTQSTraceProvider* Provider = Debugger->GetAnalysisSession()->ReadProvider<FArcTQSTraceProvider>(
		FArcTQSTraceProvider::ProviderName);
	if (!Provider)
	{
		UE_LOG(LogArcTQSTrack, Warning, TEXT("[TQS Track] HasDebugInfo: Provider is null for ObjectId=%llu"), InObjectId.GetMainId());
		return false;
	}

	const bool bHasData = Provider->HasDataForInstance(InObjectId.GetMainId());
	UE_LOG(LogArcTQSTrack, Log, TEXT("[TQS Track] HasDebugInfo: ObjectId=%llu HasData=%d TotalRecords=%d"),
		InObjectId.GetMainId(), bHasData, Provider->HasData());
	return bHasData;
}

TSharedPtr<RewindDebugger::FRewindDebuggerTrack> FArcTQSRewindDebuggerTrackCreator::CreateTrackInternal(
	const RewindDebugger::FObjectId& InObjectId) const
{
	UE_LOG(LogArcTQSTrack, Log, TEXT("[TQS Track] CreateTrackInternal: Creating track for ObjectId=%llu"), InObjectId.GetMainId());
	return MakeShared<FArcTQSRewindDebuggerTrack>(InObjectId);
}

// --- Track ---

// Lerp from red (0.0) through yellow (0.5) to green (1.0)
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
	UE_LOG(LogArcTQSTrack, Log, TEXT("[TQS Track] Created track for ObjectId=%llu"), InObjectId.GetMainId());
}

FName FArcTQSRewindDebuggerTrack::GetNameInternal() const
{
	static const FName Name(TEXT("ArcTQSQueryTrack"));
	return Name;
}

FText FArcTQSRewindDebuggerTrack::GetDisplayNameInternal() const
{
	return NSLOCTEXT("ArcTQS", "TrackDisplayName", "TQS Query");
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
	return CachedQuery != nullptr;
}

TSharedPtr<SEventTimelineView::FTimelineEventData> FArcTQSRewindDebuggerTrack::GetEventData() const
{
	if (!EventData.IsValid())
	{
		EventData = MakeShared<SEventTimelineView::FTimelineEventData>();
	}

	EventUpdateRequested++;

	return EventData;
}

bool FArcTQSRewindDebuggerTrack::UpdateInternal()
{
	const IRewindDebugger* Debugger = IRewindDebugger::Instance();
	if (!Debugger || !Debugger->GetAnalysisSession())
	{
		CachedQuery = nullptr;
		return false;
	}

	const FArcTQSTraceProvider* Provider = Debugger->GetAnalysisSession()->ReadProvider<FArcTQSTraceProvider>(
		FArcTQSTraceProvider::ProviderName);
	if (!Provider)
	{
		CachedQuery = nullptr;
		return false;
	}

	// Update CachedQuery — this is the record for the instance we represent
	const FArcTQSTraceQueryRecord* PrevQuery = CachedQuery;
	CachedQuery = Provider->GetQueryByInstanceId(ObjectId.GetMainId());

	if (CachedQuery != PrevQuery)
	{
		UE_LOG(LogArcTQSTrack, Log, TEXT("[TQS Track] UpdateInternal: ObjectId=%llu CachedQuery=%s Completed=%s"),
			ObjectId.GetMainId(),
			CachedQuery ? TEXT("Valid") : TEXT("null"),
			(CachedQuery && CachedQuery->bCompleted) ? TEXT("true") : TEXT("false"));
	}

	// Rebuild timeline event data periodically (similar to engine pattern — lazy update)
	if (EventUpdateRequested > 0)
	{
		EventUpdateRequested = 0;
		EventData->Points.SetNum(0, EAllowShrinking::No);
		EventData->Windows.SetNum(0, EAllowShrinking::No);

		if (CachedQuery && CachedQuery->bCompleted)
		{
			// Show the query as a colored window spanning its execution duration
			SEventTimelineView::FTimelineEventData::EventWindow& Window = EventData->Windows.AddDefaulted_GetRef();
			Window.TimeStart = CachedQuery->Timestamp;
			Window.TimeEnd = CachedQuery->EndTimestamp;
			Window.Color = StatusToTimelineColor(CachedQuery->Status);
			Window.Description = FText::FromString(CachedQuery->GeneratorName);

			UE_LOG(LogArcTQSTrack, Log, TEXT("[TQS Track] Timeline window: [%.4f..%.4f] Generator=%s"),
				Window.TimeStart, Window.TimeEnd, *CachedQuery->GeneratorName);
		}
	}

	return false; // No children
}

TSharedPtr<SWidget> FArcTQSRewindDebuggerTrack::GetTimelineViewInternal()
{
	// Use GetCurrentTraceRange() — our EventData timestamps are in profile/trace time
	// (from FEventTime::AsSeconds(Cycle)), matching GetCurrentTraceRange() coordinates.
	return SNew(SEventTimelineView)
		.ViewRange_Lambda([]() { return IRewindDebugger::Instance()->GetCurrentTraceRange(); })
		.EventData_Raw(this, &FArcTQSRewindDebuggerTrack::GetEventData);
}

TSharedPtr<SWidget> FArcTQSRewindDebuggerTrack::GetDetailsViewInternal()
{
	auto BuildQueryText = [this]() -> FText
	{
		if (!CachedQuery || !CachedQuery->bCompleted)
		{
			return FText::FromString(TEXT("No TQS query data at current time."));
		}

		const FArcTQSTraceQueryRecord& Q = *CachedQuery;
		FString Text;

		// Header
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

		Text += FString::Printf(TEXT("Query #%d  |  %s  |  %s (%d items)  |  %.2fms\n\n"),
			Q.QueryId, StatusStr, *Q.GeneratorName, Q.GeneratedItemCount, Q.ExecutionTimeMs);

		// Steps
		for (const FArcTQSTraceStepRecord& Step : Q.Steps)
		{
			const TCHAR* KindStr = Step.StepKind == 0 ? TEXT("Filter") : TEXT("Score");

			if (Step.StepKind == 0)
			{
				const int32 Filtered = Q.GeneratedItemCount - Step.ValidRemaining;
				Text += FString::Printf(TEXT("Step %d: %s [%s] \u2014 %d passed, %d filtered\n"),
					Step.StepIndex, *Step.StepName, KindStr, Step.ValidRemaining, Filtered);
			}
			else
			{
				float Best = 0.0f, Worst = 1.0f, Sum = 0.0f;
				int32 Count = 0;
				for (const FArcTQSTraceItemSnapshot& Snap : Step.ItemSnapshots)
				{
					if (Snap.bValid)
					{
						Best = FMath::Max(Best, Snap.Score);
						Worst = FMath::Min(Worst, Snap.Score);
						Sum += Snap.Score;
						++Count;
					}
				}
				const float Mean = Count > 0 ? Sum / Count : 0.0f;

				Text += FString::Printf(TEXT("Step %d: %s [%s, w=%.2f] \u2014 Best: %.3f  Worst: %.3f  Mean: %.3f\n"),
					Step.StepIndex, *Step.StepName, KindStr, Step.Weight, Best, Worst, Mean);
			}
		}

		// Results
		Text += TEXT("\n--- Results ---\n");
		for (int32 i = 0; i < Q.ResultIndices.Num(); ++i)
		{
			const int32 Idx = Q.ResultIndices[i];
			if (Q.FinalItems.IsValidIndex(Idx))
			{
				const FArcTQSTraceItemRecord& Item = Q.FinalItems[Idx];
				Text += FString::Printf(TEXT("Winner #%d: Score=%.4f  Loc=(%.0f, %.0f, %.0f)\n"),
					i, Item.Score, Item.Location.X, Item.Location.Y, Item.Location.Z);
			}
		}

		return FText::FromString(Text);
	};

	return SNew(SScrollBox)
		+ SScrollBox::Slot()
		[
			SNew(SMultiLineEditableText)
			.IsReadOnly(true)
			.AutoWrapText(true)
			.Text_Lambda(BuildQueryText)
		];
}
