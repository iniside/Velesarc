// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTacticsDebugger.h"

#include "ArcTacticsDebuggerDrawComponent.h"
#include "CoreGlobals.h"
#include "Debugger/ArcTacticsDebug.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Mass/ArcTacticsFragments.h"
#include "Mass/Debugger.h"
#include "Mass/EntityFragments.h"
#include "Mass/EntityManager.h"
#include "MassDebuggerWorld.h"
#include "MassEntitySubsystem.h"
#include "MassNavMeshNavigationFragments.h"
#include "Misc/Paths.h"
#include "Tactics/ArcTacticsRecording.h"
#include "UObject/Class.h"
#include "imgui.h"

namespace ArcTacticsDebuggerPrivate
{
	constexpr float TimelineGutter = 72.0f;
	constexpr float TimelineAxisHeight = 18.0f;
	constexpr float TimelineActionLaneHeight = 28.0f;
	constexpr float TimelineDecisionLaneHeight = 22.0f;
	constexpr double TimelineMinViewDuration = 0.01;

	const ImVec4 ErrorTextColor(1.0f, 0.4f, 0.4f, 1.0f);
	const ImVec4 WarningTextColor(1.0f, 0.75f, 0.3f, 1.0f);
	const ImVec4 GoodTextColor(0.45f, 0.85f, 0.45f, 1.0f);

	UWorld* GetWorld()
	{
		return GEngine && GEngine->GameViewport ? GEngine->GameViewport->GetWorld() : nullptr;
	}

	FMassEntityManager* GetEntityManager()
	{
		UWorld* World = GetWorld();
		UMassEntitySubsystem* EntitySubsystem =
			World ? World->GetSubsystem<UMassEntitySubsystem>() : nullptr;
		return EntitySubsystem ? &EntitySubsystem->GetMutableEntityManager() : nullptr;
	}

	constexpr float DrawEntityRadius = 100.0f;
	constexpr float DrawCandidatePointSize = 24.0f;
	constexpr float DrawSelectedCandidatePointSize = 48.0f;
	constexpr float DrawTargetPointSize = 32.0f;
	constexpr float DrawMarkerPointSize = 36.0f;
	constexpr float DrawFaintMarkerPointSize = 16.0f;
	constexpr float DrawPathEndPointSize = 20.0f;
	constexpr float DrawPathStartPointSize = 12.0f;
	constexpr float DrawSelectedRowPointSize = 56.0f;

	const FColor DrawLiveEntityColor = FColor::Yellow;
	const FColor DrawRecordedEntityColor = FColor(200, 200, 90);
	const FColor DrawPawnLineColor = FColor::Cyan;
	const FColor DrawEntityTargetColor = FColor::Red;
	const FColor DrawPositionTargetColor = FColor::Green;
	const FColor DrawCandidateEntityColor = FColor(120, 60, 60);
	const FColor DrawCandidatePositionColor = FColor(60, 120, 60);
	const FColor DrawSelectedCandidateColor = FColor::Orange;
	const FColor DrawLivePathColor = FColor(80, 170, 255);
	const FColor DrawDecisionColor = FColor::Magenta;
	const FColor DrawSpanStartColor = FColor(90, 230, 90);
	const FColor DrawSpanEndColor = FColor(230, 120, 60);
	const FColor DrawFaintMarkerColor = FColor(110, 110, 120);
	const FColor DrawPathColor = FColor(80, 200, 255);
	const FColor DrawRepathColor = FColor(40, 95, 125);
	const FColor DrawFailedPathColor = FColor(220, 50, 50);
	const FColor DrawSelectedRowColor = FColor::White;

	constexpr float DrawQueryItemPointSize = 14.0f;
	constexpr float DrawQueryFilteredPointSize = 8.0f;
	constexpr float DrawQuerySelectedPointSize = 30.0f;
	constexpr float DrawQueryAppliedPointSize = 52.0f;
	constexpr int32 QueryTableVisibleRows = 12;

	const FColor DrawQueryFilteredColor = FColor(120, 120, 120);
	const FColor DrawQuerierColor = FColor(90, 160, 255);
	const FColor DrawQueryAppliedLineColor = FColor(200, 255, 200);

	FMassEntityManager* GetEntityManager(UWorld& World)
	{
		UMassEntitySubsystem* EntitySubsystem = World.GetSubsystem<UMassEntitySubsystem>();
		return EntitySubsystem ? &EntitySubsystem->GetMutableEntityManager() : nullptr;
	}

	bool TryGetEntityLocation(const FMassEntityManager& EntityManager, const FMassEntityHandle Entity, FVector& OutLocation)
	{
		if (!EntityManager.IsEntityActive(Entity))
		{
			return false;
		}
		const FTransformFragment* Transform = EntityManager.GetFragmentDataPtr<FTransformFragment>(Entity);
		if (!Transform)
		{
			return false;
		}
		OutLocation = Transform->GetTransform().GetLocation();
		return true;
	}

	void AddDrawPoint(FArcTacticsDebugDrawData& Data, const FVector& Location, const float Size, const FColor& Color, const FString& Label = FString())
	{
		FArcTacticsDebugDrawPoint& Point = Data.Points.AddDefaulted_GetRef();
		Point.Location = Location;
		Point.Size = Size;
		Point.Color = Color;
		Point.Label = Label;
	}

	void AddDrawLine(FArcTacticsDebugDrawData& Data, const FVector& Start, const FVector& End, const FColor& Color, const float Thickness)
	{
		FArcTacticsDebugDrawLine& Line = Data.Lines.AddDefaulted_GetRef();
		Line.Start = Start;
		Line.End = End;
		Line.Color = Color;
		Line.Thickness = Thickness;
	}

	struct FTimelineMapping
	{
		float PlotLeft = 0.0f;
		float PlotWidth = 1.0f;
		double ViewStart = 0.0;
		double ViewDuration = 1.0;

		float ToX(const double Time) const
		{
			return PlotLeft + static_cast<float>((Time - ViewStart) / ViewDuration * PlotWidth);
		}
	};

	template <typename TEnum>
	FString GetEnumName(const TEnum Value)
	{
		const UEnum* Enum = StaticEnum<TEnum>();
		return Enum ? Enum->GetNameStringByValue(static_cast<int64>(Value)) : FString::FromInt(static_cast<int32>(Value));
	}

	ImU32 GetEndStatusColor(const EArcTacticsActionEndStatus Status)
	{
		switch (Status)
		{
		case EArcTacticsActionEndStatus::Succeeded:
			return IM_COL32(80, 210, 80, 255);
		case EArcTacticsActionEndStatus::Failed:
			return IM_COL32(235, 60, 60, 255);
		case EArcTacticsActionEndStatus::Cancelled:
			return IM_COL32(240, 170, 50, 255);
		}
		return IM_COL32(200, 200, 200, 255);
	}

	ImVec4 GetOutcomeColor(const EArcTacticsCandidateOutcome Outcome)
	{
		switch (Outcome)
		{
		case EArcTacticsCandidateOutcome::Accepted:
			return GoodTextColor;
		case EArcTacticsCandidateOutcome::BelowMinScore:
			return WarningTextColor;
		case EArcTacticsCandidateOutcome::Vetoed:
			return ErrorTextColor;
		case EArcTacticsCandidateOutcome::BindingFailed:
			return ImVec4(0.9f, 0.45f, 0.9f, 1.0f);
		}
		return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
	}

	ImU32 GetActionColor(const FString& Name, const float Alpha)
	{
		const float Hue = static_cast<float>(GetTypeHash(Name) % 1024u) / 1024.0f;
		return ImColor::HSV(Hue, 0.55f, 0.8f, Alpha);
	}

	FString FormatTarget(const FArcTacticsRecordedTarget& Target)
	{
		switch (Target.Type)
		{
		case EArcTacticsValueIdentity::Entity:
			return FString::Printf(TEXT("E%u:%u"), Target.EntityIndex, Target.EntitySerial);
		case EArcTacticsValueIdentity::Vector:
			return FString::Printf(TEXT("(%.0f, %.0f, %.0f)"), Target.Location.X, Target.Location.Y, Target.Location.Z);
		default:
			return FString();
		}
	}


	FString GetSpanName(const FArcTacticsRecordedActionSpan& Span)
	{
		if (Span.Name.IsEmpty())
		{
			return FString(TEXT("(started before recording)"));
		}
		const FString Name = Span.Package.IsEmpty() ? Span.Name : Span.Package + TEXT(" / ") + Span.Name;
		return Span.bFromSelector ? Name + TEXT(" via Selector") : Name;
	}

	FString FormatLocation(const FVector3f& Location)
	{
		return FString::Printf(TEXT("(%.0f, %.0f, %.0f)"), Location.X, Location.Y, Location.Z);
	}

	FString FormatPosition(const FArcTacticsRecordedPosition& Position)
	{
		return Position.bValid ? TEXT("at ") + FormatLocation(Position.Location) : FString();
	}

	FString FormatSpanTime(const FArcTacticsRecordedActionSpan& Span)
	{
		const FString Start = Span.bEnteredInRecording
			? FString::Printf(TEXT("%.3f s"), Span.StartTime)
			: FString(TEXT("started before recording"));
		const FString End = Span.bOpenAtRecordingEnd
			? FString(TEXT("still running"))
			: FString::Printf(TEXT("%.3f s"), Span.EndTime);
		if (!Span.bEnteredInRecording)
		{
			return FString::Printf(TEXT("%s - %s"), *Start, *End);
		}
		return FString::Printf(TEXT("%s - %s (%.3f s)"), *Start, *End, Span.EndTime - Span.StartTime);
	}

	FString FormatStepLabel(const FArcTacticsRecordedActionEvent& Event)
	{
		const FString StepNumber = Event.StepCount > 0
			? FString::Printf(TEXT("Step %d/%d"), Event.StepIndex + 1, Event.StepCount)
			: FString::Printf(TEXT("Step %d"), Event.StepIndex + 1);
		if (Event.StepName.IsEmpty() && Event.StepTypeName.IsEmpty())
		{
			return StepNumber;
		}
		if (Event.StepName.IsEmpty() || Event.StepTypeName.IsEmpty() || Event.StepName == Event.StepTypeName)
		{
			return FString::Printf(TEXT("%s: %s"), *StepNumber, Event.StepName.IsEmpty() ? *Event.StepTypeName : *Event.StepName);
		}
		return FString::Printf(TEXT("%s: %s (%s)"), *StepNumber, *Event.StepName, *Event.StepTypeName);
	}

	float GetPathLength(const FArcTacticsRecordedPath& Path)
	{
		float Length = 0.0f;
		for (int32 PointIndex = 1; PointIndex < Path.Points.Num(); ++PointIndex)
		{
			Length += FVector3f::Distance(Path.Points[PointIndex - 1], Path.Points[PointIndex]);
		}
		return Length;
	}

	FString FormatPathDetail(const FArcTacticsRecordedPath& Path)
	{
		if (!Path.bSucceeded)
		{
			return FString::Printf(TEXT("failed, destination %s"), *FormatLocation(Path.End));
		}
		return FString::Printf(TEXT("%d points, %.1f m%s, destination %s"),
			Path.Points.Num(),
			GetPathLength(Path) / 100.0f,
			Path.bPartial ? TEXT(", partial") : TEXT(""),
			*FormatLocation(Path.End));
	}

	const FArcTacticsRecordedCandidate* FindSpanWinner(
		const FArcTacticsRecordedEntity& Entity,
		const TArray<int32>& SpanDecisions,
		const int32 SpanIndex)
	{
		if (!SpanDecisions.IsValidIndex(SpanIndex) || !Entity.Decisions.IsValidIndex(SpanDecisions[SpanIndex]))
		{
			return nullptr;
		}
		return Entity.Decisions[SpanDecisions[SpanIndex]].FindSelectedCandidate();
	}

	FString FormatEndReason(
		const FArcTacticsRecordedEntity& Entity,
		const TArray<int32>& SpanDecisions,
		const int32 SpanIndex)
	{
		const FArcTacticsRecordedActionSpan& Span = Entity.Actions[SpanIndex];
		switch (Span.End.Reason)
		{
		case EArcTacticsActionEndReason::Completed:
			return TEXT("Completed");
		case EArcTacticsActionEndReason::EnterEnded:
			return TEXT("Enter ended immediately");
		case EArcTacticsActionEndReason::Replaced:
		{
			const int32 NextIndex = SpanIndex + 1;
			if (!Entity.Actions.IsValidIndex(NextIndex) || !Entity.Actions[NextIndex].bEnteredInRecording)
			{
				return TEXT("Replaced by a new decision");
			}
			const FArcTacticsRecordedCandidate* Current = FindSpanWinner(Entity, SpanDecisions, SpanIndex);
			const FArcTacticsRecordedCandidate* Next = FindSpanWinner(Entity, SpanDecisions, NextIndex);
			const FString NextName = GetSpanName(Entity.Actions[NextIndex]);
			if (Current && Next && Current->bHasPriority && Next->bHasPriority && Next->Priority > Current->Priority)
			{
				return FString::Printf(TEXT("Interrupted by a higher-priority action (%s, priority %d over %d)"),
					*NextName, Next->Priority, Current->Priority);
			}
			return FString::Printf(TEXT("Replaced by %s"), *NextName);
		}
		case EArcTacticsActionEndReason::Unresolved:
			return TEXT("Action could not be resolved");
		case EArcTacticsActionEndReason::InputsFailed:
			return TEXT("Inputs failed");
		case EArcTacticsActionEndReason::Teardown:
			return TEXT("Teardown");
		case EArcTacticsActionEndReason::PackageRemoved:
			return TEXT("Its decision package was removed");
		}
		return TEXT("Unknown reason");
	}

	FString FormatSpanEnd(
		const FArcTacticsRecordedEntity& Entity,
		const TArray<int32>& SpanDecisions,
		const int32 SpanIndex,
		const bool bDetails)
	{
		const FArcTacticsRecordedActionSpan& Span = Entity.Actions[SpanIndex];
		if (Span.bOpenAtRecordingEnd)
		{
			return TEXT("Still running when the recording ended");
		}
		if (!Span.bEnded)
		{
			return TEXT("No end recorded");
		}
		FString Text = FString::Printf(TEXT("%s: %s"), ArcTacticsDebug::EndStatusText(Span.End.Status), *FormatEndReason(Entity, SpanDecisions, SpanIndex));
		if (!bDetails)
		{
			return Text;
		}
		if (Span.End.bCooldownStamped)
		{
			Text += FString::Printf(TEXT(", cooldown %.2f s started"), Span.End.Cooldown);
		}
		if (!Span.End.HistoryTag.IsEmpty())
		{
			Text += FString::Printf(TEXT(", history tag %s%s"), *Span.End.HistoryTag, Span.End.bHistoryAppended ? TEXT("") : TEXT(" (not appended)"));
		}
		return Text;
	}

	FString FormatCandidateName(const FArcTacticsRecordedCandidate& Candidate)
	{
		const FString Name = Candidate.Package.IsEmpty() ? Candidate.Name : Candidate.Package + TEXT(" / ") + Candidate.Name;
		return Candidate.bFromSelector ? Name + TEXT(" [selector]") : Name;
	}

	FString FormatCandidateName(const FArcTacticsDebugCandidate& Candidate)
	{
		const FString Name = Candidate.Package.IsEmpty()
			? Candidate.Name.ToString()
			: Candidate.Package + TEXT(" / ") + Candidate.Name.ToString();
		return Candidate.bFromSelector ? Name + TEXT(" [selector]") : Name;
	}

	FString FormatIdentity(const FArcTacticsCandidateIdentity& Identity)
	{
		switch (Identity.Type)
		{
		case EArcTacticsValueIdentity::Entity:
			return FString::Printf(TEXT("E%d:%d"), Identity.Entity.Index, Identity.Entity.SerialNumber);
		case EArcTacticsValueIdentity::Vector:
			return FString::Printf(TEXT("(%.0f, %.0f, %.0f)"), Identity.Vector.X, Identity.Vector.Y, Identity.Vector.Z);
		default:
			return FString();
		}
	}

	FString FormatTimeFromNow(const double Time, const double Now)
	{
		if (Time <= Now)
		{
			return FString::Printf(TEXT("now (t=%.2f s)"), Time);
		}
		return FString::Printf(TEXT("t=%.2f s (in %.2f s)"), Time, Time - Now);
	}

	template <typename TCandidate>
	bool IsSamePriorityBucket(const TCandidate& Lhs, const TCandidate& Rhs)
	{
		return Lhs.bHasPriority == Rhs.bHasPriority && (!Lhs.bHasPriority || Lhs.Priority == Rhs.Priority);
	}

	template <typename TCandidate>
	void SortCandidatesByBucket(const TArray<TCandidate>& Candidates, TArray<int32>& OutSorted)
	{
		OutSorted.Reset(Candidates.Num());
		for (int32 CandidateIndex = 0; CandidateIndex < Candidates.Num(); ++CandidateIndex)
		{
			OutSorted.Add(CandidateIndex);
		}
		OutSorted.Sort([&Candidates](const int32 Lhs, const int32 Rhs)
		{
			const TCandidate& Left = Candidates[Lhs];
			const TCandidate& Right = Candidates[Rhs];
			if (Left.bHasPriority != Right.bHasPriority)
			{
				return Left.bHasPriority;
			}
			if (Left.bHasPriority && Left.Priority != Right.Priority)
			{
				return Left.Priority > Right.Priority;
			}
			if (Left.Score != Right.Score)
			{
				return Left.Score > Right.Score;
			}
			return Left.Order < Right.Order;
		});
	}

