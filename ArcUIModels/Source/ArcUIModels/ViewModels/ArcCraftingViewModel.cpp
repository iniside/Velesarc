// Copyright Lukasz Baran. All Rights Reserved.

#include "ViewModels/ArcCraftingViewModel.h"

#include "Commands/ArcReplicatedCommandHelpers.h"
#include "ArcCraft/Commands/ArcCraftFromStationInputCommand.h"
#include "ArcCraft/Commands/ArcTransferItemFromStationCommand.h"
#include "ArcCraft/Commands/ArcWithdrawCraftOutputCommand.h"
#include "ViewModels/ArcRecipeViewModel.h"
#include "ViewModels/ArcCraftQueueEntryViewModel.h"
#include "ViewModels/ArcItemViewModel.h"
#include "ViewModels/ArcIngredientCandidateViewModel.h"
#include "ViewModels/ArcRecipeIngredientViewModel.h"
#include "Items/ArcItemDefinitionData.h"

void UArcCraftingViewModel::SetBoundStation(FMassEntityHandle NewStation)
{
	if (BoundStation == NewStation)
	{
		return;
	}

	const FMassEntityHandle OldStation = BoundStation;
	BoundStation = NewStation;

	PreviewRows.Reset();
	UE_MVVM_SET_PROPERTY_VALUE(bHasInputPreview, false);

	// Fire so the struct model rebinds station input/output/queue to the new station.
	OnBoundStationChangedDelegate.Broadcast(OldStation, NewStation);
}

void UArcCraftingViewModel::SetInstigatorEntity(FMassEntityHandle NewEntity)
{
	InstigatorEntity = NewEntity;
}

void UArcCraftingViewModel::SetRecipes(TArray<TObjectPtr<UArcRecipeViewModel>>&& NewRecipes)
{
	UE_MVVM_SET_PROPERTY_VALUE(Recipes, NewRecipes);
}

void UArcCraftingViewModel::SetQueue(TArray<TObjectPtr<UArcCraftQueueEntryViewModel>>&& NewQueue)
{
	UE_MVVM_SET_PROPERTY_VALUE(Queue, NewQueue);
}

void UArcCraftingViewModel::SetStationOutput(TArray<TObjectPtr<UArcItemViewModel>>&& NewStationOutput)
{
	UE_MVVM_SET_PROPERTY_VALUE(StationOutput, NewStationOutput);
}

void UArcCraftingViewModel::SetbHasQueue(bool bNewHasQueue)
{
	UE_MVVM_SET_PROPERTY_VALUE(bHasQueue, bNewHasQueue);
}

void UArcCraftingViewModel::SetbSupportsManualDeposit(bool bNewSupportsManualDeposit)
{
	UE_MVVM_SET_PROPERTY_VALUE(bSupportsManualDeposit, bNewSupportsManualDeposit);
}

void UArcCraftingViewModel::SetStationDisplayName(const FText& NewName)
{
	UE_MVVM_SET_PROPERTY_VALUE(StationDisplayName, NewName);
}

void UArcCraftingViewModel::SetCraftAmount(int32 NewAmount)
{
	const int32 ClampedAmount = FMath::Max(NewAmount, 1);
	UE_MVVM_SET_PROPERTY_VALUE(CraftAmount, ClampedAmount);
}

void UArcCraftingViewModel::SetbShowStationOutput(bool bNewShowStationOutput)
{
	UE_MVVM_SET_PROPERTY_VALUE(bShowStationOutput, bNewShowStationOutput);
}

void UArcCraftingViewModel::SelectRecipe(UArcRecipeViewModel* InRecipe)
{
	if (SelectedRecipe == InRecipe)
	{
		return;
	}

	UE_MVVM_SET_PROPERTY_VALUE(SelectedRecipe, InRecipe);

	// Clear hover/pin/detail so the shared detail panel never dangles at a freed ingredient VM
	// (SelectRecipe(nullptr) on station-change routes through here too).
	UE_MVVM_SET_PROPERTY_VALUE(HoveredIngredient, TObjectPtr<UArcRecipeIngredientViewModel>(nullptr));
	UE_MVVM_SET_PROPERTY_VALUE(PinnedIngredient, TObjectPtr<UArcRecipeIngredientViewModel>(nullptr));
	UE_MVVM_SET_PROPERTY_VALUE(DetailIngredient, TObjectPtr<UArcRecipeIngredientViewModel>(nullptr));

	// Fire so the struct model recounts ingredient availability for the new selection.
	OnSelectedRecipeChangedDelegate.Broadcast(InRecipe);
}

