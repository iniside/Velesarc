// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcConditionImGuiDebugger.h"

#include "EngineUtils.h"
#include "imgui.h"
#include "MassAgentComponent.h"
#include "MassEntitySubsystem.h"
#include "Kismet/GameplayStatics.h"

#include "ArcConditionEffects/ArcConditionFragments.h"
#include "ArcConditionEffects/ArcConditionTypes.h"
#include "ArcConditionEffects/ArcConditionEffectsSubsystem.h"
#include "ArcConditionEffects/ArcConditionAttributeSet.h"
#include "Player/ArcHeroComponentBase.h"
#include "AbilitySystem/ArcCoreAbilitySystemComponent.h"
#include "Engine/GameViewportClient.h"

// ---------------------------------------------------------------------------
// Color constants
// ---------------------------------------------------------------------------

static const ImVec4 ColorHysteresis	= ImVec4(1.f, 0.3f, 0.3f, 1.f);
static const ImVec4 ColorLinear		= ImVec4(1.f, 1.f, 0.3f, 1.f);
static const ImVec4 ColorEnvironment	= ImVec4(0.3f, 1.f, 1.f, 1.f);
static const ImVec4 ColorActive		= ImVec4(0.3f, 1.f, 0.3f, 1.f);
static const ImVec4 ColorInactive		= ImVec4(0.5f, 0.5f, 0.5f, 1.f);

// ---------------------------------------------------------------------------
// Condition type name table
// ---------------------------------------------------------------------------

static const char* ConditionTypeNames[] =
{
	"Burning",
	"Bleeding",
	"Chilled",
	"Shocked",
	"Poisoned",
	"Diseased",
	"Weakened",
	"Oiled",
	"Wet",
	"Corroded",
	"Blinded",
	"Suffocating",
	"Exhausted",
};
static_assert(UE_ARRAY_COUNT(ConditionTypeNames) == ArcConditionTypeCount, "ConditionTypeNames must match EArcConditionType::MAX");

static const char* OverloadPhaseNames[] = { "None", "Overloaded", "Burnout" };

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace Arcx::GameplayDebugger::Conditions
{
	static UWorld* GetDebugWorld()
	{
		if (!GEngine || !GEngine->GameViewport)
		{
			return nullptr;
		}
		return GEngine->GameViewport->GetWorld();
	}	
}


// ---------------------------------------------------------------------------
// Draw a single condition row in the fragment panel
// ---------------------------------------------------------------------------

static void DrawConditionRow(
	const char* Name,
	const FArcConditionState& State,
	const FArcConditionConfig* Config,
	const ImVec4& GroupColor)
{
	ImGui::PushStyleColor(ImGuiCol_Text, GroupColor);
	ImGui::Text("%s", Name);
	ImGui::PopStyleColor();

	ImGui::SameLine(120.f);

	// Saturation progress bar
	char SatBuf[32];
	snprintf(SatBuf, sizeof(SatBuf), "%.1f / 100", State.Saturation);
	ImGui::ProgressBar(State.Saturation / 100.f, ImVec2(120.f, 0.f), SatBuf);

	// Active flag
	ImGui::SameLine();
	if (State.bActive)
	{
		ImGui::TextColored(ColorActive, "ACTIVE");
	}
	else
	{
		ImGui::TextColored(ColorInactive, "inactive");
	}

	// Overload phase
	ImGui::SameLine();
	if (State.OverloadPhase == EArcConditionOverloadPhase::Overloaded)
	{
		ImGui::TextColored(ImVec4(1.f, 0.2f, 0.2f, 1.f), "OVERLOADED %.1fs", State.OverloadTimeRemaining);
	}
	else if (State.OverloadPhase == EArcConditionOverloadPhase::Burnout)
	{
		ImGui::TextColored(ImVec4(1.f, 0.6f, 0.2f, 1.f), "BURNOUT %.1fs", State.OverloadTimeRemaining);
	}
	else
	{
		ImGui::TextColored(ColorInactive, "---");
	}

	// Resistance
	ImGui::SameLine();
	if (State.Resistance > 0.f)
	{
		ImGui::Text("Res: %.0f%%", State.Resistance * 100.f);
	}
	else
	{
		ImGui::TextColored(ColorInactive, "Res: 0%%");
	}

	// Config tooltip
	if (Config && ImGui::IsItemHovered())
	{
		ImGui::BeginTooltip();
		ImGui::Text("Decay Rate: %.1f/s", Config->DecayRate);
		ImGui::Text("Activation Threshold: %.0f", Config->ActivationThreshold);
		ImGui::Text("Overload Duration: %.1fs", Config->OverloadDuration);
		ImGui::Text("Burnout Duration: %.1fs", Config->BurnoutDuration);
		ImGui::Text("Burnout Decay Mult: %.1f", Config->BurnoutDecayMultiplier);
		ImGui::Text("Burnout Target: %.0f", Config->BurnoutTargetSaturation);
		ImGui::EndTooltip();
	}
}

