// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcInteractionDebugger.h"

#include "imgui.h"
#include "AbilitySystemComponent.h"
#include "ArcGA_Interact.h"
#include "ArcImGuiReflectionWidget.h"
#include "ArcSmartObjectInteractionComponent.h"
#include "InteractableTargetInterface.h"
#include "Interaction/ArcCoreInteractionSourceComponent.h"
#include "Interaction/ArcInteractionDisplayData.h"
#include "ArcMass/SmartObject/ArcMassSmartObjectFragments.h"
#include "ArcMass/Visualization/ArcVisEntityComponent.h"
#include "DrawDebugHelpers.h"
#include "InstancedActorsComponent.h"
#include "MassAgentComponent.h"
#include "MassEntitySubsystem.h"
#include "ArcMass/ArcMassEntityLibrary.h"
#include "SmartObjectDefinition.h"
#include "ArcGameplayInteractionContext.h"
#include "StateTreeReference.h"
#include "StateTree.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "Types/TargetingSystemTypes.h"

namespace Arcx::GameplayDebugger::Interaction
{
	UWorld* GetDebugWorld()
	{
		if (!GEngine || !GEngine->GameViewport)
		{
			return nullptr;
		}
		return GEngine->GameViewport->GetWorld();
	}

	APlayerController* GetLocalPC()
	{
		UWorld* World = GetDebugWorld();
		if (!World)
		{
			return nullptr;
		}
		return UGameplayStatics::GetPlayerController(World, 0);
	}

	UArcCoreInteractionSourceComponent* GetInteractionSource()
	{
		APlayerController* PC = GetLocalPC();
		if (!PC)
		{
			return nullptr;
		}

		// The source component lives on the PlayerState (UArcCorePlayerStateComponent)
		APlayerState* PS = PC->GetPlayerState<APlayerState>();
		if (!PS)
		{
			return nullptr;
		}

		return PS->FindComponentByClass<UArcCoreInteractionSourceComponent>();
	}

	/** Find IInteractionTarget on an actor (actor itself, hit component, or any component). */
	TScriptInterface<IInteractionTarget> FindInteractionTarget(AActor* Actor)
	{
		if (!Actor)
		{
			return nullptr;
		}

		TScriptInterface<IInteractionTarget> Target(Actor);
		if (Target)
		{
			return Target;
		}

		for (UActorComponent* Comp : Actor->GetComponents())
		{
			TScriptInterface<IInteractionTarget> CompTarget(Comp);
			if (CompTarget)
			{
				return CompTarget;
			}
		}

		return nullptr;
	}

	/** Resolve entity handle from a hit result — tries actor components first, then physics link. */
	FMassEntityHandle ResolveEntityFromHit(const FHitResult& HitResult)
	{
		AActor* HitActor = HitResult.GetActor();
		if (HitActor)
		{
			if (UInstancedActorsComponent* IAComp = HitActor->FindComponentByClass<UInstancedActorsComponent>())
			{
				return IAComp->GetMassEntityHandle();
			}
			if (UMassAgentComponent* AgentComp = HitActor->FindComponentByClass<UMassAgentComponent>())
			{
				return AgentComp->GetEntityHandle();
			}
			if (UArcVisEntityComponent* VisComp = HitActor->FindComponentByClass<UArcVisEntityComponent>())
			{
				return VisComp->GetEntityHandle();
			}
		}

		// Entity-only target — resolve via physics link
		EArcMassResult MassResult = EArcMassResult::NotValid;
		FMassEntityHandle EntityHandle = UArcMassEntityLibrary::ResolveHitToEntity(HitResult, MassResult);
		if (MassResult == EArcMassResult::Valid)
		{
			return EntityHandle;
		}
		return FMassEntityHandle();
	}

