// Copyright Lukasz Baran. All Rights Reserved.

#include "Sim/ArcProductionProcessor.h"

#include "ArcCraft/Recipe/ArcRecipeDefinitionData.h"
#include "ArcCraft/Recipe/ArcRecipeIngredient.h"
#include "ArcCraft/Shared/ArcCraftRecords.h"
#include "ArcMass/Debugger/ArcEconomyTrace.h"
#include "ArcMass/SmartObject/ArcMassSmartObjectFragments.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemSpec.h"
#include "Mass/EntityManager.h"
#include "Mass/EntityView.h"
#include "Mass/ExecutionContext.h"
#include "Operations/ArcMassItemOperations.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Sim/ArcBuildingFragments.h"
#include "Sim/ArcEconomyNames.h"
#include "Sim/ArcProductionSlots.h"
#include "Sim/ArcStaffingProcessor.h"
#include "Sim/ArcWorldSimStores.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcProductionProcessor)

namespace ArcProductionProcessorPrivate
{
int32 SumStacks(const FArcMassItemStoreFragment& Store)
{
	int32 Total = 0;
	for (const FArcMassReplicatedItem& Item : Store.ReplicatedItems.Items)
	{
		const FArcItemData* Data = Item.ToItem();
		if (Data)
		{
			Total += static_cast<int32>(Data->GetStacks());
		}
	}
	return Total;
}

bool MatchesTagIngredient(const FArcItemData& Data, const FArcRecipeIngredient_Tags& Ingredient)
{
	const FGameplayTagContainer& ItemTags = Data.GetItemAggregatedTags();
	if (!Ingredient.RequiredTags.IsEmpty() && !ItemTags.HasAll(Ingredient.RequiredTags))
	{
		return false;
	}
	if (!Ingredient.DenyTags.IsEmpty() && ItemTags.HasAny(Ingredient.DenyTags))
	{
		return false;
	}
	return true;
}

void ConsumeStacks(
	FMassEntityManager& EntityManager,
	const FMassEntityHandle Entity,
	const UScriptStruct* StoreType,
	const TArray<TPair<FArcItemId, int32>>& Candidates,
	int32 Amount)
{
	int32 Remaining = Amount;
	for (const TPair<FArcItemId, int32>& Candidate : Candidates)
	{
		if (Remaining <= 0)
		{
			break;
		}

		const int32 ToTake = FMath::Min(Candidate.Value, Remaining);
		if (ToTake <= 0)
		{
			continue;
		}

		ArcMassItems::ModifyStacks(EntityManager, Entity, StoreType, Candidate.Key, -ToTake);
		Remaining -= ToTake;
		if (Candidate.Value - ToTake <= 0)
		{
			ArcMassItems::RemoveItem(EntityManager, Entity, StoreType, Candidate.Key);
		}
	}
}

void ConsumeItemDef(
	FMassEntityManager& EntityManager,
	const FMassEntityHandle Entity,
	const FArcBuildingInputStore& InputStore,
	const FArcRecipeIngredient_ItemDef& Ingredient)
{
	TArray<TPair<FArcItemId, int32>> Candidates;
	const FPrimaryAssetId DefinitionId = Ingredient.ItemDefinitionId;
	for (const FArcMassReplicatedItem& Item : InputStore.ReplicatedItems.Items)
	{
		const FArcItemData* Data = Item.ToItem();
		if (Data && Data->GetItemDefinitionId() == DefinitionId)
		{
			Candidates.Add(TPair<FArcItemId, int32>(Data->GetItemId(), static_cast<int32>(Data->GetStacks())));
		}
	}
	ConsumeStacks(EntityManager, Entity, FArcBuildingInputStore::StaticStruct(), Candidates, Ingredient.Amount);
}

void ConsumeTagIngredient(
	FMassEntityManager& EntityManager,
	const FMassEntityHandle Entity,
	const FArcBuildingInputStore& InputStore,
	const FArcRecipeIngredient_Tags& Ingredient)
{
	TArray<TPair<FArcItemId, int32>> Candidates;
	for (const FArcMassReplicatedItem& Item : InputStore.ReplicatedItems.Items)
	{
		const FArcItemData* Data = Item.ToItem();
		if (Data && MatchesTagIngredient(*Data, Ingredient))
		{
			Candidates.Add(TPair<FArcItemId, int32>(Data->GetItemId(), static_cast<int32>(Data->GetStacks())));
		}
	}
	ConsumeStacks(EntityManager, Entity, FArcBuildingInputStore::StaticStruct(), Candidates, Ingredient.Amount);
}

bool HasIngredients(const FArcBuildingInputStore& InputStore, const FArcRecipeDefinitionData& Recipe)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ArcProductionHasIngredients);
	for (const FInstancedStruct& IngredientStruct : Recipe.Ingredients)
	{
		const FArcRecipeIngredient* Ingredient = IngredientStruct.GetPtr<FArcRecipeIngredient>();
		if (Ingredient != nullptr && Ingredient->bOptional)
		{
			continue;
		}

		if (const FArcRecipeIngredient_ItemDef* ItemDef = IngredientStruct.GetPtr<FArcRecipeIngredient_ItemDef>())
		{
			if (ArcMassItems::CountItemsByDefinition(InputStore, ItemDef->ItemDefinitionId) < ItemDef->Amount)
			{
				return false;
			}
		}
		else if (const FArcRecipeIngredient_Tags* Tags = IngredientStruct.GetPtr<FArcRecipeIngredient_Tags>())
		{
			if (ArcMassItems::CountItemsByTags(InputStore, Tags->RequiredTags, Tags->DenyTags) < Tags->Amount)
			{
				return false;
			}
		}
		else if (IngredientStruct.GetPtr<FArcRecipeIngredient>())
		{
			return false;
		}
	}
	return true;
}

