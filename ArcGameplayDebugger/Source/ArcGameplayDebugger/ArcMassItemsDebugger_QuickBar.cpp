// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassItemsDebugger.h"
#include "ArcMassItemsDebuggerShared.h"

#include "imgui.h"
#include "Mass/EntityManager.h"
#include "Mass/EntityView.h"
#include "QuickBar/ArcMassQuickBarStateFragment.h"
#include "QuickBar/ArcMassQuickBarSharedFragment.h"
#include "QuickBar/ArcMassQuickBarConfig.h"
#include "QuickBar/ArcMassQuickBarTypes.h"
#include "Fragments/ArcMassItemStoreFragment.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemDefinitionData.h"
#include "Items/ArcItemsHelpers.h"
#include "ItemFragments/ArcItemFragment_GrantedMassAbilities.h"
#include "Items/Fragments/ArcItemFragment_Tags.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Input/ArcMassInputActionConfig.h"
#include "MassActorSubsystem.h"
#include "Pawn/ArcMassPawnData.h"
#include "Pawn/ArcMassPawnExtensionComponent.h"
#include "QuickBar/ArcMassQuickBarInputBinding.h"
#include "QuickBar/ArcMassQuickBarOperations.h"
#include "Commands/ArcMassAddItemToQuickSlotCommand.h"
#include "Commands/ArcMassAddNewItemToQuickSlotCommand.h"
#include "Commands/ArcMassActivateQuickSlotCommand.h"
#include "Commands/ArcMassDeactivateQuickSlotCommand.h"
#include "Commands/ArcMassCycleQuickSlotCommand.h"
#include "Commands/ArcReplicatedCommandHelpers.h"
#include "Commands/ArcReplicatedCommand.h"
#include "Commands/ArcReplicatedCommandHost.h"

namespace ArcMassItemsDebugger::QuickBar
{
	static const ImVec4 ColorActive(0.4f, 1.0f, 0.4f, 1.0f);
	static const ImVec4 ColorInactive(0.6f, 0.6f, 0.6f, 1.0f);
	static const ImVec4 ColorWarn(1.0f, 0.3f, 0.3f, 1.0f);

	const FArcMassQuickBarActiveSlot* FindActiveSlot(
		const FArcMassQuickBarStateFragment& State, FGameplayTag BarId, FGameplayTag SlotId)
	{
		for (const FArcMassQuickBarActiveSlot& S : State.ActiveSlots)
		{
			if (S.BarId == BarId && S.QuickSlotId == SlotId)
			{
				return &S;
			}
		}
		return nullptr;
	}

	const FArcItemData* FindItemInStore(const FArcMassItemStoreFragment& Store, FArcItemId ItemId)
	{
		if (!ItemId.IsValid())
		{
			return nullptr;
		}
		for (const FArcMassReplicatedItem& Replicated : Store.ReplicatedItems.Items)
		{
			const FArcItemData* Data = Replicated.ToItem();
			if (Data && Data->GetItemId() == ItemId)
			{
				return Data;
			}
		}
		return nullptr;
	}

	struct FInputContext
	{
		UEnhancedPlayerInput* EnhancedInput = nullptr;
		TArray<TObjectPtr<UArcMassInputActionConfig>> InputConfigs;
		bool bAvailable = false;
	};

	FInputContext ResolveInputContext(FMassEntityManager& Manager, FMassEntityHandle Entity)
	{
		FInputContext Ctx;
		FMassEntityView View(Manager, Entity);
		const FMassActorFragment* ActorFrag = View.GetFragmentDataPtr<FMassActorFragment>();
		if (!ActorFrag)
		{
			return Ctx;
		}
		const AActor* Actor = ActorFrag->Get();
		if (!Actor)
		{
			return Ctx;
		}

		const APlayerController* PC = nullptr;
		if (const APawn* Pawn = Cast<APawn>(Actor))
		{
			PC = Cast<APlayerController>(Pawn->GetController());
		}
		else if (const APlayerState* PS = Cast<APlayerState>(Actor))
		{
			PC = Cast<APlayerController>(PS->GetOwner());
		}
		if (!PC)
		{
			return Ctx;
		}

		ULocalPlayer* LP = PC->GetLocalPlayer();
		if (!LP)
		{
			return Ctx;
		}

		UEnhancedInputLocalPlayerSubsystem* Subsystem =
			LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
		Ctx.EnhancedInput = Subsystem ? Subsystem->GetPlayerInput() : nullptr;

		if (UArcMassPawnExtensionComponent* Ext =
			UArcMassPawnExtensionComponent::FindPawnExtensionComponent(PC->GetPawn()))
		{
			if (const UArcMassPawnData* PawnData = Ext->GetPawnData<UArcMassPawnData>())
			{
				Ctx.InputConfigs = PawnData->InputConfig;
			}
		}

		Ctx.bAvailable = Ctx.EnhancedInput != nullptr;
		return Ctx;
	}