	/** Draw a StateTree hierarchy (states only, no runtime). */
	void DrawStateTreeHierarchy(const UStateTree* StateTree)
	{
		if (!StateTree)
		{
			ImGui::TextDisabled("(null StateTree)");
			return;
		}

		TConstArrayView<FCompactStateTreeState> AllStates = StateTree->GetStates();
		if (AllStates.IsEmpty())
		{
			ImGui::TextDisabled("(empty StateTree)");
			return;
		}

		TFunction<void(uint16, uint16)> DrawStates = [&](uint16 Begin, uint16 End)
		{
			for (uint16 Idx = Begin; Idx < End; )
			{
				if (!AllStates.IsValidIndex(Idx))
				{
					break;
				}

				const FCompactStateTreeState& State = AllStates[Idx];
				const bool bHasChildren = State.HasChildren();

				FString StateLabel = State.Name.ToString();
				if (State.Type == EStateTreeStateType::Linked || State.Type == EStateTreeStateType::LinkedAsset)
				{
					StateLabel += TEXT(" (Linked)");
				}
				else if (State.Type == EStateTreeStateType::Subtree)
				{
					StateLabel += TEXT(" (Subtree)");
				}

				ImGuiTreeNodeFlags NodeFlags = ImGuiTreeNodeFlags_SpanAvailWidth;
				if (!bHasChildren)
				{
					NodeFlags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
				}

				ImGui::PushID(Idx);
				bool bNodeOpen = ImGui::TreeNodeEx(TCHAR_TO_ANSI(*StateLabel), NodeFlags);
				ImGui::PopID();

				if (bHasChildren && bNodeOpen)
				{
					DrawStates(State.ChildrenBegin, State.ChildrenEnd);
					ImGui::TreePop();
				}

				Idx = State.GetNextSibling();
			}
		};

		DrawStates(0, static_cast<uint16>(AllStates.Num()));
	}

	/** Draw a single behavior definition, including StateTree if it's an interaction behavior. */
	void DrawBehaviorDefinition(const USmartObjectBehaviorDefinition* BehaviorDef, const char* Label)
	{
		if (!BehaviorDef)
		{
			return;
		}

		if (ImGui::TreeNode(Label))
		{
			ImGui::Text("Class: %s", TCHAR_TO_ANSI(*BehaviorDef->GetClass()->GetName()));
			ImGui::Text("Name: %s", TCHAR_TO_ANSI(*BehaviorDef->GetName()));

			const UArcGameplayInteractionSmartObjectBehaviorDefinition* InteractionBehavior =
				Cast<UArcGameplayInteractionSmartObjectBehaviorDefinition>(BehaviorDef);
			if (InteractionBehavior)
			{
				const UStateTree* StateTree = InteractionBehavior->GetStateTree();
				if (StateTree)
				{
					ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "StateTree: %s", TCHAR_TO_ANSI(*StateTree->GetName()));

					if (ImGui::TreeNode("States"))
					{
						DrawStateTreeHierarchy(StateTree);
						ImGui::TreePop();
					}
				}
				else
				{
					ImGui::TextDisabled("No StateTree set");
				}
			}

			ImGui::TreePop();
		}
	}

	/** Draw SO definition data for an entity. */
	void DrawEntitySOData(const FMassEntityManager& EM, FMassEntityHandle EntityHandle)
	{
		const FArcSmartObjectOwnerFragment* SOOwner = EM.GetFragmentDataPtr<FArcSmartObjectOwnerFragment>(EntityHandle);
		if (SOOwner)
		{
			ImGui::Text("SO Handle Valid: %s", SOOwner->SmartObjectHandle.IsValid() ? "Yes" : "No");
		}

		const FArcSmartObjectDefinitionSharedFragment* SODef = EM.GetConstSharedFragmentDataPtr<FArcSmartObjectDefinitionSharedFragment>(EntityHandle);
		if (!SODef || !SODef->SmartObjectDefinition)
		{
			ImGui::TextDisabled("No SO definition fragment found");
			return;
		}

		const USmartObjectDefinition* Definition = SODef->SmartObjectDefinition;
		ImGui::Text("SO Definition: %s", TCHAR_TO_ANSI(*Definition->GetName()));

		// Default behavior definition (object-level, resolved per slot via public API)
		// We show these as part of each slot below.

		// Per-slot data
		for (int32 SlotIdx = 0; SlotIdx < Definition->GetSlots().Num(); ++SlotIdx)
		{
			const FSmartObjectSlotDefinition& Slot = Definition->GetSlots()[SlotIdx];
			ImGui::PushID(SlotIdx);

			const FArcInteractionDisplayData* DisplayData = Slot.GetDefinitionDataPtr<FArcInteractionDisplayData>();
			FString SlotLabel;
			if (DisplayData)
			{
				SlotLabel = FString::Printf(TEXT("Slot [%d]: %s"), SlotIdx, *DisplayData->DisplayName.ToString());
			}
			else
			{
				SlotLabel = FString::Printf(TEXT("Slot [%d]"), SlotIdx);
			}

			if (ImGui::TreeNode(TCHAR_TO_ANSI(*SlotLabel)))
			{
				if (DisplayData)
				{
					ImGui::Text("Display Name: %s", TCHAR_TO_ANSI(*DisplayData->DisplayName.ToString()));
					ImGui::Text("Interaction Tag: %s", TCHAR_TO_ANSI(*DisplayData->InteractionTag.ToString()));
				}

				// Slot behavior definitions
				if (Slot.BehaviorDefinitions.Num() > 0)
				{
					ImGui::SeparatorText("Behavior Definitions");
					for (int32 DefIdx = 0; DefIdx < Slot.BehaviorDefinitions.Num(); ++DefIdx)
					{
						ImGui::PushID(DefIdx);
						FString DefLabel = FString::Printf(TEXT("Definition [%d]"), DefIdx);
						DrawBehaviorDefinition(Slot.BehaviorDefinitions[DefIdx], TCHAR_TO_ANSI(*DefLabel));
						ImGui::PopID();
					}
				}
				else
				{
					// No slot-level definitions — try resolved (falls back to object-level default)
					const USmartObjectBehaviorDefinition* ResolvedDef =
						Definition->GetBehaviorDefinition(SlotIdx, USmartObjectBehaviorDefinition::StaticClass());
					if (ResolvedDef)
					{
						ImGui::SeparatorText("Resolved Behavior Definition (default)");
						DrawBehaviorDefinition(ResolvedDef, "Default Definition");
					}
				}

				ImGui::TreePop();
			}

			ImGui::PopID();
		}
	}
} // namespace Arcx::GameplayDebugger::Interaction

