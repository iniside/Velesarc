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
// Condition type name table (ANSI for ImGui)
// ---------------------------------------------------------------------------

static const char* ConditionTypeNamesAnsi[] =
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
static_assert(UE_ARRAY_COUNT(ConditionTypeNamesAnsi) == ArcConditionTypeCount, "ConditionTypeNamesAnsi must match EArcConditionType::MAX");

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
// Helpers — condition group color lookup
// ---------------------------------------------------------------------------

namespace Arcx::GameplayDebugger::Conditions
{
	static const ImVec4& GetGroupColor(int32 ConditionIdx)
	{
		if (ConditionIdx <= (int32)EArcConditionType::Weakened)
		{
			return ColorHysteresis;
		}
		if (ConditionIdx <= (int32)EArcConditionType::Corroded)
		{
			return ColorLinear;
		}
		return ColorEnvironment;
	}

	static const char* GetGroupLabel(int32 ConditionIdx)
	{
		if (ConditionIdx == (int32)EArcConditionType::Burning)
		{
			return "--- Hysteresis ---";
		}
		if (ConditionIdx == (int32)EArcConditionType::Oiled)
		{
			return "--- Linear ---";
		}
		if (ConditionIdx == (int32)EArcConditionType::Blinded)
		{
			return "--- Environmental ---";
		}
		return nullptr;
	}
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

		// Check if entity has the consolidated condition fragment
		bool bHasCondition = EntityManager.GetFragmentDataPtr<FArcConditionStatesFragment>(Entity) != nullptr;

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
		const FArcConditionStatesFragment* StatesFrag = EntityManager.GetFragmentDataPtr<FArcConditionStatesFragment>(Entity);
		const FArcConditionConfigsShared* ConfigsShared = EntityManager.GetConstSharedFragmentDataPtr<FArcConditionConfigsShared>(Entity);

		if (StatesFrag)
		{
			for (int32 Idx = 0; Idx < ArcConditionTypeCount; ++Idx)
			{
				const char* GroupLabel = Arcx::GameplayDebugger::Conditions::GetGroupLabel(Idx);
				if (GroupLabel)
				{
					if (Idx > 0)
					{
						ImGui::Spacing();
					}
					ImGui::TextColored(Arcx::GameplayDebugger::Conditions::GetGroupColor(Idx), "%s", GroupLabel);
				}

				const FArcConditionConfig* Config = ConfigsShared ? &ConfigsShared->Configs[Idx] : nullptr;
				DrawConditionRow(ConditionTypeNamesAnsi[Idx], StatesFrag->States[Idx], Config,
					Arcx::GameplayDebugger::Conditions::GetGroupColor(Idx));
			}
		}
		else
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

		for (int32 Idx = 0; Idx < ArcConditionTypeCount; ++Idx)
		{
			const char* GroupLabel = Arcx::GameplayDebugger::Conditions::GetGroupLabel(Idx);
			if (GroupLabel)
			{
				if (Idx > 0)
				{
					ImGui::Spacing();
				}
				ImGui::TextColored(Arcx::GameplayDebugger::Conditions::GetGroupColor(Idx), "%s", GroupLabel);
			}

			const ImVec4& GroupColor = Arcx::GameplayDebugger::Conditions::GetGroupColor(Idx);
			const EArcConditionType CondType = static_cast<EArcConditionType>(Idx);

			ImGui::PushID(Idx);
			ImGui::PushStyleColor(ImGuiCol_Text, GroupColor);
			ImGui::Text("%-12s", ConditionTypeNamesAnsi[Idx]);
			ImGui::PopStyleColor();
			ImGui::SameLine(100.f);
			ImGui::SetNextItemWidth(100.f);
			ImGui::DragFloat("##amt", &ApplyAmounts[Idx], 0.5f, 0.f, 100.f, "%.1f");
			ImGui::SameLine();
			if (ImGui::Button("Add"))
			{
				CondSubsystem->ApplyCondition(Entity, CondType, ApplyAmounts[Idx]);
			}
			ImGui::SameLine();
			if (ImGui::Button("Remove"))
			{
				CondSubsystem->ApplyCondition(Entity, CondType, -ApplyAmounts[Idx]);
			}
			ImGui::PopID();
		}
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

					const FArcConditionStatesFragment* StatesFrag =
						EntityManager.GetFragmentDataPtr<FArcConditionStatesFragment>(Entity);

					UAbilitySystemComponent* ASC = ASCFrag->AbilitySystem.Get();

					for (int32 Idx = 0; Idx < ArcConditionTypeCount; ++Idx)
					{
						const char* GroupLabel = Arcx::GameplayDebugger::Conditions::GetGroupLabel(Idx);
						if (GroupLabel)
						{
							if (Idx > 0)
							{
								ImGui::Spacing();
							}
							ImGui::TextColored(Arcx::GameplayDebugger::Conditions::GetGroupColor(Idx), "%s", GroupLabel);
						}

						FGameplayAttribute SatAttr = UArcConditionAttributeSet::GetSaturationAttributeByIndex(Idx);
						float AttrSat = SatAttr.IsValid() ? ASC->GetNumericAttribute(SatAttr) : 0.f;
						float FragSat = StatesFrag ? StatesFrag->States[Idx].Saturation : 0.f;
						bool bMatch = FMath::IsNearlyEqual(AttrSat, FragSat, 0.1f);
						ImGui::Text("%-12s", ConditionTypeNamesAnsi[Idx]);
						ImGui::SameLine(100.f);
						ImGui::Text("Attr: %5.1f", AttrSat);
						ImGui::SameLine(200.f);
						ImGui::Text("Frag: %5.1f", FragSat);
						ImGui::SameLine(300.f);
						if (bMatch)
						{
							ImGui::TextColored(ColorActive, "OK");
						}
						else
						{
							ImGui::TextColored(ImVec4(1.f, 0.3f, 0.3f, 1.f), "MISMATCH");
						}
					}
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