	template <typename TCandidate>
	void DrawPriorityBucketRowIfNew(const TArray<TCandidate>& Candidates, const TArray<int32>& Sorted, const int32 SortedIndex)
	{
		const TCandidate& Candidate = Candidates[Sorted[SortedIndex]];
		if (SortedIndex > 0 && IsSamePriorityBucket(Candidates[Sorted[SortedIndex - 1]], Candidate))
		{
			return;
		}
		int32 BucketCount = 0;
		for (int32 NextIndex = SortedIndex; NextIndex < Sorted.Num(); ++NextIndex)
		{
			if (!IsSamePriorityBucket(Candidates[Sorted[NextIndex]], Candidate))
			{
				break;
			}
			++BucketCount;
		}
		ImGui::TableNextRow();
		ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg1, IM_COL32(45, 55, 85, 200));
		ImGui::TableNextColumn();
		if (Candidate.bHasPriority)
		{
			ImGui::Text("Priority %d  (%d)", Candidate.Priority, BucketCount);
		}
		else
		{
			ImGui::Text("Priority unresolved  (%d)", BucketCount);
		}
	}

	bool BeginCandidateTable(const char* Id)
	{
		const ImGuiTableFlags TableFlags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV
			| ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp;
		if (!ImGui::BeginTable(Id, 6, TableFlags))
		{
			return false;
		}
		ImGui::TableSetupColumn("Candidate", ImGuiTableColumnFlags_WidthStretch, 3.0f);
		ImGui::TableSetupColumn("Target", ImGuiTableColumnFlags_WidthStretch, 2.0f);
		ImGui::TableSetupColumn("Priority", ImGuiTableColumnFlags_WidthStretch, 0.8f);
		ImGui::TableSetupColumn("Score", ImGuiTableColumnFlags_WidthStretch, 1.2f);
		ImGui::TableSetupColumn("Outcome", ImGuiTableColumnFlags_WidthStretch, 1.3f);
		ImGui::TableSetupColumn("Reason", ImGuiTableColumnFlags_WidthStretch, 2.0f);
		ImGui::TableHeadersRow();
		return true;
	}

	void DrawCandidateRowColumns(const FString& TargetText, const bool bHasPriority, const int32 Priority, const float Score,
		const EArcTacticsCandidateOutcome Outcome, const FString& Reason)
	{
		ImGui::TableNextColumn();
		ImGui::TextUnformatted(TargetText.IsEmpty() ? "-" : TCHAR_TO_UTF8(*TargetText));
		ImGui::TableNextColumn();
		if (bHasPriority)
		{
			ImGui::Text("%d", Priority);
		}
		else
		{
			ImGui::TextDisabled("-");
		}
		ImGui::TableNextColumn();
		ImGui::Text("%.4f", Score);
		ImGui::TableNextColumn();
		ImGui::TextColored(GetOutcomeColor(Outcome), "%s", TCHAR_TO_UTF8(*GetEnumName(Outcome)));
		ImGui::TableNextColumn();
		ImGui::TextUnformatted(TCHAR_TO_UTF8(*Reason));
	}

	void DrawConsiderationRow(const int32 Index, const FString& Name, const bool bEvaluated, const float Raw, const float Curved,
		const bool bVeto, const bool bEarlyOut)
	{
		ImGui::TableNextRow();
		if (bVeto)
		{
			ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg1, IM_COL32(125, 30, 30, 200));
		}
		else if (bEarlyOut)
		{
			ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg1, IM_COL32(110, 90, 20, 180));
		}
		ImGui::TableNextColumn();
		const FString Label = FString::Printf(TEXT("    %d. %s"), Index, *Name);
		if (bEvaluated || bVeto)
		{
			ImGui::TextUnformatted(TCHAR_TO_UTF8(*Label));
		}
		else
		{
			ImGui::TextDisabled("%s", TCHAR_TO_UTF8(*Label));
		}
		ImGui::TableNextColumn();
		if (bEvaluated)
		{
			ImGui::Text("raw %.4f", Raw);
		}
		ImGui::TableNextColumn();
		ImGui::TableNextColumn();
		if (bEvaluated)
		{
			ImGui::Text("curved %.4f", Curved);
		}
		ImGui::TableNextColumn();
		if (bVeto)
		{
			ImGui::TextColored(ErrorTextColor, "VETO");
		}
		else if (bEarlyOut)
		{
			ImGui::TextColored(WarningTextColor, "early-out");
		}
		else if (!bEvaluated)
		{
			ImGui::TextDisabled("not evaluated");
		}
	}

	void DrawNoConsiderationsRow()
	{
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::TextDisabled("    no considerations");
	}

	int32 FindFilteringStep(const FArcTacticsRecordedQuery& Query, const int32 ItemIndex)
	{
		for (int32 StepIndex = 0; StepIndex < Query.Steps.Num(); ++StepIndex)
		{
			const FArcTacticsRecordedQueryStep& Step = Query.Steps[StepIndex];
			if (Step.Type == EArcTQSStepType::Filter && Step.RawScores.IsValidIndex(ItemIndex) && Step.RawScores[ItemIndex] <= 0.0f)
			{
				return StepIndex;
			}
		}
		return INDEX_NONE;
	}

	FString GetQueryStepName(const FArcTacticsRecordedQuery& Query, const int32 StepIndex)
	{
		const FString& Name = Query.Steps[StepIndex].Name;
		return Name.IsEmpty() ? FString::Printf(TEXT("Step %d"), StepIndex + 1) : Name;
	}

	FString FormatQueryName(const FArcTacticsRecordedQuery& Query)
	{
		const FString QueryName = Query.QueryName.IsEmpty() ? FString(TEXT("(unnamed)")) : Query.QueryName;
		if (Query.SourceName.IsEmpty())
		{
			return FString::Printf(TEXT("Query %s"), *QueryName);
		}
		const TCHAR* SourceLabel = Query.SourceKind == EArcTacticsQuerySource::Step ? TEXT("step") : TEXT("task");
		return FString::Printf(TEXT("Query %s (%s %s)"), *QueryName, SourceLabel, *Query.SourceName);
	}

	FString FormatQuerySummary(const FArcTacticsRecordedQuery& Query)
	{
		int32 ValidCount = 0;
		int32 AppliedCount = 0;
		int32 OtherSelectedCount = 0;
		float BestAppliedScore = 0.0f;
		for (const FArcTacticsRecordedQueryItem& Item : Query.Items)
		{
			ValidCount += Item.bValid ? 1 : 0;
			if (Item.bApplied)
			{
				BestAppliedScore = AppliedCount == 0 ? Item.Score : FMath::Max(BestAppliedScore, Item.Score);
				++AppliedCount;
			}
			else if (Item.bSelected)
			{
				++OtherSelectedCount;
			}
		}

		FString Text = FString::Printf(TEXT("%d items, %d valid"), Query.TotalItemCount, ValidCount);
		if (AppliedCount == 1)
		{
			Text += FString::Printf(TEXT(", winner score %.2f"), BestAppliedScore);
		}
		else if (AppliedCount > 1)
		{
			Text += FString::Printf(TEXT(", %d winners, top score %.2f"), AppliedCount, BestAppliedScore);
		}
		else
		{
			Text += TEXT(", no winner");
		}
		if (OtherSelectedCount > 0)
		{
			Text += FString::Printf(TEXT(", %d other selected"), OtherSelectedCount);
		}
		if (Query.Status != EArcTQSQueryStatus::Completed)
		{
			Text += TEXT(", status ") + GetEnumName(Query.Status);
		}
		if (!Query.bComplete)
		{
			Text += FString::Printf(TEXT(", incomplete (%d of %d items recorded)"), Query.Items.Num(), Query.TotalItemCount);
		}
		return Text;
	}

	FString FormatQueryItemTarget(const FArcTacticsRecordedQueryItem& Item)
	{
		if (Item.EntitySerial != 0)
		{
			return FString::Printf(TEXT("E%u:%u %s"), Item.EntityIndex, Item.EntitySerial, *FormatLocation(Item.Location));
		}
		return FormatLocation(Item.Location);
	}

	void SortQueryItems(const FArcTacticsRecordedQuery& Query, TArray<int32>& OutSorted)
	{
		OutSorted.Reset(Query.Items.Num());
		for (int32 ItemIndex = 0; ItemIndex < Query.Items.Num(); ++ItemIndex)
		{
			OutSorted.Add(ItemIndex);
		}
		OutSorted.Sort([&Query](const int32 Lhs, const int32 Rhs)
		{
			const FArcTacticsRecordedQueryItem& Left = Query.Items[Lhs];
			const FArcTacticsRecordedQueryItem& Right = Query.Items[Rhs];
			if (Left.bValid != Right.bValid)
			{
				return Left.bValid;
			}
			if (Left.Score != Right.Score)
			{
				return Left.Score > Right.Score;
			}
			return Lhs < Rhs;
		});
	}

	void DrawQueryItemTable(const FArcTacticsRecordedQuery& Query, const int32 QueryIndex, const float Width)
	{
		if (!Query.bComplete)
		{
			ImGui::TextColored(WarningTextColor, "Incomplete query: %d of %d items recorded", Query.Items.Num(), Query.TotalItemCount);
		}
		if (Query.Items.Num() == 0)
		{
			ImGui::TextDisabled("No items recorded");
			return;
		}

		TArray<int32> Sorted;
		SortQueryItems(Query, Sorted);
		constexpr int32 FixedColumnCount = 5;
		const int32 ColumnCount = FixedColumnCount + Query.Steps.Num();
		const int32 VisibleRows = FMath::Min(Query.Items.Num(), QueryTableVisibleRows);
		const float Height = ImGui::GetTextLineHeightWithSpacing() * static_cast<float>(VisibleRows + 2);
		const ImGuiTableFlags TableFlags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_BordersOuter
			| ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY;
		const FString TableId = FString::Printf(TEXT("HistoryQueryItems%d"), QueryIndex);
		if (!ImGui::BeginTable(TCHAR_TO_UTF8(*TableId), ColumnCount, TableFlags, ImVec2(Width, Height)))
		{
			return;
		}
		ImGui::TableSetupScrollFreeze(1, 1);
		ImGui::TableSetupColumn("#");
		ImGui::TableSetupColumn("Result");
		ImGui::TableSetupColumn("Valid");
		ImGui::TableSetupColumn("Score");
		ImGui::TableSetupColumn("Target");
		for (int32 StepIndex = 0; StepIndex < Query.Steps.Num(); ++StepIndex)
		{
			const FArcTacticsRecordedQueryStep& Step = Query.Steps[StepIndex];
			const FString Header = Step.Type == EArcTQSStepType::Filter
				? FString::Printf(TEXT("Filter %s (-%d) raw / cum"), *GetQueryStepName(Query, StepIndex), Step.FilteredCount)
				: FString::Printf(TEXT("Score %s raw / cum"), *GetQueryStepName(Query, StepIndex));
			ImGui::TableSetupColumn(TCHAR_TO_UTF8(*Header));
		}
		ImGui::TableHeadersRow();

		ImGuiListClipper Clipper;
		Clipper.Begin(Sorted.Num());
		while (Clipper.Step())
		{
			for (int32 Row = Clipper.DisplayStart; Row < Clipper.DisplayEnd; ++Row)
			{
				const int32 ItemIndex = Sorted[Row];
				const FArcTacticsRecordedQueryItem& Item = Query.Items[ItemIndex];
				const int32 FilteringStep = Item.bValid ? INDEX_NONE : FindFilteringStep(Query, ItemIndex);
				ImGui::TableNextRow();
				if (Item.bApplied)
				{
					ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg1, IM_COL32(50, 95, 45, 200));
				}
				else if (Item.bSelected)
				{
					ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg1, IM_COL32(40, 70, 110, 200));
				}
				ImGui::TableNextColumn();
				ImGui::Text("%d", Row + 1);
				ImGui::TableNextColumn();
				if (Item.bApplied)
				{
					ImGui::TextColored(GoodTextColor, "Winner");
				}
				else if (Item.bSelected)
				{
					ImGui::TextColored(ImVec4(0.55f, 0.75f, 1.0f, 1.0f), "Selected");
				}
				else
				{
					ImGui::TextDisabled("-");
				}
				ImGui::TableNextColumn();
				if (Item.bValid)
				{
					ImGui::TextColored(GoodTextColor, "valid");
				}
				else if (FilteringStep != INDEX_NONE)
				{
					ImGui::TextColored(ErrorTextColor, "filtered by %s", TCHAR_TO_UTF8(*GetQueryStepName(Query, FilteringStep)));
				}
				else
				{
					ImGui::TextColored(ErrorTextColor, "filtered");
				}
				ImGui::TableNextColumn();
				if (Item.bValid)
				{
					ImGui::Text("%.4f", Item.Score);
				}
				else
				{
					ImGui::TextDisabled("%.4f", Item.Score);
				}
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(TCHAR_TO_UTF8(*FormatQueryItemTarget(Item)));
				for (int32 StepIndex = 0; StepIndex < Query.Steps.Num(); ++StepIndex)
				{
					ImGui::TableNextColumn();
					const FArcTacticsRecordedQueryStep& Step = Query.Steps[StepIndex];
					if ((FilteringStep != INDEX_NONE && StepIndex > FilteringStep)
						|| !Step.RawScores.IsValidIndex(ItemIndex) || !Step.CumulativeScores.IsValidIndex(ItemIndex))
					{
						ImGui::TextDisabled("-");
					}
					else if (StepIndex == FilteringStep)
					{
						ImGui::TextColored(ErrorTextColor, "%.3f / %.3f", Step.RawScores[ItemIndex], Step.CumulativeScores[ItemIndex]);
					}
					else
					{
						ImGui::Text("%.3f / %.3f", Step.RawScores[ItemIndex], Step.CumulativeScores[ItemIndex]);
					}
				}
			}
		}
		ImGui::EndTable();
	}

	void DrawQueryItemTableRow(const FArcTacticsRecordedQuery& Query, const int32 QueryIndex)
	{
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(ImGui::TableGetColumnCount() - 1);
		const float RightX = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
		ImGui::TableSetColumnIndex(0);
		const ImVec2 Cursor = ImGui::GetCursorScreenPos();
		const ImDrawList* DrawList = ImGui::GetWindowDrawList();
		ImGui::PushClipRect(ImVec2(Cursor.x, DrawList->GetClipRectMin().y), ImVec2(RightX, DrawList->GetClipRectMax().y), false);
		DrawQueryItemTable(Query, QueryIndex, FMath::Max(RightX - Cursor.x, 1.0f));
		ImGui::PopClipRect();
	}

	FColor GetQueryScoreColor(const float Alpha)
	{
		const float Clamped = FMath::Clamp(Alpha, 0.0f, 1.0f);
		return FColor(
			static_cast<uint8>(FMath::RoundToInt(255.0f * (1.0f - Clamped))),
			static_cast<uint8>(FMath::RoundToInt(255.0f * Clamped)),
			40);
	}

	void AddQueryDrawData(const FArcTacticsRecordedQuery& Query, FArcTacticsDebugDrawData& Data)
	{
		float MinScore = TNumericLimits<float>::Max();
		float MaxScore = TNumericLimits<float>::Lowest();
		for (const FArcTacticsRecordedQueryItem& Item : Query.Items)
		{
			if (Item.bValid)
			{
				MinScore = FMath::Min(MinScore, Item.Score);
				MaxScore = FMath::Max(MaxScore, Item.Score);
			}
		}
		const float Range = MaxScore - MinScore;

		for (const FArcTacticsRecordedQueryItem& Item : Query.Items)
		{
			const FVector Location(Item.Location);
			if (!Item.bValid && !Item.bSelected)
			{
				AddDrawPoint(Data, Location, DrawQueryFilteredPointSize, DrawQueryFilteredColor);
				continue;
			}
			const float Alpha = Range > UE_KINDA_SMALL_NUMBER ? (Item.Score - MinScore) / Range : 1.0f;
			const FColor Color = GetQueryScoreColor(Alpha);
			if (Item.bApplied)
			{
				AddDrawPoint(Data, Location, DrawQueryAppliedPointSize, Color, FString::Printf(TEXT("Winner %.3f"), Item.Score));
				if (Query.QuerierLocation.bValid)
				{
					AddDrawLine(Data, FVector(Query.QuerierLocation.Location), Location, DrawQueryAppliedLineColor, 2.0f);
				}
			}
			else if (Item.bSelected)
			{
				AddDrawPoint(Data, Location, DrawQuerySelectedPointSize, Color);
			}
			else
			{
				AddDrawPoint(Data, Location, DrawQueryItemPointSize, Color);
			}
		}

		if (Query.QuerierLocation.bValid)
		{
			AddDrawPoint(Data, FVector(Query.QuerierLocation.Location), DrawMarkerPointSize, DrawQuerierColor,
				FString::Printf(TEXT("Querier  %s  %.3f s"), *FormatQueryName(Query), Query.Time));
		}
	}

	void DrawLiveCandidateTable(const FArcTacticsDebugLastDecision& Decision)
	{
		if (Decision.Candidates.Num() == 0)
		{
			ImGui::TextDisabled("No candidates captured");
			return;
		}
		TArray<int32> Sorted;
		SortCandidatesByBucket(Decision.Candidates, Sorted);
		if (!BeginCandidateTable("LiveCandidates"))
		{
			return;
		}
		for (int32 SortedIndex = 0; SortedIndex < Sorted.Num(); ++SortedIndex)
		{
			DrawPriorityBucketRowIfNew(Decision.Candidates, Sorted, SortedIndex);
			const int32 CandidateIndex = Sorted[SortedIndex];
			const FArcTacticsDebugCandidate& Candidate = Decision.Candidates[CandidateIndex];
			ImGui::TableNextRow();
			if (Candidate.bSelected)
			{
				ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg1, IM_COL32(50, 95, 45, 170));
			}
			ImGui::TableNextColumn();
			const FString NodeLabel = FString::Printf(TEXT("%s##LiveCandidate%d"), *FormatCandidateName(Candidate), CandidateIndex);
			const bool bOpen = ImGui::TreeNodeEx(TCHAR_TO_UTF8(*NodeLabel), ImGuiTreeNodeFlags_SpanAllColumns);
			DrawCandidateRowColumns(FormatIdentity(Candidate.Target), Candidate.bHasPriority, Candidate.Priority, Candidate.Score,
				Candidate.Outcome, Candidate.Reason.IsNone() ? FString() : Candidate.Reason.ToString());
			if (!bOpen)
			{
				continue;
			}
			if (Candidate.Considerations.Num() == 0)
			{
				DrawNoConsiderationsRow();
			}
			for (int32 ConsiderationIndex = 0; ConsiderationIndex < Candidate.Considerations.Num(); ++ConsiderationIndex)
			{
				const FArcTacticsDebugConsideration& Consideration = Candidate.Considerations[ConsiderationIndex];
				DrawConsiderationRow(ConsiderationIndex, Consideration.Name.ToString(), Consideration.bScored, Consideration.Raw,
					Consideration.Curved, ConsiderationIndex == Candidate.VetoIndex, ConsiderationIndex == Candidate.EarlyOutIndex);
			}
			ImGui::TreePop();
		}
		ImGui::EndTable();
	}

	void DrawLiveDecision(const FArcTacticsDebugSnapshot& Snapshot)
	{
		ImGui::Text("Status %s   generation %u   %s   history %d",
			TCHAR_TO_UTF8(*GetEnumName(Snapshot.DecisionStatus)),
			Snapshot.DecisionGeneration,
			TCHAR_TO_UTF8(*GetEnumName(Snapshot.SelectionMode)),
			Snapshot.HistorySize);
		if (Snapshot.SelectionMode == EArcTacticsSelectionMode::RandomFromTopPercent)
		{
			ImGui::SameLine();
			ImGui::Text("  top %.3f", Snapshot.TopPercent);
		}
		if (Snapshot.bDecisionRequested)
		{
			ImGui::TextColored(WarningTextColor, "Requested");
		}
		else
		{
			ImGui::TextDisabled("Not requested");
		}
		ImGui::SameLine();
		ImGui::Text("  next allowed %s   interval %.2f s",
			TCHAR_TO_UTF8(*FormatTimeFromNow(Snapshot.NextDecisionTime, Snapshot.Now)),
			Snapshot.DecisionInterval);
		for (const FArcTacticsDebugCooldown& Cooldown : Snapshot.Cooldowns)
		{
			if (Cooldown.Remaining > 0.0)
			{
				ImGui::TextColored(WarningTextColor, "Cooldown %s branch %d: %.1f s left",
					TCHAR_TO_UTF8(*Cooldown.ConfigName), Cooldown.BranchIndex, Cooldown.Remaining);
			}
		}
		if (!Snapshot.Diagnostic.IsNone())
		{
			ImGui::TextColored(WarningTextColor, "Diagnostic %s", TCHAR_TO_UTF8(*Snapshot.Diagnostic.ToString()));
		}

		const FArcTacticsDebugLastDecision& Decision = Snapshot.LastDecision;
		ImGui::SeparatorText("Last decision");
		if (!Decision.bValid)
		{
			ImGui::TextDisabled("No decision captured since this entity was selected");
			return;
		}
		ImGui::Text("t=%.2f s (%.2f s ago)   generation %u   status %s   %d candidates",
			Decision.Time,
			Snapshot.Now - Decision.Time,
			Decision.DecisionGeneration,
			TCHAR_TO_UTF8(*GetEnumName(Decision.Status)),
			Decision.Candidates.Num());
		if (!Decision.Diagnostic.IsNone())
		{
			ImGui::TextColored(WarningTextColor, "Diagnostic %s", TCHAR_TO_UTF8(*Decision.Diagnostic.ToString()));
		}

		const FArcTacticsDebugCandidate* Selected = Decision.Candidates.FindByPredicate([](const FArcTacticsDebugCandidate& Candidate)
		{
			return Candidate.bSelected;
		});
		ImGui::PushStyleColor(ImGuiCol_ChildBg, Selected ? ImVec4(0.14f, 0.28f, 0.14f, 1.0f) : ImVec4(0.25f, 0.2f, 0.1f, 1.0f));
		if (ImGui::BeginChild("LiveSelected", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY))
		{
			if (Selected)
			{
				ImGui::TextColored(GoodTextColor, "Selected  %s", TCHAR_TO_UTF8(*FormatCandidateName(*Selected)));
				const FString TargetText = FormatIdentity(Selected->Target);
				const FString PriorityText = Selected->bHasPriority ? FString::FromInt(Selected->Priority) : FString(TEXT("unresolved"));
				ImGui::Text("target %s   score %.4f   priority %s   order %d",
					TargetText.IsEmpty() ? "-" : TCHAR_TO_UTF8(*TargetText),
					Selected->Score,
					TCHAR_TO_UTF8(*PriorityText),
					Selected->Order);
			}
			else if (Decision.SelectedOrder != INDEX_NONE)
			{
				ImGui::TextColored(ErrorTextColor, "Selected order %d has no captured candidate", Decision.SelectedOrder);
			}
			else
			{
				ImGui::TextColored(WarningTextColor, "No action selected");
			}
		}
		ImGui::EndChild();
		ImGui::PopStyleColor();

		DrawLiveCandidateTable(Decision);
	}

	struct FLiveBranchCandidates
	{
		int32 Count = 0;
		const FArcTacticsDebugCandidate* Representative = nullptr;
	};

	FLiveBranchCandidates FindBranchCandidates(const FArcTacticsDebugLastDecision& Decision, const FString& PackageName,
		const FArcTacticsDebugBranch& Branch)
	{
		FLiveBranchCandidates Result;
		if (!Decision.bValid)
		{
			return Result;
		}
		for (const FArcTacticsDebugCandidate& Candidate : Decision.Candidates)
		{
			if (Candidate.Package != PackageName || Candidate.bFromSelector != Branch.bSelector
				|| Candidate.SourceIndex != Branch.SourceIndex)
			{
				continue;
			}
			++Result.Count;
			if (!Result.Representative || Candidate.bSelected
				|| (!Result.Representative->bSelected && Candidate.Score > Result.Representative->Score))
			{
				Result.Representative = &Candidate;
			}
		}
		return Result;
	}

	void DrawBranchLastDecision(const FArcTacticsDebugLastDecision& Decision, const FArcTacticsDebugBranch& Branch,
		const FLiveBranchCandidates& Candidates)
	{
		if (!Decision.bValid)
		{
			ImGui::TextDisabled("-");
			return;
		}
		const FArcTacticsDebugCandidate* Candidate = Candidates.Representative;
		if (!Candidate)
		{
			ImGui::TextDisabled("not a candidate");
			return;
		}
		FString Text;
		if (Branch.bSelector)
		{
			Text = FString::Printf(TEXT("%d items, %s %s %.4f %s"),
				Candidates.Count,
				Candidate->bSelected ? TEXT("selected") : TEXT("best"),
				*FormatIdentity(Candidate->Target),
				Candidate->Score,
				*GetEnumName(Candidate->Outcome));
		}
		else
		{
			Text = FString::Printf(TEXT("%.4f %s"), Candidate->Score, *GetEnumName(Candidate->Outcome));
		}
		if (!Candidate->Reason.IsNone())
		{
			Text += FString::Printf(TEXT(" (%s)"), *Candidate->Reason.ToString());
		}
		ImGui::TextColored(GetOutcomeColor(Candidate->Outcome), "%s", TCHAR_TO_UTF8(*Text));
		if (Candidate->bSelected && !Branch.bSelector)
		{
			ImGui::SameLine();
			ImGui::TextColored(GoodTextColor, "SELECTED");
		}
	}

	void DrawLiveBranch(const FArcTacticsDebugSnapshot& Snapshot, const FString& PackageName,
		const FArcTacticsDebugBranch& Branch, const int32 BranchIndex)
	{
		const FArcTacticsDebugLastDecision& Decision = Snapshot.LastDecision;
		const FLiveBranchCandidates Candidates = FindBranchCandidates(Decision, PackageName, Branch);
		ImGui::PushID(BranchIndex);
		ImGui::TableNextRow();
		if (Branch.bActive)
		{
			ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg1, IM_COL32(50, 95, 45, 170));
		}
		ImGui::TableNextColumn();
		ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_SpanAllColumns | ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
		if (Branch.bActive)
		{
			Flags |= ImGuiTreeNodeFlags_DefaultOpen;
		}
		const bool bOpen = ImGui::TreeNodeEx("##Branch", Flags, "%s", TCHAR_TO_UTF8(*Branch.Name.ToString()));
		if (Branch.bActive)
		{
			ImGui::SameLine();
			ImGui::TextColored(GoodTextColor, "[active]");
		}
		ImGui::TableNextColumn();
		const FString TypeText = Branch.bSelector
			? FString::Printf(TEXT("%s > %s"), *Branch.SelectorTypeName, *Branch.TypeName)
			: Branch.TypeName;
		ImGui::TextUnformatted(TCHAR_TO_UTF8(*TypeText));
		ImGui::TableNextColumn();
		ImGui::Text("%d", Branch.Priority);
		ImGui::TableNextColumn();
		ImGui::Text("%.2f", Branch.Weight);
		ImGui::TableNextColumn();
		if (Branch.CooldownRemaining > 0.0)
		{
			ImGui::TextColored(WarningTextColor, "%.1f s left", Branch.CooldownRemaining);
		}
		else if (Branch.Cooldown > 0.0f)
		{
			ImGui::Text("%.2f s %s", Branch.Cooldown, TCHAR_TO_UTF8(*GetEnumName(Branch.CooldownStart)));
		}
		else
		{
			ImGui::TextDisabled("-");
		}
		ImGui::TableNextColumn();
		DrawBranchLastDecision(Decision, Branch, Candidates);
		if (!bOpen)
		{
			ImGui::PopID();
			return;
		}

		const ImGuiTreeNodeFlags GroupFlags = ImGuiTreeNodeFlags_SpanAllColumns | ImGuiTreeNodeFlags_DefaultOpen;
		const ImGuiTreeNodeFlags LeafFlags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAllColumns;
		if (Branch.Steps.Num() > 0)
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			if (ImGui::TreeNodeEx("##Steps", GroupFlags, "Steps (%d)", Branch.Steps.Num()))
			{
				for (int32 StepIndex = 0; StepIndex < Branch.Steps.Num(); ++StepIndex)
				{
					const FArcTacticsDebugStep& Step = Branch.Steps[StepIndex];
					ImGui::TableNextRow();
					if (Step.bCurrent)
					{
						ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg1, IM_COL32(55, 70, 110, 220));
					}
					ImGui::TableNextColumn();
					ImGui::PushID(StepIndex);
					ImGui::TreeNodeEx("##Step", LeafFlags | (Step.bCurrent ? ImGuiTreeNodeFlags_Selected : ImGuiTreeNodeFlags_None),
						"%d. %s", StepIndex + 1, TCHAR_TO_UTF8(*Step.Name.ToString()));
					ImGui::PopID();
					ImGui::TableNextColumn();
					ImGui::TextUnformatted(TCHAR_TO_UTF8(*Step.TypeName));
					ImGui::TableSetColumnIndex(5);
					if (Step.bCurrent)
					{
						ImGui::TextColored(GoodTextColor, "current");
					}
				}
				ImGui::TreePop();
			}
		}

		if (Branch.ActiveSteps.Num() > 0)
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			if (ImGui::TreeNodeEx("##ActiveSteps", GroupFlags, "While Active (%d)", Branch.ActiveSteps.Num()))
			{
				for (int32 ActiveStepIndex = 0; ActiveStepIndex < Branch.ActiveSteps.Num(); ++ActiveStepIndex)
				{
					const FArcTacticsDebugActiveStep& ActiveStep = Branch.ActiveSteps[ActiveStepIndex];
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::PushID(ActiveStepIndex);
					ImGui::TreeNodeEx("##ActiveStep", LeafFlags, "%d. %s", ActiveStepIndex + 1, TCHAR_TO_UTF8(*ActiveStep.Name.ToString()));
					ImGui::PopID();
					ImGui::TableNextColumn();
					ImGui::TextUnformatted(TCHAR_TO_UTF8(*ActiveStep.TypeName));
					ImGui::TableSetColumnIndex(5);
					if (ActiveStep.bEntered)
					{
						ImGui::TextColored(GoodTextColor, "entered");
					}
				}
				ImGui::TreePop();
			}
		}

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		if (ImGui::TreeNodeEx("##Considerations", GroupFlags, "Considerations (%d)", Branch.Considerations.Num()))
		{
			const FArcTacticsDebugCandidate* Candidate = Candidates.Representative;
			for (int32 ConsiderationIndex = 0; ConsiderationIndex < Branch.Considerations.Num(); ++ConsiderationIndex)
			{
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::PushID(ConsiderationIndex);
				ImGui::TreeNodeEx("##Consideration", LeafFlags, "%s", TCHAR_TO_UTF8(*Branch.Considerations[ConsiderationIndex].ToString()));
				ImGui::PopID();
				ImGui::TableSetColumnIndex(5);
				if (!Candidate || !Candidate->Considerations.IsValidIndex(ConsiderationIndex))
				{
					ImGui::TextDisabled("-");
					continue;
				}
				const FArcTacticsDebugConsideration& Consideration = Candidate->Considerations[ConsiderationIndex];
				if (ConsiderationIndex == Candidate->VetoIndex)
				{
					ImGui::TextColored(ErrorTextColor, "raw %.4f  curved %.4f  VETO", Consideration.Raw, Consideration.Curved);
				}
				else if (!Consideration.bScored)
				{
					ImGui::TextDisabled("not evaluated");
				}
				else if (ConsiderationIndex == Candidate->EarlyOutIndex)
				{
					ImGui::TextColored(WarningTextColor, "raw %.4f  curved %.4f  early-out", Consideration.Raw, Consideration.Curved);
				}
				else
				{
					ImGui::Text("raw %.4f  curved %.4f", Consideration.Raw, Consideration.Curved);
				}
			}
			ImGui::TreePop();
		}
		ImGui::TreePop();
		ImGui::PopID();
	}

	void DrawLivePackage(const FArcTacticsDebugSnapshot& Snapshot, const FArcTacticsDebugPackage& Package)
	{
		ImGui::Text("seen revision %u   compiled revision %u   static %d   runtime %d   min score %.3f",
			Package.SeenRevision, Package.Revision, Package.StaticCount, Package.RuntimeCount, Package.MinScore);
		if (!Package.bActive)
		{
			ImGui::TextColored(ErrorTextColor, "Inactive: not executable or missing required fragments");
			return;
		}
		if (Package.bExcluded)
		{
			ImGui::TextColored(WarningTextColor, "Excluded from the current decision");
		}
		if (!Package.Diagnostic.IsNone())
		{
			ImGui::TextColored(WarningTextColor, "Diagnostic %s", TCHAR_TO_UTF8(*Package.Diagnostic.ToString()));
		}
		const ImGuiTableFlags TableFlags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV
			| ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp;
		if (!ImGui::BeginTable("LiveConfig", 6, TableFlags))
		{
			return;
		}
		ImGui::TableSetupColumn("Node", ImGuiTableColumnFlags_WidthStretch, 3.0f);
		ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthStretch, 2.2f);
		ImGui::TableSetupColumn("Priority", ImGuiTableColumnFlags_WidthStretch, 0.8f);
		ImGui::TableSetupColumn("Weight", ImGuiTableColumnFlags_WidthStretch, 0.8f);
		ImGui::TableSetupColumn("Cooldown", ImGuiTableColumnFlags_WidthStretch, 1.4f);
		ImGui::TableSetupColumn("Last decision", ImGuiTableColumnFlags_WidthStretch, 3.0f);
		ImGui::TableHeadersRow();

		const ImGuiTreeNodeFlags GroupFlags = ImGuiTreeNodeFlags_SpanAllColumns | ImGuiTreeNodeFlags_DefaultOpen;
		const ImGuiTreeNodeFlags LeafFlags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAllColumns;

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		if (ImGui::TreeNodeEx("##GlobalTasks", GroupFlags, "Global Tasks (%d)", Package.GlobalTasks.Num()))
		{
			for (int32 TaskIndex = 0; TaskIndex < Package.GlobalTasks.Num(); ++TaskIndex)
			{
				const FArcTacticsDebugNode& Task = Package.GlobalTasks[TaskIndex];
				const bool bRunning = Package.bGlobalTaskRunning && TaskIndex == Package.GlobalTaskIndex;
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::PushID(TaskIndex);
				ImGui::TreeNodeEx("##Task", LeafFlags, "%d. %s", TaskIndex + 1, TCHAR_TO_UTF8(*Task.Name.ToString()));
				ImGui::PopID();
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(TCHAR_TO_UTF8(*Task.TypeName));
				ImGui::TableSetColumnIndex(5);
				if (bRunning)
				{
					ImGui::TextColored(WarningTextColor, "running");
				}
			}
			ImGui::TreePop();
		}

		int32 ActionCount = 0;
		for (const FArcTacticsDebugBranch& Branch : Package.Branches)
		{
			ActionCount += Branch.bSelector ? 0 : 1;
		}
		const int32 SelectorCount = Package.Branches.Num() - ActionCount;
		for (int32 GroupIndex = 0; GroupIndex < 2; ++GroupIndex)
		{
			const bool bSelectorGroup = GroupIndex == 1;
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			const bool bGroupOpen = bSelectorGroup
				? ImGui::TreeNodeEx("##Selectors", GroupFlags, "Selectors (%d)", SelectorCount)
				: ImGui::TreeNodeEx("##Actions", GroupFlags, "Actions (%d)", ActionCount);
			if (!bGroupOpen)
			{
				continue;
			}
			for (int32 BranchIndex = 0; BranchIndex < Package.Branches.Num(); ++BranchIndex)
			{
				if (Package.Branches[BranchIndex].bSelector == bSelectorGroup)
				{
					DrawLiveBranch(Snapshot, Package.ConfigName, Package.Branches[BranchIndex], BranchIndex);
				}
			}
			ImGui::TreePop();
		}
		ImGui::EndTable();
	}

	void DrawLiveConfig(const FArcTacticsDebugSnapshot& Snapshot)
	{
		if (Snapshot.Packages.Num() == 0)
		{
			ImGui::TextDisabled("No decision packages");
			return;
		}
		for (int32 PackageIndex = 0; PackageIndex < Snapshot.Packages.Num(); ++PackageIndex)
		{
			const FArcTacticsDebugPackage& Package = Snapshot.Packages[PackageIndex];
			ImGui::PushID(PackageIndex);
			const FString Label = FString::Printf(TEXT("%d. %s%s"), PackageIndex + 1,
				Package.ConfigName.IsEmpty() ? TEXT("(no config)") : *Package.ConfigName,
				Package.bActive ? TEXT("") : TEXT("  [inactive]"));
			if (ImGui::TreeNodeEx("##Package", ImGuiTreeNodeFlags_DefaultOpen, "%s", TCHAR_TO_UTF8(*Label)))
			{
				DrawLivePackage(Snapshot, Package);
				ImGui::TreePop();
			}
			ImGui::PopID();
		}
	}

	void DrawLiveHistory(const FArcTacticsDebugSnapshot& Snapshot)
	{
		if (Snapshot.History.Num() == 0)
		{
			ImGui::TextDisabled("No history entries");
			return;
		}
		const ImGuiTableFlags TableFlags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp;
		if (!ImGui::BeginTable("LiveHistory", 2, TableFlags))
		{
			return;
		}
		ImGui::TableSetupColumn("Tag (newest first)", ImGuiTableColumnFlags_WidthStretch, 3.0f);
		ImGui::TableSetupColumn("Age", ImGuiTableColumnFlags_WidthStretch, 1.0f);
		ImGui::TableHeadersRow();
		for (const FArcTacticsDebugHistoryEntry& Entry : Snapshot.History)
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(TCHAR_TO_UTF8(*Entry.Tag.ToString()));
			ImGui::TableNextColumn();
			ImGui::Text("%.1f s ago", Entry.Age);
		}
		ImGui::EndTable();
	}

	void DrawLiveMemory(const FArcTacticsDebugSnapshot& Snapshot)
	{
		if (!Snapshot.bHasMemory)
		{
			ImGui::TextDisabled("No memory fragment");
			return;
		}
		if (Snapshot.Memory.Num() == 0)
		{
			ImGui::TextDisabled("No memory entries");
			return;
		}
		const ImGuiTableFlags TableFlags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp;
		if (!ImGui::BeginTable("LiveMemory", 3, TableFlags))
		{
			return;
		}
		ImGui::TableSetupColumn("Key", ImGuiTableColumnFlags_WidthStretch, 2.0f);
		ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthStretch, 1.0f);
		ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 3.0f);
		ImGui::TableHeadersRow();
		for (const FArcTacticsDebugMemoryEntry& Entry : Snapshot.Memory)
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(TCHAR_TO_UTF8(*Entry.Key.ToString()));
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(TCHAR_TO_UTF8(*Entry.TypeName));
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(TCHAR_TO_UTF8(*Entry.ValueText));
		}
		ImGui::EndTable();
	}

	void DrawKeyValueRow(const char* Key, const FString& Value)
	{
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::TextDisabled("%s", Key);
		ImGui::TableNextColumn();
		ImGui::TextUnformatted(TCHAR_TO_UTF8(*Value));
	}

	FString FormatEntityLabel(const FArcTacticsRecording& Recording, const FArcTacticsRecordedEntity& Entity)
	{
		FString Label;
		if (Recording.Worlds.Num() > 1)
		{
			const int32 WorldIndex = Recording.Worlds.IndexOfByPredicate([&Entity](const FArcTacticsRecordedWorld& World)
			{
				return World.WorldId == Entity.WorldId;
			});
			Label = FString::Printf(TEXT("W%d "), WorldIndex);
		}
		Label += FString::Printf(TEXT("%u:%u  %s  (%d)"),
			Entity.EntityIndex,
			Entity.EntitySerial,
			Entity.ConfigNames.IsEmpty() ? TEXT("(no config)") : *FString::Join(Entity.ConfigNames, TEXT(", ")),
			Entity.Decisions.Num());
		return Label;
	}

	double PickAxisStep(const double PixelsPerSecond)
	{
		static const double Steps[] = {0.01, 0.02, 0.05, 0.1, 0.2, 0.5, 1.0, 2.0, 5.0, 10.0, 20.0, 30.0, 60.0, 120.0, 300.0, 600.0};
		for (const double Step : Steps)
		{
			if (Step * PixelsPerSecond >= 70.0)
			{
				return Step;
			}
		}
		return Steps[UE_ARRAY_COUNT(Steps) - 1];
	}

	void DrawEndMarker(ImDrawList* DrawList, const float X, const float Top, const float Bottom, const FArcTacticsRecordedActionEnd& End)
	{
		const ImU32 Color = GetEndStatusColor(End.Status);
		const float Mid = (Top + Bottom) * 0.5f;
		const float Half = 5.0f;
		DrawList->AddLine(ImVec2(X, Top), ImVec2(X, Bottom), Color, 2.0f);
		switch (End.Reason)
		{
		case EArcTacticsActionEndReason::Completed:
			DrawList->AddRectFilled(ImVec2(X - 2.0f * Half, Mid - Half), ImVec2(X, Mid + Half), Color);
			break;
		case EArcTacticsActionEndReason::EnterEnded:
			DrawList->AddCircleFilled(ImVec2(X - Half, Mid), Half, Color);
			break;
		case EArcTacticsActionEndReason::Replaced:
			DrawList->AddTriangleFilled(ImVec2(X - 2.0f * Half, Mid - Half), ImVec2(X, Mid), ImVec2(X - 2.0f * Half, Mid + Half), Color);
			break;
		case EArcTacticsActionEndReason::Unresolved:
		case EArcTacticsActionEndReason::InputsFailed:
			DrawList->AddLine(ImVec2(X - 2.0f * Half, Mid - Half), ImVec2(X, Mid + Half), Color, 2.0f);
			DrawList->AddLine(ImVec2(X - 2.0f * Half, Mid + Half), ImVec2(X, Mid - Half), Color, 2.0f);
			break;
		case EArcTacticsActionEndReason::Teardown:
			DrawList->AddRectFilled(ImVec2(X - 4.0f, Top), ImVec2(X, Bottom), Color);
			break;
		case EArcTacticsActionEndReason::PackageRemoved:
			DrawList->AddRect(ImVec2(X - 2.0f * Half, Mid - Half), ImVec2(X, Mid + Half), Color, 0.0f, 2.0f);
			break;
		}
	}
}