// ---------------------------------------------------------------------------
// Macro to reduce repetition for all 13 conditions
// ---------------------------------------------------------------------------

#define ARC_IMGUI_DRAW_CONDITION(Name, GroupColor) \
	{ \
		const FArc##Name##ConditionFragment* Frag = EntityManager.GetFragmentDataPtr<FArc##Name##ConditionFragment>(Entity); \
		if (Frag) \
		{ \
			bHasAnyCondition = true; \
			const FArc##Name##ConditionConfig* Cfg = EntityManager.GetConstSharedFragmentDataPtr<FArc##Name##ConditionConfig>(Entity); \
			DrawConditionRow(#Name, Frag->State, Cfg ? &Cfg->Config : nullptr, GroupColor); \
		} \
	}

#define ARC_IMGUI_CHECK_HAS_CONDITION(Name) \
	EntityManager.GetFragmentDataPtr<FArc##Name##ConditionFragment>(Entity) != nullptr

// ---------------------------------------------------------------------------
// Macro for attribute set comparison row
// ---------------------------------------------------------------------------

#define ARC_IMGUI_ATTR_ROW(Name) \
	{ \
		const FArc##Name##ConditionFragment* Frag = EntityManager.GetFragmentDataPtr<FArc##Name##ConditionFragment>(Entity); \
		float AttrSat = AttrSet->Get##Name##Saturation(); \
		float FragSat = Frag ? Frag->State.Saturation : 0.f; \
		bool bMatch = FMath::IsNearlyEqual(AttrSat, FragSat, 0.1f); \
		ImGui::Text("%-12s", #Name); \
		ImGui::SameLine(100.f); \
		ImGui::Text("Attr: %5.1f", AttrSat); \
		ImGui::SameLine(200.f); \
		ImGui::Text("Frag: %5.1f", FragSat); \
		ImGui::SameLine(300.f); \
		if (bMatch) \
		{ \
			ImGui::TextColored(ColorActive, "OK"); \
		} \
		else \
		{ \
			ImGui::TextColored(ImVec4(1.f, 0.3f, 0.3f, 1.f), "MISMATCH"); \
		} \
	}

// ---------------------------------------------------------------------------
// Initialize
// ---------------------------------------------------------------------------

void FArcConditionImGuiDebugger::Initialize()
{
	UWorld* World = Arcx::GameplayDebugger::Conditions::GetDebugWorld();
	if (!World)
	{
		return;
	}

	Entities.Reset(128);

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		UMassAgentComponent* AgentComponent = It->FindComponentByClass<UMassAgentComponent>();
		if (AgentComponent)
		{
			Entities.Add(AgentComponent->GetEntityHandle());
		}
	}

	SelectedEntityIdx = -1;
}

// ---------------------------------------------------------------------------
// Uninitialize
// ---------------------------------------------------------------------------

void FArcConditionImGuiDebugger::Uninitialize()
{
	Entities.Reset();
	SelectedEntityIdx = -1;
}

// ---------------------------------------------------------------------------
// Draw
// ---------------------------------------------------------------------------

void FArcConditionImGuiDebugger::Draw()
{
	if (!bShow)
	{
		return;
	}

	UWorld* World = Arcx::GameplayDebugger::Conditions::GetDebugWorld();
	if (!World)
	{
		return;
	}

	UMassEntitySubsystem* MassSub = World->GetSubsystem<UMassEntitySubsystem>();
	if (!MassSub)
	{
		return;
	}

	FMassEntityManager& EntityManager = MassSub->GetMutableEntityManager();

	ImGui::SetNextWindowSize(ImVec2(1200.f, 600.f), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Condition Debugger", &bShow))
	{
		ImGui::End();
		return;
	}

	// --- Pending requests info ---
	UArcConditionEffectsSubsystem* CondSubsystem = World->GetSubsystem<UArcConditionEffectsSubsystem>();
	if (CondSubsystem)
	{
		int32 PendingCount = CondSubsystem->GetPendingRequests().Num();
		ImGui::Text("Pending Application Requests: %d", PendingCount);
	}
	else
	{
		ImGui::TextColored(ColorInactive, "UArcConditionEffectsSubsystem not available");
	}

	ImGui::Separator();

	// ===================================================================
	// Panel 1: Entity List (left)
	// ===================================================================

	ImGui::BeginChild("entity_list", ImVec2(280.f, 0.f), true);
	ImGui::Text("Entities (%d)", Entities.Num());

	if (ImGui::Button("Refresh"))
	{
		Initialize();
	}

	ImGui::Separator();

	for (int32 Idx = 0; Idx < Entities.Num(); ++Idx)
	{
		FMassEntityHandle Entity = Entities[Idx];
		if (!EntityManager.IsEntityValid(Entity))
		{
			continue;
		}

		FString Desc = Entity.DebugGetDescription();

		// Check if entity has at least one condition fragment
		bool bHasCondition =
			ARC_IMGUI_CHECK_HAS_CONDITION(Burning) ||
			ARC_IMGUI_CHECK_HAS_CONDITION(Bleeding) ||
			ARC_IMGUI_CHECK_HAS_CONDITION(Chilled) ||
			ARC_IMGUI_CHECK_HAS_CONDITION(Shocked) ||
			ARC_IMGUI_CHECK_HAS_CONDITION(Poisoned) ||
			ARC_IMGUI_CHECK_HAS_CONDITION(Diseased) ||
			ARC_IMGUI_CHECK_HAS_CONDITION(Weakened) ||
			ARC_IMGUI_CHECK_HAS_CONDITION(Oiled) ||
			ARC_IMGUI_CHECK_HAS_CONDITION(Wet) ||
			ARC_IMGUI_CHECK_HAS_CONDITION(Corroded) ||
			ARC_IMGUI_CHECK_HAS_CONDITION(Blinded) ||
			ARC_IMGUI_CHECK_HAS_CONDITION(Suffocating) ||
			ARC_IMGUI_CHECK_HAS_CONDITION(Exhausted);

		if (bHasCondition)
		{
			ImGui::PushStyleColor(ImGuiCol_Text, ColorActive);
		}

		bool bSelected = (Idx == SelectedEntityIdx);
		if (ImGui::Selectable(TCHAR_TO_ANSI(*Desc), bSelected))
		{
			SelectedEntityIdx = Idx;
		}

		if (bHasCondition)
		{
			ImGui::PopStyleColor();
		}
	}

	ImGui::EndChild();

	ImGui::SameLine();

	// ===================================================================
	// Panel 2: Mass Fragment Data (center) — split top/bottom
	// ===================================================================

	ImGui::BeginChild("fragment_column", ImVec2(500.f, 0.f), false);

	bool bHasValidEntity = SelectedEntityIdx >= 0 && SelectedEntityIdx < Entities.Num();
	FMassEntityHandle SelectedEntity;
	if (bHasValidEntity)
	{
		SelectedEntity = Entities[SelectedEntityIdx];
		bHasValidEntity = EntityManager.IsEntityValid(SelectedEntity);
	}

	// --- Top: Condition Values ---
	float AvailableHeight = ImGui::GetContentRegionAvail().y;
	ImGui::BeginChild("fragment_values", ImVec2(0.f, AvailableHeight * 0.55f), true);
	ImGui::Text("Condition Values");
	ImGui::Separator();

	if (bHasValidEntity)
	{
		FMassEntityHandle Entity = SelectedEntity;
		bool bHasAnyCondition = false;

		// Group A: Hysteresis
		ImGui::TextColored(ColorHysteresis, "--- Hysteresis ---");
		ARC_IMGUI_DRAW_CONDITION(Burning, ColorHysteresis)
		ARC_IMGUI_DRAW_CONDITION(Bleeding, ColorHysteresis)
		ARC_IMGUI_DRAW_CONDITION(Chilled, ColorHysteresis)
		ARC_IMGUI_DRAW_CONDITION(Shocked, ColorHysteresis)
		ARC_IMGUI_DRAW_CONDITION(Poisoned, ColorHysteresis)
		ARC_IMGUI_DRAW_CONDITION(Diseased, ColorHysteresis)
		ARC_IMGUI_DRAW_CONDITION(Weakened, ColorHysteresis)

		ImGui::Spacing();

		// Group B: Linear
		ImGui::TextColored(ColorLinear, "--- Linear ---");
		ARC_IMGUI_DRAW_CONDITION(Oiled, ColorLinear)
		ARC_IMGUI_DRAW_CONDITION(Wet, ColorLinear)
		ARC_IMGUI_DRAW_CONDITION(Corroded, ColorLinear)

		ImGui::Spacing();

		// Group C: Environmental
		ImGui::TextColored(ColorEnvironment, "--- Environmental ---");
		ARC_IMGUI_DRAW_CONDITION(Blinded, ColorEnvironment)
		ARC_IMGUI_DRAW_CONDITION(Suffocating, ColorEnvironment)
		ARC_IMGUI_DRAW_CONDITION(Exhausted, ColorEnvironment)

		if (!bHasAnyCondition)
		{
			ImGui::TextColored(ColorInactive, "No condition fragments on this entity.");
		}
	}
	else
	{
		ImGui::TextColored(ColorInactive, "Select an entity from the list.");
	}

	ImGui::EndChild();

	// --- Bottom: Apply/Remove Controls ---
	ImGui::BeginChild("apply_controls", ImVec2(0.f, 0.f), true);
	ImGui::Text("Apply / Remove");
	ImGui::Separator();

	if (bHasValidEntity && CondSubsystem)
	{
		FMassEntityHandle Entity = SelectedEntity;

		// Per-condition row: name, drag amount, Add button, Remove button
		#define ARC_IMGUI_APPLY_ROW(Name, EnumVal, GroupColor) \
		{ \
			ImGui::PushID(#Name); \
			ImGui::PushStyleColor(ImGuiCol_Text, GroupColor); \
			ImGui::Text("%-12s", #Name); \
			ImGui::PopStyleColor(); \
			ImGui::SameLine(100.f); \
			ImGui::SetNextItemWidth(100.f); \
			ImGui::DragFloat("##amt", &ApplyAmounts[static_cast<int32>(EArcConditionType::EnumVal)], 0.5f, 0.f, 100.f, "%.1f"); \
			ImGui::SameLine(); \
			if (ImGui::Button("Add")) \
			{ \
				CondSubsystem->ApplyCondition(Entity, EArcConditionType::EnumVal, ApplyAmounts[static_cast<int32>(EArcConditionType::EnumVal)]); \
			} \
			ImGui::SameLine(); \
			if (ImGui::Button("Remove")) \
			{ \
				CondSubsystem->ApplyCondition(Entity, EArcConditionType::EnumVal, -ApplyAmounts[static_cast<int32>(EArcConditionType::EnumVal)]); \
			} \
			ImGui::PopID(); \
		}

		ImGui::TextColored(ColorHysteresis, "--- Hysteresis ---");
		ARC_IMGUI_APPLY_ROW(Burning, Burning, ColorHysteresis)
		ARC_IMGUI_APPLY_ROW(Bleeding, Bleeding, ColorHysteresis)
		ARC_IMGUI_APPLY_ROW(Chilled, Chilled, ColorHysteresis)
		ARC_IMGUI_APPLY_ROW(Shocked, Shocked, ColorHysteresis)
		ARC_IMGUI_APPLY_ROW(Poisoned, Poisoned, ColorHysteresis)
		ARC_IMGUI_APPLY_ROW(Diseased, Diseased, ColorHysteresis)
		ARC_IMGUI_APPLY_ROW(Weakened, Weakened, ColorHysteresis)

		ImGui::Spacing();
		ImGui::TextColored(ColorLinear, "--- Linear ---");
		ARC_IMGUI_APPLY_ROW(Oiled, Oiled, ColorLinear)
		ARC_IMGUI_APPLY_ROW(Wet, Wet, ColorLinear)
		ARC_IMGUI_APPLY_ROW(Corroded, Corroded, ColorLinear)

		ImGui::Spacing();
		ImGui::TextColored(ColorEnvironment, "--- Environmental ---");
		ARC_IMGUI_APPLY_ROW(Blinded, Blinded, ColorEnvironment)
		ARC_IMGUI_APPLY_ROW(Suffocating, Suffocating, ColorEnvironment)
		ARC_IMGUI_APPLY_ROW(Exhausted, Exhausted, ColorEnvironment)

		#undef ARC_IMGUI_APPLY_ROW
	}
	else
	{
		ImGui::TextColored(ColorInactive, "Select a valid entity to apply conditions.");
	}

	ImGui::EndChild();

	ImGui::EndChild();

	ImGui::SameLine();

	// ===================================================================
	// Panel 3: GAS Attribute Set Data (right)
	// ===================================================================

	ImGui::BeginChild("attribute_data", ImVec2(0.f, 0.f), true);
	ImGui::Text("GAS Attribute Set");
	ImGui::Separator();

	if (SelectedEntityIdx >= 0 && SelectedEntityIdx < Entities.Num())
	{
		FMassEntityHandle Entity = Entities[SelectedEntityIdx];
		if (EntityManager.IsEntityValid(Entity))
		{
			const FArcCoreAbilitySystemFragment* ASCFrag =
				EntityManager.GetFragmentDataPtr<FArcCoreAbilitySystemFragment>(Entity);

			if (ASCFrag && ASCFrag->AbilitySystem.IsValid())
			{
				const UArcConditionAttributeSet* AttrSet =
					ASCFrag->AbilitySystem->GetSet<UArcConditionAttributeSet>();

				if (AttrSet)
				{
					ImGui::Text("%-12s  %-12s  %-12s  %s", "Condition", "Attr Sat", "Frag Sat", "Sync");
					ImGui::Separator();

					// Group A
					ImGui::TextColored(ColorHysteresis, "--- Hysteresis ---");
					ARC_IMGUI_ATTR_ROW(Burning)
					ARC_IMGUI_ATTR_ROW(Bleeding)
					ARC_IMGUI_ATTR_ROW(Chilled)
					ARC_IMGUI_ATTR_ROW(Shocked)
					ARC_IMGUI_ATTR_ROW(Poisoned)
					ARC_IMGUI_ATTR_ROW(Diseased)
					ARC_IMGUI_ATTR_ROW(Weakened)

					ImGui::Spacing();

					// Group B
					ImGui::TextColored(ColorLinear, "--- Linear ---");
					ARC_IMGUI_ATTR_ROW(Oiled)
					ARC_IMGUI_ATTR_ROW(Wet)
					ARC_IMGUI_ATTR_ROW(Corroded)

					ImGui::Spacing();

					// Group C
					ImGui::TextColored(ColorEnvironment, "--- Environmental ---");
					ARC_IMGUI_ATTR_ROW(Blinded)
					ARC_IMGUI_ATTR_ROW(Suffocating)
					ARC_IMGUI_ATTR_ROW(Exhausted)
				}
				else
				{
					ImGui::TextColored(ColorInactive, "No UArcConditionAttributeSet on this entity.");
				}
			}
			else
			{
				ImGui::TextColored(ColorInactive, "No AbilitySystemComponent on this entity.");
			}
		}
	}
	else
	{
		ImGui::TextColored(ColorInactive, "Select an entity from the list.");
	}

	ImGui::EndChild();

	ImGui::End();
}

#undef ARC_IMGUI_DRAW_CONDITION
#undef ARC_IMGUI_CHECK_HAS_CONDITION
#undef ARC_IMGUI_ATTR_ROW
