// Copyright Lukasz Baran. All Rights Reserved.

#include "ViewModels/ArcRecipeIngredientViewModel.h"

#include "Engine/AssetManager.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Items/ArcItemSpec.h"
#include "Items/ArcItemDefinitionData.h"
#include "Items/Fragments/ArcItemFragment_UIDescription.h"
#include "Items/Fragments/ArcItemFragment_Icon.h"
#include "ArcCraft/Recipe/ArcRecipeIngredient.h"
#include "Display/ArcTagDisplayRegistry.h"
#include "ViewModels/ArcIngredientCandidateViewModel.h"
#include "ViewModels/ArcItemViewModel.h"

#define LOCTEXT_NAMESPACE "ArcRecipeIngredientViewModel"

namespace ArcRecipeIngredientVMPrivate
{
	enum class ECandidateOrigin : uint8
	{
		ServerInput,
		Preview,
		Inventory
	};

	using FCandidateKey = TTuple<FArcItemId, ECandidateOrigin>;

	UArcIngredientCandidateViewModel* TakeCandidate(
		TMap<FCandidateKey, TObjectPtr<UArcIngredientCandidateViewModel>>& Existing,
		const FCandidateKey& Key,
		UObject* Outer)
	{
		TObjectPtr<UArcIngredientCandidateViewModel> Found;
		if (Existing.RemoveAndCopyValue(Key, Found) && Found != nullptr)
		{
			return Found.Get();
		}
		return NewObject<UArcIngredientCandidateViewModel>(Outer);
	}
}

bool UArcRecipeIngredientViewModel::DoesItemSatisfy(const FArcItemSpec& Spec) const
{
	const FArcRecipeIngredient* Base = Ingredient.GetPtr<FArcRecipeIngredient>();
	return Base != nullptr && Base->DoesItemSatisfy(Spec);
}

