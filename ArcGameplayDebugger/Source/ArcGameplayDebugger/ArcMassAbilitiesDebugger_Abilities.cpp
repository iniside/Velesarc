// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassAbilitiesDebugger.h"

#if WITH_MASSENTITY_DEBUG

#include "imgui.h"
#include "MassEntitySubsystem.h"
#include "Mass/EntityView.h"
#include "Fragments/ArcAbilityCollectionFragment.h"
#include "Fragments/ArcAbilityCooldownFragment.h"
#include "Fragments/ArcAbilityInputFragment.h"
#include "Abilities/ArcAbilityDefinition.h"
#include "Abilities/ArcAbilityStateTreeSubsystem.h"
#include "StateTreeInstanceData.h"
#include "ArcImGuiReflectionWidget.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"

namespace
{
	UWorld* GetDebugWorldAbilities()
	{
		if (!GEngine || !GEngine->GameViewport)
		{
			return nullptr;
		}
		return GEngine->GameViewport->GetWorld();
	}

	const char* RunStatusToString(EStateTreeRunStatus Status)
	{
		switch (Status)
		{
			case EStateTreeRunStatus::Unset:     return "Unset";
			case EStateTreeRunStatus::Running:   return "Running";
			case EStateTreeRunStatus::Succeeded: return "Succeeded";
			case EStateTreeRunStatus::Failed:    return "Failed";
			default:                             return "Unknown";
		}
	}

	ImVec4 RunStatusColor(EStateTreeRunStatus Status)
	{
		switch (Status)
		{
			case EStateTreeRunStatus::Running:   return ImVec4(0.2f, 1.0f, 0.2f, 1.0f);
			case EStateTreeRunStatus::Succeeded: return ImVec4(0.4f, 0.8f, 1.0f, 1.0f);
			case EStateTreeRunStatus::Failed:    return ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
			default:                             return ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
		}
	}
}