void FArcInteractionDebugger::Initialize()
{
	SelectedCandidateIdx = INDEX_NONE;
	LastRefreshTime = 0.f;
}

void FArcInteractionDebugger::Uninitialize()
{
	SelectedCandidateIdx = INDEX_NONE;
}

void FArcInteractionDebugger::Draw()
{
	UWorld* World = Arcx::GameplayDebugger::Interaction::GetDebugWorld();
	if (!World)
	{
		return;
	}

	ImGui::SetNextWindowSize(ImVec2(900, 550), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Interaction Debugger", &bShow))
	{
		ImGui::End();
		return;
	}

	UArcCoreInteractionSourceComponent* Source = Arcx::GameplayDebugger::Interaction::GetInteractionSource();
	if (!Source)
	{
		ImGui::TextDisabled("No UArcCoreInteractionSourceComponent found on local player");
		ImGui::End();
		return;
	}

	const FTargetingDefaultResultsSet& Results = Source->GetCachedTargetingResults();
	TScriptInterface<IInteractionTarget> CurrentTarget = Source->GetCurrentTarget();
	FMassEntityHandle CurrentEntityTarget = Source->GetCurrentEntityTarget();

	// Header
	ImGui::Text("Candidates: %d", Results.TargetResults.Num());
	ImGui::SameLine();
	if (CurrentTarget)
	{
		UObject* TargetObj = CurrentTarget.GetObject();
		ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Current: %s", TCHAR_TO_ANSI(*TargetObj->GetName()));
	}
	else if (CurrentEntityTarget.IsValid())
	{
		ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Current: Entity %s", TCHAR_TO_ANSI(*CurrentEntityTarget.DebugGetDescription()));
	}
	else
	{
		ImGui::TextDisabled("Current: None");
	}

	// Trigger interaction button
	{
		bool bCanTrigger = CurrentTarget.GetObject() != nullptr || CurrentEntityTarget.IsValid();
		if (!bCanTrigger)
		{
			ImGui::BeginDisabled();
		}

		if (ImGui::Button("Trigger Interaction"))
		{
			APlayerController* PC = Arcx::GameplayDebugger::Interaction::GetLocalPC();
			if (PC)
			{
				APawn* Pawn = PC->GetPawn();
				if (Pawn)
				{
					UAbilitySystemComponent* ASC = Pawn->FindComponentByClass<UAbilitySystemComponent>();
					if (!ASC)
					{
						APlayerState* PS = PC->GetPlayerState<APlayerState>();
						if (PS)
						{
							ASC = PS->FindComponentByClass<UAbilitySystemComponent>();
						}
					}
					if (ASC)
					{
						FGameplayAbilitySpec* Spec =  ASC->FindAbilitySpecFromClass(UArcGA_Interact::StaticClass());
						if (Spec)
						{
							ASC->TryActivateAbility(Spec->Handle);
						}
					}
				}
			}
		}

		if (!bCanTrigger)
		{
			ImGui::EndDisabled();
			if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
			{
				ImGui::SetTooltip("No current interaction target");
			}
		}
	}

	ImGui::Separator();

	// Two-pane layout
	float PanelWidth = ImGui::GetContentRegionAvail().x;
	float LeftPanelWidth = PanelWidth * 0.4f;

	if (ImGui::BeginChild("InteractionCandidatesPanel", ImVec2(LeftPanelWidth, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX))
	{
		DrawCandidatesPanel();
	}
	ImGui::EndChild();

	ImGui::SameLine();

	if (ImGui::BeginChild("InteractionDetailPanel", ImVec2(0, 0), ImGuiChildFlags_Borders))
	{
		DrawDetailPanel();
	}
	ImGui::EndChild();

	ImGui::End();

	// Viewport debug drawing
	DrawViewportDebug();
}

void FArcInteractionDebugger::DrawCandidatesPanel()
{
	UArcCoreInteractionSourceComponent* Source = Arcx::GameplayDebugger::Interaction::GetInteractionSource();
	if (!Source)
	{
		return;
	}

	const FTargetingDefaultResultsSet& Results = Source->GetCachedTargetingResults();
	TScriptInterface<IInteractionTarget> CurrentTarget = Source->GetCurrentTarget();
	FMassEntityHandle CurrentEntityTarget = Source->GetCurrentEntityTarget();

	ImGui::Text("Targeting Candidates");
	ImGui::Separator();

	constexpr ImGuiTableFlags TableFlags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV | ImGuiTableFlags_ScrollY;
	if (ImGui::BeginTable("CandidatesTable", 5, TableFlags))
	{
		ImGui::TableSetupColumn("#",          ImGuiTableColumnFlags_WidthFixed, 25.f);
		ImGui::TableSetupColumn("Target",     ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Dist",       ImGuiTableColumnFlags_WidthFixed, 55.f);
		ImGui::TableSetupColumn("Score",      ImGuiTableColumnFlags_WidthFixed, 55.f);
		ImGui::TableSetupColumn("Type",       ImGuiTableColumnFlags_WidthFixed, 55.f);
		ImGui::TableHeadersRow();

		for (int32 i = 0; i < Results.TargetResults.Num(); ++i)
		{
			const FTargetingDefaultResultData& Result = Results.TargetResults[i];
			AActor* HitActor = Result.HitResult.GetActor();

			ImGui::PushID(i);
			ImGui::TableNextRow();

			// Determine if this is the current target (actor or entity path)
			bool bIsCurrent = false;
			if (HitActor && CurrentTarget)
			{
				TScriptInterface<IInteractionTarget> CandidateTarget = Arcx::GameplayDebugger::Interaction::FindInteractionTarget(HitActor);
				bIsCurrent = (CandidateTarget.GetObject() == CurrentTarget.GetObject());
			}
			else if (!HitActor && CurrentEntityTarget.IsValid())
			{
				FMassEntityHandle ResolvedEntity = Arcx::GameplayDebugger::Interaction::ResolveEntityFromHit(Result.HitResult);
				bIsCurrent = (ResolvedEntity == CurrentEntityTarget);
			}

			// Highlight current target row
			if (bIsCurrent)
			{
				ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg1, ImGui::GetColorU32(ImVec4(0.15f, 0.4f, 0.15f, 1.0f)));
			}

			// Index
			ImGui::TableSetColumnIndex(0);
			const bool bSelected = (SelectedCandidateIdx == i);
			FString IdxLabel = FString::Printf(TEXT("%d"), i);
			if (ImGui::Selectable(TCHAR_TO_ANSI(*IdxLabel), bSelected, ImGuiSelectableFlags_SpanAllColumns))
			{
				SelectedCandidateIdx = i;
			}

			// Actor/Entity name
			ImGui::TableSetColumnIndex(1);
			if (HitActor)
			{
				ImGui::Text("%s", TCHAR_TO_ANSI(*HitActor->GetActorNameOrLabel()));
			}
			else
			{
				FMassEntityHandle ResolvedEntity = Arcx::GameplayDebugger::Interaction::ResolveEntityFromHit(Result.HitResult);
				if (ResolvedEntity.IsValid())
				{
					ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "[Entity] %s", TCHAR_TO_ANSI(*ResolvedEntity.DebugGetDescription()));
				}
				else
				{
					ImGui::TextDisabled("(unresolved)");
				}
			}

			// Distance
			ImGui::TableSetColumnIndex(2);
			ImGui::Text("%.0f", Result.HitResult.Distance);

			// Score
			ImGui::TableSetColumnIndex(3);
			ImGui::Text("%.2f", Result.Score);

			// Has IInteractionTarget / Entity target
			ImGui::TableSetColumnIndex(4);
			if (HitActor)
			{
				TScriptInterface<IInteractionTarget> CandidateTarget = Arcx::GameplayDebugger::Interaction::FindInteractionTarget(HitActor);
				if (CandidateTarget)
				{
					ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Actor");
				}
				else
				{
					ImGui::TextDisabled("No");
				}
			}
			else
			{
				FMassEntityHandle ResolvedEntity = Arcx::GameplayDebugger::Interaction::ResolveEntityFromHit(Result.HitResult);
				if (ResolvedEntity.IsValid())
				{
					ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "Entity");
				}
				else
				{
					ImGui::TextDisabled("No");
				}
			}

			ImGui::PopID();
		}

		ImGui::EndTable();
	}
}

