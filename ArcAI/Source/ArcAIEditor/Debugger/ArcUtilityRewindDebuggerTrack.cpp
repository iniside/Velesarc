// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcUtilityRewindDebuggerTrack.h"
#include "ArcUtilityTraceProvider.h"
#include "ArcUtilityTraceTypes.h"
#include "UtilityAI/ArcUtilityTypes.h"
#include "IRewindDebugger.h"
#include "Widgets/Text/SMultiLineEditableText.h"
#include "Widgets/Layout/SScrollBox.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcUtilityTrack, Log, All);

// --- Helper Strings ---

static const TCHAR* GetStatusString(uint8 Status)
{
	switch (static_cast<EArcUtilityScoringStatus>(Status))
	{
	case EArcUtilityScoringStatus::Pending:    return TEXT("Pending");
	case EArcUtilityScoringStatus::Processing: return TEXT("Processing");
	case EArcUtilityScoringStatus::Selecting:  return TEXT("Selecting");
	case EArcUtilityScoringStatus::Completed:  return TEXT("Completed");
	case EArcUtilityScoringStatus::Failed:     return TEXT("Failed");
	case EArcUtilityScoringStatus::Aborted:    return TEXT("Aborted");
	default:                                   return TEXT("?");
	}
}

static const TCHAR* GetSelectionModeString(uint8 Mode)
{
	switch (static_cast<EArcUtilitySelectionMode>(Mode))
	{
	case EArcUtilitySelectionMode::HighestScore:         return TEXT("HighestScore");
	case EArcUtilitySelectionMode::RandomFromTopPercent: return TEXT("RandomFromTopPercent");
	case EArcUtilitySelectionMode::WeightedRandom:       return TEXT("WeightedRandom");
	default:                                             return TEXT("?");
	}
}

static const TCHAR* GetTargetTypeString(uint8 Type)
{
	switch (static_cast<EArcUtilityTargetType>(Type))
	{
	case EArcUtilityTargetType::None:     return TEXT("None");
	case EArcUtilityTargetType::Actor:    return TEXT("Actor");
	case EArcUtilityTargetType::Entity:   return TEXT("Entity");
	case EArcUtilityTargetType::Location: return TEXT("Location");
	default:                              return TEXT("?");
	}
}

// --- Track Creator ---

FName FArcUtilityRewindDebuggerTrackCreator::GetTargetTypeNameInternal() const
{
	return FArcUtilityContext::StaticStruct()->GetFName();
}

FName FArcUtilityRewindDebuggerTrackCreator::GetNameInternal() const
{
	static const FName Name(TEXT("ArcUtilityTrack"));
	return Name;
}

void FArcUtilityRewindDebuggerTrackCreator::GetTrackTypesInternal(
	TArray<RewindDebugger::FRewindDebuggerTrackType>& Types) const
{
	Types.Add({FName(TEXT("ArcUtilityTrack")), NSLOCTEXT("ArcUtility", "TrackName", "Utility AI")});
}

bool FArcUtilityRewindDebuggerTrackCreator::HasDebugInfoInternal(
	const RewindDebugger::FObjectId& InObjectId) const
{
	const IRewindDebugger* Debugger = IRewindDebugger::Instance();
	if (!Debugger || !Debugger->GetAnalysisSession())
	{
		return false;
	}

	const FArcUtilityTraceProvider* Provider = Debugger->GetAnalysisSession()->ReadProvider<FArcUtilityTraceProvider>(
		FArcUtilityTraceProvider::ProviderName);

	return Provider && Provider->HasDataForInstance(InObjectId.GetMainId());
}

TSharedPtr<RewindDebugger::FRewindDebuggerTrack> FArcUtilityRewindDebuggerTrackCreator::CreateTrackInternal(
	const RewindDebugger::FObjectId& InObjectId) const
{
	return MakeShared<FArcUtilityRewindDebuggerTrack>(InObjectId);
}

// --- Track ---