void FArcTacticsDebugger::Initialize()
{
	CachedEntities.Reset();
	SelectedEntityIndex = INDEX_NONE;
	Filter[0] = '\0';
	LastRefreshTime = 0.0f;
	RefreshEntityList();
	ResetHistoryState();
	TraceRecorder.Initialize();
}

void FArcTacticsDebugger::Uninitialize()
{
	CachedEntities.Reset();
	SelectedEntityIndex = INDEX_NONE;
	ArcTacticsDebug::ClearCaptureTarget();
	ResetHistoryState();
	TraceRecorder.Uninitialize();
	DestroyDrawActor();
}

void FArcTacticsDebugger::Tick()
{
	TraceRecorder.Tick();
	if (!bDrawnThisFrame && bHasDrawShapes)
	{
		if (DrawComponent.IsValid())
		{
			DrawComponent->SetShapes(FArcTacticsDebugDrawData());
		}
		bHasDrawShapes = false;
	}
	bDrawnThisFrame = false;
}

void FArcTacticsDebugger::RefreshEntityList()
{
	const FMassEntityHandle Previous = CachedEntities.IsValidIndex(SelectedEntityIndex)
		? CachedEntities[SelectedEntityIndex].Entity : FMassEntityHandle();
	CachedEntities.Reset();
	SelectedEntityIndex = INDEX_NONE;
#if WITH_MASSENTITY_DEBUG
	FMassEntityManager* EntityManager = ArcTacticsDebuggerPrivate::GetEntityManager();
	if (!EntityManager)
	{
		return;
	}
	const UScriptStruct* FragmentType = FArcTacticsInstanceFragment::StaticStruct();
	const TArray<FMassArchetypeHandle> Archetypes = FMassDebugger::GetAllArchetypes(*EntityManager);
	for (const FMassArchetypeHandle& Archetype : Archetypes)
	{
		if (!FMassDebugger::GetArchetypeComposition(Archetype).Contains(FragmentType))
		{
			continue;
		}
		const TArray<FMassEntityHandle> Entities = FMassDebugger::GetEntitiesOfArchetype(Archetype);
		for (const FMassEntityHandle Entity : Entities)
		{
			FEntityEntry& Entry = CachedEntities.AddDefaulted_GetRef();
			Entry.Entity = Entity;
			Entry.Label = Entity.DebugGetDescription();
			if (Entity == Previous)
			{
				SelectedEntityIndex = CachedEntities.Num() - 1;
			}
		}
	}
#endif
}