void FArcInteractionDebugger::DrawDetailPanel()
{
	UArcCoreInteractionSourceComponent* Source = Arcx::GameplayDebugger::Interaction::GetInteractionSource();
	if (!Source)
	{
		return;
	}

	const FTargetingDefaultResultsSet& Results = Source->GetCachedTargetingResults();

	if (SelectedCandidateIdx == INDEX_NONE || !Results.TargetResults.IsValidIndex(SelectedCandidateIdx))
	{
		ImGui::TextDisabled("Select a candidate from the list");
		return;
	}

	const FTargetingDefaultResultData& Result = Results.TargetResults[SelectedCandidateIdx];
	AActor* HitActor = Result.HitResult.GetActor();

	ImGui::Text("Score: %.3f  Distance: %.1f", Result.Score, Result.HitResult.Distance);

	// Actor-based target
	if (HitActor)
	{
		ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "%s", TCHAR_TO_ANSI(*HitActor->GetActorNameOrLabel()));
		ImGui::Text("Class: %s", TCHAR_TO_ANSI(*HitActor->GetClass()->GetName()));
		FVector ActorLoc = HitActor->GetActorLocation();
		ImGui::Text("Location: %.0f, %.0f, %.0f", ActorLoc.X, ActorLoc.Y, ActorLoc.Z);

		ImGui::Separator();

		TScriptInterface<IInteractionTarget> InteractionTarget = Arcx::GameplayDebugger::Interaction::FindInteractionTarget(HitActor);

		if (InteractionTarget)
		{
			UObject* TargetObj = InteractionTarget.GetObject();
			ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "IInteractionTarget: %s", TCHAR_TO_ANSI(*TargetObj->GetName()));
			ImGui::Text("Implementor class: %s", TCHAR_TO_ANSI(*TargetObj->GetClass()->GetName()));

			UArcSmartObjectInteractionComponent* SOInteractionComp = Cast<UArcSmartObjectInteractionComponent>(TargetObj);
			if (!SOInteractionComp)
			{
				SOInteractionComp = HitActor->FindComponentByClass<UArcSmartObjectInteractionComponent>();
			}

			if (SOInteractionComp)
			{
				if (ImGui::CollapsingHeader("Query Results (AppendTargetConfiguration)", ImGuiTreeNodeFlags_DefaultOpen))
				{
					FInteractionQueryResults QueryResults;
					FInteractionContext Ctx;
					Ctx.Target = InteractionTarget;
					InteractionTarget->AppendTargetConfiguration(Ctx, QueryResults);

					if (QueryResults.AvailableInteractions.IsEmpty())
					{
						ImGui::TextDisabled("No available interactions");
					}
					else
					{
						for (int32 j = 0; j < QueryResults.AvailableInteractions.Num(); ++j)
						{
							ImGui::PushID(j);
							FString InteractionLabel = FString::Printf(TEXT("Interaction [%d]"), j);
							ArcImGuiReflection::DrawStructView(*InteractionLabel, QueryResults.AvailableInteractions[j]);
							ImGui::PopID();
						}
					}
				}

				if (ImGui::CollapsingHeader("Interaction State"))
				{
					bool bInteracting = SOInteractionComp->IsInteracting();
					ImGui::Text("Is Interacting: %s", bInteracting ? "Yes" : "No");
				}
			}
		}
		else
		{
			ImGui::TextDisabled("No IInteractionTarget on this actor");
		}
	}

	// Resolve entity — works for both actor-backed and entity-only targets
	UWorld* World = Arcx::GameplayDebugger::Interaction::GetDebugWorld();
	UMassEntitySubsystem* MassSub = World ? World->GetSubsystem<UMassEntitySubsystem>() : nullptr;
	if (!MassSub)
	{
		return;
	}

	FMassEntityHandle EntityHandle = Arcx::GameplayDebugger::Interaction::ResolveEntityFromHit(Result.HitResult);
	if (!EntityHandle.IsValid())
	{
		if (!HitActor)
		{
			ImGui::TextDisabled("Could not resolve entity from hit result");
		}
		return;
	}

	const FMassEntityManager& EM = MassSub->GetEntityManager();

	if (!HitActor)
	{
		// Entity-only target — show entity info header
		ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Entity: %s", TCHAR_TO_ANSI(*EntityHandle.DebugGetDescription()));
		ImGui::Separator();
	}

	// Smart Object data — shown for both actor and entity targets
	if (ImGui::CollapsingHeader("Smart Object Data", ImGuiTreeNodeFlags_DefaultOpen))
	{
		Arcx::GameplayDebugger::Interaction::DrawEntitySOData(EM, EntityHandle);
	}
}

