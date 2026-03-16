// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcNeedsDebugger.h"

#include "CoreMinimal.h"
#include "imgui.h"
#include "MassActorSubsystem.h"
#include "MassDebugger.h"
#include "MassEntityElementTypes.h"
#include "MassEntitySubsystem.h"
#include "AbilitySystemComponent.h"
#include "Attributes/ArcNeedsSurvivalAttributeSet.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Fragments/ArcNeedFragment.h"
#include "Player/ArcHeroComponentBase.h"

namespace Arcx::GameplayDebugger::Needs
{
	UWorld* GetDebugWorld()
	{
		if (!GEngine || !GEngine->GameViewport)
		{
			return nullptr;
		}
		return GEngine->GameViewport->GetWorld();
	}

	FMassEntityManager* GetEntityManager()
	{
		UWorld* World = GetDebugWorld();
		if (!World)
		{
			return nullptr;
		}
		UMassEntitySubsystem* MassSub = World->GetSubsystem<UMassEntitySubsystem>();
		if (!MassSub)
		{
			return nullptr;
		}
		return &MassSub->GetMutableEntityManager();
	}

	ImVec4 GetNeedValueColor(float Value)
	{
		// Green (0) -> Yellow (50) -> Red (100)
		if (Value <= 50.f)
		{
			float T = Value / 50.f;
			return ImVec4(T, 1.0f, 0.0f, 1.0f);
		}
		else
		{
			float T = (Value - 50.f) / 50.f;
			return ImVec4(1.0f, 1.0f - T, 0.0f, 1.0f);
		}
	}
} // namespace Arcx::GameplayDebugger::Needs

void FArcNeedsDebugger::Initialize()
{
	SelectedEntityIdx = INDEX_NONE;
	FilterBuf[0] = '\0';
	CachedEntities.Reset();
	LastRefreshTime = 0.f;
	RefreshEntityList();
}

void FArcNeedsDebugger::Uninitialize()
{
	CachedEntities.Reset();
	SelectedEntityIdx = INDEX_NONE;
}

void FArcNeedsDebugger::RefreshEntityList()
{
	CachedEntities.Reset();

#if WITH_MASSENTITY_DEBUG
	FMassEntityManager* Manager = Arcx::GameplayDebugger::Needs::GetEntityManager();
	if (!Manager)
	{
		return;
	}

	const UScriptStruct* HungerType = FArcHungerNeedFragment::StaticStruct();
	const UScriptStruct* ThirstType = FArcThirstNeedFragment::StaticStruct();
	const UScriptStruct* FatigueType = FArcFatigueNeedFragment::StaticStruct();

	TArray<FMassArchetypeHandle> Archetypes = FMassDebugger::GetAllArchetypes(*Manager);

	for (const FMassArchetypeHandle& Archetype : Archetypes)
	{
		FMassArchetypeCompositionDescriptor Composition = FMassDebugger::GetArchetypeComposition(Archetype);

		const bool bHasHunger = Composition.Contains(HungerType);
		const bool bHasThirst = Composition.Contains(ThirstType);
		const bool bHasFatigue = Composition.Contains(FatigueType);

		if (!bHasHunger && !bHasThirst && !bHasFatigue)
		{
			continue;
		}

		TArray<FMassEntityHandle> Entities = FMassDebugger::GetEntitiesOfArchetype(Archetype);
		for (const FMassEntityHandle& Entity : Entities)
		{
			FEntityEntry& Entry = CachedEntities.AddDefaulted_GetRef();
			Entry.Entity = Entity;
			Entry.NeedInfo.bHasHunger = bHasHunger;
			Entry.NeedInfo.bHasThirst = bHasThirst;
			Entry.NeedInfo.bHasFatigue = bHasFatigue;

			// Try to get actor name
			FStructView ActorFragView = Manager->GetFragmentDataStruct(Entity, FMassActorFragment::StaticStruct());
			if (ActorFragView.IsValid())
			{
				const FMassActorFragment& ActorFrag = ActorFragView.Get<FMassActorFragment>();
				if (const AActor* Actor = ActorFrag.Get())
				{
					Entry.Label = Actor->GetActorNameOrLabel();
				}
			}

			if (Entry.Label.IsEmpty())
			{
				Entry.Label = Entity.DebugGetDescription();
			}
		}
	}
#endif
}