void UArcRecipeIngredientViewModel::RefreshFrom(
	const FInstancedStruct& IngredientStruct,
	TConstArrayView<FArcCraftStoreItemRow> InventoryItems,
	TConstArrayView<FArcItemSpec> ServerInput,
	TConstArrayView<FArcCraftStoreItemRow> PreviewInput,
	const UArcTagDisplayRegistry* Registry,
	const UArcStatDisplayRegistry* StatReg)
{
	using namespace ArcRecipeIngredientVMPrivate;

	const FArcRecipeIngredient* Base = IngredientStruct.GetPtr<FArcRecipeIngredient>();
	if (Base == nullptr)
	{
		return;
	}
	Ingredient = IngredientStruct;

	int32 NewRequired = Base->Amount;
	FText NewDisplayName = Base->SlotName;
	FText NewTagDescription;
	TSoftObjectPtr<UObject> NewIcon;
	EArcRecipeIngredientSource NewSource = EArcRecipeIngredientSource::ByItemDef;
	int32 NewHaveFromInventory = 0;
	FArcNamedPrimaryAssetId NewRequiredItemId;
	FGameplayTagContainer NewRequiredTags;
	FGameplayTagContainer NewDenyTags;
	float NewMinRarityStep = Base->MinRarityStep;
	const FArcItemDefinitionData* ResolvedRequiredDef = nullptr;

	if (const FArcRecipeIngredient_ItemDef* DefIngredient = IngredientStruct.GetPtr<FArcRecipeIngredient_ItemDef>())
	{
		NewSource = EArcRecipeIngredientSource::ByItemDef;
		NewRequiredItemId = DefIngredient->ItemDefinitionId;
		NewDenyTags = DefIngredient->DenyTags;
		// RequiredTags stays default (empty) for ByItemDef slots.

		const FArcItemDefinitionData* Def = FArcItemDefinitionData::Find(DefIngredient->ItemDefinitionId.AssetId);
		ResolvedRequiredDef = Def;
		if (Def != nullptr)
		{
			if (const FArcItemFragment_UIDescription* UIDesc = Def->FindFragment<FArcItemFragment_UIDescription>())
			{
				if (!UIDesc->ItemName.IsEmpty())
				{
					NewDisplayName = UIDesc->ItemName;
				}
			}
			if (const FArcItemFragment_Icon* IconFrag = Def->FindFragment<FArcItemFragment_Icon>())
			{
				NewIcon = IconFrag->ItemIcon;
			}
		}
	}
	else if (const FArcRecipeIngredient_Tags* TagIngredient = IngredientStruct.GetPtr<FArcRecipeIngredient_Tags>())
	{
		NewSource = EArcRecipeIngredientSource::ByTag;
		NewRequiredTags = TagIngredient->RequiredTags;
		NewDenyTags = TagIngredient->DenyTags;
		// RequiredItemId stays default (invalid) for ByTag slots.

		if (!TagIngredient->RequiredTags.IsEmpty())
		{
			// Pick the leaf-most tag: greatest depth measured by '.' count in ToString().
			FGameplayTag ChosenTag;
			int32 BestDepth = -1;
			for (const FGameplayTag& Tag : TagIngredient->RequiredTags)
			{
				const FString TagStr = Tag.ToString();
				int32 Depth = 0;
				for (const TCHAR Ch : TagStr)
				{
					if (Ch == TEXT('.'))
					{
						++Depth;
					}
				}
				if (Depth > BestDepth)
				{
					BestDepth = Depth;
					ChosenTag = Tag;
				}
			}

			if (ChosenTag.IsValid() && Registry != nullptr)
			{
				const FArcTagDisplayData* Disp = Registry->FindDisplay(ChosenTag);
				if (Disp != nullptr)
				{
					if (!Disp->DisplayName.IsEmpty())
					{
						NewDisplayName = Disp->DisplayName;
					}
					NewTagDescription = Disp->Description;
					NewIcon = Disp->Icon;
				}
			}
		}
		// Empty RequiredTags => keep SlotName, no registry lookup.
	}

	for (const FArcCraftStoreItemRow& Row : InventoryItems)
	{
		if (Base->DoesItemSatisfy(Row.Spec))
		{
			NewHaveFromInventory += Row.Stacks;
		}
	}

	int32 NewHaveFromStation = 0;
	for (const FArcItemSpec& Spec : ServerInput)
	{
		if (Base->DoesItemSatisfy(Spec))
		{
			NewHaveFromStation += static_cast<int32>(Spec.Amount);
		}
	}
	for (const FArcCraftStoreItemRow& Row : PreviewInput)
	{
		if (Base->DoesItemSatisfy(Row.Spec))
		{
			NewHaveFromStation += Row.Stacks;
		}
	}

	const bool bNewOptional = Base->bOptional;
	const int32 NewHave = NewHaveFromInventory + NewHaveFromStation;
	const int32 NewMissing = FMath::Max(0, NewRequired - NewHaveFromStation);
	const bool bNewFilled = NewHaveFromStation >= NewRequired;
	const bool bNewSatisfied = bNewOptional || bNewFilled;
	const bool bNewOptionalEmpty = bNewOptional && NewHaveFromStation == 0;

	UE_MVVM_SET_PROPERTY_VALUE(Source, NewSource);
	UE_MVVM_SET_PROPERTY_VALUE(DisplayName, NewDisplayName);
	UE_MVVM_SET_PROPERTY_VALUE(Icon, NewIcon);
	UE_MVVM_SET_PROPERTY_VALUE(Required, NewRequired);
	UE_MVVM_SET_PROPERTY_VALUE(Have, NewHave);
	UE_MVVM_SET_PROPERTY_VALUE(Missing, NewMissing);
	UE_MVVM_SET_PROPERTY_VALUE(bSatisfied, bNewSatisfied);
	UE_MVVM_SET_PROPERTY_VALUE(bOptional, bNewOptional);
	UE_MVVM_SET_PROPERTY_VALUE(bFilled, bNewFilled);
	UE_MVVM_SET_PROPERTY_VALUE(bOptionalEmpty, bNewOptionalEmpty);
	UE_MVVM_SET_PROPERTY_VALUE(HaveFromInventory, NewHaveFromInventory);
	UE_MVVM_SET_PROPERTY_VALUE(HaveFromStation, NewHaveFromStation);
	UE_MVVM_SET_PROPERTY_VALUE(RequiredItemId, NewRequiredItemId);
	UE_MVVM_SET_PROPERTY_VALUE(RequiredTags, NewRequiredTags);
	UE_MVVM_SET_PROPERTY_VALUE(DenyTags, NewDenyTags);
	UE_MVVM_SET_PROPERTY_VALUE(MinRarityStep, NewMinRarityStep);

	const bool bNewIsTagSource = (NewSource == EArcRecipeIngredientSource::ByTag);
	const bool bNewIsItemSource = (NewSource == EArcRecipeIngredientSource::ByItemDef);
	UE_MVVM_SET_PROPERTY_VALUE(TagDescription, NewTagDescription);
	UE_MVVM_SET_PROPERTY_VALUE(bIsTagSource, bNewIsTagSource);
	UE_MVVM_SET_PROPERTY_VALUE(bIsItemSource, bNewIsItemSource);

	// Required-item detail VM: built only for ByItemDef slots from the resolved definition.
	// Create-once/recycle; if the def failed to resolve, clear the detail fields via null def.
	if (bNewIsItemSource)
	{
		if (RequiredItemViewModel == nullptr)
		{
			UArcItemViewModel* NewRequiredVM = NewObject<UArcItemViewModel>(this);
			UE_MVVM_SET_PROPERTY_VALUE(RequiredItemViewModel, NewRequiredVM);
		}
		RequiredItemViewModel->RefreshFromDefinition(ResolvedRequiredDef, StatReg);
	}
	else if (RequiredItemViewModel != nullptr)
	{
		UE_MVVM_SET_PROPERTY_VALUE(RequiredItemViewModel, TObjectPtr<UArcItemViewModel>(nullptr));
	}

	// Recycled by (ItemId, origin): a preview row and the inventory row it is taken from share one ItemId.
	TMap<FCandidateKey, TObjectPtr<UArcIngredientCandidateViewModel>> ExistingCandidateMap;
	ExistingCandidateMap.Reserve(Candidates.Num());
	for (TObjectPtr<UArcIngredientCandidateViewModel> ExistingVM : Candidates)
	{
		if (ExistingVM == nullptr || !ExistingVM->GetItemId().IsValid())
		{
			continue;
		}
		const ECandidateOrigin Origin = !ExistingVM->GetbFromStation()
			? ECandidateOrigin::Inventory
			: (ExistingVM->GetbPreview() ? ECandidateOrigin::Preview : ECandidateOrigin::ServerInput);
		ExistingCandidateMap.Add(FCandidateKey(ExistingVM->GetItemId(), Origin), ExistingVM);
	}

	TArray<TObjectPtr<UArcIngredientCandidateViewModel>> NewCandidates;
	TSet<FPrimaryAssetId> OwnedCandidateDefIds;

	for (const FArcItemSpec& Spec : ServerInput)
	{
		const FArcItemDefinitionData* Def = Spec.GetItemDefinition();
		if (Def == nullptr || !Base->DoesItemSatisfy(Spec))
		{
			continue;
		}

		UArcIngredientCandidateViewModel* CandidateVM =
			TakeCandidate(ExistingCandidateMap, FCandidateKey(Spec.ItemId, ECandidateOrigin::ServerInput), this);
		CandidateVM->RefreshFrom(Spec, static_cast<int32>(Spec.Amount), Def, Registry);
		CandidateVM->SetOrigin(true, false, FMassEntityHandle(), nullptr);

		OwnedCandidateDefIds.Add(Spec.GetItemDefinitionId());
		NewCandidates.Add(CandidateVM);
	}

	for (const FArcCraftStoreItemRow& Row : PreviewInput)
	{
		const FArcItemDefinitionData* Def = Row.Spec.GetItemDefinition();
		if (Def == nullptr || !Base->DoesItemSatisfy(Row.Spec))
		{
			continue;
		}

		UArcIngredientCandidateViewModel* CandidateVM =
			TakeCandidate(ExistingCandidateMap, FCandidateKey(Row.Spec.ItemId, ECandidateOrigin::Preview), this);
		CandidateVM->RefreshFrom(Row.Spec, Row.Stacks, Def, Registry);
		CandidateVM->SetOrigin(true, true, Row.OwnerEntity, Row.StoreType);

		OwnedCandidateDefIds.Add(Row.Spec.GetItemDefinitionId());
		NewCandidates.Add(CandidateVM);
	}

	for (const FArcCraftStoreItemRow& Row : InventoryItems)
	{
		const FArcItemDefinitionData* Def = Row.Spec.GetItemDefinition();
		if (Def == nullptr || !Base->DoesItemSatisfy(Row.Spec))
		{
			continue;
		}

		UArcIngredientCandidateViewModel* CandidateVM =
			TakeCandidate(ExistingCandidateMap, FCandidateKey(Row.Spec.ItemId, ECandidateOrigin::Inventory), this);
		CandidateVM->RefreshFrom(Row.Spec, Row.Stacks, Def, Registry);
		CandidateVM->SetOrigin(false, false, Row.OwnerEntity, Row.StoreType);

		OwnedCandidateDefIds.Add(Row.Spec.GetItemDefinitionId());
		NewCandidates.Add(CandidateVM);
	}

	// Unowned tag-family potentials — enumerate item definitions from the AssetRegistry whose
	// authored AssetTags satisfy this tag slot but that the player does NOT own, so the detail
	// panel surfaces every craftable-toward item with an owned count of 0. ByTag slots only.
	if (bNewIsTagSource && !NewRequiredTags.IsEmpty())
	{
		IAssetRegistry* AssetRegistry = IAssetRegistry::Get();
		if (AssetRegistry != nullptr)
		{
			TArray<FAssetData> DefAssets;
			AssetRegistry->GetAssetsByClass(
				FArcItemDefinitionData::StaticStruct()->GetStructPathName(), DefAssets, /*bRecursiveClasses*/ false);

			for (const FAssetData& DefAsset : DefAssets)
			{
				// Same tag semantics as FArcRecipeIngredient_Tags::DoesItemSatisfy — rarity is
				// intentionally skipped (unowned defs have no runtime spec; shown as potentials).
				const FGameplayTagContainer AssetTags = FArcItemDefinitionData::GetAssetTags(DefAsset);
				if (!AssetTags.HasAll(NewRequiredTags))
				{
					continue;
				}
				if (!NewDenyTags.IsEmpty() && AssetTags.HasAny(NewDenyTags))
				{
					continue;
				}

				const FPrimaryAssetId DefId = DefAsset.GetPrimaryAssetId();
				if (!DefId.IsValid())
				{
					continue;
				}

				// Player already owns this def (station/inventory candidate above shows its real
				// count) or it matched twice in the registry set — skip.
				if (OwnedCandidateDefIds.Contains(DefId))
				{
					continue;
				}

				if (!UAssetManager::Get().GetPrimaryAssetPath(DefId).IsValid())
				{
					continue;
				}

				const FArcItemDefinitionData* Def = FArcItemDefinitionData::Find(DefId);
				if (Def == nullptr)
				{
					continue;
				}

				int32 Owned = 0;
				for (const FArcCraftStoreItemRow& Row : InventoryItems)
				{
					if (Row.Spec.GetItemDefinitionId() == DefId)
					{
						Owned += Row.Stacks;
					}
				}

				// Unowned candidates carry an INVALID FArcItemId — never recycle them via the
				// item-id map (they would all collide on the invalid key). Always fresh NewObject.
				UArcIngredientCandidateViewModel* CandidateVM = NewObject<UArcIngredientCandidateViewModel>(this);

				FArcItemSpec PotentialSpec;
				PotentialSpec.SetItemDefinition(DefId);
				CandidateVM->RefreshFrom(PotentialSpec, Owned, Def, Registry);
				CandidateVM->SetOrigin(false, false, FMassEntityHandle(), nullptr);

				OwnedCandidateDefIds.Add(DefId);
				NewCandidates.Add(CandidateVM);
			}
		}
	}

	SetCandidates(MoveTemp(NewCandidates));

	// Refresh the formatted "N Available." text from the updated Have count.
	RefreshFormattedText();
}