void FArcInteractionDebugger::DrawViewportDebug()
{
	UWorld* World = Arcx::GameplayDebugger::Interaction::GetDebugWorld();
	if (!World)
	{
		return;
	}

	UArcCoreInteractionSourceComponent* Source = Arcx::GameplayDebugger::Interaction::GetInteractionSource();
	if (!Source)
	{
		return;
	}

	TScriptInterface<IInteractionTarget> CurrentTarget = Source->GetCurrentTarget();
	if (!CurrentTarget)
	{
		return;
	}

	AActor* TargetActor = nullptr;
	UObject* TargetObj = CurrentTarget.GetObject();

	// If the implementor is a component, get its owner
	if (UActorComponent* Comp = Cast<UActorComponent>(TargetObj))
	{
		TargetActor = Comp->GetOwner();
	}
	else
	{
		TargetActor = Cast<AActor>(TargetObj);
	}

	if (!TargetActor)
	{
		return;
	}

	FVector Origin;
	FVector Extent;
	TargetActor->GetActorBounds(false, Origin, Extent);
	float Radius = Extent.GetMax();
	if (Radius < 10.f)
	{
		Radius = 50.f;
	}

	DrawDebugSphere(World, Origin, Radius, 16, FColor::Green, false, -1.f, SDPG_World, 2.f);
}
