// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcEconomyDebugger.h"

#include "Fragments/ArcNeedFragment.h"
#include "Mass/EntityManager.h"
#include "Mass/EntityView.h"
#include "MassSmartObjectFragments.h"
#include "Sim/ArcBodyUtilityFragments.h"
#include "Sim/ArcEconomyNames.h"
#include "Sim/ArcItemInteractionFragment.h"
#include "Sim/ArcSmartObjectBehaviorFragments.h"
#include "imgui.h"

namespace ArcEconomyDebuggerNPCDetailPrivate
{
	const char* InteractionStatusName(EMassSmartObjectInteractionStatus Status)
	{
		switch (Status)
		{
		case EMassSmartObjectInteractionStatus::Unset:             return "Unset";
		case EMassSmartObjectInteractionStatus::InProgress:        return "InProgress";
		case EMassSmartObjectInteractionStatus::BehaviorCompleted: return "BehaviorCompleted";
		case EMassSmartObjectInteractionStatus::TaskCompleted:     return "TaskCompleted";
		case EMassSmartObjectInteractionStatus::Aborted:           return "Aborted";
		default:                                                   return "?";
		}
	}

	const char* EmploymentKindName(EArcNPCEmploymentKind Kind)
	{
		switch (Kind)
		{
		case EArcNPCEmploymentKind::Work:    return "Worker";
		case EArcNPCEmploymentKind::Gather:  return "Gatherer";
		case EArcNPCEmploymentKind::Carrier: return "Carrier";
		case EArcNPCEmploymentKind::None:    return "None";
		default:                     return "?";
		}
	}

	const char* CommandResultName(EArcBodyCommandResult Result)
	{
		switch (Result)
		{
		case EArcBodyCommandResult::None:      return "None";
		case EArcBodyCommandResult::Succeeded: return "Succeeded";
		case EArcBodyCommandResult::Failed:    return "Failed";
		case EArcBodyCommandResult::Cancelled: return "Cancelled";
		case EArcBodyCommandResult::Aborted:   return "Aborted";
		default:                               return "?";
		}
	}

	ImVec4 GetNeedValueColor(float Value)
	{
		if (Value <= 50.f)
		{
			const float T = Value / 50.f;
			return ImVec4(T, 1.0f, 0.0f, 1.0f);
		}
		const float T = (Value - 50.f) / 50.f;
		return ImVec4(1.0f, 1.0f - T, 0.0f, 1.0f);
	}

	void DrawNeed(const char* Name, const FArcNeedFragment* Need)
	{
		if (Need == nullptr)
		{
			return;
		}

		ImGui::Text("%s", Name);
		const float NormalizedVal = FMath::Clamp(Need->CurrentValue / 100.f, 0.f, 1.f);
		const ImVec4 BarColor = GetNeedValueColor(Need->CurrentValue);
		ImGui::PushStyleColor(ImGuiCol_PlotHistogram, BarColor);
		const FString BarLabel = FString::Printf(TEXT("%.1f"), Need->CurrentValue);
		ImGui::ProgressBar(NormalizedVal, ImVec2(-1.f, 0.f), TCHAR_TO_ANSI(*BarLabel));
		ImGui::PopStyleColor();
		ImGui::Text("ChangeRate: %.3f", Need->ChangeRate);
		ImGui::Text("LastBand: %u", Need->LastBand);
	}
}