void FArcTacticsDebugger::Draw()
{
	ImGui::SetNextWindowSize(ImVec2(1100.0f, 720.0f), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Tactics Debugger", &bShow))
	{
		ImGui::End();
		return;
	}
	if (ImGui::BeginTabBar("TacticsTabs"))
	{
		if (ImGui::BeginTabItem("Live"))
		{
			DrawLive();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("History"))
		{
			DrawHistory();
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}
	ImGui::End();
}

void FArcTacticsDebugger::DrawLive()
{
#if WITH_MASSENTITY_DEBUG
	UWorld* World = ArcTacticsDebuggerPrivate::GetWorld();
	if (!World || !ArcTacticsDebuggerPrivate::GetEntityManager())
	{
		ApplyDrawData(World, FArcTacticsDebugDrawData());
		ImGui::TextDisabled("No Mass world available");
		return;
	}
	if (World->GetTimeSeconds() - LastRefreshTime >= 2.0f)
	{
		LastRefreshTime = World->GetTimeSeconds();
		RefreshEntityList();
	}
	if (ImGui::Button("Refresh"))
	{
		RefreshEntityList();
	}
	ImGui::SameLine();
	ImGui::Text("Entities: %d", CachedEntities.Num());
	ImGui::InputText("Filter", Filter, IM_ARRAYSIZE(Filter));
	const FString FilterText = FString(ANSI_TO_TCHAR(Filter)).ToLower();
	const float ListWidth = ImGui::GetContentRegionAvail().x * 0.30f;
	if (ImGui::BeginChild("TacticsEntities", ImVec2(ListWidth, 0.0f), ImGuiChildFlags_Borders))
	{
		for (int32 EntityIndex = 0; EntityIndex < CachedEntities.Num(); ++EntityIndex)
		{
			const FEntityEntry& Entry = CachedEntities[EntityIndex];
			if (!FilterText.IsEmpty() && !Entry.Label.ToLower().Contains(FilterText))
			{
				continue;
			}
			if (ImGui::Selectable(TCHAR_TO_UTF8(*Entry.Label), SelectedEntityIndex == EntityIndex))
			{
				SelectedEntityIndex = EntityIndex;
				ArcTacticsDebug::SetCaptureTarget(*World, Entry.Entity);
			}
		}
	}
	ImGui::EndChild();
	ImGui::SameLine();

	FArcTacticsDebugSnapshot Snapshot;
	const bool bHasSnapshot = CachedEntities.IsValidIndex(SelectedEntityIndex)
		&& ArcTacticsDebug::BuildEntitySnapshot(*World, CachedEntities[SelectedEntityIndex].Entity, Snapshot);
	FArcTacticsDebugDrawData DrawData;
	if (bHasSnapshot)
	{
		BuildLiveDrawData(*World, CachedEntities[SelectedEntityIndex], Snapshot, DrawData);
	}
	ApplyDrawData(World, DrawData);
	if (bHasSnapshot)
	{
		TrackLiveActiveAction(*World, Snapshot);
	}

	if (ImGui::BeginChild("TacticsDetails", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders))
	{
		if (!CachedEntities.IsValidIndex(SelectedEntityIndex))
		{
			ImGui::TextDisabled("Select an ArcTactics entity");
		}
		else if (!bHasSnapshot)
		{
			ImGui::TextDisabled("Entity is no longer active");
		}
		else
		{
			DrawLiveSnapshot(Snapshot);
		}
	}
	ImGui::EndChild();
#else
	ImGui::TextDisabled("Live view requires WITH_MASSENTITY_DEBUG");
#endif
}

void FArcTacticsDebugger::DrawLiveSnapshot(const FArcTacticsDebugSnapshot& Snapshot) const
{
	using namespace ArcTacticsDebuggerPrivate;

	ImGui::Text("E%d:%d   %d packages   pool %d:%u",
		Snapshot.Entity.Index,
		Snapshot.Entity.SerialNumber,
		Snapshot.Packages.Num(),
		Snapshot.Pool.Index,
		Snapshot.Pool.Generation);
	if (!Snapshot.bEnabled)
	{
		ImGui::SameLine();
		ImGui::TextColored(WarningTextColor, "  disabled");
	}
	if (!Snapshot.bPoolValid)
	{
		ImGui::TextColored(ErrorTextColor, "Pool handle is not valid");
		return;
	}

	if (ImGui::CollapsingHeader("Decision", ImGuiTreeNodeFlags_DefaultOpen))
	{
		DrawLiveDecision(Snapshot);
	}
	if (ImGui::CollapsingHeader("Active action", ImGuiTreeNodeFlags_DefaultOpen))
	{
		DrawLiveActiveAction(Snapshot);
	}
	if (ImGui::CollapsingHeader("Config", ImGuiTreeNodeFlags_DefaultOpen))
	{
		DrawLiveConfig(Snapshot);
	}
	if (ImGui::CollapsingHeader("History", ImGuiTreeNodeFlags_DefaultOpen))
	{
		DrawLiveHistory(Snapshot);
	}
	if (ImGui::CollapsingHeader("Memory", ImGuiTreeNodeFlags_DefaultOpen))
	{
		DrawLiveMemory(Snapshot);
	}
}

void FArcTacticsDebugger::DrawLiveActiveAction(const FArcTacticsDebugSnapshot& Snapshot) const
{
	using namespace ArcTacticsDebuggerPrivate;

	const FArcTacticsDebugEndedAction& Ended = Snapshot.LastEndedAction;
	if (Ended.bSet)
	{
		ImGui::TextDisabled("Last ended:");
		ImGui::SameLine();
		const FString Reason = Ended.FailureReason.IsNone()
			? FString(ArcTacticsDebug::EndReasonText(Ended.Reason))
			: FString::Printf(TEXT("%s, %s"), ArcTacticsDebug::EndReasonText(Ended.Reason), *Ended.FailureReason.ToString());
		ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(GetEndStatusColor(Ended.Status)), "%s / %s  %s (%s)  t=%.2f s (%.2f s ago)",
			TCHAR_TO_UTF8(*Ended.Package), TCHAR_TO_UTF8(*Ended.Name.ToString()),
			TCHAR_TO_UTF8(ArcTacticsDebug::EndStatusText(Ended.Status)), TCHAR_TO_UTF8(*Reason),
			Ended.Time, Snapshot.Now - Ended.Time);
	}
	else
	{
		ImGui::TextDisabled("Last ended: none");
	}

	const FArcTacticsDebugActiveAction& Active = Snapshot.ActiveAction;
	if (!Active.bSet)
	{
		ImGui::TextDisabled("No active action");
		return;
	}
	ImGui::TextColored(GoodTextColor, "%s / %s%s", TCHAR_TO_UTF8(*Active.Package), TCHAR_TO_UTF8(*Active.Name.ToString()),
		Active.bFromSelector ? " [selector]" : "");
	if (!Active.TypeName.IsEmpty())
	{
		ImGui::SameLine();
		ImGui::TextDisabled("(%s)", TCHAR_TO_UTF8(*Active.TypeName));
	}
	if (!Active.bEntered)
	{
		ImGui::SameLine();
		ImGui::TextColored(WarningTextColor, "  not entered");
	}

	const ImGuiTableFlags TableFlags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp;
	if (!ImGui::BeginTable("LiveActive", 2, TableFlags))
	{
		return;
	}
	ImGui::TableSetupColumn("Key", ImGuiTableColumnFlags_WidthStretch, 1.0f);
	ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 4.0f);

	FString TargetText = FormatIdentity(Active.Target);
	if (TargetText.IsEmpty())
	{
		TargetText = TEXT("none");
	}
	else if (Active.Target.Type == EArcTacticsValueIdentity::Entity)
	{
		TargetText += Active.bHasTargetLocation
			? FString::Printf(TEXT(" at (%.0f, %.0f, %.0f)"), Active.TargetLocation.X, Active.TargetLocation.Y, Active.TargetLocation.Z)
			: FString(TEXT(" (inactive)"));
	}
	DrawKeyValueRow("Target", TargetText);
	DrawKeyValueRow("Priority", FString::FromInt(Active.Priority));
	DrawKeyValueRow("Score", FString::Printf(TEXT("%.4f"), Active.Score));

	FString StepText;
	if (Active.StepCount == 0)
	{
		StepText = TEXT("no steps");
	}
	else if (Active.StepIndex >= 0 && Active.StepIndex < Active.StepCount)
	{
		StepText = FString::Printf(TEXT("%d/%d %s"), Active.StepIndex + 1, Active.StepCount, *Active.StepName.ToString());
		if (!Active.StepTypeName.IsEmpty() && Active.StepTypeName != Active.StepName.ToString())
		{
			StepText += FString::Printf(TEXT(" (%s)"), *Active.StepTypeName);
		}
	}
	else
	{
		StepText = FString::Printf(TEXT("none of %d"), Active.StepCount);
	}
	DrawKeyValueRow("Step", StepText);
	DrawKeyValueRow("Lane", GetEnumName(Active.Lane));
	DrawKeyValueRow("Blocks decision", Active.bBlocksDecision ? TEXT("yes") : TEXT("no"));
	DrawKeyValueRow("Wake", Active.WakeTime > 0.0 ? FormatTimeFromNow(Active.WakeTime, Snapshot.Now) : FString(TEXT("none")));
	DrawKeyValueRow("Hold", Active.bHoldOwned ? FString::Printf(TEXT("owned (action %u)"), Active.HoldActionID) : FString(TEXT("free")));

	FString ElapsedText = TEXT("-");
	if (LiveTrackedSerial == Active.Serial)
	{
		const double Elapsed = Snapshot.Now - LiveTrackedSince;
		ElapsedText = bLiveTrackedFromEnter
			? FString::Printf(TEXT("%.2f s"), Elapsed)
			: FString::Printf(TEXT("at least %.2f s (running when first shown)"), Elapsed);
	}
	DrawKeyValueRow("Elapsed", ElapsedText);
	DrawKeyValueRow("Serial", FString::Printf(TEXT("%u"), Active.Serial));
	ImGui::EndTable();
}