namespace Arcx::Rewind
{
	static FLinearColor StatusToTimelineColor(uint8 Status)
	{
		switch (static_cast<EArcUtilityScoringStatus>(Status))
		{
			case EArcUtilityScoringStatus::Completed: return FLinearColor::Green;
			case EArcUtilityScoringStatus::Failed:    return FLinearColor(1.0f, 0.2f, 0.2f);
			case EArcUtilityScoringStatus::Aborted:   return FLinearColor(0.6f, 0.6f, 0.6f);
			default:                                  return FLinearColor(0.3f, 0.6f, 1.0f);
		}
	}	
}


FArcUtilityRewindDebuggerTrack::FArcUtilityRewindDebuggerTrack(const RewindDebugger::FObjectId& InObjectId)
	: ObjectId(InObjectId)
{
	EventData = MakeShared<SEventTimelineView::FTimelineEventData>();
}

FName FArcUtilityRewindDebuggerTrack::GetNameInternal() const
{
	static const FName Name(TEXT("ArcUtilityTrack"));
	return Name;
}

FText FArcUtilityRewindDebuggerTrack::GetDisplayNameInternal() const
{
	const IRewindDebugger* Debugger = IRewindDebugger::Instance();
	if (Debugger && Debugger->GetAnalysisSession())
	{
		const FArcUtilityTraceProvider* Provider = Debugger->GetAnalysisSession()->ReadProvider<FArcUtilityTraceProvider>(
			FArcUtilityTraceProvider::ProviderName);
		if (Provider)
		{
			const FString& Name = Provider->GetInstanceDisplayName(ObjectId.GetMainId());
			if (!Name.IsEmpty())
			{
				return FText::FromString(Name);
			}
		}
	}
	return NSLOCTEXT("ArcUtility", "TrackDisplayName", "Utility AI");
}

FSlateIcon FArcUtilityRewindDebuggerTrack::GetIconInternal()
{
	return FSlateIcon("EditorStyle", "ClassIcon.Default");
}

uint64 FArcUtilityRewindDebuggerTrack::GetObjectIdInternal() const
{
	return ObjectId.GetMainId();
}

bool FArcUtilityRewindDebuggerTrack::HasDebugDataInternal() const
{
	return CachedRequests != nullptr && CachedRequests->Num() > 0;
}

bool FArcUtilityRewindDebuggerTrack::UpdateInternal()
{
	const IRewindDebugger* Debugger = IRewindDebugger::Instance();
	if (!Debugger || !Debugger->GetAnalysisSession())
	{
		CachedRequests = nullptr;
		return false;
	}

	const FArcUtilityTraceProvider* Provider = Debugger->GetAnalysisSession()->ReadProvider<FArcUtilityTraceProvider>(
		FArcUtilityTraceProvider::ProviderName);
	if (!Provider)
	{
		CachedRequests = nullptr;
		return false;
	}

	CachedRequests = Provider->GetRequestsForInstance(ObjectId.GetMainId());

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

	if (CachedRequests)
	{
		for (const FArcUtilityTraceRequestRecord& R : *CachedRequests)
		{
			SEventTimelineView::FTimelineEventData::EventWindow& Window = EventData->Windows.AddDefaulted_GetRef();
			Window.TimeStart = ProfileToRecording(R.Timestamp);
			Window.TimeEnd = ProfileToRecording(R.EndTimestamp);
			Window.Color = Arcx::Rewind::StatusToTimelineColor(R.Status);
			Window.Description = FText::FromString(FString::Printf(TEXT("#%d %s"), R.RequestId, GetStatusString(R.Status)));
		}
	}

	return (PrevNumPoints != EventData->Points.Num() || PrevNumWindows != EventData->Windows.Num());
}