	IArcReplicatedCommandHost* ResolveOwningArcPlayerController(FMassEntityManager& Manager, FMassEntityHandle Entity)
	{
		FMassEntityView View(Manager, Entity);
		const FMassActorFragment* ActorFrag = View.GetFragmentDataPtr<FMassActorFragment>();
		if (!ActorFrag)
		{
			return nullptr;
		}
		const AActor* Actor = ActorFrag->Get();
		if (!Actor)
		{
			return nullptr;
		}
		APlayerController* PC = nullptr;
		if (const APawn* Pawn = Cast<APawn>(Actor))
		{
			PC = Cast<APlayerController>(Pawn->GetController());
		}
		else if (const APlayerState* PS = Cast<APlayerState>(Actor))
		{
			PC = Cast<APlayerController>(PS->GetOwner());
		}
		return Cast<IArcReplicatedCommandHost>(PC);
	}

	template <typename CommandT>
	bool SendQuickSlotCommand(IArcReplicatedCommandHost* PC, CommandT&& Cmd)
	{
		if (!PC)
		{
			UE_LOGF(LogTemp, Warning, "MassItemsDebugger: no owning PlayerController; quick slot command not sent");
			return false;
		}
		TSharedPtr<FArcReplicatedCommand> Shared(
			new std::remove_reference_t<CommandT>(Forward<CommandT>(Cmd)));
		const bool bSent = PC->SendReplicatedCommand(FArcReplicatedCommandHandle(Shared, FGuid::NewGuid()));
		UE_LOGF(LogTemp, Log, "MassItemsDebugger: quick slot command %ls sent=%d",
			*Shared->GetScriptStruct()->GetName(), bSent ? 1 : 0);
		return bSent;
	}

	void DrawInputPreviewLine(const FInputContext& Ctx, const FInstancedStruct& InputBindStruct)
	{
		if (!InputBindStruct.IsValid())
		{
			return;
		}
		const FArcMassQuickBarInputBinding* Binding =
			InputBindStruct.GetPtr<FArcMassQuickBarInputBinding>();
		if (!Binding || !Binding->InputTag.IsValid())
		{
			return;
		}

		if (!Ctx.bAvailable)
		{
			ImGui::TextDisabled("Input: %s  (no input owner)",
				TCHAR_TO_ANSI(*Binding->InputTag.ToString()));
			return;
		}

		const UInputAction* Action = nullptr;
		for (const UArcMassInputActionConfig* Config : Ctx.InputConfigs)
		{
			if (!Config)
			{
				continue;
			}
			Action = Config->FindAbilityInputActionForTag(Binding->InputTag, false);
			if (Action)
			{
				break;
			}
		}
		if (!Action)
		{
			ImGui::TextDisabled("Input: %s  (no action found)",
				TCHAR_TO_ANSI(*Binding->InputTag.ToString()));
			return;
		}

		const FInputActionValue Value = Ctx.EnhancedInput->GetActionValue(Action);
		ImGui::Text("Input: %s  (%s)",
			TCHAR_TO_ANSI(*Binding->InputTag.ToString()),
			TCHAR_TO_ANSI(*Value.ToString()));
	}
}