void UArcCraftingViewModel::SetHoveredIngredient(UArcRecipeIngredientViewModel* InIngredient)
{
	if (HoveredIngredient == InIngredient)
	{
		return;
	}

	UE_MVVM_SET_PROPERTY_VALUE(HoveredIngredient, InIngredient);
	RefreshDetailIngredient();
}

void UArcCraftingViewModel::PinIngredient(UArcRecipeIngredientViewModel* InIngredient)
{
	// Re-pinning the currently pinned ingredient toggles the pin off.
	UArcRecipeIngredientViewModel* const NewPinned =
		(PinnedIngredient == InIngredient) ? nullptr : InIngredient;

	if (PinnedIngredient == NewPinned)
	{
		return;
	}

	UE_MVVM_SET_PROPERTY_VALUE(PinnedIngredient, NewPinned);
	RefreshDetailIngredient();
}

void UArcCraftingViewModel::RefreshDetailIngredient()
{
	UArcRecipeIngredientViewModel* const NewDetail =
		PinnedIngredient ? PinnedIngredient.Get() : HoveredIngredient.Get();
	UE_MVVM_SET_PROPERTY_VALUE(DetailIngredient, NewDetail);
}

bool UArcCraftingViewModel::TransferItemToStation(UArcItemViewModel* Item, int32 Stacks)
{
	if (Item == nullptr || Stacks < 1 || !bSupportsManualDeposit)
	{
		return false;
	}

	if (AddToPreview(Item->GetItemId(), Stacks) < 1)
	{
		return false;
	}
	NotifyPreviewChanged();
	return true;
}

bool UArcCraftingViewModel::WithdrawInputItem(UObject* WorldContextObject, UArcIngredientCandidateViewModel* Row, int32 Stacks)
{
	if (Row == nullptr || Stacks < 1 || !Row->GetbFromStation())
	{
		return false;
	}

	if (Row->GetbPreview())
	{
		FArcCraftStoreItemRow Key;
		Key.OwnerEntity = Row->GetOwnerEntity();
		Key.StoreType = Row->GetStoreType();
		Key.Spec.ItemId = Row->GetItemId();

		const int32 Index = PreviewRows.IndexOfByPredicate(
			[&Key](const FArcCraftStoreItemRow& PreviewRow)
			{
				return PreviewRow.IsSameItem(Key);
			});
		if (Index == INDEX_NONE)
		{
			return false;
		}

		PreviewRows[Index].Stacks -= Stacks;
		if (PreviewRows[Index].Stacks < 1)
		{
			PreviewRows.RemoveAt(Index);
		}
		NotifyPreviewChanged();
		return true;
	}

	if (!HasValidCommandContext(WorldContextObject))
	{
		return false;
	}

	return Arcx::SendServerCommand<FArcTransferItemFromStationCommand>(
		WorldContextObject,
		BoundStation,
		InstigatorEntity,
		Row->GetItemId(),
		Stacks);
}

bool UArcCraftingViewModel::CraftFromStationInput(UObject* WorldContextObject, int32 Amount)
{
	if (!HasValidCommandContext(WorldContextObject) || SelectedRecipe == nullptr || Amount < 1)
	{
		return false;
	}

	TArray<FArcCraftInputSourceGroup> Sources;
	for (const FArcCraftStoreItemRow& Row : PreviewRows)
	{
		FArcCraftInputSourceGroup* Group = Sources.FindByPredicate(
			[&Row](const FArcCraftInputSourceGroup& Existing)
			{
				return Existing.OwnerEntity == Row.OwnerEntity && Existing.StoreType == Row.StoreType;
			});
		if (Group == nullptr)
		{
			Group = &Sources.AddDefaulted_GetRef();
			Group->OwnerEntity = Row.OwnerEntity;
			Group->StoreType = Row.StoreType;
		}

		FArcCraftInputItem& Item = Group->Items.AddDefaulted_GetRef();
		Item.ItemId = Row.Spec.ItemId;
		Item.Stacks = Row.Stacks;
	}

	const FMassEntityHandle SentStation = BoundStation;
	const FArcCommandConfirmedDelegate OnConfirmed = FArcCommandConfirmedDelegate::CreateWeakLambda(this,
		[this, SentStation, Sent = Sources](bool bSuccess)
		{
			if (bSuccess && BoundStation == SentStation)
			{
				RemoveSentFromPreview(Sent);
			}
		});

	return Arcx::SendServerCommandConfirm<FArcCraftFromStationInputCommand>(
		WorldContextObject,
		OnConfirmed,
		SelectedRecipe->GetRecipeId(),
		BoundStation,
		InstigatorEntity,
		Amount,
		MoveTemp(Sources));
}

