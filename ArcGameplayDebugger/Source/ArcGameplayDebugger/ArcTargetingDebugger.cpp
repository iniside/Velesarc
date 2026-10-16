// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTargetingDebugger.h"

#if UE_DEBUG_RENDERING_ENABLED

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "imgui.h"
#include "GameplayTagContainer.h"

#include "ArcTargetingSubsystem.h"
#include "ArcTargetingDebug.h"
#include "ArcTargetingPreset.h"
#include "ArcTargetingDebuggerDrawActor.h"
#include "Nodes/ArcTargetingNode.h"

// ---------------------------------------------------------------------------
// File-local helpers
// ---------------------------------------------------------------------------

namespace ArcTargetingDebuggerLocal
{
	static UWorld* FindDebugWorld()
	{
		if (!GEngine)
		{
			return nullptr;
		}

		if (GEngine->GameViewport)
		{
			UWorld* ViewportWorld = GEngine->GameViewport->GetWorld();
			if (ViewportWorld)
			{
				return ViewportWorld;
			}
		}

		for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
		{
			if (Ctx.WorldType == EWorldType::Game || Ctx.WorldType == EWorldType::PIE)
			{
				if (Ctx.World())
				{
					return Ctx.World();
				}
			}
		}

		return nullptr;
	}

	/** Format FPlatformTime::Seconds() offset to HH:MM:SS.mmm into a char buffer. */
	static void FormatTimestamp(double Seconds, char* OutBuf, int32 BufLen)
	{
		int32 TotalSec = (int32)Seconds;
		int32 Hours    = TotalSec / 3600;
		int32 Minutes  = (TotalSec % 3600) / 60;
		int32 Secs     = TotalSec % 60;
		int32 Millis   = (int32)((Seconds - (double)TotalSec) * 1000.0);
		snprintf(OutBuf, BufLen, "%02d:%02d:%02d.%03d", Hours, Minutes, Secs, Millis);
	}

	/** Copy selected traces into the draw actor for op-dispatch replay. */
	static void PushTracesToActor(
		AArcTargetingDebuggerDrawActor* Actor,
		TConstArrayView<FArcTargetingDebugTrace> AllTraces,
		const TSet<int32>& SelectedIndices)
	{
		TArray<FArcTargetingDebugTrace> Selected;
		Selected.Reserve(SelectedIndices.Num());
		for (int32 Idx : SelectedIndices)
		{
			if (AllTraces.IsValidIndex(Idx))
			{
				Selected.Add(AllTraces[Idx]);
			}
		}
		Actor->SetTraces(MoveTemp(Selected));
	}

	/** Build persistent-hit list from selected global tags and push to actor. */
	static void PushPersistentHitsToActor(
		AArcTargetingDebuggerDrawActor* Actor,
		UArcTargetingSubsystem* Sub,
		const TSet<FArcTargetingPersistentKey>& SelectedKeys)
	{
		TArray<FArcTargetingDebugPersistentHit> Hits;
		for (const FArcTargetingPersistentKey& Key : SelectedKeys)
		{
			const TArray<FArcTargetCandidate>* Candidates = Sub->GetLatestPersistentResult(Key.OwnerEntity, Key.Tag);
			if (!Candidates)
			{
				continue;
			}
			for (const FArcTargetCandidate& Candidate : *Candidates)
			{
				FArcTargetingDebugPersistentHit Hit;
				Hit.Location = Candidate.HitResult.ImpactPoint;
				Hit.Score    = Candidate.Score;
				Hit.Color    = FColor::White;
				Hit.Label    = FString::Printf(TEXT("[%d:%d] %s"), Key.OwnerEntity.Index, Key.OwnerEntity.SerialNumber, *Key.Tag.ToString());
				Hits.Add(MoveTemp(Hit));
			}
		}
		Actor->SetPersistentHits(MoveTemp(Hits));
	}

	static const char* NodeKindLabel(EArcTargetingNodeKind Kind)
	{
		switch (Kind)
		{
			case EArcTargetingNodeKind::Origin:  return "Origin";
			case EArcTargetingNodeKind::Trace:   return "Trace";
			case EArcTargetingNodeKind::Overlap: return "Overlap";
			case EArcTargetingNodeKind::Filter:  return "Filter";
			case EArcTargetingNodeKind::Score:   return "Score";
			case EArcTargetingNodeKind::Sort:    return "Sort";
			default:                              return "Misc";
		}
	}