void FArcTacticsDebugger::TrackLiveActiveAction(UWorld& World, const FArcTacticsDebugSnapshot& Snapshot)
{
	const uint32 Serial = Snapshot.ActiveAction.bSet ? Snapshot.ActiveAction.Serial : 0;
	const bool bContinuous = LiveTrackedWorld.Get() == &World
		&& LiveTrackedEntity == Snapshot.Entity
		&& LiveTrackedFrame + 1 >= GFrameCounter;
	if (!bContinuous)
	{
		LiveTrackedSerial = Serial;
		LiveTrackedSince = Snapshot.Now;
		bLiveTrackedFromEnter = false;
	}
	else if (Serial != LiveTrackedSerial)
	{
		LiveTrackedSerial = Serial;
		LiveTrackedSince = Snapshot.Now;
		bLiveTrackedFromEnter = true;
	}
	LiveTrackedWorld = &World;
	LiveTrackedEntity = Snapshot.Entity;
	LiveTrackedFrame = GFrameCounter;
}

void FArcTacticsDebugger::DrawHistory()
{
	DrawHistoryToolbar();
	ImGui::Separator();

	const FArcTacticsRecording* Recording = TraceRecorder.GetRecording();
	SyncHistoryRecording(Recording);
	UWorld* World = ArcTacticsDebuggerPrivate::GetWorld();
	FArcTacticsDebugDrawData DrawData;
	if (Recording && Recording->Entities.IsValidIndex(HistoryEntityIndex))
	{
		BuildHistoryDrawData(World, Recording->Entities[HistoryEntityIndex], DrawData);
	}
	ApplyDrawData(World, DrawData);
	if (!Recording)
	{
		ImGui::TextDisabled("No recording loaded. Record or load a .utrace file.");
		return;
	}

	ImGui::Text("%s  |  %.2f s  |  %d entities  |  %d worlds",
		TCHAR_TO_UTF8(*FPaths::GetCleanFilename(Recording->FilePath)),
		Recording->Duration,
		Recording->Entities.Num(),
		Recording->Worlds.Num());
	if (Recording->UnclaimedCandidateCount > 0)
	{
		ImGui::SameLine();
		ImGui::TextColored(ArcTacticsDebuggerPrivate::WarningTextColor, "%d candidates had no following decision", Recording->UnclaimedCandidateCount);
	}
	if (Recording->UnclaimedQueryCount > 0)
	{
		ImGui::SameLine();
		ImGui::TextColored(ArcTacticsDebuggerPrivate::WarningTextColor, "%d global task queries had no following decision", Recording->UnclaimedQueryCount);
	}

	const float ListWidth = ImGui::GetContentRegionAvail().x * 0.22f;
	if (ImGui::BeginChild("HistoryEntities", ImVec2(ListWidth, 0.0f), ImGuiChildFlags_Borders))
	{
		DrawHistoryEntityList(*Recording);
	}
	ImGui::EndChild();
	ImGui::SameLine();
	if (ImGui::BeginChild("HistoryMain", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders))
	{
		if (Recording->Entities.IsValidIndex(HistoryEntityIndex))
		{
			const FArcTacticsRecordedEntity& Entity = Recording->Entities[HistoryEntityIndex];
			DrawHistoryTimeline(*Recording, Entity);
			const float DetailHeight = ImGui::GetContentRegionAvail().y * 0.5f;
			if (ImGui::BeginChild("HistoryDecision", ImVec2(0.0f, DetailHeight), ImGuiChildFlags_Borders))
			{
				DrawHistoryDecision(Entity);
			}
			ImGui::EndChild();
			if (ImGui::BeginChild("HistoryRuns", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders))
			{
				DrawHistoryRuns(Entity);
			}
			ImGui::EndChild();
		}
		else
		{
			ImGui::TextDisabled("Select a recorded entity");
		}
	}
	ImGui::EndChild();
}

void FArcTacticsDebugger::DrawHistoryToolbar()
{
	const EArcTacticsTraceState State = TraceRecorder.GetState();
	if (State == EArcTacticsTraceState::Unavailable)
	{
		ImGui::TextDisabled("%s", TCHAR_TO_UTF8(*TraceRecorder.GetStatusText()));
		return;
	}
	const bool bIdle = State == EArcTacticsTraceState::Idle;

	ImGui::BeginDisabled(!bIdle || bHistoryRecordUntilStop);
	ImGui::SetNextItemWidth(70.0f);
	ImGui::InputFloat("s##HistoryRecordSeconds", &HistoryRecordSeconds, 0.0f, 0.0f, "%.1f");
	ImGui::EndDisabled();
	HistoryRecordSeconds = FMath::Max(HistoryRecordSeconds, 0.5f);
	ImGui::SameLine();
	ImGui::BeginDisabled(!bIdle);
	ImGui::Checkbox("Until Stop", &bHistoryRecordUntilStop);
	ImGui::SameLine();
	if (ImGui::Button("Record"))
	{
		TraceRecorder.StartRecording(bHistoryRecordUntilStop ? 0.0f : HistoryRecordSeconds);
	}
	ImGui::EndDisabled();
	ImGui::SameLine();
	ImGui::BeginDisabled(State != EArcTacticsTraceState::Recording);
	if (ImGui::Button("Stop"))
	{
		TraceRecorder.StopRecording();
	}
	ImGui::EndDisabled();

	ImGui::SameLine();
	ImGui::TextDisabled("|");
	ImGui::SameLine();
	const TArray<FString>& Files = TraceRecorder.GetFiles();
	if (!Files.Contains(HistorySelectedFile))
	{
		HistorySelectedFile = Files.Num() > 0 ? Files[0] : FString();
	}
	const FString Preview = HistorySelectedFile.IsEmpty() ? FString(TEXT("(no traces)")) : FPaths::GetCleanFilename(HistorySelectedFile);
	ImGui::SetNextItemWidth(320.0f);
	if (ImGui::BeginCombo("##HistoryFiles", TCHAR_TO_UTF8(*Preview)))
	{
		for (const FString& File : Files)
		{
			const bool bSelected = File == HistorySelectedFile;
			if (ImGui::Selectable(TCHAR_TO_UTF8(*FPaths::GetCleanFilename(File)), bSelected))
			{
				HistorySelectedFile = File;
			}
			if (bSelected)
			{
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}
	ImGui::SameLine();
	ImGui::BeginDisabled(!bIdle || HistorySelectedFile.IsEmpty());
	if (ImGui::Button("Load"))
	{
		TraceRecorder.LoadFile(HistorySelectedFile);
	}
	ImGui::EndDisabled();
	ImGui::SameLine();
	ImGui::BeginDisabled(!bIdle);
	if (ImGui::Button("Refresh##HistoryFiles"))
	{
		TraceRecorder.RefreshFiles();
	}
	ImGui::EndDisabled();

	if (State == EArcTacticsTraceState::Recording)
	{
		if (TraceRecorder.GetRequestedDuration() > 0.0f)
		{
			ImGui::Text("Recording %.1f / %.1f s", TraceRecorder.GetRecordingElapsed(), TraceRecorder.GetRequestedDuration());
		}
		else
		{
			ImGui::Text("Recording %.1f s (until Stop)", TraceRecorder.GetRecordingElapsed());
		}
		ImGui::SameLine();
	}
	const FString& StatusText = TraceRecorder.GetStatusText();
	if (StatusText.IsEmpty())
	{
		ImGui::TextDisabled("Idle");
	}
	else if (TraceRecorder.HasError())
	{
		ImGui::TextColored(ArcTacticsDebuggerPrivate::ErrorTextColor, "%s", TCHAR_TO_UTF8(*StatusText));
	}
	else
	{
		ImGui::TextUnformatted(TCHAR_TO_UTF8(*StatusText));
	}
}

void FArcTacticsDebugger::DrawHistoryEntityList(const FArcTacticsRecording& Recording)
{
	ImGui::SetNextItemWidth(-1.0f);
	ImGui::InputTextWithHint("##HistoryFilter", "Filter", HistoryFilter, IM_ARRAYSIZE(HistoryFilter));
	const FString FilterText = FString(UTF8_TO_TCHAR(HistoryFilter)).ToLower();
	for (int32 EntityIndex = 0; EntityIndex < Recording.Entities.Num(); ++EntityIndex)
	{
		const FString Label = ArcTacticsDebuggerPrivate::FormatEntityLabel(Recording, Recording.Entities[EntityIndex]);
		if (!FilterText.IsEmpty() && !Label.ToLower().Contains(FilterText))
		{
			continue;
		}
		const FString SelectableLabel = FString::Printf(TEXT("%s##HistoryEntity%d"), *Label, EntityIndex);
		if (ImGui::Selectable(TCHAR_TO_UTF8(*SelectableLabel), EntityIndex == HistoryEntityIndex))
		{
			SelectHistoryEntity(EntityIndex, Recording);
		}
	}
}

void FArcTacticsDebugger::DrawHistoryTimeline(const FArcTacticsRecording& Recording, const FArcTacticsRecordedEntity& Entity)
{
	using namespace ArcTacticsDebuggerPrivate;

	if (ImGui::Button("Fit"))
	{
		FitHistoryView(Recording);
	}
	ImGui::SameLine();
	ImGui::BeginDisabled(Entity.Decisions.Num() == 0);
	if (ImGui::ArrowButton("##HistoryPrevDecision", ImGuiDir_Left))
	{
		SelectHistoryDecision(Entity.Decisions.IsValidIndex(HistoryDecisionIndex)
			? FMath::Max(HistoryDecisionIndex - 1, 0) : Entity.Decisions.Num() - 1);
	}
	ImGui::SameLine();
	if (ImGui::ArrowButton("##HistoryNextDecision", ImGuiDir_Right))
	{
		SelectHistoryDecision(Entity.Decisions.IsValidIndex(HistoryDecisionIndex)
			? FMath::Min(HistoryDecisionIndex + 1, Entity.Decisions.Num() - 1) : 0);
	}
	ImGui::EndDisabled();
	ImGui::SameLine();
	ImGui::TextDisabled("Wheel zoom, drag pan, click a run or a decision tick to select  |  %.2f - %.2f s",
		HistoryViewStart, HistoryViewStart + HistoryViewDuration);

	const float CanvasWidth = FMath::Max(ImGui::GetContentRegionAvail().x, TimelineGutter + 60.0f);
	const float CanvasHeight = TimelineAxisHeight + TimelineActionLaneHeight + TimelineDecisionLaneHeight + 6.0f;
	const ImVec2 CanvasMin = ImGui::GetCursorScreenPos();
	const ImVec2 CanvasMax(CanvasMin.x + CanvasWidth, CanvasMin.y + CanvasHeight);
	ImGui::InvisibleButton("HistoryTimeline", ImVec2(CanvasWidth, CanvasHeight));
	ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelY);
	const bool bHovered = ImGui::IsItemHovered();
	const bool bActive = ImGui::IsItemActive();
	const bool bReleased = ImGui::IsItemDeactivated();
	const ImGuiIO& IO = ImGui::GetIO();

	const float PlotLeft = CanvasMin.x + TimelineGutter;
	const float PlotRight = CanvasMax.x - 4.0f;
	const float PlotWidth = FMath::Max(PlotRight - PlotLeft, 1.0f);
	const double MaxViewDuration = FMath::Max(Recording.Duration, 1.0) * 4.0;

	if (bHovered && IO.MouseWheel != 0.0f && IO.MousePos.x >= PlotLeft)
	{
		const double MouseFraction = static_cast<double>(IO.MousePos.x - PlotLeft) / PlotWidth;
		const double MouseTime = HistoryViewStart + MouseFraction * HistoryViewDuration;
		const double NewDuration = FMath::Clamp(
			HistoryViewDuration * FMath::Pow(0.8, static_cast<double>(IO.MouseWheel)),
			TimelineMinViewDuration,
			MaxViewDuration);
		HistoryViewStart = MouseTime - MouseFraction * NewDuration;
		HistoryViewDuration = NewDuration;
	}
	if (bActive && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 3.0f))
	{
		bHistoryPanning = true;
		HistoryViewStart -= static_cast<double>(IO.MouseDelta.x) / PlotWidth * HistoryViewDuration;
	}

	FTimelineMapping Mapping;
	Mapping.PlotLeft = PlotLeft;
	Mapping.PlotWidth = PlotWidth;
	Mapping.ViewStart = HistoryViewStart;
	Mapping.ViewDuration = HistoryViewDuration;
	const double ViewEnd = HistoryViewStart + HistoryViewDuration;

	const float ActionTop = CanvasMin.y + TimelineAxisHeight;
	const float ActionBottom = ActionTop + TimelineActionLaneHeight;
	const float DecisionTop = ActionBottom + 2.0f;
	const float DecisionBottom = DecisionTop + TimelineDecisionLaneHeight;

	int32 HoveredSpan = INDEX_NONE;
	int32 HoveredDecision = INDEX_NONE;
	if (bHovered && !bHistoryPanning && IO.MousePos.x >= PlotLeft)
	{
		const ImVec2 Mouse = IO.MousePos;
		if (Mouse.y >= ActionTop && Mouse.y < ActionBottom)
		{
			for (int32 SpanIndex = 0; SpanIndex < Entity.Actions.Num(); ++SpanIndex)
			{
				const FArcTacticsRecordedActionSpan& Span = Entity.Actions[SpanIndex];
				const float X0 = Mapping.ToX(Span.StartTime);
				const float X1 = FMath::Max(Mapping.ToX(Span.EndTime), X0 + 2.0f);
				if (Mouse.x >= X0 && Mouse.x <= X1)
				{
					HoveredSpan = SpanIndex;
				}
			}
		}
		else if (Mouse.y >= DecisionTop && Mouse.y < DecisionBottom)
		{
			float BestDistance = 5.0f;
			for (int32 DecisionIndex = 0; DecisionIndex < Entity.Decisions.Num(); ++DecisionIndex)
			{
				const float Distance = FMath::Abs(Mapping.ToX(Entity.Decisions[DecisionIndex].Time) - Mouse.x);
				if (Distance <= BestDistance)
				{
					BestDistance = Distance;
					HoveredDecision = DecisionIndex;
				}
			}
		}
	}

	if (bReleased)
	{
		if (!bHistoryPanning)
		{
			if (HoveredDecision != INDEX_NONE)
			{
				SelectHistoryDecision(HoveredDecision);
			}
			else if (HoveredSpan != INDEX_NONE)
			{
				SelectHistorySpan(HoveredSpan, true);
			}
		}
		bHistoryPanning = false;
	}

	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	DrawList->PushClipRect(CanvasMin, CanvasMax, true);
	DrawList->AddRectFilled(CanvasMin, CanvasMax, IM_COL32(24, 24, 28, 255));
	DrawList->AddRectFilled(ImVec2(PlotLeft, ActionTop), ImVec2(PlotRight, ActionBottom), IM_COL32(34, 34, 40, 255));
	DrawList->AddRectFilled(ImVec2(PlotLeft, DecisionTop), ImVec2(PlotRight, DecisionBottom), IM_COL32(34, 34, 40, 255));

	DrawList->PushClipRect(ImVec2(PlotLeft, CanvasMin.y), ImVec2(PlotRight, CanvasMax.y), true);
	const float RecordingStartX = Mapping.ToX(0.0);
	const float RecordingEndX = Mapping.ToX(Recording.Duration);
	if (RecordingStartX > PlotLeft)
	{
		DrawList->AddRectFilled(ImVec2(PlotLeft, ActionTop), ImVec2(RecordingStartX, DecisionBottom), IM_COL32(0, 0, 0, 110));
	}
	if (RecordingEndX < PlotRight)
	{
		DrawList->AddRectFilled(ImVec2(RecordingEndX, ActionTop), ImVec2(PlotRight, DecisionBottom), IM_COL32(0, 0, 0, 110));
	}

	const double AxisStep = PickAxisStep(PlotWidth / HistoryViewDuration);
	const double FirstTick = FMath::CeilToDouble(HistoryViewStart / AxisStep) * AxisStep;
	for (int32 TickIndex = 0; TickIndex < 512; ++TickIndex)
	{
		const double TickTime = FirstTick + AxisStep * TickIndex;
		if (TickTime > ViewEnd)
		{
			break;
		}
		const float X = Mapping.ToX(TickTime);
		DrawList->AddLine(ImVec2(X, ActionTop - 4.0f), ImVec2(X, DecisionBottom), IM_COL32(70, 70, 80, 255));
		const FString TickLabel = AxisStep < 1.0 ? FString::Printf(TEXT("%.2fs"), TickTime) : FString::Printf(TEXT("%.0fs"), TickTime);
		DrawList->AddText(ImVec2(X + 2.0f, CanvasMin.y + 1.0f), IM_COL32(170, 170, 180, 255), TCHAR_TO_UTF8(*TickLabel));
	}

	const float FontHeight = ImGui::GetFontSize();
	for (int32 SpanIndex = 0; SpanIndex < Entity.Actions.Num(); ++SpanIndex)
	{
		const FArcTacticsRecordedActionSpan& Span = Entity.Actions[SpanIndex];
		const float X0 = Mapping.ToX(Span.StartTime);
		const float X1 = FMath::Max(Mapping.ToX(Span.EndTime), X0 + 1.0f);
		if (X1 < PlotLeft || X0 > PlotRight)
		{
			continue;
		}
		const ImVec2 BarMin(X0, ActionTop + 3.0f);
		const ImVec2 BarMax(X1, ActionBottom - 3.0f);
		DrawList->AddRectFilled(BarMin, BarMax, GetActionColor(Span.Name, Span.bEnteredInRecording ? 0.9f : 0.4f), 2.0f);
		if (SpanIndex == HoveredSpan || SpanIndex == HistorySpanIndex)
		{
			DrawList->AddRect(BarMin, BarMax, IM_COL32(255, 255, 255, 255), 2.0f, SpanIndex == HistorySpanIndex ? 2.5f : 1.5f);
		}
		if (X1 - X0 > 24.0f)
		{
			FString Label = GetSpanName(Span);
			const FString TargetText = Span.bEnteredInRecording ? FormatTarget(Span.Target) : FString();
			if (!TargetText.IsEmpty())
			{
				Label += TEXT(" -> ") + TargetText;
			}
			const float TextLeft = FMath::Max(X0, PlotLeft) + 4.0f;
			DrawList->PushClipRect(ImVec2(TextLeft, BarMin.y), ImVec2(FMath::Min(X1, PlotRight) - 3.0f, BarMax.y), true);
			DrawList->AddText(ImVec2(TextLeft, ActionTop + (TimelineActionLaneHeight - FontHeight) * 0.5f),
				IM_COL32(15, 15, 15, 255), TCHAR_TO_UTF8(*Label));
			DrawList->PopClipRect();
		}
	}

	for (const FArcTacticsRecordedActionSpan& Span : Entity.Actions)
	{
		const float StartX = Mapping.ToX(Span.StartTime);
		const float EndX = Mapping.ToX(Span.EndTime);
		const float Top = ActionTop + 3.0f;
		const float Bottom = ActionBottom - 3.0f;
		const float Mid = (Top + Bottom) * 0.5f;
		if (!Span.bEnteredInRecording)
		{
			DrawList->AddTriangleFilled(ImVec2(StartX + 1.0f, Mid), ImVec2(StartX + 8.0f, Top + 2.0f), ImVec2(StartX + 8.0f, Bottom - 2.0f), IM_COL32(230, 230, 230, 220));
		}
		if (Span.bOpenAtRecordingEnd)
		{
			DrawList->AddTriangle(ImVec2(EndX - 8.0f, Top + 2.0f), ImVec2(EndX - 1.0f, Mid), ImVec2(EndX - 8.0f, Bottom - 2.0f), IM_COL32(230, 230, 230, 220), 2.0f);
		}
		else if (Span.bEnded)
		{
			DrawEndMarker(DrawList, EndX, Top, Bottom, Span.End);
		}
	}

	for (int32 DecisionIndex = 0; DecisionIndex < Entity.Decisions.Num(); ++DecisionIndex)
	{
		const FArcTacticsRecordedDecision& Decision = Entity.Decisions[DecisionIndex];
		const float X = Mapping.ToX(Decision.Time);
		if (X < PlotLeft - 4.0f || X > PlotRight + 4.0f)
		{
			continue;
		}
		const EHistoryDecisionChange Change = HistoryDecisionChanges.IsValidIndex(DecisionIndex)
			? HistoryDecisionChanges[DecisionIndex] : EHistoryDecisionChange::Unknown;
		ImU32 Color = IM_COL32(130, 130, 130, 255);
		float Top = DecisionTop + 6.0f;
		float Thickness = 2.0f;
		switch (Change)
		{
		case EHistoryDecisionChange::Changed:
			Color = IM_COL32(255, 200, 40, 255);
			Top = DecisionTop + 1.0f;
			Thickness = 3.0f;
			break;
		case EHistoryDecisionChange::Unknown:
			Color = IM_COL32(170, 170, 210, 255);
			break;
		default:
			break;
		}
		if (DecisionIndex == HoveredDecision || DecisionIndex == HistoryDecisionIndex)
		{
			Color = IM_COL32(255, 255, 255, 255);
		}
		DrawList->AddLine(ImVec2(X, Top), ImVec2(X, DecisionBottom - 1.0f), Color, Thickness);
		if (Decision.Status != EArcTacticsDecisionStatus::Succeeded)
		{
			DrawList->AddCircleFilled(ImVec2(X, DecisionBottom - 3.0f), 2.5f, IM_COL32(235, 60, 60, 255));
		}
	}

	if (Entity.Decisions.IsValidIndex(HistoryDecisionIndex))
	{
		const float X = Mapping.ToX(Entity.Decisions[HistoryDecisionIndex].Time);
		DrawList->AddLine(ImVec2(X, ActionTop), ImVec2(X, DecisionBottom), IM_COL32(255, 255, 255, 160), 1.0f);
	}
	DrawList->PopClipRect();

	DrawList->AddRectFilled(CanvasMin, ImVec2(PlotLeft, CanvasMax.y), IM_COL32(30, 30, 36, 255));
	DrawList->AddText(ImVec2(CanvasMin.x + 4.0f, ActionTop + (TimelineActionLaneHeight - FontHeight) * 0.5f), IM_COL32(200, 200, 200, 255), "Actions");
	DrawList->AddText(ImVec2(CanvasMin.x + 4.0f, DecisionTop + (TimelineDecisionLaneHeight - FontHeight) * 0.5f), IM_COL32(200, 200, 200, 255), "Decisions");
	DrawList->PopClipRect();

	if (HoveredSpan != INDEX_NONE && ImGui::BeginTooltip())
	{
		const FArcTacticsRecordedActionSpan& Span = Entity.Actions[HoveredSpan];
		ImGui::TextUnformatted(TCHAR_TO_UTF8(*GetSpanName(Span)));
		ImGui::Text("Serial %u  source %d", Span.ActionSerial, Span.SourceIndex);
		if (Span.bEnteredInRecording)
		{
			FString TargetText = FormatTarget(Span.Target);
			if (Span.Target.Type == EArcTacticsValueIdentity::Entity && Span.Target.bHasLocation)
			{
				TargetText += FString::Printf(TEXT(" at (%.0f, %.0f, %.0f)"), Span.Target.Location.X, Span.Target.Location.Y, Span.Target.Location.Z);
			}
			ImGui::Text("Target %s", TCHAR_TO_UTF8(*TargetText));
		}
		else
		{
			ImGui::TextDisabled("Target unknown");
		}
		ImGui::TextUnformatted(TCHAR_TO_UTF8(*FormatSpanTime(Span)));
		const FArcTacticsRecordedActionEvent* LastStep = nullptr;
		if (HistorySpanEvents.IsValidIndex(HoveredSpan))
		{
			const TArray<int32>& SpanEvents = HistorySpanEvents[HoveredSpan];
			for (int32 Slot = SpanEvents.Num() - 1; Slot >= 0; --Slot)
			{
				const FArcTacticsRecordedActionEvent& Event = Entity.Events[SpanEvents[Slot]];
				if (Event.Type == EArcTacticsRecordedActionEventType::StepChanged)
				{
					LastStep = &Event;
					break;
				}
			}
		}
		if (LastStep)
		{
			ImGui::Text("Last %s", TCHAR_TO_UTF8(*FormatStepLabel(*LastStep)));
		}
		if (Span.bOpenAtRecordingEnd)
		{
			ImGui::TextColored(WarningTextColor, "%s", TCHAR_TO_UTF8(*FormatSpanEnd(Entity, HistorySpanDecisions, HoveredSpan, true)));
		}
		else
		{
			ImGui::TextUnformatted(TCHAR_TO_UTF8(*FormatSpanEnd(Entity, HistorySpanDecisions, HoveredSpan, true)));
		}
		ImGui::EndTooltip();
	}
	else if (HoveredDecision != INDEX_NONE && ImGui::BeginTooltip())
	{
		const FArcTacticsRecordedDecision& Decision = Entity.Decisions[HoveredDecision];
		const FArcTacticsRecordedCandidate* Winner = Decision.FindSelectedCandidate();
		ImGui::Text("Decision %d at %.3f s (generation %u)", HoveredDecision + 1, Decision.Time, Decision.DecisionGeneration);
		ImGui::Text("Status %s, %d candidates", TCHAR_TO_UTF8(*GetEnumName(Decision.Status)), Decision.Candidates.Num());
		if (Winner)
		{
			ImGui::Text("Winner %s %s  score %.4f",
				TCHAR_TO_UTF8(*FormatCandidateName(*Winner)),
				TCHAR_TO_UTF8(*FormatTarget(Winner->Target)),
				Winner->Score);
		}
		else
		{
			ImGui::TextDisabled("No winner");
		}
		if (HistoryDecisionChanges.IsValidIndex(HoveredDecision))
		{
			switch (HistoryDecisionChanges[HoveredDecision])
			{
			case EHistoryDecisionChange::Changed:
				ImGui::TextColored(WarningTextColor, "Selected an action");
				break;
			case EHistoryDecisionChange::Unknown:
				ImGui::TextDisabled("Active action entered before the recording");
				break;
			default:
				break;
			}
		}
		if (!Decision.Diagnostic.IsEmpty())
		{
			ImGui::TextColored(WarningTextColor, "%s", TCHAR_TO_UTF8(*Decision.Diagnostic));
		}
		ImGui::EndTooltip();
	}

	ImGui::TextDisabled("End marker colour: green Succeeded, red Failed, orange Cancelled. Shape: square Completed, circle Enter ended immediately, "
		"triangle Replaced, cross Unresolved/Inputs failed, bar Teardown, hollow square Package removed. "
		"Faded bar: started before recording; hollow arrow: still running at end. Yellow tick: decision selected an action.");
	ImGui::TextDisabled("Selected query in the viewport: items red (low) to green (high) by score among valid items, grey filtered; "
		"large labelled point: winner; medium point: other selected result; blue: querier.");
}