bool UArcCraftingViewModel::FillStationInput(int32 Amount)
{
	if (SelectedRecipe == nullptr || Amount < 1 || !bSupportsManualDeposit)
	{
		return false;
	}

	bool bAddedAny = false;
	for (const TObjectPtr<UArcRecipeIngredientViewModel>& Slot : SelectedRecipe->GetRequiredIngredients())
	{
		if (Slot == nullptr)
		{
			continue;
		}

		int32 Deficit = Slot->GetRequired() * Amount - CountInputViewStacks(*Slot);
		for (const FArcCraftStoreItemRow& Row : GetInventoryRowsAfterPreview())
		{
			if (Deficit < 1)
			{
				break;
			}
			if (!Slot->DoesItemSatisfy(Row.Spec))
			{
				continue;
			}
			const int32 Added = AddToPreview(Row.Spec.ItemId, FMath::Min(Deficit, Row.Stacks));
			Deficit -= Added;
			bAddedAny |= Added > 0;
		}
	}

	if (bAddedAny)
	{
		NotifyPreviewChanged();
	}
	return bAddedAny;
}

bool UArcCraftingViewModel::AddFirstMatchToInput(UArcRecipeIngredientViewModel* Slot)
{
	if (Slot == nullptr || !bSupportsManualDeposit)
	{
		return false;
	}

	const int32 Need = Slot->GetRequired() - CountInputViewStacks(*Slot);
	if (Need < 1)
	{
		return false;
	}

	for (const FArcCraftStoreItemRow& Row : GetInventoryRowsAfterPreview())
	{
		if (Slot->DoesItemSatisfy(Row.Spec))
		{
			if (AddToPreview(Row.Spec.ItemId, FMath::Min(Need, Row.Stacks)) < 1)
			{
				return false;
			}
			NotifyPreviewChanged();
			return true;
		}
	}
	return false;
}

void UArcCraftingViewModel::ClearInputPreview()
{
	if (PreviewRows.IsEmpty())
	{
		return;
	}
	PreviewRows.Reset();
	NotifyPreviewChanged();
}

bool UArcCraftingViewModel::WithdrawOutputItem(UObject* WorldContextObject, UArcItemViewModel* Item, int32 Stacks)
{
	if (Item == nullptr || !HasValidCommandContext(WorldContextObject))
	{
		return false;
	}

	if (Stacks < 1)
	{
		return false;
	}

	return Arcx::SendServerCommand<FArcWithdrawCraftOutputCommand>(
		WorldContextObject,
		BoundStation,
		InstigatorEntity,
		Item->GetItemId(),
		Stacks);
}

bool UArcCraftingViewModel::WithdrawAllOutput(UObject* WorldContextObject)
{
	if (!HasValidCommandContext(WorldContextObject))
	{
		return false;
	}

	bool bSentAny = false;
	for (const TObjectPtr<UArcItemViewModel>& Item : StationOutput)
	{
		if (Item == nullptr || Item->GetStacks() < 1)
		{
			continue;
		}

		bSentAny |= Arcx::SendServerCommand<FArcWithdrawCraftOutputCommand>(
			WorldContextObject,
			BoundStation,
			InstigatorEntity,
			Item->GetItemId(),
			Item->GetStacks());
	}
	return bSentAny;
}

// ---------------------------------------------------------------------------
// Shared command-context guard
// ---------------------------------------------------------------------------

bool UArcCraftingViewModel::HasValidCommandContext(const UObject* WorldContextObject) const
{
	return WorldContextObject != nullptr && BoundStation.IsValid() && InstigatorEntity.IsValid();
}

// ---------------------------------------------------------------------------
// Station input/output flat-view rebuild
// ---------------------------------------------------------------------------

void UArcCraftingViewModel::SetServerInput(TConstArrayView<FArcItemSpec> Specs, const UArcTagDisplayRegistry* TagReg)
{
	InputTagRegistry = TagReg;
	ServerInputSpecs.Reset(Specs.Num());
	ServerInputSpecs.Append(Specs.GetData(), Specs.Num());
	RebuildStationInputView();
}

