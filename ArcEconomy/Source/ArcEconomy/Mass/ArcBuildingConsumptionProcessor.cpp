// Copyright Lukasz Baran. All Rights Reserved.

#include "Mass/ArcBuildingConsumptionProcessor.h"
#include "Mass/ArcEconomyFragments.h"
#include "Mass/ArcMassItemFragments.h"
#include "Knowledge/ArcEconomyKnowledgeTypes.h"
#include "ArcKnowledgeSubsystem.h"
#include "ArcKnowledgeEntry.h"
#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"
#include "Items/ArcItemDefinition.h"
#include "Items/ArcItemSpec.h"
#include "Items/Fragments/ArcItemFragment_Tags.h"
#include "ArcItemEconomyFragment.h"

UArcBuildingConsumptionProcessor::UArcBuildingConsumptionProcessor()
	: ConsumerQuery{*this}
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
	ProcessingPhase = EMassProcessingPhase::DuringPhysics;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Server | EProcessorExecutionFlags::Standalone);
}

void UArcBuildingConsumptionProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ConsumerQuery.AddRequirement<FArcBuildingFragment>(EMassFragmentAccess::ReadWrite);
	ConsumerQuery.AddRequirement<FArcMassItemSpecArrayFragment>(EMassFragmentAccess::ReadOnly);
	ConsumerQuery.AddSharedRequirement<FArcBuildingEconomyConfig>(EMassFragmentAccess::ReadOnly);
}

void UArcBuildingConsumptionProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
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

	TRACE_CPUPROFILER_EVENT_SCOPE(ArcBuildingConsumption);

	ConsumerQuery.ForEachEntityChunk(Context, [&EntityManager, KnowledgeSub, SignalSubsystem](FMassExecutionContext& Ctx)
	{
		const FArcBuildingEconomyConfig& EconomyConfig = Ctx.GetSharedFragment<FArcBuildingEconomyConfig>();
		if (EconomyConfig.ConsumptionNeeds.IsEmpty())
		{
			return;
		}

		TArrayView<FArcBuildingFragment> Buildings = Ctx.GetMutableFragmentView<FArcBuildingFragment>();
		const TConstArrayView<FArcMassItemSpecArrayFragment> ItemArrays = Ctx.GetFragmentView<FArcMassItemSpecArrayFragment>();
		const int32 NumEntities = Ctx.GetNumEntities();
		const int32 NeedCount = EconomyConfig.ConsumptionNeeds.Num();

		for (int32 EntityIndex = 0; EntityIndex < NumEntities; ++EntityIndex)
		{
			FArcBuildingFragment& Building = Buildings[EntityIndex];
			const FArcMassItemSpecArrayFragment& ItemArrayFrag = ItemArrays[EntityIndex];
			const FMassEntityHandle Entity = Ctx.GetEntity(EntityIndex);

			// Remove excess handles if config changed
			for (int32 ExcessIdx = NeedCount; ExcessIdx < Building.ConsumptionDemandHandles.Num(); ++ExcessIdx)
			{
				if (Building.ConsumptionDemandHandles[ExcessIdx].IsValid())
				{
					KnowledgeSub->ForceRemoveKnowledge(Building.ConsumptionDemandHandles[ExcessIdx]);
				}
			}
			Building.ConsumptionDemandHandles.SetNum(NeedCount);

			// No settlement yet — clean up any stale handles and skip
			if (!Building.SettlementHandle.IsValid())
			{
				for (FArcKnowledgeHandle& Handle : Building.ConsumptionDemandHandles)
				{
					if (Handle.IsValid())
					{
						KnowledgeSub->ForceRemoveKnowledge(Handle);
						Handle = FArcKnowledgeHandle();
					}
				}
				continue;
			}

			for (int32 NeedIdx = 0; NeedIdx < NeedCount; ++NeedIdx)
			{
				const FArcBuildingConsumptionEntry& Need = EconomyConfig.ConsumptionNeeds[NeedIdx];
				FArcKnowledgeHandle& Handle = Building.ConsumptionDemandHandles[NeedIdx];

				// Count current stock that satisfies this need
				int32 CurrentStock = 0;
				const bool bIsItemDefBased = (Need.Item != nullptr);

				for (const FArcItemSpec& Spec : ItemArrayFrag.Items)
				{
					if (bIsItemDefBased)
					{
						if (Spec.ItemDefinition == Need.Item)
						{
							CurrentStock += Spec.Amount;
						}
					}
					else if (Need.ItemTag.IsValid() && Spec.ItemDefinition)
					{
						const FArcItemFragment_Tags* TagsFrag = Spec.ItemDefinition->FindFragment<FArcItemFragment_Tags>();
						if (TagsFrag && TagsFrag->AssetTags.HasTag(Need.ItemTag))
						{
							CurrentStock += Spec.Amount;
						}
					}
				}

				const int32 Deficit = Need.DesiredStockLevel - CurrentStock;

				// Satisfied — remove stale handle
				if (Deficit <= 0)
				{
					if (Handle.IsValid())
					{
						KnowledgeSub->ForceRemoveKnowledge(Handle);
						Handle = FArcKnowledgeHandle();
					}
					continue;
				}

				// Check supply existence before posting
				if (bIsItemDefBased)
				{
					FArcSettlementMarketFragment* SupplyCheckMarket =
						EntityManager.GetFragmentDataPtr<FArcSettlementMarketFragment>(Building.SettlementHandle);
					if (SupplyCheckMarket)
					{
						const FArcResourceMarketData* SupplyMarketData = SupplyCheckMarket->PriceTable.Find(Need.Item);
						if (!SupplyMarketData || SupplyMarketData->SupplySources.IsEmpty())
						{
							if (Handle.IsValid())
							{
								KnowledgeSub->ForceRemoveKnowledge(Handle);
								Handle = FArcKnowledgeHandle();
							}
							continue;
						}
					}
				}
				else if (Need.ItemTag.IsValid())
				{
					FArcSettlementMarketFragment* SupplyCheckMarket =
						EntityManager.GetFragmentDataPtr<FArcSettlementMarketFragment>(Building.SettlementHandle);
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
								if (TagsFrag && TagsFrag->AssetTags.HasTag(Need.ItemTag))
								{
									bAnyTagSupply = true;
									break;
								}
							}
						}
					}
					if (!bAnyTagSupply)
					{
						if (Handle.IsValid())
						{
							KnowledgeSub->ForceRemoveKnowledge(Handle);
							Handle = FArcKnowledgeHandle();
						}
						continue;
					}
				}
				else
				{
					// No item def and no tag — skip
					continue;
				}

				// Build demand entry
				FArcKnowledgeEntry DemandEntry;
				DemandEntry.Tags.AddTag(ArcEconomy::Tags::TAG_Knowledge_Economy_Transport);
				DemandEntry.Location = Building.BuildingLocation;
				DemandEntry.SourceEntity = Entity;
				DemandEntry.Relevance = FMath::Clamp(static_cast<float>(Deficit) / static_cast<float>(Need.DesiredStockLevel), 0.0f, 1.0f);

				FArcEconomyDemandPayload DemandPayload;
				DemandPayload.QuantityNeeded = Deficit;
				DemandPayload.SettlementHandle = Building.SettlementHandle;

				if (bIsItemDefBased)
				{
					DemandPayload.ItemDefinition = Need.Item;

					FArcSettlementMarketFragment* Market =
						EntityManager.GetFragmentDataPtr<FArcSettlementMarketFragment>(Building.SettlementHandle);
					if (Market && Need.Item)
					{
						FArcResourceMarketData* MarketData = Market->PriceTable.Find(Need.Item);
						if (MarketData)
						{
							DemandPayload.OfferingPrice = MarketData->Price;
						}

						FArcResourceMarketData& CounterData = Market->PriceTable.FindOrAdd(Need.Item);
						if (CounterData.Price <= 0.0f)
						{
							const FArcItemEconomyFragment* EconFrag = Need.Item->FindFragment<FArcItemEconomyFragment>();
							if (EconFrag)
							{
								CounterData.Price = EconFrag->BasePrice;
							}
						}
						CounterData.DemandCounter += Deficit;
					}
				}
				else if (Need.ItemTag.IsValid())
				{
					DemandPayload.RequiredTags.AddTag(Need.ItemTag);
				}

				DemandEntry.Payload.InitializeAs<FArcEconomyDemandPayload>(DemandPayload);

				if (Handle.IsValid())
				{
					KnowledgeSub->UpdateKnowledge(Handle, DemandEntry);
				}
				else
				{
					Handle = KnowledgeSub->PostAdvertisement(DemandEntry);
					if (SignalSubsystem && Building.SettlementHandle.IsValid())
					{
						SignalSubsystem->SignalEntity(ArcEconomy::Signals::DemandPosted, Building.SettlementHandle);
					}
				}
			}
		}
	});
}