void FindMissingIngredient(
	const FArcBuildingInputStore& InputStore,
	const FArcRecipeDefinitionData& Recipe,
	FString& OutItem,
	FString& OutTags)
{
	OutItem.Reset();
	OutTags.Reset();
	for (const FInstancedStruct& IngredientStruct : Recipe.Ingredients)
	{
		const FArcRecipeIngredient* Ingredient = IngredientStruct.GetPtr<FArcRecipeIngredient>();
		if (Ingredient != nullptr && Ingredient->bOptional)
		{
			continue;
		}

		if (const FArcRecipeIngredient_ItemDef* ItemDef = IngredientStruct.GetPtr<FArcRecipeIngredient_ItemDef>())
		{
			if (ArcMassItems::CountItemsByDefinition(InputStore, ItemDef->ItemDefinitionId) < ItemDef->Amount)
			{
				OutItem = ArcEconomyNames::ItemName(ItemDef->ItemDefinitionId);
				return;
			}
		}
		else if (const FArcRecipeIngredient_Tags* Tags = IngredientStruct.GetPtr<FArcRecipeIngredient_Tags>())
		{
			if (ArcMassItems::CountItemsByTags(InputStore, Tags->RequiredTags, Tags->DenyTags) < Tags->Amount)
			{
				OutTags = Tags->RequiredTags.ToStringSimple();
				return;
			}
		}
	}
}

void ConsumeCraftIngredients(
	FMassEntityManager& EntityManager,
	const FMassEntityHandle Entity,
	const FArcBuildingInputStore& InputStore,
	const FArcRecipeDefinitionData& Recipe)
{
	for (const FInstancedStruct& IngredientStruct : Recipe.Ingredients)
	{
		const FArcRecipeIngredient* Ingredient = IngredientStruct.GetPtr<FArcRecipeIngredient>();
		if (Ingredient != nullptr && Ingredient->bOptional)
		{
			continue;
		}

		if (const FArcRecipeIngredient_ItemDef* ItemDef = IngredientStruct.GetPtr<FArcRecipeIngredient_ItemDef>())
		{
			if (ItemDef->bConsumeOnCraft)
			{
				ConsumeItemDef(EntityManager, Entity, InputStore, *ItemDef);
			}
		}
		else if (const FArcRecipeIngredient_Tags* Tags = IngredientStruct.GetPtr<FArcRecipeIngredient_Tags>())
		{
			if (Tags->bConsumeOnCraft)
			{
				ConsumeTagIngredient(EntityManager, Entity, InputStore, *Tags);
			}
		}
	}
}
}

UArcProductionProcessor::UArcProductionProcessor()
	: EntityQuery(*this)
{
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	ExecutionOrder.ExecuteAfter.Add(UArcStaffingProcessor::StaticClass()->GetFName());
	bRequiresGameThreadExecution = true;
}

void UArcProductionProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.Initialize(EntityManager);
	EntityQuery.AddConstSharedRequirement<FArcProductionConfigFragment>(EMassFragmentPresence::All);
	EntityQuery.AddRequirement<FArcProductionSlotsFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FArcStaffedFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddSparseRequirement<FArcBuildingInputStore>(EMassFragmentPresence::All);
	EntityQuery.AddSparseRequirement<FArcBuildingOutputStore>(EMassFragmentPresence::All);
	EntityQuery.RegisterWithProcessor(*this);
}

void UArcProductionProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	int32 UnstaffedSlots = 0;
	int32 StarvedSlots = 0;
	int32 ProducingSlots = 0;
	int32 BlockedSlots = 0;
	int32 TotalStaff = 0;
	int32 AssignedRecipeSlots = 0;
	int32 CompletedThisTick = 0;

	EntityQuery.ForEachEntityChunk(Context,
		[&EntityManager, &UnstaffedSlots, &StarvedSlots, &ProducingSlots, &BlockedSlots, &TotalStaff, &AssignedRecipeSlots, &CompletedThisTick](
			FMassExecutionContext& Ctx)
	{
		const FArcProductionConfigFragment& Config = Ctx.GetConstSharedFragment<FArcProductionConfigFragment>();
		TArrayView<FArcProductionSlotsFragment> ProductionSlots = Ctx.GetMutableFragmentView<FArcProductionSlotsFragment>();
		const TConstArrayView<FArcStaffedFragment> StaffedViews = Ctx.GetFragmentView<FArcStaffedFragment>();
		const float DeltaTime = Ctx.GetDeltaTimeSeconds();

		for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			FArcProductionSlotsFragment& Production = ProductionSlots[EntityIt];
			const int32 StaffCount = StaffedViews[EntityIt].StaffCount;
			TotalStaff += StaffCount;
			const FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);
			const FMassEntityView EntityView(EntityManager, Entity);
			const FArcSmartObjectDefinitionSharedFragment* Def =
				EntityView.GetConstSharedFragmentDataPtr<FArcSmartObjectDefinitionSharedFragment>();
			ArcWorldSim::SyncProductionSlots(
				Production,
				Config,
				Def ? Def->SmartObjectDefinition.Get() : nullptr);
			const FArcBuildingInputStore* InputStore = EntityView.GetSparseFragmentDataPtr<FArcBuildingInputStore>();
			const FArcBuildingOutputStore* OutputStore = EntityView.GetSparseFragmentDataPtr<FArcBuildingOutputStore>();
			if (!InputStore || !OutputStore)
			{
				continue;
			}

			const FString BuildingLabel = ArcEconomyNames::BuildingName(EntityManager, Entity);
			int32 WorkersNeeded = 0;
			for (int32 SlotIndex = 0; SlotIndex < Production.Slots.Num(); ++SlotIndex)
			{
				FArcProductionSlot& Slot = Production.Slots[SlotIndex];
				const EArcProductionState OldState = Slot.State;
				FPrimaryAssetId RecipeId;
				const FArcRecipeDefinitionData* Recipe = nullptr;
				if (Config.Recipes.IsValidIndex(Slot.RecipeIndex))
				{
					RecipeId = Config.Recipes[Slot.RecipeIndex].AssetId;
					Recipe = ArcCraftRecords::Find<FArcRecipeDefinitionData>(RecipeId);
				}
				if (Recipe)
				{
					++AssignedRecipeSlots;
				}

				if (!Production.bProductionEnabled)
				{
					Slot.State = EArcProductionState::Unstaffed;
				}
				else if (!Recipe)
				{
					// Checked before WorkersNeeded += so a (none) slot does not starve later assigned slots of staff.
					Slot.State = EArcProductionState::Unstaffed;
				}
				else
				{
					WorkersNeeded += Slot.RequiredWorkerCount;
					if (StaffCount < WorkersNeeded)
					{
						Slot.State = EArcProductionState::Unstaffed;
						Slot.Progress = 0.0f;
						Slot.HaltDuration += DeltaTime;
					}
					else
					{
						// AddItem mutates the store synchronously via EntityManager (ArcMassItemOperations.cpp:147-151), not Defer.
						InputStore = EntityView.GetSparseFragmentDataPtr<FArcBuildingInputStore>();
						OutputStore = EntityView.GetSparseFragmentDataPtr<FArcBuildingOutputStore>();
						if (!InputStore || !OutputStore)
						{
							break;
						}

						if (ArcProductionProcessorPrivate::SumStacks(*OutputStore) >= Config.OutputCapacity)
						{
							Slot.State = EArcProductionState::Blocked;
							Slot.HaltDuration += DeltaTime;
						}
						else if (!ArcProductionProcessorPrivate::HasIngredients(*InputStore, *Recipe))
						{
							Slot.State = EArcProductionState::Starved;
							Slot.HaltDuration += DeltaTime;
						}
						else
						{
							Slot.State = EArcProductionState::Producing;
							Slot.HaltDuration = 0.0f;
							Slot.Progress += DeltaTime;
							if (Slot.Progress >= Recipe->CraftTime)
							{
								ArcProductionProcessorPrivate::ConsumeCraftIngredients(EntityManager, Entity, *InputStore, *Recipe);
#if WITH_ARC_ECONOMY_TRACE
								FString DiagOutputs;
#endif
								for (const FArcRecipeOutput& Output : Recipe->Outputs)
								{
									if (!Output.ItemDefinition.IsValid())
									{
										continue;
									}
									const FArcItemSpec OutputSpec = FArcItemSpec::NewItem(
										Output.ItemDefinition,
										Output.Level,
										Output.Amount);
									ArcMassItems::AddItem(EntityManager, Entity, FArcBuildingOutputStore::StaticStruct(), OutputSpec);
#if WITH_ARC_ECONOMY_TRACE
									if (!DiagOutputs.IsEmpty())
									{
										DiagOutputs += TEXT(",");
									}
									DiagOutputs += FString::Printf(TEXT("%sx%d"),
										*ArcEconomyNames::ItemName(Output.ItemDefinition.AssetId), Output.Amount);
#endif
								}
								Slot.Progress -= Recipe->CraftTime;
								++CompletedThisTick;
								TRACE_ARC_ECONOMY_BOOKMARKF(
									TEXT("PROD-COMPLETE building=%s slot=%d recipe=%s outputs=%s"),
									*BuildingLabel,
									SlotIndex,
									*ArcEconomyNames::RecipeName(RecipeId),
									*DiagOutputs);
							}
						}
					}
				}

				if (Slot.State != OldState)
				{
					if (Slot.State == EArcProductionState::Producing)
					{
						TRACE_ARC_ECONOMY_BOOKMARKF(
							TEXT("PROD-PRODUCING building=%s slot=%d recipe=%s"),
							*BuildingLabel,
							SlotIndex,
							*ArcEconomyNames::RecipeName(RecipeId));
					}
					else if (Slot.State == EArcProductionState::Starved && Recipe)
					{
						FString MissingItem;
						FString MissingTags;
						ArcProductionProcessorPrivate::FindMissingIngredient(*InputStore, *Recipe, MissingItem, MissingTags);
						TRACE_ARC_ECONOMY_BOOKMARKF(
							TEXT("PROD-STARVED building=%s slot=%d missingItem=%s missingTags=%s"),
							*BuildingLabel,
							SlotIndex,
							*MissingItem,
							*MissingTags);
					}
					else if (Slot.State == EArcProductionState::Blocked)
					{
						TRACE_ARC_ECONOMY_BOOKMARKF(
							TEXT("PROD-BLOCKED building=%s slot=%d recipe=%s"),
							*BuildingLabel,
							SlotIndex,
							*ArcEconomyNames::RecipeName(RecipeId));
					}
				}

				switch (Slot.State)
				{
				case EArcProductionState::Unstaffed:
					++UnstaffedSlots;
					break;
				case EArcProductionState::Starved:
					++StarvedSlots;
					break;
				case EArcProductionState::Producing:
					++ProducingSlots;
					break;
				case EArcProductionState::Blocked:
					++BlockedSlots;
					break;
				}
			}
		}
	});

	CompletedCrafts += CompletedThisTick;
	// Bookmarks fire on state/value transitions; counters are absolute and written once per Execute.
	TRACE_ARC_ECONOMY_COUNTER_INT(TEXT("Production"), TEXT("Unstaffed"), UnstaffedSlots);
	TRACE_ARC_ECONOMY_COUNTER_INT(TEXT("Production"), TEXT("Starved"), StarvedSlots);
	TRACE_ARC_ECONOMY_COUNTER_INT(TEXT("Production"), TEXT("Producing"), ProducingSlots);
	TRACE_ARC_ECONOMY_COUNTER_INT(TEXT("Production"), TEXT("Blocked"), BlockedSlots);
	TRACE_ARC_ECONOMY_COUNTER_INT(TEXT("Production"), TEXT("StaffCount"), TotalStaff);
	TRACE_ARC_ECONOMY_COUNTER_INT(TEXT("Production"), TEXT("AssignedSlots"), AssignedRecipeSlots);
	TRACE_ARC_ECONOMY_COUNTER_INT(TEXT("Production"), TEXT("CompletedCrafts"), CompletedCrafts);
}