void FArcEconomyDebugger::DrawNPCDetailPanel()
{
#if WITH_MASSENTITY_DEBUG
	if (!CachedNPCs.IsValidIndex(SelectedNPCIdx))
	{
		ImGui::TextDisabled("Click an NPC row to inspect");
		return;
	}

	FMassEntityManager* Manager = ArcEconomyDebuggerShared::GetEntityManager();
	if (!Manager)
	{
		return;
	}

	const FNPCEntry& NPC = CachedNPCs[SelectedNPCIdx];
	if (!Manager->IsEntityActive(NPC.Entity))
	{
		ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "NPC entity is no longer active");
		return;
	}

	ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "NPC: %s", TCHAR_TO_ANSI(*NPC.Label));
	ImGui::Text("Entity: %s", TCHAR_TO_ANSI(*NPC.Entity.DebugGetDescription()));

	const FMassEntityView EntityView(*Manager, NPC.Entity);
	const FArcBodyFragment* LiveBody = EntityView.GetFragmentDataPtr<FArcBodyFragment>();
	const FArcSettlementMembershipFragment* Membership =
		EntityView.GetFragmentDataPtr<FArcSettlementMembershipFragment>();
	ImGui::Text("Location: (%.0f, %.0f, %.0f)", NPC.Location.X, NPC.Location.Y, NPC.Location.Z);

	ImGui::SeparatorText("Permanent assignment");
	ImGui::Text("Employment: %s", ArcEconomyDebuggerNPCDetailPrivate::EmploymentKindName(NPC.Kind));
	if (Membership && Membership->Settlement.IsValid())
	{
		ImGui::Text("Settlement: %s", TCHAR_TO_ANSI(*ArcEconomyDebuggerShared::EntityLabel(*Manager, Membership->Settlement)));
	}
	if (NPC.Workplace.IsValid())
	{
		ImGui::Text("Workplace: %s", TCHAR_TO_ANSI(*ArcEconomyDebuggerShared::EntityLabel(*Manager, NPC.Workplace)));
	}
	else if (NPC.Kind == EArcNPCEmploymentKind::Carrier)
	{
		ImGui::TextUnformatted("Assignment: Carrier pool");
	}

	ImGui::SeparatorText("Needs");
	const FArcHungerNeedFragment* Hunger = EntityView.GetFragmentDataPtr<FArcHungerNeedFragment>();
	const FArcThirstNeedFragment* Thirst = EntityView.GetFragmentDataPtr<FArcThirstNeedFragment>();
	const FArcFatigueNeedFragment* Fatigue = EntityView.GetFragmentDataPtr<FArcFatigueNeedFragment>();
	if (Hunger == nullptr && Thirst == nullptr && Fatigue == nullptr)
	{
		ImGui::TextDisabled("No need fragments");
	}
	else
	{
		ArcEconomyDebuggerNPCDetailPrivate::DrawNeed("Hunger", Hunger);
		ArcEconomyDebuggerNPCDetailPrivate::DrawNeed("Thirst", Thirst);
		ArcEconomyDebuggerNPCDetailPrivate::DrawNeed("Fatigue", Fatigue);
	}

	ImGui::SeparatorText("Temporary tasks");
	bool bHasTemporaryTask = false;
	const FArcBodyUtilityIntentFragment* UtilityIntent =
		EntityView.GetSparseFragmentDataPtr<FArcBodyUtilityIntentFragment>();
	if (UtilityIntent && (UtilityIntent->SelectedGoal.IsValid() || UtilityIntent->ActiveCommandSerial != 0))
	{
		bHasTemporaryTask = true;
		const FString Goal = UtilityIntent->SelectedGoal.ToString();
		ImGui::Text("Utility: %s, command %u, request %d, next %.2f",
			TCHAR_TO_ANSI(*Goal),
			UtilityIntent->ActiveCommandSerial,
			UtilityIntent->ScoringRequestId,
			UtilityIntent->NextEvaluationTime);
	}
	if (NPC.bHasTransportJob)
	{
		bHasTemporaryTask = true;
		ImGui::Text("Transport: generation %u, phase %s, command %u",
			NPC.TransportGeneration,
			NPC.TransportPhase == EArcTransportJobPhase::Pickup ? "Pickup" : "Drop",
			NPC.TransportCommandSerial);
		ImGui::Text("Source: %s", TCHAR_TO_ANSI(*ArcEconomyDebuggerShared::EntityLabel(*Manager, NPC.Source)));
		ImGui::Text("Sink: %s", TCHAR_TO_ANSI(*ArcEconomyDebuggerShared::EntityLabel(*Manager, NPC.Sink)));
		ImGui::Text("Order: %s x%d", TCHAR_TO_ANSI(*ArcEconomyNames::ItemName(NPC.Item)), NPC.Quantity);
	}
	if (NPC.Kind == EArcNPCEmploymentKind::Gather)
	{
		bHasTemporaryTask = true;
		ImGui::Text("Gather phase: %s, command %u",
			NPC.GatherPhase == EArcGatherAssignmentPhase::Gather ? "Gather" : "Drop",
			NPC.GatherCommandSerial);
		if (NPC.GatherArea.IsValid())
		{
			ImGui::Text("Gather area: %s",
				TCHAR_TO_ANSI(*ArcEconomyDebuggerShared::EntityLabel(*Manager, NPC.GatherArea)));
		}
		if (NPC.GatherItem.IsValid())
		{
			ImGui::Text("Gather payload: %s x%d",
				TCHAR_TO_ANSI(*ArcEconomyNames::ItemName(NPC.GatherItem)), NPC.GatherQuantity);
		}
	}
	const FArcItemInteractionFragment* ItemInteraction =
		EntityView.GetSparseFragmentDataPtr<FArcItemInteractionFragment>();
	if (ItemInteraction)
	{
		bHasTemporaryTask = true;
		ImGui::Text("Item interaction: generation %u, command %u, %s requested %d, processed %d",
			ItemInteraction->Generation,
			ItemInteraction->CommandSerial,
			TCHAR_TO_ANSI(*ArcEconomyNames::ItemName(ItemInteraction->Item)),
			ItemInteraction->RequestedQuantity,
			ItemInteraction->ProcessedQuantity);
	}
	if (!bHasTemporaryTask)
	{
		ImGui::TextDisabled("None");
	}

	ImGui::SeparatorText("Body command");
	if (LiveBody)
	{
		ImGui::Text("State: %s", ArcEconomyDebuggerShared::BodyStateName(LiveBody->State));
		ImGui::Text("Last command: %u (%s)", LiveBody->LastCommandSerial,
			ArcEconomyDebuggerNPCDetailPrivate::CommandResultName(LiveBody->LastCommandResult));
		if (LiveBody->Command.IsActive())
		{
			ImGui::Text("Command serial: %u", LiveBody->Command.Serial);
			ImGui::Text("Command target: %s", TCHAR_TO_ANSI(*ArcEconomyDebuggerShared::EntityLabel(*Manager, LiveBody->Command.TargetEntity)));
			ImGui::Text("Target slot: %s", TCHAR_TO_ANSI(*LexToString(LiveBody->Command.TargetSlot)));
			ImGui::Text("Claim: %s", TCHAR_TO_ANSI(*LexToString(LiveBody->Command.ClaimHandle)));
			ImGui::Text("Interruptible: %s", LiveBody->Command.bInterruptible ? "true" : "false");
			ImGui::Text("Started using: %s", LiveBody->Command.bStartedUsing ? "true" : "false");
			ImGui::Text("Behavior result: %s",
				ArcEconomyDebuggerNPCDetailPrivate::CommandResultName(LiveBody->Command.BehaviorResult));
		}
	}
	else
	{
		ImGui::Text("State: %s", TCHAR_TO_ANSI(*NPC.StateText));
	}

	ImGui::SeparatorText("Smart Object");
	const FMassSmartObjectUserFragment* User = Manager->GetFragmentDataPtr<FMassSmartObjectUserFragment>(NPC.Entity);
	if (User)
	{
		ImGui::Text("Interaction: %s", ArcEconomyDebuggerNPCDetailPrivate::InteractionStatusName(User->InteractionStatus));
		ImGui::Text("Claim: %s", TCHAR_TO_ANSI(*LexToString(User->InteractionHandle)));
	}
	else
	{
		ImGui::TextDisabled("No FMassSmartObjectUserFragment");
	}

	const FArcSmartObjectBehaviorFragment* Behavior =
		EntityView.GetSparseFragmentDataPtr<FArcSmartObjectBehaviorFragment>();
	if (Behavior)
	{
		ImGui::Text("Behavior serial: %u", Behavior->CommandSerial);
		ImGui::Text("Behavior use: %.1f / %.1f", Behavior->UseElapsed, Behavior->UseDuration);
		ImGui::Text("Effect applied: %s", Behavior->bEffectApplied ? "true" : "false");
		ImGui::Text("Completion queued: %s", Behavior->bCompletionQueued ? "true" : "false");

		const FArcSmartObjectWorkBehaviorFragment* Work =
			EntityView.GetSparseFragmentDataPtr<FArcSmartObjectWorkBehaviorFragment>();
		const FArcSmartObjectPickupBehaviorFragment* Pickup =
			EntityView.GetSparseFragmentDataPtr<FArcSmartObjectPickupBehaviorFragment>();
		const FArcSmartObjectDropBehaviorFragment* Drop =
			EntityView.GetSparseFragmentDataPtr<FArcSmartObjectDropBehaviorFragment>();
		const FArcSmartObjectGatherBehaviorFragment* Gather =
			EntityView.GetSparseFragmentDataPtr<FArcSmartObjectGatherBehaviorFragment>();
		const FArcSmartObjectDrawBehaviorFragment* Draw =
			EntityView.GetSparseFragmentDataPtr<FArcSmartObjectDrawBehaviorFragment>();
		const FArcSmartObjectEatBehaviorFragment* Eat =
			EntityView.GetSparseFragmentDataPtr<FArcSmartObjectEatBehaviorFragment>();
		if (Work && Work->CommandSerial == Behavior->CommandSerial)
		{
			ImGui::Text("Active behavior: Work");
			ImGui::Text("Work shift: %.1f / %.1f", Work->WorkShiftElapsed, Work->WorkShiftDuration);
			ImGui::Text("Work loop: %.1f / %.1f", Work->WorkLoopElapsed, Work->WorkLoopDuration);
		}
		else if (Pickup && Pickup->CommandSerial == Behavior->CommandSerial)
		{
			ImGui::Text("Active behavior: Pickup");
		}
		else if (Drop && Drop->CommandSerial == Behavior->CommandSerial)
		{
			ImGui::Text("Active behavior: Drop");
		}
		else if (Gather && Gather->CommandSerial == Behavior->CommandSerial)
		{
			ImGui::Text("Active behavior: Gather");
		}
		else if (Draw && Draw->CommandSerial == Behavior->CommandSerial)
		{
			ImGui::Text("Active behavior: Draw");
		}
		else if (Eat && Eat->CommandSerial == Behavior->CommandSerial)
		{
			ImGui::Text("Active behavior: Eat");
		}
		else
		{
			ImGui::Text("Active behavior: Unknown");
		}
	}

	ImGui::SeparatorText("Physical cargo");
	if (NPC.Cargo.IsEmpty())
	{
		ImGui::TextDisabled("Empty");
		return;
	}

	constexpr ImGuiTableFlags TableFlags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter
		| ImGuiTableFlags_BordersV | ImGuiTableFlags_SizingStretchProp;
	if (ImGui::BeginTable("BodyStore", 2, TableFlags))
	{
		ImGui::TableSetupColumn("Item", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Qty", ImGuiTableColumnFlags_WidthFixed, 60.0f);
		ImGui::TableHeadersRow();

		for (const FStoreLine& Line : NPC.Cargo)
		{
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::Text("%s", TCHAR_TO_ANSI(*ArcEconomyNames::ItemName(Line.Item)));
			ImGui::TableSetColumnIndex(1);
			ImGui::Text("%d", Line.Count);
		}

		ImGui::EndTable();
	}
#endif
}