void FArcTacticsDebugger::DrawHistoryDecision(const FArcTacticsRecordedEntity& Entity)
{
	using namespace ArcTacticsDebuggerPrivate;

	if (!Entity.Decisions.IsValidIndex(HistoryDecisionIndex))
	{
		ImGui::TextDisabled("Click a decision tick to inspect it (%d decisions)", Entity.Decisions.Num());
		return;
	}
	const FArcTacticsRecordedDecision& Decision = Entity.Decisions[HistoryDecisionIndex];
	const FArcTacticsRecordedCandidate* Winner = Decision.FindSelectedCandidate();

	ImGui::Text("Decision %d / %d   t = %.3f s   generation %u",
		HistoryDecisionIndex + 1,
		Entity.Decisions.Num(),
		Decision.Time,
		Decision.DecisionGeneration);
	ImGui::Text("Mode %s   Status %s",
		TCHAR_TO_UTF8(*GetEnumName(Decision.SelectionMode)),
		TCHAR_TO_UTF8(*GetEnumName(Decision.Status)));
	for (const FArcTacticsRecordedDecisionPackage& Package : Decision.Packages)
	{
		const FString PackageText = FString::Printf(TEXT("Package %s   MinScore %.3f%s%s%s"),
			*Package.ConfigName, Package.MinScore,
			Package.bActive ? TEXT("") : TEXT("   inactive"),
			Package.bExcluded && Package.bActive ? TEXT("   excluded") : TEXT(""),
			Package.Diagnostic.IsEmpty() ? TEXT("") : *(TEXT("   ") + Package.Diagnostic));
		if (Package.bActive && !Package.bExcluded)
		{
			ImGui::TextUnformatted(TCHAR_TO_UTF8(*PackageText));
		}
		else
		{
			ImGui::TextColored(WarningTextColor, "%s", TCHAR_TO_UTF8(*PackageText));
		}
	}
	if (Winner)
	{
		ImGui::TextColored(GoodTextColor, "Winner %s %s   order %d   score %.4f",
			TCHAR_TO_UTF8(*FormatCandidateName(*Winner)),
			TCHAR_TO_UTF8(*FormatTarget(Winner->Target)),
			Winner->Order,
			Winner->Score);
	}
	else if (Decision.SelectedOrder != INDEX_NONE)
	{
		ImGui::TextColored(ErrorTextColor, "Winner order %d has no recorded candidate", Decision.SelectedOrder);
	}
	else
	{
		ImGui::TextDisabled("Winner none");
	}
	if (Decision.Position.bValid)
	{
		ImGui::Text("Position %s", TCHAR_TO_UTF8(*FormatLocation(Decision.Position.Location)));
	}
	if (!Decision.Diagnostic.IsEmpty())
	{
		ImGui::TextColored(WarningTextColor, "Diagnostic %s", TCHAR_TO_UTF8(*Decision.Diagnostic));
	}
	ImGui::Separator();

	if (Decision.Candidates.Num() == 0)
	{
		ImGui::TextDisabled("No candidates recorded");
	}
	else
	{
		TArray<int32> SortedCandidates;
		SortCandidatesByBucket(Decision.Candidates, SortedCandidates);
		if (BeginCandidateTable("HistoryCandidates"))
		{
			for (int32 SortedIndex = 0; SortedIndex < SortedCandidates.Num(); ++SortedIndex)
			{
				DrawPriorityBucketRowIfNew(Decision.Candidates, SortedCandidates, SortedIndex);
				const int32 CandidateIndex = SortedCandidates[SortedIndex];
				const FArcTacticsRecordedCandidate& Candidate = Decision.Candidates[CandidateIndex];
				ImGui::TableNextRow();
				if (&Candidate == Winner)
				{
					ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg1, IM_COL32(50, 95, 45, 170));
				}
				ImGui::TableNextColumn();
				const FString NodeLabel = FString::Printf(TEXT("%s##HistoryCandidate%d"), *FormatCandidateName(Candidate), CandidateIndex);
				const bool bOpen = ImGui::TreeNodeEx(TCHAR_TO_UTF8(*NodeLabel), ImGuiTreeNodeFlags_SpanAllColumns);
				DrawCandidateRowColumns(FormatTarget(Candidate.Target), Candidate.bHasPriority, Candidate.Priority, Candidate.Score,
					Candidate.Outcome, Candidate.Reason);
				if (bOpen)
				{
					DrawHistoryConsiderations(Candidate);
					ImGui::TreePop();
				}
			}
			ImGui::EndTable();
		}
	}
	DrawHistoryDecisionQueries(Entity, Decision);
}

void FArcTacticsDebugger::DrawHistoryConsiderations(const FArcTacticsRecordedCandidate& Candidate)
{
	using namespace ArcTacticsDebuggerPrivate;

	if (Candidate.ConsiderationNames.Num() == 0)
	{
		DrawNoConsiderationsRow();
		return;
	}
	const int32 EvaluatedCount = FMath::Min(Candidate.Raw.Num(), Candidate.Curved.Num());
	for (int32 ConsiderationIndex = 0; ConsiderationIndex < Candidate.ConsiderationNames.Num(); ++ConsiderationIndex)
	{
		const bool bEvaluated = ConsiderationIndex < EvaluatedCount;
		DrawConsiderationRow(ConsiderationIndex, Candidate.ConsiderationNames[ConsiderationIndex], bEvaluated,
			bEvaluated ? Candidate.Raw[ConsiderationIndex] : 0.0f,
			bEvaluated ? Candidate.Curved[ConsiderationIndex] : 0.0f,
			ConsiderationIndex == Candidate.VetoIndex,
			ConsiderationIndex == Candidate.EarlyOutIndex);
	}
}

void FArcTacticsDebugger::DrawHistoryDecisionQueries(const FArcTacticsRecordedEntity& Entity, const FArcTacticsRecordedDecision& Decision)
{
	ImGui::Separator();
	if (Decision.QueryIndices.Num() == 0)
	{
		ImGui::TextDisabled("Global task queries: none");
		return;
	}
	ImGui::Text("Global task queries (%d)", Decision.QueryIndices.Num());
	const ImGuiTableFlags TableFlags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV
		| ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp;
	if (!ImGui::BeginTable("HistoryDecisionQueries", 3, TableFlags))
	{
		return;
	}
	ImGui::TableSetupColumn("Query", ImGuiTableColumnFlags_WidthStretch, 2.6f);
	ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthStretch, 1.8f);
	ImGui::TableSetupColumn("Detail", ImGuiTableColumnFlags_WidthStretch, 4.5f);
	ImGui::TableHeadersRow();
	for (const int32 QueryIndex : Decision.QueryIndices)
	{
		if (!Entity.Queries.IsValidIndex(QueryIndex))
		{
			continue;
		}
		const FArcTacticsRecordedQuery& Query = Entity.Queries[QueryIndex];
		if (DrawHistoryQueryRow(Query, QueryIndex, FString::Printf(TEXT("%.3f s (-%.3f)"), Query.Time, Decision.Time - Query.Time)))
		{
			HistoryRunEventIndex = INDEX_NONE;
		}
	}
	ImGui::EndTable();
}

