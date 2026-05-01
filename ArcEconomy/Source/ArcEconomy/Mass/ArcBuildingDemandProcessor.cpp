// Copyright Lukasz Baran. All Rights Reserved.

#include "Mass/ArcBuildingDemandProcessor.h"
#include "Mass/ArcEconomyFragments.h"
#include "Knowledge/ArcEconomyKnowledgeTypes.h"
#include "ArcCraft/Mass/ArcCraftMassFragments.h"
#include "ArcKnowledgeSubsystem.h"
#include "ArcKnowledgeEntry.h"
#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"
#include "ArcCraft/Recipe/ArcRecipeIngredient.h"
#include "Items/ArcItemDefinition.h"
#include "Items/ArcItemSpec.h"
#include "Core/ArcCoreAssetManager.h"
#include "ArcItemEconomyFragment.h"
#include "ArcCraft/Recipe/ArcRecipeDefinition.h"
#include "ArcCraft/Recipe/ArcRecipeQuality.h"
#include "Items/Fragments/ArcItemFragment_Tags.h"

UArcBuildingDemandProcessor::UArcBuildingDemandProcessor()
	: BuildingQuery{*this}
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
	ProcessingPhase = EMassProcessingPhase::DuringPhysics;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Server | EProcessorExecutionFlags::Standalone);
}

void UArcBuildingDemandProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	BuildingQuery.AddRequirement<FArcBuildingFragment>(EMassFragmentAccess::ReadOnly);
	BuildingQuery.AddRequirement<FArcBuildingWorkforceFragment>(EMassFragmentAccess::ReadWrite);
	BuildingQuery.AddRequirement<FArcCraftInputFragment>(EMassFragmentAccess::ReadOnly);
}

void UArcBuildingDemandProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	TimeSinceLastTick += Context.GetDeltaTimeSeconds();
	if (TimeSinceLastTick < TickInterval)
	{
		return;
	}
	TimeSinceLastTick = 0.0f;

	UArcKnowledgeSubsystem* KnowledgeSub = Context.GetWorld()->GetSubsystem<UArcKnowledgeSubsystem>();
	if (!KnowledgeSub)
	{
		return;
	}

	UMassSignalSubsystem* SignalSubsystem = Context.GetWorld()->GetSubsystem<UMassSignalSubsystem>();

	TRACE_CPUPROFILER_EVENT_SCOPE(ArcBuildingDemand);

	BuildingQuery.ForEachEntityChunk(Context, [&EntityManager, KnowledgeSub, SignalSubsystem](FMassExecutionContext& Ctx)
	{
		const TConstArrayView<FArcBuildingFragment> Buildings = Ctx.GetFragmentView<FArcBuildingFragment>();
		TArrayView<FArcBuildingWorkforceFragment> Workforces = Ctx.GetMutableFragmentView<FArcBuildingWorkforceFragment>();
		const TConstArrayView<FArcCraftInputFragment> Inputs = Ctx.GetFragmentView<FArcCraftInputFragment>();
		const int32 NumEntities = Ctx.GetNumEntities();

		for (int32 EntityIndex = 0; EntityIndex < NumEntities; ++EntityIndex)
		{
			const FArcBuildingFragment& Building = Buildings[EntityIndex];
			FArcBuildingWorkforceFragment& Workforce = Workforces[EntityIndex];
			const FMassEntityHandle Entity = Ctx.GetEntity(EntityIndex);

			for (int32 SlotIndex = 0; SlotIndex < Workforce.Slots.Num(); ++SlotIndex)
			{
				FArcBuildingSlotData& Slot = Workforce.Slots[SlotIndex];

				// Slot not halted or no recipe â remove all stale demand entries
				if (!Slot.DesiredRecipe || Slot.HaltDuration <= 0.0f)
				{
					for (FArcKnowledgeHandle& Handle : Slot.DemandKnowledgeHandles)
					{
						if (Handle.IsValid())
						{
							KnowledgeSub->ForceRemoveKnowledge(Handle);
							Handle = FArcKnowledgeHandle();
						}
					}
					Slot.DemandKnowledgeHandles.Reset();
					continue;
				}

				const FArcCraftInputFragment& InputFrag = Inputs[EntityIndex];
				const int32 IngredientCount = Slot.DesiredRecipe->GetIngredientCount();

				// Ensure handle array is sized to ingredient count, preserving existing handles
				if (Slot.DemandKnowledgeHandles.Num() < IngredientCount)
				{
					Slot.DemandKnowledgeHandles.SetNum(IngredientCount);
				}

				// Remove excess handles if recipe changed to one with fewer ingredients
				for (int32 ExcessIdx = IngredientCount; ExcessIdx < Slot.DemandKnowledgeHandles.Num(); ++ExcessIdx)
				{
					if (Slot.DemandKnowledgeHandles[ExcessIdx].IsValid())
					{
						KnowledgeSub->ForceRemoveKnowledge(Slot.DemandKnowledgeHandles[ExcessIdx]);
					}
				}
				Slot.DemandKnowledgeHandles.SetNum(IngredientCount);

				UArcQualityTierTable* TierTable = nullptr;
				if (!Slot.DesiredRecipe->QualityTierTable.IsNull())
				{
					TierTable = Slot.DesiredRecipe->QualityTierTable.LoadSynchronous();
				}

				for (int32 IngIdx = 0; IngIdx < IngredientCount; ++IngIdx)
				{
					const FArcRecipeIngredient* Ingredient = Slot.DesiredRecipe->GetIngredientBase(IngIdx);
					if (!Ingredient)
					{
						// Invalid ingredient â remove stale handle if any
						if (Slot.DemandKnowledgeHandles[IngIdx].IsValid())
						{
							KnowledgeSub->ForceRemoveKnowledge(Slot.DemandKnowledgeHandles[IngIdx]);
							Slot.DemandKnowledgeHandles[IngIdx] = FArcKnowledgeHandle();
						}
						continue;
					}

					// Check satisfaction: accumulate matching items
					int32 Available = 0;
					for (const FArcItemSpec& InputItem : InputFrag.InputItems)
					{
						if (Ingredient->DoesItemSatisfy(InputItem, TierTable))
						{
							Available += InputItem.Amount;
						}
					}

					const bool bSatisfied = Available >= Ingredient->Amount;

					if (bSatisfied)
					{
						// Ingredient satisfied â remove advertisement if posted
						if (Slot.DemandKnowledgeHandles[IngIdx].IsValid())
						{
							KnowledgeSub->ForceRemoveKnowledge(Slot.DemandKnowledgeHandles[IngIdx]);
							Slot.DemandKnowledgeHandles[IngIdx] = FArcKnowledgeHandle();
						}
						continue;
					}

					// Ingredient unsatisfied â build demand advertisement
					const int32 MissingQuantity = Ingredient->Amount - Available;

					// Check if any supply exists for this ingredient before posting transport ad
					FArcSettlementMarketFragment* SupplyCheckMarket =
						EntityManager.GetFragmentDataPtr<FArcSettlementMarketFragment>(Building.SettlementHandle);

					FArcKnowledgeEntry DemandEntry;
					DemandEntry.Tags.AddTag(ArcEconomy::Tags::TAG_Knowledge_Economy_Transport);
					DemandEntry.Location = Building.BuildingLocation;
					DemandEntry.SourceEntity = Entity;
					DemandEntry.Relevance = FMath::Min(Slot.HaltDuration / 60.0f, 1.0f);

					FArcEconomyDemandPayload DemandPayload;
					DemandPayload.QuantityNeeded = MissingQuantity;
					DemandPayload.SettlementHandle = Building.SettlementHandle;

					// Populate item def or tags depending on ingredient type
					const FArcRecipeIngredient_ItemDef* ItemDefIngredient =
						Slot.DesiredRecipe->GetIngredient<FArcRecipeIngredient_ItemDef>(IngIdx);
					const FArcRecipeIngredient_Tags* TagsIngredient =
						Slot.DesiredRecipe->GetIngredient<FArcRecipeIngredient_Tags>(IngIdx);

					if (ItemDefIngredient && ItemDefIngredient->ItemDefinitionId.IsValid())
					{
						UArcItemDefinition* ItemDef = UArcCoreAssetManager::GetAsset<UArcItemDefinition>(
							ItemDefIngredient->ItemDefinitionId.AssetId);
						DemandPayload.ItemDefinition = ItemDef;

						// Skip transport ad if no supply exists
						if (SupplyCheckMarket)
						{
							const FArcResourceMarketData* SupplyMarketData = SupplyCheckMarket->PriceTable.Find(ItemDef);
							if (!SupplyMarketData || SupplyMarketData->SupplySources.IsEmpty())
							{
								if (Slot.DemandKnowledgeHandles[IngIdx].IsValid())
								{
									KnowledgeSub->ForceRemoveKnowledge(Slot.DemandKnowledgeHandles[IngIdx]);
									Slot.DemandKnowledgeHandles[IngIdx] = FArcKnowledgeHandle();
								}
								continue;
							}
						}

						// Price lookup from settlement market
						FArcSettlementMarketFragment* Market =
							EntityManager.GetFragmentDataPtr<FArcSettlementMarketFragment>(Building.SettlementHandle);
						if (Market && ItemDef)
						{
							FArcResourceMarketData* MarketData = Market->PriceTable.Find(ItemDef);
							if (MarketData)
							{
								DemandPayload.OfferingPrice = MarketData->Price;
							}
							FArcResourceMarketData& CounterData = Market->PriceTable.FindOrAdd(ItemDef);
							if (CounterData.Price <= 0.0f)
							{
								const FArcItemEconomyFragment* EconFrag = ItemDef->FindFragment<FArcItemEconomyFragment>();
								if (EconFrag)
								{
									CounterData.Price = EconFrag->BasePrice;
								}
							}
							CounterData.DemandCounter += MissingQuantity;
						}
					}
					else if (TagsIngredient)
					{
						DemandPayload.RequiredTags = TagsIngredient->RequiredTags;

						// Skip transport ad if no supply with matching tags exists
						bool bAnyTagSupply = false;
						if (SupplyCheckMarket)
						{
							for (const TPair<TObjectPtr<UArcItemDefinition>, FArcResourceMarketData>& MarketPair : SupplyCheckMarket->PriceTable)
							{
								if (MarketPair.Value.SupplySources.IsEmpty())
								{
									continue;
								}
								const UArcItemDefinition* MarketItemDef = MarketPair.Key;
								if (MarketItemDef)
								{
									const FArcItemFragment_Tags* TagsFrag = MarketItemDef->FindFragment<FArcItemFragment_Tags>();
									if (TagsFrag && TagsFrag->AssetTags.HasAll(TagsIngredient->RequiredTags))
									{
										bAnyTagSupply = true;
										break;
									}
								}
							}
						}
						if (!bAnyTagSupply)
						{
							if (Slot.DemandKnowledgeHandles[IngIdx].IsValid())
							{
								KnowledgeSub->ForceRemoveKnowledge(Slot.DemandKnowledgeHandles[IngIdx]);
								Slot.DemandKnowledgeHandles[IngIdx] = FArcKnowledgeHandle();
							}
							continue;
						}

						// No concrete item to price â OfferingPrice stays 0
					}
					else
					{
						// Unknown ingredient subtype â skip
						continue;
					}

					DemandEntry.Payload.InitializeAs<FArcEconomyDemandPayload>(DemandPayload);

					if (Slot.DemandKnowledgeHandles[IngIdx].IsValid())
					{
						KnowledgeSub->UpdateKnowledge(Slot.DemandKnowledgeHandles[IngIdx], DemandEntry);
					}
					else
					{
						Slot.DemandKnowledgeHandles[IngIdx] = KnowledgeSub->PostAdvertisement(DemandEntry);
						if (SignalSubsystem && Building.SettlementHandle.IsValid())
						{
							SignalSubsystem->SignalEntity(ArcEconomy::Signals::DemandPosted, Building.SettlementHandle);
						}
					}
				}
			}
		}
	});
}