	static bool IsKindVisible(EArcTargetingNodeKind Kind, bool bO, bool bT, bool bV, bool bF, bool bS, bool bSort, bool bM)
	{
		switch (Kind)
		{
			case EArcTargetingNodeKind::Origin:  return bO;
			case EArcTargetingNodeKind::Trace:   return bT;
			case EArcTargetingNodeKind::Overlap: return bV;
			case EArcTargetingNodeKind::Filter:  return bF;
			case EArcTargetingNodeKind::Score:   return bS;
			case EArcTargetingNodeKind::Sort:    return bSort;
			default:                              return bM;
		}
	}

	/** Draw the right-pane detail view for a single trace. */
	static void DrawTraceDetail(
		const FArcTargetingDebugTrace& Trace,
		bool bShowOrigin,
		bool bShowTrace,
		bool bShowOverlap,
		bool bShowFilter,
		bool bShowScore,
		bool bShowSort,
		bool bShowMisc)
	{
		// Initial location
		FString CursorStr = FString::Printf(
			TEXT("Initial location: (%.1f, %.1f, %.1f)"),
			Trace.InitialCursor.X, Trace.InitialCursor.Y, Trace.InitialCursor.Z);
		ImGui::TextUnformatted(TCHAR_TO_UTF8(*CursorStr));

		// Source actor
		FString ActorName = Trace.SourceActor.IsValid()
			? Trace.SourceActor->GetName()
			: FString(TEXT("<expired>"));
		FString ActorStr = FString::Printf(TEXT("Source actor: %s"), *ActorName);
		ImGui::TextUnformatted(TCHAR_TO_UTF8(*ActorStr));

		ImGui::Separator();

		// Final candidates
		FString CandidatesHeader = FString::Printf(
			TEXT("Final candidates (%d)"), Trace.FinalCandidates.Num());
		if (ImGui::CollapsingHeader(TCHAR_TO_UTF8(*CandidatesHeader), ImGuiTreeNodeFlags_DefaultOpen))
		{
			if (ImGui::BeginTable("candidates_table", 2,
				ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
			{
				ImGui::TableSetupColumn("Score",        ImGuiTableColumnFlags_WidthFixed, 80.f);
				ImGui::TableSetupColumn("Impact Point", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableHeadersRow();

				for (const FArcTargetCandidate& Candidate : Trace.FinalCandidates)
				{
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("%.3f", Candidate.Score);
					ImGui::TableSetColumnIndex(1);
					FString ImpactStr = FString::Printf(
						TEXT("(%.1f, %.1f, %.1f)"),
						Candidate.HitResult.ImpactPoint.X,
						Candidate.HitResult.ImpactPoint.Y,
						Candidate.HitResult.ImpactPoint.Z);
					ImGui::TextUnformatted(TCHAR_TO_UTF8(*ImpactStr));
				}
				ImGui::EndTable();
			}
		}

		// Nodes
		if (!Trace.Nodes.IsEmpty())
		{
			ImGui::Separator();
			if (ImGui::CollapsingHeader("Nodes", ImGuiTreeNodeFlags_DefaultOpen))
			{
				const UArcTargetingPreset* Preset = Trace.Preset.Get();
				for (const FArcTargetingDebugNodeEntry& Entry : Trace.Nodes)
				{
					if (Preset == nullptr || !Preset->Nodes.IsValidIndex(Entry.NodeIndex))
					{
						continue;
					}
					const FArcTargetingNode* Node = Preset->Nodes[Entry.NodeIndex].GetPtr<FArcTargetingNode>();
					if (Node == nullptr)
					{
						continue;
					}
					const EArcTargetingNodeKind Kind = Node->GetKind();
					if (!IsKindVisible(Kind, bShowOrigin, bShowTrace, bShowOverlap, bShowFilter, bShowScore, bShowSort, bShowMisc))
					{
						continue;
					}
					const UScriptStruct* Struct = Preset->Nodes[Entry.NodeIndex].GetScriptStruct();
					FString StructName = Struct != nullptr ? Struct->GetName() : FString(TEXT("<unknown>"));
					FString NodeLabel = FString::Printf(
						TEXT("[%d] %s (%s)  before=%d -> after=%d"),
						Entry.NodeIndex,
						*StructName,
						UTF8_TO_TCHAR(NodeKindLabel(Kind)),
						Entry.CandidatesBefore.Num(),
						Entry.CandidatesAfter.Num());
					ImGui::BulletText("%s", TCHAR_TO_UTF8(*NodeLabel));
				}
			}
		}
	}

} // namespace ArcTargetingDebuggerLocal

// ---------------------------------------------------------------------------
// Initialize
// ---------------------------------------------------------------------------

void FArcTargetingDebugger::Initialize()
{
	UWorld* World = ArcTargetingDebuggerLocal::FindDebugWorld();
	if (!World)
	{
		return;
	}

	CachedWorld = World;

	UArcTargetingSubsystem* Sub = UArcTargetingSubsystem::Get(World);
	if (Sub)
	{
		Sub->SetHistoryRecordingRefCount(+1);
		bRefCountBumped = true;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.ObjectFlags                   = RF_Transient;

	DrawActor = World->SpawnActor<AArcTargetingDebuggerDrawActor>(SpawnParams);
}

// ---------------------------------------------------------------------------
// Uninitialize
// ---------------------------------------------------------------------------

void FArcTargetingDebugger::Uninitialize()
{
	if (DrawActor.IsValid())
	{
		DrawActor->Destroy();
		DrawActor.Reset();
	}

	if (bRefCountBumped)
	{
		UWorld* World = CachedWorld.Get();
		if (World)
		{
			UArcTargetingSubsystem* Sub = UArcTargetingSubsystem::Get(World);
			if (Sub)
			{
				Sub->SetHistoryRecordingRefCount(-1);
			}
		}
		bRefCountBumped = false;
	}

	CachedWorld.Reset();
	SelectedHistoryIndices.Empty();
	SelectedGlobalKeys.Empty();
}

// ---------------------------------------------------------------------------
// Draw
// ---------------------------------------------------------------------------

void FArcTargetingDebugger::Draw()
{
	if (!bShow)
	{
		return;
	}

	UWorld* World = ArcTargetingDebuggerLocal::FindDebugWorld();
	if (!World)
	{
		return;
	}

	UArcTargetingSubsystem* Sub = UArcTargetingSubsystem::Get(World);
	if (!Sub)
	{
		return;
	}

	ImGui::SetNextWindowSize(ImVec2(1100.f, 650.f), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("ArcTargeting Debugger", &bShow))
	{
		ImGui::End();
		return;
	}

	TConstArrayView<FArcTargetingDebugTrace> Traces = Sub->GetRecentTraces();

	// -----------------------------------------------------------------------
	// Top toolbar
	// -----------------------------------------------------------------------

	if (ImGui::Button("Clear history"))
	{
		Sub->ClearRecentTraces();
		SelectedHistoryIndices.Empty();
	}

	ImGui::SameLine();
	ImGui::Checkbox("Auto-select latest", &bAutoSelectLatest);

	ImGui::SameLine();
	FString RingLabel = FString::Printf(TEXT("History: %d entries"), Traces.Num());
	ImGui::TextUnformatted(TCHAR_TO_UTF8(*RingLabel));

	ImGui::SameLine();
	ImGui::Checkbox("Origin", &bShowOrigin);
	ImGui::SameLine();
	ImGui::Checkbox("Trace", &bShowTrace);
	ImGui::SameLine();
	ImGui::Checkbox("Overlap", &bShowOverlap);
	ImGui::SameLine();
	ImGui::Checkbox("Filter", &bShowFilter);
	ImGui::SameLine();
	ImGui::Checkbox("Score", &bShowScore);
	ImGui::SameLine();
	ImGui::Checkbox("Sort", &bShowSort);
	ImGui::SameLine();
	ImGui::Checkbox("Misc", &bShowMisc);

	ImGui::Separator();

	// -----------------------------------------------------------------------
	// Auto-select latest
	// -----------------------------------------------------------------------

	if (bAutoSelectLatest && !Traces.IsEmpty())
	{
		SelectedHistoryIndices.Empty();
		SelectedHistoryIndices.Add(Traces.Num() - 1);
	}

	// -----------------------------------------------------------------------
	// Two-column split: left = history list, right = detail / globals
	// -----------------------------------------------------------------------

	// Left pane — history list
	ImGui::BeginChild("targeting_history_list", ImVec2(320.f, 0.f), true);
	ImGui::TextUnformatted("Recent traces (newest first)");
	ImGui::Separator();

	// Iterate newest-first
	for (int32 ListPos = 0; ListPos < Traces.Num(); ++ListPos)
	{
		// Map display position to array index: newest (highest index) shown first
		int32 ArrayIdx = Traces.Num() - 1 - ListPos;
		const FArcTargetingDebugTrace& Trace = Traces[ArrayIdx];

		char TimeBuf[32];
		ArcTargetingDebuggerLocal::FormatTimestamp(Trace.CompletedRealTimeSeconds, TimeBuf, sizeof(TimeBuf));

		FString ActorName = Trace.SourceActor.IsValid()
			? Trace.SourceActor->GetName()
			: FString(TEXT("<expired>"));

		FString PresetName = Trace.Preset.IsValid()
			? Trace.Preset->GetName()
			: FString(TEXT("<none>"));

		FString RowLabel = FString::Printf(
			TEXT("[%d] %s  %s  %s"),
			ArrayIdx,
			UTF8_TO_TCHAR(TimeBuf),
			*ActorName,
			*PresetName);

		bool bSelected = SelectedHistoryIndices.Contains(ArrayIdx);
		ImGui::PushID(ArrayIdx);

		if (ImGui::Selectable(TCHAR_TO_UTF8(*RowLabel), bSelected))
		{
			if (ImGui::GetIO().KeyCtrl)
			{
				// Toggle this index while keeping others
				if (bSelected)
				{
					SelectedHistoryIndices.Remove(ArrayIdx);
				}
				else
				{
					SelectedHistoryIndices.Add(ArrayIdx);
				}
			}
			else
			{
				SelectedHistoryIndices.Empty();
				SelectedHistoryIndices.Add(ArrayIdx);
			}
		}

		ImGui::PopID();
	}

	ImGui::EndChild();

	ImGui::SameLine();

	// Right pane
	ImGui::BeginChild("targeting_right_pane", ImVec2(0.f, 0.f), false);

	// Find the focused trace (most-recently added to selection set, approximated
	// by picking the highest index in the selected set as the "primary").
	int32 FocusedIdx = -1;
	for (int32 Idx : SelectedHistoryIndices)
	{
		if (Idx > FocusedIdx)
		{
			FocusedIdx = Idx;
		}
	}

	if (ImGui::BeginTabBar("targeting_tabs"))
	{
		// ===================================================================
		// Tab 1: Trace detail
		// ===================================================================

		if (ImGui::BeginTabItem("Trace detail"))
		{
			if (FocusedIdx >= 0 && Traces.IsValidIndex(FocusedIdx))
			{
				ArcTargetingDebuggerLocal::DrawTraceDetail(
					Traces[FocusedIdx],
					bShowOrigin, bShowTrace, bShowOverlap,
					bShowFilter, bShowScore, bShowSort, bShowMisc);
			}
			else
			{
				ImGui::TextUnformatted("Select a trace from the list on the left.");
			}
			ImGui::EndTabItem();
		}

		// ===================================================================
		// Tab 2: Global / Persistent
		// ===================================================================

		if (ImGui::BeginTabItem("Global / Persistent"))
		{
			TArray<FArcTargetingPersistentKey> PersistentKeys;
			Sub->GetPersistentKeys(PersistentKeys);

			if (PersistentKeys.IsEmpty())
			{
				ImGui::TextUnformatted("No persistent targeting entries registered.");
			}
			else
			{
				if (ImGui::BeginTable("persistent_table", 3,
					ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
				{
					ImGui::TableSetupColumn("Entity / Tag",   ImGuiTableColumnFlags_WidthStretch);
					ImGui::TableSetupColumn("Candidates",     ImGuiTableColumnFlags_WidthFixed, 90.f);
					ImGui::TableSetupColumn("Draw",           ImGuiTableColumnFlags_WidthFixed, 50.f);
					ImGui::TableHeadersRow();

					for (const FArcTargetingPersistentKey& Key : PersistentKeys)
					{
						ImGui::TableNextRow();

						const TArray<FArcTargetCandidate>* Candidates = Sub->GetLatestPersistentResult(Key.OwnerEntity, Key.Tag);
						int32 CandidateCount = Candidates ? Candidates->Num() : 0;

						ImGui::TableSetColumnIndex(0);
						FString KeyStr = FString::Printf(TEXT("[%d:%d] %s"), Key.OwnerEntity.Index, Key.OwnerEntity.SerialNumber, *Key.Tag.ToString());
						ImGui::PushID(TCHAR_TO_UTF8(*KeyStr));
						bool bExpanded = ImGui::TreeNodeEx("##entry", ImGuiTreeNodeFlags_None, "%s", TCHAR_TO_UTF8(*KeyStr));

						ImGui::TableSetColumnIndex(1);
						ImGui::Text("%d", CandidateCount);

						ImGui::TableSetColumnIndex(2);
						bool bDrawKey = SelectedGlobalKeys.Contains(Key);
						if (ImGui::Checkbox("##draw", &bDrawKey))
						{
							if (bDrawKey)
							{
								SelectedGlobalKeys.Add(Key);
							}
							else
							{
								SelectedGlobalKeys.Remove(Key);
							}
						}

						if (bExpanded)
						{
							ImGui::TableNextRow();
							ImGui::TableSetColumnIndex(0);
							if (Candidates == nullptr)
							{
								ImGui::TextDisabled("Pending result.");
							}
							else if (Candidates->IsEmpty())
							{
								ImGui::TextDisabled("No candidates.");
							}
							else if (ImGui::BeginTable("candidate_details", 4,
								ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
							{
								ImGui::TableSetupColumn("Entity", ImGuiTableColumnFlags_WidthFixed, 90.f);
								ImGui::TableSetupColumn("Score", ImGuiTableColumnFlags_WidthFixed, 70.f);
								ImGui::TableSetupColumn("Hit Actor", ImGuiTableColumnFlags_WidthStretch);
								ImGui::TableSetupColumn("Impact Point", ImGuiTableColumnFlags_WidthStretch);
								ImGui::TableHeadersRow();

								for (const FArcTargetCandidate& Candidate : *Candidates)
								{
									ImGui::TableNextRow();
									ImGui::TableSetColumnIndex(0);
									ImGui::Text("[%d:%d]", Candidate.Entity.Index, Candidate.Entity.SerialNumber);
									ImGui::TableSetColumnIndex(1);
									ImGui::Text("%.3f", Candidate.Score);
									ImGui::TableSetColumnIndex(2);
									const AActor* HitActor = Candidate.HitResult.GetActor();
									FString HitActorName = IsValid(HitActor) ? HitActor->GetName() : FString(TEXT("<none>"));
									ImGui::TextUnformatted(TCHAR_TO_UTF8(*HitActorName));
									ImGui::TableSetColumnIndex(3);
									const FVector& ImpactPoint = Candidate.HitResult.ImpactPoint;
									ImGui::Text("(%.1f, %.1f, %.1f)", ImpactPoint.X, ImpactPoint.Y, ImpactPoint.Z);
								}

								ImGui::EndTable();
							}
							ImGui::TreePop();
						}
						ImGui::PopID();
					}

					ImGui::EndTable();
				}
			}

			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
	}

	ImGui::EndChild();

	// -----------------------------------------------------------------------
	// Push render data to the draw actor
	// -----------------------------------------------------------------------

	AArcTargetingDebuggerDrawActor* Actor = DrawActor.Get();
	if (Actor)
	{
		ArcTargetingDebuggerLocal::PushTracesToActor(Actor, Traces, SelectedHistoryIndices);
		ArcTargetingDebuggerLocal::PushPersistentHitsToActor(Actor, Sub, SelectedGlobalKeys);
	}

	ImGui::End();
}

// ---------------------------------------------------------------------------
// Non-debug stubs — defined outside the guard so the translation unit always
// provides definitions when UE_DEBUG_RENDERING_ENABLED is off.
// ---------------------------------------------------------------------------

#else // UE_DEBUG_RENDERING_ENABLED

void FArcTargetingDebugger::Initialize()   {}
void FArcTargetingDebugger::Uninitialize() {}
void FArcTargetingDebugger::Draw()         {}

#endif // UE_DEBUG_RENDERING_ENABLED