bool FArcTacticsDebugger::DrawHistoryQueryRow(const FArcTacticsRecordedQuery& Query, const int32 QueryIndex, const FString& TimeText)
{
	using namespace ArcTacticsDebuggerPrivate;

	const bool bSelected = QueryIndex == HistoryQueryIndex;
	ImGui::TableNextRow();
	if (bSelected)
	{
		ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg1, IM_COL32(55, 70, 110, 220));
	}
	ImGui::TableNextColumn();
	ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_SpanAllColumns | ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
	if (bSelected)
	{
		Flags |= ImGuiTreeNodeFlags_Selected;
	}
	const FString Label = FString::Printf(TEXT("%s##HistoryQuery%d"), *FormatQueryName(Query), QueryIndex);
	const bool bOpen = ImGui::TreeNodeEx(TCHAR_TO_UTF8(*Label), Flags);
	const bool bClicked = ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen();
	if (bClicked)
	{
		HistoryQueryIndex = QueryIndex;
	}
	if (ImGui::IsItemHovered() && ImGui::BeginTooltip())
	{
		ImGui::Text("Query id %d   status %s   selection %s",
			Query.QueryId,
			TCHAR_TO_UTF8(*GetEnumName(Query.Status)),
			TCHAR_TO_UTF8(*GetEnumName(Query.SelectionMode)));
		if (Query.SourceKind == EArcTacticsQuerySource::Step)
		{
			ImGui::Text("Action serial %u", Query.ActionSerial);
		}
		else
		{
			ImGui::Text("Decision generation %u", Query.DecisionGeneration);
		}
		if (Query.QuerierLocation.bValid)
		{
			ImGui::Text("Querier %s", TCHAR_TO_UTF8(*FormatLocation(Query.QuerierLocation.Location)));
		}
		ImGui::Text("%d steps", Query.Steps.Num());
		ImGui::EndTooltip();
	}
	ImGui::TableNextColumn();
	ImGui::TextUnformatted(TCHAR_TO_UTF8(*TimeText));
	ImGui::TableNextColumn();
	const FString Summary = FormatQuerySummary(Query);
	if (!Query.bComplete || Query.Status != EArcTQSQueryStatus::Completed)
	{
		ImGui::TextColored(WarningTextColor, "%s", TCHAR_TO_UTF8(*Summary));
	}
	else
	{
		ImGui::TextUnformatted(TCHAR_TO_UTF8(*Summary));
	}
	if (bOpen)
	{
		DrawQueryItemTableRow(Query, QueryIndex);
		ImGui::TreePop();
	}
	return bClicked;
}

void FArcTacticsDebugger::DrawHistoryRuns(const FArcTacticsRecordedEntity& Entity)
{
	using namespace ArcTacticsDebuggerPrivate;

	const double ViewEnd = HistoryViewStart + HistoryViewDuration;
	TArray<int32> VisibleSpans;
	for (int32 SpanIndex = 0; SpanIndex < Entity.Actions.Num(); ++SpanIndex)
	{
		const FArcTacticsRecordedActionSpan& Span = Entity.Actions[SpanIndex];
		if (SpanIndex == HistorySpanIndex || (Span.EndTime >= HistoryViewStart && Span.StartTime <= ViewEnd))
		{
			VisibleSpans.Add(SpanIndex);
		}
	}
	VisibleSpans.StableSort([&Entity](const int32 Lhs, const int32 Rhs)
	{
		return Entity.Actions[Lhs].StartTime < Entity.Actions[Rhs].StartTime;
	});

	ImGui::Text("Action runs  |  %d of %d in view (%.2f - %.2f s)", VisibleSpans.Num(), Entity.Actions.Num(), HistoryViewStart, ViewEnd);
	if (VisibleSpans.Num() == 0)
	{
		ImGui::TextDisabled("No action runs in the visible time range");
		bHistoryRevealSpan = false;
		return;
	}

	const ImGuiTableFlags TableFlags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV
		| ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp;
	if (ImGui::BeginTable("HistoryRunsTable", 3, TableFlags))
	{
		ImGui::TableSetupColumn("Run", ImGuiTableColumnFlags_WidthStretch, 2.6f);
		ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthStretch, 1.8f);
		ImGui::TableSetupColumn("Detail", ImGuiTableColumnFlags_WidthStretch, 4.5f);
		ImGui::TableHeadersRow();
		for (const int32 SpanIndex : VisibleSpans)
		{
			DrawHistoryRun(Entity, SpanIndex);
		}
		ImGui::EndTable();
	}
	bHistoryRevealSpan = false;
}

void FArcTacticsDebugger::DrawHistoryRun(const FArcTacticsRecordedEntity& Entity, const int32 SpanIndex)
{
	using namespace ArcTacticsDebuggerPrivate;

	const FArcTacticsRecordedActionSpan& Span = Entity.Actions[SpanIndex];
	const bool bSelected = SpanIndex == HistorySpanIndex;
	ImGui::TableNextRow();
	if (bSelected)
	{
		ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg1, IM_COL32(55, 70, 110, 220));
	}
	ImGui::TableNextColumn();
	if (bSelected && bHistoryRevealSpan)
	{
		ImGui::SetNextItemOpen(true);
		ImGui::SetScrollHereY(0.0f);
	}
	ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_SpanAllColumns | ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
	if (bSelected && HistoryRunEventIndex == INDEX_NONE)
	{
		Flags |= ImGuiTreeNodeFlags_Selected;
	}
	const FString Label = FString::Printf(TEXT("%s##HistoryRun%d"), *GetSpanName(Span), SpanIndex);
	const bool bOpen = ImGui::TreeNodeEx(TCHAR_TO_UTF8(*Label), Flags);
	if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
	{
		SelectHistorySpan(SpanIndex, false);
	}
	if (ImGui::IsItemHovered() && (Span.StartPosition.bValid || Span.EndPosition.bValid || Span.PathIndices.Num() > 0 || Span.QueryIndices.Num() > 0)
		&& ImGui::BeginTooltip())
	{
		if (Span.StartPosition.bValid)
		{
			ImGui::Text("Start %s", TCHAR_TO_UTF8(*FormatLocation(Span.StartPosition.Location)));
		}
		if (Span.EndPosition.bValid)
		{
			ImGui::Text("End %s", TCHAR_TO_UTF8(*FormatLocation(Span.EndPosition.Location)));
		}
		if (Span.PathIndices.Num() > 0)
		{
			ImGui::Text("%d path requests", Span.PathIndices.Num());
		}
		if (Span.QueryIndices.Num() > 0)
		{
			ImGui::Text("%d TQS queries", Span.QueryIndices.Num());
		}
		ImGui::EndTooltip();
	}
	ImGui::TableNextColumn();
	ImGui::TextUnformatted(TCHAR_TO_UTF8(*FormatSpanTime(Span)));
	ImGui::TableNextColumn();
	const FString TargetText = FormatTarget(Span.Target);
	if (!TargetText.IsEmpty())
	{
		ImGui::Text("-> %s", TCHAR_TO_UTF8(*TargetText));
		ImGui::SameLine();
	}
	const FString EndText = FormatSpanEnd(Entity, HistorySpanDecisions, SpanIndex, false);
	if (Span.bEnded && !Span.bOpenAtRecordingEnd)
	{
		ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(GetEndStatusColor(Span.End.Status)), "%s", TCHAR_TO_UTF8(*EndText));
	}
	else
	{
		ImGui::TextDisabled("%s", TCHAR_TO_UTF8(*EndText));
	}
	if (!bOpen)
	{
		return;
	}

	const ImGuiTreeNodeFlags InfoFlags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAllColumns;
	if (!Span.bEnteredInRecording)
	{
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::TreeNodeEx("Started##HistoryRunBefore", InfoFlags);
		ImGui::TableNextColumn();
		ImGui::TextDisabled("-");
		ImGui::TableNextColumn();
		ImGui::TextColored(WarningTextColor, "Started before recording");
	}
	if (HistorySpanEvents.IsValidIndex(SpanIndex))
	{
		for (const int32 EventIndex : HistorySpanEvents[SpanIndex])
		{
			DrawHistoryRunEvent(Entity, SpanIndex, EventIndex);
		}
	}
	if (Span.bOpenAtRecordingEnd)
	{
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::TreeNodeEx("Running##HistoryRunOpen", InfoFlags);
		ImGui::TableNextColumn();
		ImGui::Text("%.3f s", Span.EndTime);
		ImGui::TableNextColumn();
		ImGui::TextColored(WarningTextColor, "Still running when the recording ended");
	}
	ImGui::TreePop();
}

void FArcTacticsDebugger::DrawHistoryRunEvent(const FArcTacticsRecordedEntity& Entity, const int32 SpanIndex, const int32 EventIndex)
{
	using namespace ArcTacticsDebuggerPrivate;

	const FArcTacticsRecordedActionSpan& Span = Entity.Actions[SpanIndex];
	const FArcTacticsRecordedActionEvent& Event = Entity.Events[EventIndex];
	const FArcTacticsRecordedPath* Path = Event.Type == EArcTacticsRecordedActionEventType::Path && Entity.Paths.IsValidIndex(Event.PathIndex)
		? &Entity.Paths[Event.PathIndex] : nullptr;
	const int32 DecisionIndex = HistorySpanDecisions.IsValidIndex(SpanIndex) ? HistorySpanDecisions[SpanIndex] : INDEX_NONE;
	const FString TimeText = Span.bEnteredInRecording
		? FString::Printf(TEXT("%.3f s (+%.3f)"), Event.Time, Event.Time - Span.StartTime)
		: FString::Printf(TEXT("%.3f s"), Event.Time);

	if (Event.Type == EArcTacticsRecordedActionEventType::Query)
	{
		if (!Entity.Queries.IsValidIndex(Event.QueryIndex))
		{
			return;
		}
		if (DrawHistoryQueryRow(Entity.Queries[Event.QueryIndex], Event.QueryIndex, TimeText))
		{
			HistorySpanIndex = SpanIndex;
			HistoryRunEventIndex = EventIndex;
		}
		return;
	}

	FString Label;
	FString Detail;
	switch (Event.Type)
	{
	case EArcTacticsRecordedActionEventType::Entered:
	{
		Label = TEXT("Started");
		const FArcTacticsRecordedCandidate* Winner = Entity.Decisions.IsValidIndex(DecisionIndex)
			? Entity.Decisions[DecisionIndex].FindSelectedCandidate() : nullptr;
		if (Winner)
		{
			Detail = FString::Printf(TEXT("decision %d, winner score %.4f"), DecisionIndex + 1, Winner->Score);
			if (Winner->bHasPriority)
			{
				Detail += FString::Printf(TEXT(", priority %d"), Winner->Priority);
			}
		}
		else
		{
			Detail = TEXT("no deciding decision recorded");
		}
		break;
	}
	case EArcTacticsRecordedActionEventType::StepChanged:
		Label = FormatStepLabel(Event);
		break;
	case EArcTacticsRecordedActionEventType::Path:
		Label = Path && Path->bRepath ? TEXT("Repath") : TEXT("Path");
		Detail = Path ? FormatPathDetail(*Path) : FString(TEXT("path not recorded"));
		break;
	case EArcTacticsRecordedActionEventType::Query:
		checkNoEntry();
		break;
	case EArcTacticsRecordedActionEventType::Ended:
		Label = TEXT("Ended");
		Detail = FormatSpanEnd(Entity, HistorySpanDecisions, SpanIndex, true);
		break;
	}
	if (Event.Type != EArcTacticsRecordedActionEventType::Path)
	{
		const FString PositionText = FormatPosition(Event.Position);
		if (!PositionText.IsEmpty())
		{
			Detail = Detail.IsEmpty() ? PositionText : Detail + TEXT(", ") + PositionText;
		}
	}

	const bool bSelected = SpanIndex == HistorySpanIndex && EventIndex == HistoryRunEventIndex;
	ImGui::TableNextRow();
	if (bSelected)
	{
		ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg1, IM_COL32(55, 70, 110, 220));
	}
	ImGui::TableNextColumn();
	ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAllColumns;
	if (bSelected)
	{
		Flags |= ImGuiTreeNodeFlags_Selected;
	}
	const FString NodeLabel = FString::Printf(TEXT("%s##HistoryRunEvent%d"), *Label, EventIndex);
	ImGui::TreeNodeEx(TCHAR_TO_UTF8(*NodeLabel), Flags);
	if (ImGui::IsItemClicked())
	{
		HistorySpanIndex = SpanIndex;
		HistoryRunEventIndex = EventIndex;
		HistoryQueryIndex = INDEX_NONE;
		if (Event.Type == EArcTacticsRecordedActionEventType::Entered && Entity.Decisions.IsValidIndex(DecisionIndex))
		{
			HistoryDecisionIndex = DecisionIndex;
		}
	}
	if (ImGui::IsItemHovered() && (Path || Event.Position.bValid) && ImGui::BeginTooltip())
	{
		if (Path)
		{
			ImGui::Text("%s requested at %.3f s", Path->bRepath ? "Repath" : "Path", Path->Time);
			ImGui::Text("Start %s", TCHAR_TO_UTF8(*FormatLocation(Path->Start)));
			ImGui::Text("Destination %s", TCHAR_TO_UTF8(*FormatLocation(Path->End)));
			if (Path->bSucceeded)
			{
				ImGui::Text("%d points, %.1f m%s", Path->Points.Num(), GetPathLength(*Path) / 100.0f, Path->bPartial ? ", partial" : "");
			}
			else
			{
				ImGui::TextColored(ErrorTextColor, "Request failed, no path points");
			}
		}
		else
		{
			ImGui::Text("Position %s", TCHAR_TO_UTF8(*FormatLocation(Event.Position.Location)));
		}
		ImGui::EndTooltip();
	}
	ImGui::TableNextColumn();
	ImGui::TextUnformatted(TCHAR_TO_UTF8(*TimeText));
	ImGui::TableNextColumn();
	if (Event.Type == EArcTacticsRecordedActionEventType::Ended && Span.bEnded)
	{
		ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(GetEndStatusColor(Span.End.Status)), "%s", TCHAR_TO_UTF8(*Detail));
	}
	else if (Path && !Path->bSucceeded)
	{
		ImGui::TextColored(ErrorTextColor, "%s", TCHAR_TO_UTF8(*Detail));
	}
	else
	{
		ImGui::TextUnformatted(TCHAR_TO_UTF8(*Detail));
	}
}

void FArcTacticsDebugger::SelectHistoryDecision(const int32 DecisionIndex)
{
	HistoryDecisionIndex = DecisionIndex;
	const int32 StartedSpan = DecisionIndex == INDEX_NONE ? INDEX_NONE : HistorySpanDecisions.IndexOfByKey(DecisionIndex);
	SelectHistorySpan(StartedSpan, true);
}

void FArcTacticsDebugger::SelectHistorySpan(const int32 SpanIndex, const bool bReveal)
{
	HistorySpanIndex = SpanIndex;
	HistoryRunEventIndex = INDEX_NONE;
	HistoryQueryIndex = INDEX_NONE;
	bHistoryRevealSpan = bReveal && SpanIndex != INDEX_NONE;
}

void FArcTacticsDebugger::SyncHistoryRecording(const FArcTacticsRecording* Recording)
{
	const FString RecordingPath = Recording ? Recording->FilePath : FString();
	if (Recording == HistoryRecording && RecordingPath == HistoryRecordingPath)
	{
		return;
	}
	HistoryRecording = Recording;
	HistoryRecordingPath = RecordingPath;
	HistoryEntityIndex = INDEX_NONE;
	HistoryDecisionIndex = INDEX_NONE;
	HistorySpanIndex = INDEX_NONE;
	HistoryRunEventIndex = INDEX_NONE;
	HistoryQueryIndex = INDEX_NONE;
	bHistoryRevealSpan = false;
	HistoryDecisionChanges.Reset();
	HistorySpanDecisions.Reset();
	HistorySpanEvents.Reset();
	bHistoryPanning = false;
	if (!Recording)
	{
		return;
	}
	FitHistoryView(*Recording);
	int32 EntityToSelect = Recording->Entities.Num() > 0 ? 0 : INDEX_NONE;
	if (bHistoryHasEntityKey)
	{
		const int32 Match = Recording->Entities.IndexOfByPredicate([this](const FArcTacticsRecordedEntity& Entity)
		{
			return Entity.WorldId == HistoryEntityWorldId
				&& Entity.EntityIndex == HistoryEntityKeyIndex
				&& Entity.EntitySerial == HistoryEntityKeySerial;
		});
		if (Match != INDEX_NONE)
		{
			EntityToSelect = Match;
		}
	}
	if (EntityToSelect != INDEX_NONE)
	{
		SelectHistoryEntity(EntityToSelect, *Recording);
	}
}

void FArcTacticsDebugger::SelectHistoryEntity(const int32 EntityIndex, const FArcTacticsRecording& Recording)
{
	if (!Recording.Entities.IsValidIndex(EntityIndex))
	{
		return;
	}
	const FArcTacticsRecordedEntity& Entity = Recording.Entities[EntityIndex];
	HistoryEntityIndex = EntityIndex;
	HistoryDecisionIndex = INDEX_NONE;
	HistorySpanIndex = INDEX_NONE;
	HistoryRunEventIndex = INDEX_NONE;
	HistoryQueryIndex = INDEX_NONE;
	bHistoryRevealSpan = false;
	bHistoryHasEntityKey = true;
	HistoryEntityWorldId = Entity.WorldId;
	HistoryEntityKeyIndex = Entity.EntityIndex;
	HistoryEntityKeySerial = Entity.EntitySerial;
	RebuildHistoryEntityCache(Entity);
}