void FArcNeedsDebugger::Draw()
{
#if WITH_MASSENTITY_DEBUG
	ImGui::SetNextWindowSize(ImVec2(850, 500), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Needs Debugger", &bShow))
	{
		ImGui::End();
		return;
	}

	FMassEntityManager* Manager = Arcx::GameplayDebugger::Needs::GetEntityManager();
	if (!Manager)
	{
		ImGui::TextDisabled("No MassEntitySubsystem available");
		ImGui::End();
		return;
	}

	// Auto-refresh every 2 seconds
	UWorld* World = Arcx::GameplayDebugger::Needs::GetDebugWorld();
	if (World)
	{
		float CurrentTime = World->GetTimeSeconds();
		if (CurrentTime - LastRefreshTime > 2.0f)
		{
			LastRefreshTime = CurrentTime;
			RefreshEntityList();
		}
	}

	if (ImGui::Button("Refresh"))
	{
		RefreshEntityList();
	}
	ImGui::SameLine();
	ImGui::Text("Entities: %d", CachedEntities.Num());

	ImGui::Separator();

	// Two-pane layout
	float PanelWidth = ImGui::GetContentRegionAvail().x;
	float LeftPanelWidth = PanelWidth * 0.35f;

	// Left panel: entity list
	if (ImGui::BeginChild("NeedsEntityListPanel", ImVec2(LeftPanelWidth, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX))
	{
		DrawEntityListPanel();
	}
	ImGui::EndChild();

	ImGui::SameLine();

	// Right panel: detail
	if (ImGui::BeginChild("NeedsDetailPanel", ImVec2(0, 0), ImGuiChildFlags_Borders))
	{
		DrawDetailPanel();
	}
	ImGui::EndChild();

	ImGui::End();
#endif
}

void FArcNeedsDebugger::DrawEntityListPanel()
{
#if WITH_MASSENTITY_DEBUG
	ImGui::Text("Entity List");
	ImGui::Separator();

	ImGui::InputText("Filter", FilterBuf, IM_ARRAYSIZE(FilterBuf));

	FString Filter = FString(ANSI_TO_TCHAR(FilterBuf)).ToLower();

	if (ImGui::BeginChild("NeedsEntityScroll", ImVec2(0, 0)))
	{
		for (int32 i = 0; i < CachedEntities.Num(); ++i)
		{
			const FEntityEntry& Entry = CachedEntities[i];

			if (!Filter.IsEmpty() && !Entry.Label.ToLower().Contains(Filter))
			{
				continue;
			}

			// Build label with need badges
			FString DisplayLabel = Entry.Label;
			if (Entry.NeedInfo.bHasHunger)  { DisplayLabel += TEXT(" [H]"); }
			if (Entry.NeedInfo.bHasThirst)  { DisplayLabel += TEXT(" [T]"); }
			if (Entry.NeedInfo.bHasFatigue) { DisplayLabel += TEXT(" [F]"); }

			const bool bSelected = (SelectedEntityIdx == i);
			if (ImGui::Selectable(TCHAR_TO_ANSI(*DisplayLabel), bSelected))
			{
				SelectedEntityIdx = i;
			}
		}
	}
	ImGui::EndChild();
#endif
}

void FArcNeedsDebugger::DrawDetailPanel()
{
#if WITH_MASSENTITY_DEBUG
	if (SelectedEntityIdx == INDEX_NONE || !CachedEntities.IsValidIndex(SelectedEntityIdx))
	{
		ImGui::TextDisabled("Select an entity from the list");
		return;
	}

	FMassEntityManager* Manager = Arcx::GameplayDebugger::Needs::GetEntityManager();
	if (!Manager)
	{
		return;
	}

	const FEntityEntry& Entry = CachedEntities[SelectedEntityIdx];
	const FMassEntityHandle Entity = Entry.Entity;

	if (!Manager->IsEntityActive(Entity))
	{
		ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Entity is no longer active");
		return;
	}

	ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "%s", TCHAR_TO_ANSI(*Entry.Label));
	ImGui::Separator();

	// Need Values section
	ImGuiTreeNodeFlags HeaderFlags = ImGuiTreeNodeFlags_DefaultOpen;
	if (ImGui::CollapsingHeader("Need Values", HeaderFlags))
	{
		constexpr ImGuiTableFlags TableFlags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV | ImGuiTableFlags_SizingStretchProp;
		if (ImGui::BeginTable("NeedValuesTable", 5, TableFlags))
		{
			ImGui::TableSetupColumn("Need",         ImGuiTableColumnFlags_WidthFixed, 80.f);
			ImGui::TableSetupColumn("CurrentValue", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("ChangeRate",   ImGuiTableColumnFlags_WidthFixed, 80.f);
			ImGui::TableSetupColumn("Resistance",   ImGuiTableColumnFlags_WidthFixed, 80.f);
			ImGui::TableSetupColumn("NeedType",     ImGuiTableColumnFlags_WidthFixed, 70.f);
			ImGui::TableHeadersRow();

			struct FNeedRow
			{
				const char* Name;
				const UScriptStruct* FragType;
				bool bPresent;
			};

			FNeedRow Rows[] = {
				{ "Hunger",  FArcHungerNeedFragment::StaticStruct(),  Entry.NeedInfo.bHasHunger  },
				{ "Thirst",  FArcThirstNeedFragment::StaticStruct(),  Entry.NeedInfo.bHasThirst  },
				{ "Fatigue", FArcFatigueNeedFragment::StaticStruct(), Entry.NeedInfo.bHasFatigue },
			};

			for (const FNeedRow& Row : Rows)
			{
				if (!Row.bPresent)
				{
					continue;
				}

				FStructView FragView = Manager->GetFragmentDataStruct(Entity, Row.FragType);
				if (!FragView.IsValid())
				{
					continue;
				}

				const FArcNeedFragment& Frag = FragView.Get<FArcNeedFragment>();

				ImGui::TableNextRow();

				ImGui::TableSetColumnIndex(0);
				ImGui::Text("%s", Row.Name);

				ImGui::TableSetColumnIndex(1);
				{
					float NormalizedVal = FMath::Clamp(Frag.CurrentValue / 100.f, 0.f, 1.f);
					ImVec4 BarColor = Arcx::GameplayDebugger::Needs::GetNeedValueColor(Frag.CurrentValue);
					ImGui::PushStyleColor(ImGuiCol_PlotHistogram, BarColor);
					FString BarLabel = FString::Printf(TEXT("%.1f"), Frag.CurrentValue);
					ImGui::ProgressBar(NormalizedVal, ImVec2(-1.f, 0.f), TCHAR_TO_ANSI(*BarLabel));
					ImGui::PopStyleColor();
				}

				ImGui::TableSetColumnIndex(2);
				ImGui::Text("%.3f", Frag.ChangeRate);

				ImGui::TableSetColumnIndex(3);
				ImGui::Text("%.3f", Frag.Resistance);

				ImGui::TableSetColumnIndex(4);
				ImGui::Text("%u", Frag.NeedType);
			}

			ImGui::EndTable();
		}
	}

	ImGui::Spacing();

	// Survival Attributes section
	{
		const FArcCoreAbilitySystemFragment* ASCFrag =
			Manager->GetFragmentDataPtr<FArcCoreAbilitySystemFragment>(Entity);

		if (ASCFrag && ASCFrag->AbilitySystem.IsValid())
		{
			UArcCoreAbilitySystemComponent* ASC = ASCFrag->AbilitySystem.Get();

			const UArcNeedsSurvivalAttributeSet* SurvivalSet = nullptr;
			for (const UAttributeSet* AttrSet : ASC->GetSpawnedAttributes())
			{
				SurvivalSet = Cast<UArcNeedsSurvivalAttributeSet>(AttrSet);
				if (SurvivalSet)
				{
					break;
				}
			}

			if (SurvivalSet)
			{
				ImGui::SeparatorText("Survival Attributes");

				constexpr ImGuiTableFlags AttrTableFlags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV | ImGuiTableFlags_SizingStretchProp;
				if (ImGui::BeginTable("SurvivalAttribTable", 2, AttrTableFlags))
				{
					ImGui::TableSetupColumn("Attribute", ImGuiTableColumnFlags_WidthFixed, 160.f);
					ImGui::TableSetupColumn("Value",     ImGuiTableColumnFlags_WidthStretch);
					ImGui::TableHeadersRow();

					struct FAttrRow { const char* Name; float Value; };
					FAttrRow AttrRows[] = {
						{ "AddHunger",    SurvivalSet->GetAddHunger()    },
						{ "RemoveHunger", SurvivalSet->GetRemoveHunger() },
						{ "AddThirst",    SurvivalSet->GetAddThirst()    },
						{ "RemoveThirst", SurvivalSet->GetRemoveThirst() },
						{ "AddFatigue",   SurvivalSet->GetAddFatigue()   },
						{ "RemoveFatigue",SurvivalSet->GetRemoveFatigue()},
						{ "Fatigue",      SurvivalSet->GetFatigue()      },
					};

					for (const FAttrRow& AR : AttrRows)
					{
						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::Text("%s", AR.Name);
						ImGui::TableSetColumnIndex(1);
						ImGui::Text("%.4f", AR.Value);
					}

					ImGui::EndTable();
				}

				ImGui::TextDisabled("Add/Remove attributes are meta-attributes that drain to zero immediately.");
			}
		}
	}
#endif
}