void FArcMassAbilitiesDebugger::DrawAbilitiesTab(FMassEntityManager& Manager, FMassEntityHandle Entity)
{
	const FArcAbilityCollectionFragment* CollectionFragment = FMassEntityView(Manager, Entity).GetSparseFragmentDataPtr<FArcAbilityCollectionFragment>();
	if (!CollectionFragment)
	{
		ImGui::TextDisabled("No FArcAbilityCollectionFragment on entity");
		return;
	}

	const TArray<FArcActiveAbility>& Abilities = CollectionFragment->Abilities;

	const float PanelHeight = ImGui::GetContentRegionAvail().y;
	const ImGuiTableFlags PanelTableFlags = ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV;
	if (!ImGui::BeginTable("AbilityPanels", 3, PanelTableFlags, ImVec2(0, PanelHeight)))
	{
		return;
	}

	ImGui::TableSetupColumn("Abilities", ImGuiTableColumnFlags_WidthStretch, 0.25f);
	ImGui::TableSetupColumn("Overview", ImGuiTableColumnFlags_WidthStretch, 0.40f);
	ImGui::TableSetupColumn("StateTree", ImGuiTableColumnFlags_WidthStretch, 0.35f);
	ImGui::TableNextRow();
	ImGui::TableSetColumnIndex(0);

	if (ImGui::BeginChild("AbilityListPanel", ImVec2(0, PanelHeight), ImGuiChildFlags_Borders))
	{
		ImGui::Text("Abilities: %d", Abilities.Num());
		ImGui::Separator();

		for (int32 i = 0; i < Abilities.Num(); ++i)
		{
			const FArcActiveAbility& Ability = Abilities[i];

			FString LabelStr;
			if (!Ability.Definition)
			{
				LabelStr = FString::Printf(TEXT("[%d] (no definition)"), i);
			}
			else
			{
				LabelStr = FString::Printf(TEXT("[%d] %s"), i, *Ability.Definition->GetName());
			}

			bool bSelected = (AbilitySelectedIndex == i);

			if (Ability.bIsActive)
			{
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 1.0f, 0.2f, 1.0f));
			}

			if (ImGui::Selectable(TCHAR_TO_ANSI(*LabelStr), bSelected))
			{
				if (AbilitySelectedIndex != i)
				{
					AbilitySelectedIndex = i;
					AbilitySTSelectedFrameIdx = INDEX_NONE;
					AbilitySTSelectedStateIdx = MAX_uint16;
					bAbilitySTSelectedFrameGlobal = false;
				}
			}

			if (Ability.bIsActive)
			{
				ImGui::PopStyleColor();
			}
		}
	}
	ImGui::EndChild();

	ImGui::TableSetColumnIndex(1);

	if (ImGui::BeginChild("AbilityOverviewPanel", ImVec2(0, PanelHeight), ImGuiChildFlags_Borders))
	{
		if (AbilitySelectedIndex == INDEX_NONE || !Abilities.IsValidIndex(AbilitySelectedIndex))
		{
			ImGui::TextDisabled("Select an ability from the list");
		}
		else
		{
			const FArcActiveAbility& Ability = Abilities[AbilitySelectedIndex];
			const UArcAbilityDefinition* Definition = Ability.Definition;

			if (ImGui::BeginTable("AbilityOverviewTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
			{
				ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthStretch, 0.35f);
				ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.65f);
				ImGui::TableHeadersRow();

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("Handle (Index)");
				ImGui::TableSetColumnIndex(1); ImGui::Text("%d", Ability.Handle.GetIndex());

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("Handle (Generation)");
				ImGui::TableSetColumnIndex(1); ImGui::Text("%d", Ability.Handle.GetGeneration());

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("Definition");
				ImGui::TableSetColumnIndex(1);
				if (Definition)
				{
					ImGui::Text("%s", TCHAR_TO_ANSI(*Definition->GetName()));
				}
				else
				{
					ImGui::TextDisabled("(none)");
				}

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("Definition Path");
				ImGui::TableSetColumnIndex(1);
				if (Definition)
				{
					ImGui::Text("%s", TCHAR_TO_ANSI(*Definition->GetPathName()));
				}
				else
				{
					ImGui::TextDisabled("(none)");
				}

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("InputTag");
				ImGui::TableSetColumnIndex(1);
				if (Ability.InputTag.IsValid())
				{
					ImGui::Text("%s", TCHAR_TO_ANSI(*Ability.InputTag.ToString()));
				}
				else
				{
					ImGui::TextDisabled("(none)");
				}

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("SourceEntity");
				ImGui::TableSetColumnIndex(1);
				if (Ability.SourceEntity.IsSet())
				{
					ImGui::Text("%s", TCHAR_TO_ANSI(*Ability.SourceEntity.DebugGetDescription()));
				}
				else
				{
					ImGui::TextDisabled("(none)");
				}

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("RunStatus");
				ImGui::TableSetColumnIndex(1);
				ImGui::TextColored(RunStatusColor(Ability.RunStatus), "%s", RunStatusToString(Ability.RunStatus));

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("bIsActive");
				ImGui::TableSetColumnIndex(1);
				if (Ability.bIsActive)
				{
					ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "true");
				}
				else
				{
					ImGui::TextUnformatted("false");
				}

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("LastUpdateTime");
				ImGui::TableSetColumnIndex(1); ImGui::Text("%.4f", Ability.LastUpdateTimeInSeconds);

				ImGui::EndTable();
			}

			if (Definition && ImGui::CollapsingHeader("Definition Tags"))
			{
				ImGui::Text("AbilityTags: %s", TCHAR_TO_ANSI(*Definition->AbilityTags.ToStringSimple()));
				ImGui::Text("ActivationRequiredTags: %s", TCHAR_TO_ANSI(*Definition->ActivationRequiredTags.ToStringSimple()));
				ImGui::Text("ActivationBlockedTags: %s", TCHAR_TO_ANSI(*Definition->ActivationBlockedTags.ToStringSimple()));
				ImGui::Text("GrantTagsWhileActive: %s", TCHAR_TO_ANSI(*Definition->GrantTagsWhileActive.ToStringSimple()));
				ImGui::Text("CancelAbilitiesWithTags: %s", TCHAR_TO_ANSI(*Definition->CancelAbilitiesWithTags.ToStringSimple()));
				ImGui::Text("BlockAbilitiesWithTags: %s", TCHAR_TO_ANSI(*Definition->BlockAbilitiesWithTags.ToStringSimple()));
			}

			if (Definition && Definition->Cost.IsValid() && ImGui::CollapsingHeader("Cost"))
			{
				ArcImGuiReflection::DrawStructView(TEXT("Cost"), FConstStructView(Definition->Cost));
			}

			if (Definition && Definition->Cooldown.IsValid() && ImGui::CollapsingHeader("Cooldown Config"))
			{
				ArcImGuiReflection::DrawStructView(TEXT("Cooldown"), FConstStructView(Definition->Cooldown));
			}

			if (!Ability.CustomData.IsEmpty() && ImGui::CollapsingHeader("Custom Data"))
			{
				ArcImGuiReflection::DrawStructView(TEXT("CustomData"), FConstStructView(Ability.CustomData.GetPrimary()));
			}

			const FArcAbilityCooldownFragment* CooldownFragment = Manager.GetFragmentDataPtr<FArcAbilityCooldownFragment>(Entity);
			if (CooldownFragment && ImGui::CollapsingHeader("Cooldowns"))
			{
				const TArray<FArcCooldownEntry>& Cooldowns = CooldownFragment->ActiveCooldowns;
				if (Cooldowns.Num() == 0)
				{
					ImGui::TextDisabled("(none)");
				}
				else if (ImGui::BeginTable("CooldownTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
				{
					ImGui::TableSetupColumn("Ability Handle", ImGuiTableColumnFlags_WidthStretch, 0.25f);
					ImGui::TableSetupColumn("Remaining",      ImGuiTableColumnFlags_WidthStretch, 0.25f);
					ImGui::TableSetupColumn("Max",            ImGuiTableColumnFlags_WidthStretch, 0.25f);
					ImGui::TableSetupColumn("Progress",       ImGuiTableColumnFlags_WidthStretch, 0.25f);
					ImGui::TableHeadersRow();

					for (int32 CIdx = 0; CIdx < Cooldowns.Num(); ++CIdx)
					{
						const FArcCooldownEntry& Entry = Cooldowns[CIdx];

						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0); ImGui::Text("i=%d g=%d", Entry.AbilityHandle.GetIndex(), Entry.AbilityHandle.GetGeneration());
						ImGui::TableSetColumnIndex(1); ImGui::Text("%.3f", Entry.RemainingTime);
						ImGui::TableSetColumnIndex(2); ImGui::Text("%.3f", Entry.MaxCooldownTime);
						ImGui::TableSetColumnIndex(3);
						float Progress = 1.0f - Entry.RemainingTime / FMath::Max(Entry.MaxCooldownTime, 0.001f);
						ImGui::ProgressBar(Progress);
					}

					ImGui::EndTable();
				}
			}

			const FArcAbilityInputFragment* InputFragment = FMassEntityView(Manager, Entity).GetSparseFragmentDataPtr<FArcAbilityInputFragment>();
			if (InputFragment && ImGui::CollapsingHeader("Input State"))
			{
				ImGui::Text("Pressed This Frame:");
				if (InputFragment->PressedThisFrame.Num() == 0)
				{
					ImGui::SameLine();
					ImGui::TextDisabled("(none)");
				}
				else
				{
					ImGui::Indent();
					for (int32 TIdx = 0; TIdx < InputFragment->PressedThisFrame.Num(); ++TIdx)
					{
						ImGui::Text("%s", TCHAR_TO_ANSI(*InputFragment->PressedThisFrame[TIdx].ToString()));
					}
					ImGui::Unindent();
				}

				ImGui::Text("Released This Frame:");
				if (InputFragment->ReleasedThisFrame.Num() == 0)
				{
					ImGui::SameLine();
					ImGui::TextDisabled("(none)");
				}
				else
				{
					ImGui::Indent();
					for (int32 TIdx = 0; TIdx < InputFragment->ReleasedThisFrame.Num(); ++TIdx)
					{
						ImGui::Text("%s", TCHAR_TO_ANSI(*InputFragment->ReleasedThisFrame[TIdx].ToString()));
					}
					ImGui::Unindent();
				}

				ImGui::Text("Held Inputs: %s", TCHAR_TO_ANSI(*InputFragment->HeldInputs.ToStringSimple()));
			}
		}
	}
	ImGui::EndChild();

	ImGui::TableSetColumnIndex(2);

	if (ImGui::BeginChild("AbilityStateTreePanel", ImVec2(0, PanelHeight), ImGuiChildFlags_Borders))
	{
		if (AbilitySelectedIndex == INDEX_NONE || !Abilities.IsValidIndex(AbilitySelectedIndex))
		{
			ImGui::TextDisabled("Select an ability from the list");
		}
		else
		{
			const FArcActiveAbility& Ability = Abilities[AbilitySelectedIndex];
			const UArcAbilityDefinition* Definition = Ability.Definition;

			ImGui::TextUnformatted("State Tree Execution");
			ImGui::Separator();

			if (!Definition)
			{
				ImGui::TextDisabled("Ability has no definition");
			}
			else if (!Ability.InstanceHandle.IsValid())
			{
				ImGui::TextDisabled("Ability has no active StateTree instance");
			}
			else
			{
				UWorld* World = GetDebugWorldAbilities();
				UArcAbilityStateTreeSubsystem* Subsystem = World ? World->GetSubsystem<UArcAbilityStateTreeSubsystem>() : nullptr;
				if (!Subsystem)
				{
					ImGui::TextDisabled("No UArcAbilityStateTreeSubsystem available");
				}
				else
				{
					FStateTreeInstanceData* InstanceData = Subsystem->GetInstanceData(Ability.InstanceHandle);
					if (!InstanceData)
					{
						ImGui::TextDisabled("InstanceData not found for handle");
					}
					else
					{
						DrawStateTreeIntrospection(*InstanceData, Definition->StateTree, AbilitySTSelectedFrameIdx, AbilitySTSelectedStateIdx, bAbilitySTSelectedFrameGlobal, "ability");

						if (AbilitySTSelectedFrameIdx != INDEX_NONE)
						{
							ImGui::Separator();
							DrawStateTreeSelectedStateDetails(*InstanceData, Definition->StateTree, AbilitySTSelectedFrameIdx, AbilitySTSelectedStateIdx, bAbilitySTSelectedFrameGlobal);
						}
					}
				}
			}
		}
	}
	ImGui::EndChild();
	ImGui::EndTable();
}

#endif // WITH_MASSENTITY_DEBUG