void UArcCraftingViewModel::SetInventoryRows(TArray<FArcCraftStoreItemRow>&& NewRows)
{
	InventoryRows = MoveTemp(NewRows);

	bool bPreviewClamped = false;
	for (FArcCraftStoreItemRow& PreviewRow : PreviewRows)
	{
		const FArcCraftStoreItemRow* Held = InventoryRows.FindByPredicate(
			[&PreviewRow](const FArcCraftStoreItemRow& Row)
			{
				return Row.IsSameItem(PreviewRow);
			});
		const int32 HeldStacks = Held != nullptr ? Held->Stacks : 0;
		if (PreviewRow.Stacks > HeldStacks)
		{
			PreviewRow.Stacks = HeldStacks;
			bPreviewClamped = true;
		}
	}

	if (bPreviewClamped)
	{
		PreviewRows.RemoveAll(
			[](const FArcCraftStoreItemRow& Row)
			{
				return Row.Stacks < 1;
			});
		UE_MVVM_SET_PROPERTY_VALUE(bHasInputPreview, !PreviewRows.IsEmpty());
		RebuildStationInputView();
	}
}

TArray<FArcCraftStoreItemRow> UArcCraftingViewModel::GetInventoryRowsAfterPreview() const
{
	TArray<FArcCraftStoreItemRow> Result;
	Result.Reserve(InventoryRows.Num());
	for (const FArcCraftStoreItemRow& Row : InventoryRows)
	{
		int32 Left = Row.Stacks;
		for (const FArcCraftStoreItemRow& PreviewRow : PreviewRows)
		{
			if (PreviewRow.IsSameItem(Row))
			{
				Left -= PreviewRow.Stacks;
			}
		}
		if (Left > 0)
		{
			FArcCraftStoreItemRow& LeftRow = Result.Add_GetRef(Row);
			LeftRow.Stacks = Left;
			LeftRow.Spec.Amount = static_cast<uint16>(FMath::Min(Left, static_cast<int32>(MAX_uint16)));
		}
	}
	return Result;
}

int32 UArcCraftingViewModel::AddToPreview(const FArcItemId& ItemId, int32 Stacks)
{
	const FArcCraftStoreItemRow* Held = InventoryRows.FindByPredicate(
		[&ItemId](const FArcCraftStoreItemRow& Row)
		{
			return Row.Spec.ItemId == ItemId;
		});
	if (Held == nullptr || Stacks < 1)
	{
		return 0;
	}

	FArcCraftStoreItemRow* PreviewRow = PreviewRows.FindByPredicate(
		[Held](const FArcCraftStoreItemRow& Row)
		{
			return Row.IsSameItem(*Held);
		});
	const int32 AlreadyInPreview = PreviewRow != nullptr ? PreviewRow->Stacks : 0;
	const int32 Added = FMath::Min(Stacks, Held->Stacks - AlreadyInPreview);
	if (Added < 1)
	{
		return 0;
	}

	if (PreviewRow != nullptr)
	{
		PreviewRow->Stacks += Added;
	}
	else
	{
		FArcCraftStoreItemRow& NewRow = PreviewRows.Add_GetRef(*Held);
		NewRow.Stacks = Added;
	}
	return Added;
}

int32 UArcCraftingViewModel::CountInputViewStacks(const UArcRecipeIngredientViewModel& Slot) const
{
	int32 Count = 0;
	for (const FArcItemSpec& Spec : ServerInputSpecs)
	{
		if (Slot.DoesItemSatisfy(Spec))
		{
			Count += static_cast<int32>(Spec.Amount);
		}
	}
	for (const FArcCraftStoreItemRow& Row : PreviewRows)
	{
		if (Slot.DoesItemSatisfy(Row.Spec))
		{
			Count += Row.Stacks;
		}
	}
	return Count;
}

void UArcCraftingViewModel::NotifyPreviewChanged()
{
	UE_MVVM_SET_PROPERTY_VALUE(bHasInputPreview, !PreviewRows.IsEmpty());
	RebuildStationInputView();
	OnInputPreviewChangedDelegate.Broadcast();
}

void UArcCraftingViewModel::RemoveSentFromPreview(TConstArrayView<FArcCraftInputSourceGroup> Sent)
{
	bool bChanged = false;
	for (const FArcCraftInputSourceGroup& Group : Sent)
	{
		for (const FArcCraftInputItem& Item : Group.Items)
		{
			FArcCraftStoreItemRow* PreviewRow = PreviewRows.FindByPredicate(
				[&Group, &Item](const FArcCraftStoreItemRow& Row)
				{
					return Row.OwnerEntity == Group.OwnerEntity && Row.StoreType == Group.StoreType && Row.Spec.ItemId == Item.ItemId;
				});
			if (PreviewRow != nullptr)
			{
				PreviewRow->Stacks -= Item.Stacks;
				bChanged = true;
			}
		}
	}
	if (!bChanged)
	{
		return;
	}

	PreviewRows.RemoveAll(
		[](const FArcCraftStoreItemRow& Row)
		{
			return Row.Stacks < 1;
		});
	NotifyPreviewChanged();
}