void UArcRecipeIngredientViewModel::SetCandidates(
	TArray<TObjectPtr<UArcIngredientCandidateViewModel>>&& NewCandidates)
{
	UE_MVVM_SET_PROPERTY_VALUE(Candidates, NewCandidates);
}

void UArcRecipeIngredientViewModel::RefreshFormattedText()
{
	FFormatNamedArguments AvailableArgs;
	AvailableArgs.Add(TEXT("Available"), FText::AsNumber(Have));
	const FText NewAvailableText = FText::Format(LOCTEXT("AvailableFmt", "{Available} Available."), AvailableArgs);

	UE_MVVM_SET_PROPERTY_VALUE(AvailableText, NewAvailableText);

	// "Slotted / Required": Slotted = matching items currently in the station input (HaveFromStation),
	// Required = the amount this slot needs for one craft. Property name kept stable for widget binding.
	FFormatNamedArguments SlottedArgs;
	SlottedArgs.Add(TEXT("Slotted"), FText::AsNumber(HaveFromStation));
	SlottedArgs.Add(TEXT("Required"), FText::AsNumber(Required));
	const FText NewSlottedAvailableText = FText::Format(
		LOCTEXT("SlottedRequiredFmt", "{Slotted} / {Required}"), SlottedArgs);

	UE_MVVM_SET_PROPERTY_VALUE(SlottedAvailableText, NewSlottedAvailableText);
}

#undef LOCTEXT_NAMESPACE