void FArcTacticsDebugger::RebuildHistoryEntityCache(const FArcTacticsRecordedEntity& Entity)
{
	using namespace ArcTacticsDebuggerPrivate;

	HistoryDecisionChanges.Reset(Entity.Decisions.Num());
	for (const FArcTacticsRecordedDecision& Decision : Entity.Decisions)
	{
		const FArcTacticsRecordedCandidate* Winner = Decision.FindSelectedCandidate();
		if (!Winner)
		{
			HistoryDecisionChanges.Add(EHistoryDecisionChange::NoWinner);
			continue;
		}
		const FArcTacticsRecordedActionSpan* Active = FindHistorySpanAt(Entity, Decision.Time);
		HistoryDecisionChanges.Add(Active && Active->Name.IsEmpty()
			? EHistoryDecisionChange::Unknown
			: EHistoryDecisionChange::Changed);
	}

	HistorySpanEvents.Reset();
	HistorySpanEvents.SetNum(Entity.Actions.Num());
	for (int32 EventIndex = 0; EventIndex < Entity.Events.Num(); ++EventIndex)
	{
		const int32 SpanIndex = Entity.Events[EventIndex].SpanIndex;
		if (HistorySpanEvents.IsValidIndex(SpanIndex))
		{
			HistorySpanEvents[SpanIndex].Add(EventIndex);
		}
	}
	for (TArray<int32>& SpanEvents : HistorySpanEvents)
	{
		SpanEvents.StableSort([&Entity](const int32 Lhs, const int32 Rhs)
		{
			return Entity.Events[Lhs].Time < Entity.Events[Rhs].Time;
		});
	}

	HistorySpanDecisions.Init(INDEX_NONE, Entity.Actions.Num());
	int32 LastAssignedDecision = INDEX_NONE;
	for (int32 SpanIndex = 0; SpanIndex < Entity.Actions.Num(); ++SpanIndex)
	{
		const FArcTacticsRecordedActionSpan& Span = Entity.Actions[SpanIndex];
		if (!Span.bEnteredInRecording)
		{
			continue;
		}
		const double EarliestTime = SpanIndex > 0 ? Entity.Actions[SpanIndex - 1].StartTime - UE_KINDA_SMALL_NUMBER : -UE_BIG_NUMBER;
		for (int32 DecisionIndex = Entity.Decisions.Num() - 1; DecisionIndex > LastAssignedDecision; --DecisionIndex)
		{
			const FArcTacticsRecordedDecision& Decision = Entity.Decisions[DecisionIndex];
			if (Decision.Time > Span.StartTime + UE_KINDA_SMALL_NUMBER)
			{
				continue;
			}
			if (Decision.Time < EarliestTime)
			{
				break;
			}
			const FArcTacticsRecordedCandidate* Winner = Decision.FindSelectedCandidate();
			if (Winner && Winner->SourceIndex == Span.SourceIndex && Winner->bFromSelector == Span.bFromSelector)
			{
				HistorySpanDecisions[SpanIndex] = DecisionIndex;
				LastAssignedDecision = DecisionIndex;
				break;
			}
		}
	}
}

void FArcTacticsDebugger::FitHistoryView(const FArcTacticsRecording& Recording)
{
	const double Duration = FMath::Max(Recording.Duration, 0.1);
	HistoryViewStart = -Duration * 0.01;
	HistoryViewDuration = Duration * 1.02;
}

const FArcTacticsRecordedActionSpan* FArcTacticsDebugger::FindHistorySpanAt(
	const FArcTacticsRecordedEntity& Entity,
	const double Time)
{
	const FArcTacticsRecordedActionSpan* Best = nullptr;
	for (const FArcTacticsRecordedActionSpan& Span : Entity.Actions)
	{
		if (Span.StartTime < Time && Time <= Span.EndTime && (!Best || Span.StartTime > Best->StartTime))
		{
			Best = &Span;
		}
	}
	return Best;
}

void FArcTacticsDebugger::ResetHistoryState()
{
	HistoryRecording = nullptr;
	HistoryRecordingPath.Reset();
	HistoryFilter[0] = '\0';
	HistoryEntityIndex = INDEX_NONE;
	bHistoryHasEntityKey = false;
	HistoryDecisionIndex = INDEX_NONE;
	HistorySpanIndex = INDEX_NONE;
	HistoryRunEventIndex = INDEX_NONE;
	HistoryQueryIndex = INDEX_NONE;
	bHistoryRevealSpan = false;
	HistoryDecisionChanges.Reset();
	HistorySpanDecisions.Reset();
	HistorySpanEvents.Reset();
	HistoryViewStart = 0.0;
	HistoryViewDuration = 1.0;
	bHistoryPanning = false;
}

void FArcTacticsDebugger::BuildLiveDrawData(
	UWorld& World,
	const FEntityEntry& Entry,
	const FArcTacticsDebugSnapshot& Snapshot,
	FArcTacticsDebugDrawData& OutData) const
{
	using namespace ArcTacticsDebuggerPrivate;

	const FMassEntityManager* EntityManager = GetEntityManager(World);
	FVector EntityLocation = FVector::ZeroVector;
	if (!EntityManager || !TryGetEntityLocation(*EntityManager, Entry.Entity, EntityLocation))
	{
		return;
	}

	const FArcTacticsDebugActiveAction& Active = Snapshot.ActiveAction;
	FArcTacticsDebugDrawEntity& DrawEntity = OutData.Entities.AddDefaulted_GetRef();
	DrawEntity.Location = EntityLocation;
	DrawEntity.Radius = DrawEntityRadius;
	DrawEntity.Color = DrawLiveEntityColor;
	DrawEntity.Label = Active.bSet
		? FString::Printf(TEXT("%s  %s"), *Entry.Label, *Active.Name.ToString())
		: Entry.Label;

	if (const APawn* Pawn = UGameplayStatics::GetPlayerPawn(&World, 0))
	{
		AddDrawLine(OutData, Pawn->GetActorLocation(), EntityLocation, DrawPawnLineColor, 0.0f);
	}

	if (Active.bSet && Active.bHasTargetLocation)
	{
		const FColor TargetColor = Active.Target.Type == EArcTacticsValueIdentity::Entity ? DrawEntityTargetColor : DrawPositionTargetColor;
		AddDrawLine(OutData, EntityLocation, Active.TargetLocation, TargetColor, 2.0f);
		AddDrawPoint(OutData, Active.TargetLocation, DrawTargetPointSize, TargetColor);
	}

	if (Snapshot.LastDecision.bValid)
	{
		for (const FArcTacticsDebugCandidate& Candidate : Snapshot.LastDecision.Candidates)
		{
			FVector TargetLocation = FVector::ZeroVector;
			if (Candidate.Target.Type == EArcTacticsValueIdentity::Entity)
			{
				if (!TryGetEntityLocation(*EntityManager, Candidate.Target.Entity, TargetLocation))
				{
					continue;
				}
			}
			else if (Candidate.Target.Type == EArcTacticsValueIdentity::Vector)
			{
				TargetLocation = Candidate.Target.Vector;
			}
			else
			{
				continue;
			}
			if (Candidate.bSelected)
			{
				AddDrawPoint(OutData, TargetLocation, DrawSelectedCandidatePointSize, DrawSelectedCandidateColor,
					FString::Printf(TEXT("%s %.3f"), *Candidate.Name.ToString(), Candidate.Score));
			}
			else
			{
				AddDrawPoint(OutData, TargetLocation, DrawCandidatePointSize,
					Candidate.Target.Type == EArcTacticsValueIdentity::Entity ? DrawCandidateEntityColor : DrawCandidatePositionColor);
			}
		}
	}

	const FMassNavMeshShortPathFragment* ShortPath = EntityManager->GetFragmentDataPtr<FMassNavMeshShortPathFragment>(Entry.Entity);
	if (ShortPath && ShortPath->NumPoints > 1)
	{
		FArcTacticsDebugDrawPolyline& Polyline = OutData.Polylines.AddDefaulted_GetRef();
		Polyline.Color = DrawLivePathColor;
		Polyline.Thickness = 2.0f;
		Polyline.Points.Reserve(ShortPath->NumPoints);
		for (int32 PointIndex = 0; PointIndex < ShortPath->NumPoints; ++PointIndex)
		{
			Polyline.Points.Add(ShortPath->Points[PointIndex].Position);
		}
		AddDrawPoint(OutData, Polyline.Points.Last(), DrawPathEndPointSize, DrawLivePathColor);
	}
}

void FArcTacticsDebugger::BuildHistoryDrawData(
	UWorld* World,
	const FArcTacticsRecordedEntity& Entity,
	FArcTacticsDebugDrawData& OutData) const
{
	using namespace ArcTacticsDebuggerPrivate;

	const FMassEntityManager* EntityManager = nullptr;
#if WITH_ARCTACTICS_TRACE
	if (World && ArcTacticsTrace::GetWorldId(World) == Entity.WorldId)
	{
		EntityManager = GetEntityManager(*World);
	}
#endif

	if (EntityManager)
	{
		FVector EntityLocation = FVector::ZeroVector;
		const FMassEntityHandle Handle(static_cast<int32>(Entity.EntityIndex), static_cast<int32>(Entity.EntitySerial));
		if (TryGetEntityLocation(*EntityManager, Handle, EntityLocation))
		{
			FArcTacticsDebugDrawEntity& DrawEntity = OutData.Entities.AddDefaulted_GetRef();
			DrawEntity.Location = EntityLocation;
			DrawEntity.Radius = DrawEntityRadius;
			DrawEntity.Color = DrawRecordedEntityColor;
			DrawEntity.Label = FString::Printf(TEXT("%u:%u now"), Entity.EntityIndex, Entity.EntitySerial);
		}
	}

	const double ViewEnd = HistoryViewStart + HistoryViewDuration;
	for (int32 SpanIndex = 0; SpanIndex < Entity.Actions.Num(); ++SpanIndex)
	{
		const FArcTacticsRecordedActionSpan& Span = Entity.Actions[SpanIndex];
		if (SpanIndex != HistorySpanIndex && Span.StartPosition.bValid
			&& Span.StartTime >= HistoryViewStart && Span.StartTime <= ViewEnd)
		{
			AddDrawPoint(OutData, FVector(Span.StartPosition.Location), DrawFaintMarkerPointSize, DrawFaintMarkerColor);
		}
	}

	if (Entity.Decisions.IsValidIndex(HistoryDecisionIndex))
	{
		const FArcTacticsRecordedDecision& Decision = Entity.Decisions[HistoryDecisionIndex];
		if (Decision.Position.bValid)
		{
			const FArcTacticsRecordedCandidate* Winner = Decision.FindSelectedCandidate();
			const FString WinnerText = Winner ? FormatCandidateName(*Winner) : FString(TEXT("no winner"));
			AddDrawPoint(OutData, FVector(Decision.Position.Location), DrawMarkerPointSize, DrawDecisionColor,
				FString::Printf(TEXT("Decision %d  %.3f s  %s"), HistoryDecisionIndex + 1, Decision.Time, *WinnerText));
		}
	}

	if (Entity.Queries.IsValidIndex(HistoryQueryIndex))
	{
		AddQueryDrawData(Entity.Queries[HistoryQueryIndex], OutData);
	}

	if (!Entity.Actions.IsValidIndex(HistorySpanIndex))
	{
		return;
	}
	const FArcTacticsRecordedActionSpan& Span = Entity.Actions[HistorySpanIndex];
	const FString SpanName = GetSpanName(Span);
	if (Span.StartPosition.bValid)
	{
		AddDrawPoint(OutData, FVector(Span.StartPosition.Location), DrawMarkerPointSize, DrawSpanStartColor,
			FString::Printf(TEXT("Start %s  %.3f s"), *SpanName, Span.StartTime));
	}
	if (Span.EndPosition.bValid)
	{
		AddDrawPoint(OutData, FVector(Span.EndPosition.Location), DrawMarkerPointSize, DrawSpanEndColor,
			FString::Printf(TEXT("End %s  %.3f s"), *SpanName, Span.EndTime));
	}

	if (Span.bEnteredInRecording && (Span.StartPosition.bValid || Span.EndPosition.bValid))
	{
		const FVector LineStart(Span.StartPosition.bValid ? Span.StartPosition.Location : Span.EndPosition.Location);
		if (Span.Target.bHasLocation)
		{
			const bool bEntityTarget = Span.Target.Type == EArcTacticsValueIdentity::Entity;
			const FColor TargetColor = bEntityTarget ? DrawEntityTargetColor : DrawPositionTargetColor;
			const FVector TargetLocation(Span.Target.Location);
			AddDrawLine(OutData, LineStart, TargetLocation, TargetColor, 2.0f);
			AddDrawPoint(OutData, TargetLocation, DrawTargetPointSize, TargetColor,
				bEntityTarget ? FString::Printf(TEXT("Target %s"), *FormatTarget(Span.Target)) : FString(TEXT("Target")));
		}
	}

	const FArcTacticsRecordedActionEvent* SelectedEvent = Entity.Events.IsValidIndex(HistoryRunEventIndex)
		? &Entity.Events[HistoryRunEventIndex] : nullptr;
	const int32 SelectedPathIndex = SelectedEvent && SelectedEvent->Type == EArcTacticsRecordedActionEventType::Path
		? SelectedEvent->PathIndex : INDEX_NONE;

	for (const int32 PathIndex : Span.PathIndices)
	{
		if (!Entity.Paths.IsValidIndex(PathIndex))
		{
			continue;
		}
		const FArcTacticsRecordedPath& Path = Entity.Paths[PathIndex];
		const bool bSelectedPath = PathIndex == SelectedPathIndex;
		FColor Color = Path.bSucceeded ? (Path.bRepath ? DrawRepathColor : DrawPathColor) : DrawFailedPathColor;
		if (bSelectedPath)
		{
			Color = DrawSelectedRowColor;
		}
		const float Thickness = bSelectedPath ? 4.0f : (Path.bRepath ? 1.0f : 2.0f);
		if (Path.bSucceeded && Path.Points.Num() > 1)
		{
			FArcTacticsDebugDrawPolyline& Polyline = OutData.Polylines.AddDefaulted_GetRef();
			Polyline.Color = Color;
			Polyline.Thickness = Thickness;
			Polyline.Points.Reserve(Path.Points.Num());
			for (const FVector3f& Point : Path.Points)
			{
				Polyline.Points.Add(FVector(Point));
			}
		}
		else
		{
			AddDrawLine(OutData, FVector(Path.Start), FVector(Path.End), Color, 1.0f);
		}
		const FString EndLabel = bSelectedPath
			? FString::Printf(TEXT("%s %.3f s"), Path.bRepath ? TEXT("Repath") : TEXT("Path"), Path.Time)
			: FString();
		AddDrawPoint(OutData, FVector(Path.Start), DrawPathStartPointSize, Color);
		AddDrawPoint(OutData, FVector(Path.End), DrawPathEndPointSize, Color, EndLabel);
	}

	if (SelectedEvent && SelectedEvent->SpanIndex == HistorySpanIndex
		&& SelectedEvent->Type != EArcTacticsRecordedActionEventType::Path
		&& SelectedEvent->Type != EArcTacticsRecordedActionEventType::Query && SelectedEvent->Position.bValid)
	{
		FString Label;
		switch (SelectedEvent->Type)
		{
		case EArcTacticsRecordedActionEventType::Entered:
			Label = TEXT("Started");
			break;
		case EArcTacticsRecordedActionEventType::StepChanged:
			Label = FormatStepLabel(*SelectedEvent);
			break;
		case EArcTacticsRecordedActionEventType::Ended:
			Label = TEXT("Ended");
			break;
		default:
			break;
		}
		AddDrawPoint(OutData, FVector(SelectedEvent->Position.Location), DrawSelectedRowPointSize, DrawSelectedRowColor,
			FString::Printf(TEXT("%s  %.3f s"), *Label, SelectedEvent->Time));
	}
}

void FArcTacticsDebugger::ApplyDrawData(UWorld* World, const FArcTacticsDebugDrawData& Data)
{
	bDrawnThisFrame = true;
	if (!World || Data.IsEmpty())
	{
		if (bHasDrawShapes && DrawComponent.IsValid())
		{
			DrawComponent->SetShapes(FArcTacticsDebugDrawData());
		}
		bHasDrawShapes = false;
		return;
	}
	EnsureDrawActor(*World);
	if (!DrawComponent.IsValid())
	{
		bHasDrawShapes = false;
		return;
	}
	DrawComponent->SetShapes(Data);
	bHasDrawShapes = true;
}

void FArcTacticsDebugger::EnsureDrawActor(UWorld& World)
{
	if (DrawActor.IsValid() && DrawComponent.IsValid() && DrawActor->GetWorld() == &World)
	{
		return;
	}
	DestroyDrawActor();

	FActorSpawnParameters SpawnParams;
	SpawnParams.ObjectFlags = RF_Transient;
	AActor* NewActor = World.SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, SpawnParams);
	if (!NewActor)
	{
		return;
	}
#if WITH_EDITOR
	NewActor->SetActorLabel(TEXT("TacticsDebuggerDraw"));
#endif
	UArcTacticsDebuggerDrawComponent* NewComponent = NewObject<UArcTacticsDebuggerDrawComponent>(NewActor);
	NewComponent->RegisterComponent();
	NewActor->AddInstanceComponent(NewComponent);

	DrawActor = NewActor;
	DrawComponent = NewComponent;
}

void FArcTacticsDebugger::DestroyDrawActor()
{
	if (DrawActor.IsValid())
	{
		DrawActor->Destroy();
	}
	DrawActor.Reset();
	DrawComponent.Reset();
	bHasDrawShapes = false;
}