void UArcCraftingViewModel::RebuildStationInputView()
{
	const UArcTagDisplayRegistry* TagReg = InputTagRegistry.Get();
	using FRowKey = TTuple<FArcItemId, bool>;
	TMap<FRowKey, TObjectPtr<UArcIngredientCandidateViewModel>> Existing;
	Existing.Reserve(StationInput.Num());
	for (const TObjectPtr<UArcIngredientCandidateViewModel>& RowVM : StationInput)
	{
		if (RowVM != nullptr)
		{
			Existing.Add(FRowKey(RowVM->GetItemId(), RowVM->GetbPreview()), RowVM);
		}
	}

	TArray<TObjectPtr<UArcIngredientCandidateViewModel>> NewView;
	NewView.Reserve(ServerInputSpecs.Num() + PreviewRows.Num());

	for (const FArcItemSpec& Spec : ServerInputSpecs)
	{
		if (Spec.Amount == 0)
		{
			continue;
		}
		TObjectPtr<UArcIngredientCandidateViewModel> RowVM;
		if (!Existing.RemoveAndCopyValue(FRowKey(Spec.ItemId, false), RowVM) || RowVM == nullptr)
		{
			RowVM = NewObject<UArcIngredientCandidateViewModel>(this);
		}
		RowVM->RefreshFrom(Spec, static_cast<int32>(Spec.Amount), Spec.GetItemDefinition(), TagReg);
		RowVM->SetOrigin(true, false, FMassEntityHandle(), nullptr);
		NewView.Add(RowVM);
	}

	for (const FArcCraftStoreItemRow& Row : PreviewRows)
	{
		TObjectPtr<UArcIngredientCandidateViewModel> RowVM;
		if (!Existing.RemoveAndCopyValue(FRowKey(Row.Spec.ItemId, true), RowVM) || RowVM == nullptr)
		{
			RowVM = NewObject<UArcIngredientCandidateViewModel>(this);
		}
		RowVM->RefreshFrom(Row.Spec, Row.Stacks, Row.Spec.GetItemDefinition(), TagReg);
		RowVM->SetOrigin(true, true, Row.OwnerEntity, Row.StoreType);
		NewView.Add(RowVM);
	}

	UE_MVVM_SET_PROPERTY_VALUE(StationInput, NewView);
}

void UArcCraftingViewModel::RebuildStationOutput(const TArray<FArcItemSpec>& Specs, const UArcTagDisplayRegistry* TagReg)
{
	TArray<TObjectPtr<UArcItemViewModel>> NewArray;

	for (const FArcItemSpec& Spec : Specs)
	{
		// Defensive skip — zero-amount specs carry no displayable content.
		if (Spec.Amount == 0)
		{
			continue;
		}

		TObjectPtr<UArcItemViewModel>& Slot = StationOutputById.FindOrAdd(Spec.ItemId);
		if (Slot == nullptr)
		{
			Slot = NewObject<UArcItemViewModel>(this);
		}
		Slot->RefreshFrom(Spec, static_cast<int32>(Spec.Amount), TagReg);
		NewArray.Add(Slot);
	}

	// Two-pass prune: collect ids no longer in the new list, then bulk-remove from the map.
	TArray<FArcItemId> DeadIds;
	for (const TTuple<FArcItemId, TObjectPtr<UArcItemViewModel>>& Pair : StationOutputById)
	{
		bool bStillPresent = false;
		for (const TObjectPtr<UArcItemViewModel>& VM : NewArray)
		{
			if (VM != nullptr && VM->GetItemId() == Pair.Key)
			{
				bStillPresent = true;
				break;
			}
		}
		if (!bStillPresent)
		{
			DeadIds.Add(Pair.Key);
		}
	}
	for (const FArcItemId& DeadId : DeadIds)
	{
		StationOutputById.Remove(DeadId);
	}

	SetStationOutput(MoveTemp(NewArray));
}

void UArcCraftingViewModel::ClearStationInput()
{
	ServerInputSpecs.Reset();
	RebuildStationInputView();
}

void UArcCraftingViewModel::ClearStationOutput()
{
	StationOutputById.Reset();
	SetStationOutput(TArray<TObjectPtr<UArcItemViewModel>>());
}