TSharedPtr<SWidget> FArcUtilityRewindDebuggerTrack::GetTimelineViewInternal()
{
	TWeakPtr<SEventTimelineView::FTimelineEventData> WeakEventData = EventData;
	return SNew(SEventTimelineView)
		.ViewRange_Lambda([]() { return IRewindDebugger::Instance()->GetCurrentViewRange(); })
		.EventData_Lambda([WeakEventData]()
		{
			return WeakEventData.Pin();
		});
}

// --- Details Panel ---

static void AppendRequestText(FString& Text, const FArcUtilityTraceRequestRecord& R)
{
	// Header line
	Text += FString::Printf(TEXT("Request #%d  |  %s  |  %s  |  %.2fms\n"),
		R.RequestId, GetStatusString(R.Status), GetSelectionModeString(R.SelectionMode), R.ExecutionTimeMs);

	Text += FString::Printf(TEXT("  MinScore: %.4f  |  %d entries x %d targets\n"),
		R.MinScore, R.NumEntries, R.NumTargets);

	// Per-entry, per-target breakdown
	for (int32 EntryIdx = 0; EntryIdx < R.NumEntries; ++EntryIdx)
	{
		for (int32 TargetIdx = 0; TargetIdx < R.NumTargets; ++TargetIdx)
		{
			// Target info
			if (R.Targets.IsValidIndex(TargetIdx))
			{
				const FArcUtilityTraceTargetRecord& Target = R.Targets[TargetIdx];
				Text += FString::Printf(TEXT("  Entry[%d] x Target[%d] (%s, Loc=(%.0f, %.0f, %.0f))\n"),
					EntryIdx, TargetIdx, GetTargetTypeString(Target.TargetType),
					Target.Location.X, Target.Location.Y, Target.Location.Z);
			}
			else
			{
				Text += FString::Printf(TEXT("  Entry[%d] x Target[%d]\n"), EntryIdx, TargetIdx);
			}

			// Consideration snapshots for this (entry, target) pair
			for (const FArcUtilityTraceConsiderationSnapshot& Snap : R.ConsiderationSnapshots)
			{
				if (Snap.EntryIndex == EntryIdx && Snap.TargetIndex == TargetIdx)
				{
					Text += FString::Printf(TEXT("    %s: raw=%.3f curve=%.3f comp=%.3f\n"),
						*Snap.ConsiderationName, Snap.RawScore, Snap.CurvedScore, Snap.CompensatedScore);
				}
			}

			// Check if this pair passed (exists in ScoredPairs)
			const FArcUtilityTraceScoredPair* FoundPair = nullptr;
			for (const FArcUtilityTraceScoredPair& Pair : R.ScoredPairs)
			{
				if (Pair.EntryIndex == EntryIdx && Pair.TargetIndex == TargetIdx)
				{
					FoundPair = &Pair;
					break;
				}
			}

			if (FoundPair)
			{
				Text += FString::Printf(TEXT("    -> Final: %.4f\n"), FoundPair->Score);
			}
			else
			{
				Text += TEXT("    -> EARLY OUT / below MinScore\n");
			}
		}
	}

	// Winner line
	if (R.WinnerEntryIndex != INDEX_NONE && R.WinnerTargetIndex != INDEX_NONE)
	{
		Text += FString::Printf(TEXT("  Winner: Entry[%d] x Target[%d] -- Score: %.4f\n"),
			R.WinnerEntryIndex, R.WinnerTargetIndex, R.WinnerScore);
	}
	else
	{
		Text += TEXT("  No winner\n");
	}
}

TSharedPtr<SWidget> FArcUtilityRewindDebuggerTrack::GetDetailsViewInternal()
{
	auto BuildDetailsText = [this]() -> FText
	{
		if (!CachedRequests || CachedRequests->Num() == 0)
		{
			return FText::FromString(TEXT("No utility evaluation data."));
		}

		FString Text;
		Text += FString::Printf(TEXT("%d requests recorded\n\n"), CachedRequests->Num());

		for (const FArcUtilityTraceRequestRecord& R : *CachedRequests)
		{
			AppendRequestText(Text, R);
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