void FArcMassItemsDebugger::DrawQuickBarTab(FMassEntityManager& Manager, FMassEntityHandle Entity)
{
	using namespace ArcMassItemsDebugger::QuickBar;

	FMassEntityView View(Manager, Entity);
	const FArcMassQuickBarStateFragment* State =
		View.GetSparseFragmentDataPtr<FArcMassQuickBarStateFragment>();
	if (!State)
	{
		ImGui::TextColored(ColorWarn,
			"Entity has no FArcMassQuickBarStateFragment.");
		ImGui::TextDisabled(
			"Add UArcMassQuickBarTrait (with a UArcMassQuickBarConfig) to this entity's template.");
		return;
	}

	const FArcMassQuickBarSharedFragment* Shared =
		View.GetConstSharedFragmentDataPtr<FArcMassQuickBarSharedFragment>();
	if (!Shared || Shared->Configs.IsEmpty())
	{
		ImGui::TextColored(ColorWarn,
			"Entity has FArcMassQuickBarStateFragment but no UArcMassQuickBarConfig.");
		return;
	}

	int32 TotalBarCount = 0;
	for (const TObjectPtr<UArcMassQuickBarConfig>& CountedConfig : Shared->Configs)
	{
		if (CountedConfig == nullptr)
		{
			continue;
		}
		TotalBarCount += CountedConfig->QuickBars.Num();
		for (const FArcMassQuickBarGroup& Group : CountedConfig->Groups)
		{
			TotalBarCount += Group.Bars.Num();
		}
	}
	ImGui::Text("Configs: %d   Bars: %d   Active slots: %d",
		Shared->Configs.Num(), TotalBarCount, State->ActiveSlots.Num());
	ImGui::Separator();

	const FInputContext InputCtx = ResolveInputContext(Manager, Entity);

	int32 BarIdx = 0;
	for (const TObjectPtr<UArcMassQuickBarConfig>& ConfigPtr : Shared->Configs)
	{
		const UArcMassQuickBarConfig* Config = ConfigPtr;
		if (Config == nullptr)
		{
			continue;
		}

		const UScriptStruct* ConfigStoreType = Config->StoreType;
		const FArcMassItemStoreFragment* Store = nullptr;
		if (ConfigStoreType != nullptr)
		{
			FStructView StoreView = Manager.GetMutableSparseElementDataForEntity(ConfigStoreType, Entity);
			Store = StoreView.GetPtr<FArcMassItemStoreFragment>();
		}
		ImGui::TextDisabled("Config: %s  Store: %s",
			TCHAR_TO_ANSI(*GetNameSafe(Config)),
			TCHAR_TO_ANSI(*GetNameSafe(ConfigStoreType)));

		ArcMassItems::QuickBar::ForEachBarConfig(*Config,
			[&](const FArcMassQuickBarEntry& Bar, const FArcMassQuickBarGroup* Group) -> bool
		{
			const bool bBarActive = ArcMassItems::QuickBar::IsBarActive(Manager, Entity, Bar.BarId);
			const FString BarLabel = Group
				? FString::Printf(TEXT("%s  (%d slots)  [group: %s]%s"),
					*Bar.BarId.ToString(), Bar.Slots.Num(), *Group->GroupId.ToString(), bBarActive ? TEXT("  [active]") : TEXT(""))
				: FString::Printf(TEXT("%s  (%d slots)%s"),
					*Bar.BarId.ToString(), Bar.Slots.Num(), bBarActive ? TEXT("  [active]") : TEXT(""));

			ImGui::PushID(BarIdx);
			++BarIdx;
			if (ImGui::TreeNode(TCHAR_TO_ANSI(*BarLabel)))
			{
				if (ImGui::SmallButton("Activate Bar"))
				{
					ArcMassItems::QuickBar::ActivateBar(
						Manager, Entity, Bar.BarId);
				}
				ImGui::SameLine();
				if (ImGui::SmallButton("Deactivate Bar"))
				{
					ArcMassItems::QuickBar::DeactivateBar(
						Manager, Entity, Bar.BarId);
				}
				if (Group && Group->bCyclable)
				{
					ImGui::SameLine();
					if (ImGui::SmallButton("Cycle Group <"))
					{
						ArcMassItems::QuickBar::CycleActiveBar(
							Manager, Entity, Group->GroupId, -1);
					}
					ImGui::SameLine();
					if (ImGui::SmallButton("Cycle Group >"))
					{
						ArcMassItems::QuickBar::CycleActiveBar(
							Manager, Entity, Group->GroupId, 1);
					}
				}

				for (int32 SlotIdx = 0; SlotIdx < Bar.Slots.Num(); ++SlotIdx)
				{
					const FArcMassQuickBarSlot& Slot = Bar.Slots[SlotIdx];
					const FArcMassQuickBarActiveSlot* Active =
						FindActiveSlot(*State, Bar.BarId, Slot.QuickBarSlotId);

					ImGui::PushID(SlotIdx);
					const bool bActive = Active && Active->bIsSlotActive;
					ImGui::TextColored(bActive ? ColorActive : ColorInactive,
						"%s%s",
						TCHAR_TO_ANSI(*Slot.QuickBarSlotId.ToString()),
						bActive ? "  [active]" : "");
					ImGui::Indent();
					if (Active && Active->AssignedItemId.IsValid())
					{
						ImGui::Text("AssignedItemId: %s",
							TCHAR_TO_ANSI(*ArcMassItemsDebugger::FormatItemId(Active->AssignedItemId)));
						ImGui::SameLine();
						if (ImGui::SmallButton("Show in Items"))
						{
							RequestJumpToItem(Active->AssignedItemId);
						}
						ImGui::Text("ItemSlot: %s",
							TCHAR_TO_ANSI(*ArcMassItemsDebugger::FormatGameplayTag(Active->ItemSlot)));

						const FArcItemData* Item = Store ? FindItemInStore(*Store, Active->AssignedItemId) : nullptr;
						if (Item)
						{
							ImGui::Text("Item: %s",
								TCHAR_TO_ANSI(*ArcMassItemsDebugger::FormatItemDefinition(Item->GetItemDefinitionId())));

							if (const FArcItemInstance_GrantedMassAbilities* Granted =
								ArcItemsHelper::FindInstance<FArcItemInstance_GrantedMassAbilities>(Item))
							{
								if (ImGui::TreeNode("Granted Mass Abilities"))
								{
									// Two-pass: collect strings, then render.
									TArray<FString> Lines;
									Lines.Reserve(Granted->GrantedAbilities.Num());
									for (const FArcAbilityHandle& Handle : Granted->GrantedAbilities)
									{
										Lines.Add(Handle.ToString());
									}
									for (const FString& Line : Lines)
									{
										ImGui::BulletText("%s", TCHAR_TO_ANSI(*Line));
									}
									ImGui::TreePop();
								}
							}

							if (const FArcItemFragment_Tags* TagFrag =
								ArcItemsHelper::FindFragment<FArcItemFragment_Tags>(Item))
							{
								ArcMassItemsDebugger::DrawTags("Item Tags", TagFrag->ItemTags);
							}
						}
						else
						{
							ImGui::TextColored(ColorWarn, "Item not found in store");
						}
					}
					else
					{
						ImGui::TextDisabled("(empty)");
					}
					DrawInputPreviewLine(InputCtx, Slot.InputBind);

					if (ImGui::SmallButton("Activate"))
					{
						Arcx::SendServerCommand<FArcMassActivateQuickSlotCommand>(
							ArcMassItemsDebugger::GetDebugWorld(), Entity, Bar.BarId, Slot.QuickBarSlotId);
					}
					ImGui::SameLine();
					if (ImGui::SmallButton("Deactivate"))
					{
						Arcx::SendServerCommand<FArcMassDeactivateQuickSlotCommand>(
							ArcMassItemsDebugger::GetDebugWorld(), Entity, Bar.BarId, Slot.QuickBarSlotId);
					}
					ImGui::SameLine();
					if (ImGui::SmallButton("Cycle Slot <"))
					{
						Arcx::SendServerCommand<FArcMassCycleQuickSlotCommand>(
							ArcMassItemsDebugger::GetDebugWorld(), Entity, Bar.BarId, Slot.QuickBarSlotId, -1);
					}
					ImGui::SameLine();
					if (ImGui::SmallButton("Cycle Slot >"))
					{
						Arcx::SendServerCommand<FArcMassCycleQuickSlotCommand>(
							ArcMassItemsDebugger::GetDebugWorld(), Entity, Bar.BarId, Slot.QuickBarSlotId, 1);
					}
					ImGui::SameLine();
					if (ImGui::SmallButton("Remove"))
					{
						ArcMassItems::QuickBar::RemoveSlot(
							Manager, Entity,
							Bar.BarId, Slot.QuickBarSlotId);
					}

					if (Store && Store->ReplicatedItems.Items.Num() > 0)
					{
						if (ImGui::BeginCombo("##AssignItem", "Assign item..."))
						{
							// Two-pass: collect filtered candidates, then render.
							TArray<FArcItemId> Candidates;
							Candidates.Reserve(Store->ReplicatedItems.Items.Num());
							for (const FArcMassReplicatedItem& R : Store->ReplicatedItems.Items)
							{
								if (const FArcItemData* I = R.ToItem())
								{
									if (Slot.ItemRequiredTags.Num() == 0 ||
										I->GetItemAggregatedTags().HasAll(Slot.ItemRequiredTags))
									{
										Candidates.Add(I->GetItemId());
									}
								}
							}
							for (const FArcItemId& CandidateId : Candidates)
							{
								const FArcItemData* I = FindItemInStore(*Store, CandidateId);
								if (!I)
								{
									continue;
								}
								const FString Label = FString::Printf(TEXT("%s  (%s)"),
									*ArcMassItemsDebugger::FormatItemDefinition(I->GetItemDefinitionId()),
									*ArcMassItemsDebugger::FormatItemId(CandidateId));
								if (ImGui::Selectable(TCHAR_TO_ANSI(*Label)))
								{
									IArcReplicatedCommandHost* OwningPC =
										ResolveOwningArcPlayerController(Manager, Entity);
									if (OwningPC)
									{
										FArcMassAddItemToQuickSlotCommand Cmd(Entity, CandidateId, Bar.BarId, Slot.QuickBarSlotId);
										SendQuickSlotCommand(OwningPC, MoveTemp(Cmd));
									}
									else
									{
										// Fallback: non-pawn entities (e.g. world stores) can't route
										// through a PC; keep the legacy direct call so the debugger
										// still functions. This bypasses replication intentionally.
										ArcMassItems::QuickBar::AddAndActivateSlot(
											Manager, Entity,
											Bar.BarId, Slot.QuickBarSlotId, CandidateId);
									}
								}
							}
							ImGui::EndCombo();
						}
					}

					{
						// "Add new from definition..." — creates a fresh item and assigns it via
						// FArcMassAddNewItemToQuickSlotCommand. Filter intentionally omitted: a
						// definition's aggregated item tags are only available after instantiation
						// (FArcItemData::ItemAggregatedTags), so pre-flight filtering on
						// Slot.ItemRequiredTags would require materializing every candidate. The
						// server will simply create the item; mismatches will be visible in the
						// store.
						if (CachedItemDefinitions.Num() == 0)
						{
							RefreshItemDefinitions();
						}
						if (CachedItemDefinitions.Num() > 0)
						{
							if (ImGui::BeginCombo("##AddNewFromDef", "Add new from definition..."))
							{
								for (int32 DefIdx = 0; DefIdx < CachedItemDefinitions.Num(); ++DefIdx)
								{
									const FString& DefName = CachedItemDefinitionNames[DefIdx];
									ImGui::PushID(DefIdx);
									const bool bSelected = ImGui::Selectable(TCHAR_TO_ANSI(*DefName));
									ImGui::PopID();
									if (!bSelected)
									{
										continue;
									}
									IArcReplicatedCommandHost* OwningPC =
										ResolveOwningArcPlayerController(Manager, Entity);
									if (!OwningPC)
									{
										ImGui::TextColored(ColorWarn,
											"No owning PlayerController on entity; cannot send command.");
										continue;
									}
									FArcMassAddNewItemToQuickSlotCommand Cmd(Entity, CachedItemDefinitions[DefIdx], Bar.BarId, Slot.QuickBarSlotId);
									SendQuickSlotCommand(OwningPC, MoveTemp(Cmd));
								}
								ImGui::EndCombo();
							}
						}
					}
					ImGui::Unindent();
					ImGui::PopID();
				}
				ImGui::TreePop();
			}
			ImGui::PopID();
			return true;
		});
	}
}
