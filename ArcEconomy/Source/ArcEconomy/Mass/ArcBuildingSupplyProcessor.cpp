// Copyright Lukasz Baran. All Rights Reserved.

#include "Mass/ArcBuildingSupplyProcessor.h"
#include "Mass/ArcEconomyFragments.h"
#include "Knowledge/ArcEconomyKnowledgeTypes.h"
#include "ArcCraft/Mass/ArcCraftMassFragments.h"
#include "ArcKnowledgeSubsystem.h"
#include "ArcKnowledgeEntry.h"
#include "Items/ArcItemSpec.h"
#include "ArcItemEconomyFragment.h"
#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"
#include "Items/ArcItemDefinition.h"
#include "Items/Fragments/ArcItemFragment_Tags.h"

UArcBuildingSupplyProcessor::UArcBuildingSupplyProcessor()
	: BuildingQuery{*this}
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
	ProcessingPhase = EMassProcessingPhase::DuringPhysics;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Server | EProcessorExecutionFlags::Standalone);
}

void UArcBuildingSupplyProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	BuildingQuery.AddRequirement<FArcBuildingFragment>(EMassFragmentAccess::ReadWrite);
	BuildingQuery.AddRequirement<FArcCraftOutputFragment>(EMassFragmentAccess::ReadWrite);
}

void UArcBuildingSupplyProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	TimeSinceLastTick += Context.GetDeltaTimeSeconds();
	const bool bTimerElapsed = TimeSinceLastTick >= TickInterval;
	if (bTimerElapsed)
	{
		TimeSinceLastTick = 0.0f;
	}

	UArcKnowledgeSubsystem* KnowledgeSub = Context.GetWorld()->GetSubsystem<UArcKnowledgeSubsystem>();
	if (!KnowledgeSub)
	{
		return;
	}

	UMassSignalSubsystem* SignalSubsystem = Context.GetWorld()->GetSubsystem<UMassSignalSubsystem>();

	TRACE_CPUPROFILER_EVENT_SCOPE(ArcBuildingSupply);

	BuildingQuery.ForEachEntityChunk(Context, [&EntityManager, KnowledgeSub, SignalSubsystem, bTimerElapsed](FMassExecutionContext& Ctx)
	{
		TArrayView<FArcBuildingFragment> Buildings = Ctx.GetMutableFragmentView<FArcBuildingFragment>();
		TArrayView<FArcCraftOutputFragment> Outputs = Ctx.GetMutableFragmentView<FArcCraftOutputFragment>();
		const int32 NumEntities = Ctx.GetNumEntities();

		for (int32 EntityIndex = 0; EntityIndex < NumEntities; ++EntityIndex)
		{
			FArcBuildingFragment& Building = Buildings[EntityIndex];
			FArcCraftOutputFragment& Output = Outputs[EntityIndex];

			const bool bDirty = Output.bOutputDirty;
			if (!bTimerElapsed && !bDirty)
			{
				continue;
			}
			Output.bOutputDirty = false;

			// Update output count for backpressure tracking
			int32 TotalOutputCount = 0;
			for (const FArcItemSpec& OutItem : Output.OutputItems)
			{
				TotalOutputCount += OutItem.Amount;
			}
			Building.CurrentOutputCount = TotalOutputCount;

			const FMassEntityHandle Entity = Ctx.GetEntity(EntityIndex);
			if (!Building.SettlementHandle.IsValid())
			{
				continue;
			}
			
			const FArcSettlementMarketFragment* Market = EntityManager.GetFragmentDataPtr<FArcSettlementMarketFragment>(Building.SettlementHandle);

			TSet<TObjectPtr<UArcItemDefinition>> SeenItems;
			TArray<TObjectPtr<UArcItemDefinition>> FreshlyRegisteredItems;

			for (const FArcItemSpec& ItemSpec : Output.OutputItems)
			{
				UArcItemDefinition* ItemDef = const_cast<UArcItemDefinition*>(ItemSpec.GetItemDefinition());
				if (!ItemDef || ItemSpec.Amount <= 0)
				{
					continue;
				}

				SeenItems.Add(ItemDef);

				FArcKnowledgeEntry SupplyEntry;
				SupplyEntry.Tags.AddTag(ArcEconomy::Tags::TAG_Knowledge_Economy_Supply);
				SupplyEntry.Location = Building.BuildingLocation;
				SupplyEntry.SourceEntity = Entity;
				SupplyEntry.Relevance = 1.0f;

				FArcEconomySupplyPayload SupplyPayload;
				SupplyPayload.ItemDefinition = ItemDef;
				SupplyPayload.QuantityAvailable = ItemSpec.Amount;
				SupplyPayload.SettlementHandle = Building.SettlementHandle;

				if (Market)
				{
					const FArcResourceMarketData* MarketData = Market->PriceTable.Find(ItemDef);
					if (MarketData)
					{
						SupplyPayload.AskingPrice = MarketData->Price;
					}
				}

				SupplyEntry.Payload.InitializeAs<FArcEconomySupplyPayload>(SupplyPayload);

				FArcKnowledgeHandle* ExistingHandle = Building.SupplyKnowledgeHandles.Find(ItemDef);
				if (ExistingHandle && ExistingHandle->IsValid())
				{
					KnowledgeSub->UpdateKnowledge(*ExistingHandle, SupplyEntry);
				}
				else
				{
					FArcKnowledgeHandle NewHandle = KnowledgeSub->RegisterKnowledge(SupplyEntry);
					Building.SupplyKnowledgeHandles.Add(ItemDef, NewHandle);
					FreshlyRegisteredItems.Add(ItemDef);
				}

				FArcSettlementMarketFragment* MutableMarket = EntityManager.GetFragmentDataPtr<FArcSettlementMarketFragment>(Building.SettlementHandle);
				if (MutableMarket)
				{
					FArcResourceMarketData& MarketData = MutableMarket->PriceTable.FindOrAdd(ItemDef);
					if (MarketData.Price <= 0.0f)
					{
						const FArcItemEconomyFragment* EconFrag = ItemDef->FindFragment<FArcItemEconomyFragment>();
						if (EconFrag)
						{
							MarketData.Price = EconFrag->BasePrice;
						}
					}
					MarketData.SupplyCounter += ItemSpec.Amount;

					// Update supply source entry for this building + item
					bool bFoundSource = false;
					for (FArcMarketSupplySource& Source : MarketData.SupplySources)
					{
						if (Source.BuildingHandle == Entity)
						{
							Source.Quantity = ItemSpec.Amount;
							Source.ReservedQuantity = FMath::Min(Source.ReservedQuantity, Source.Quantity);
							bFoundSource = true;
							break;
						}
					}
					if (!bFoundSource)
					{
						FArcMarketSupplySource NewSource;
						NewSource.BuildingHandle = Entity;
						NewSource.Quantity = ItemSpec.Amount;
						MarketData.SupplySources.Add(NewSource);
					}
				}
			}

			// Nudge transport ads that match newly available supply.
			// Single batched query instead of per-item to avoid redundant spatial lookups.
			if (FreshlyRegisteredItems.Num() > 0)
			{
				static constexpr float TransportNudgeRadius = 50000.0f;
				static const FGameplayTagQuery TransportTagQuery = FGameplayTagQuery::MakeQuery_MatchTag(ArcEconomy::Tags::TAG_Knowledge_Economy_Transport);
				TArray<FArcKnowledgeHandle> TransportHandles;
				KnowledgeSub->QueryKnowledgeInRadius(Building.BuildingLocation, TransportNudgeRadius, TransportHandles, TransportTagQuery);

				for (const FArcKnowledgeHandle& TransportHandle : TransportHandles)
				{
					const FArcKnowledgeEntry* TransportEntry = KnowledgeSub->GetKnowledgeEntry(TransportHandle);
					if (!TransportEntry)
					{
						continue;
					}

					const FArcEconomyDemandPayload* DemandPayload = TransportEntry->Payload.GetPtr<FArcEconomyDemandPayload>();
					if (!DemandPayload)
					{
						continue;
					}

					bool bMatches = false;
					for (UArcItemDefinition* FreshItem : FreshlyRegisteredItems)
					{
						if (DemandPayload->ItemDefinition)
						{
							if (DemandPayload->ItemDefinition == FreshItem)
							{
								bMatches = true;
								break;
							}
						}
						else if (!DemandPayload->RequiredTags.IsEmpty())
						{
							const FArcItemFragment_Tags* TagsFrag = FreshItem->FindFragment<FArcItemFragment_Tags>();
							if (TagsFrag && TagsFrag->AssetTags.HasAll(DemandPayload->RequiredTags))
							{
								bMatches = true;
								break;
							}
						}
					}

					if (bMatches)
					{
						FArcKnowledgeEntry UpdatedEntry = *TransportEntry;
						KnowledgeSub->UpdateKnowledge(TransportHandle, UpdatedEntry);
					}
				}
			}

			// Signal settlement when new supply is registered or craft output arrived
			if (SignalSubsystem && Building.SettlementHandle.IsValid() && (FreshlyRegisteredItems.Num() > 0 || bDirty))
			{
				SignalSubsystem->SignalEntity(ArcEconomy::Signals::SupplyAvailable, Building.SettlementHandle);
			}

			// Remove handles for items no longer in output
			TArray<TObjectPtr<UArcItemDefinition>> StaleItems;
			for (const TPair<TObjectPtr<UArcItemDefinition>, FArcKnowledgeHandle>& Pair : Building.SupplyKnowledgeHandles)
			{
				if (!SeenItems.Contains(Pair.Key))
				{
					StaleItems.Add(Pair.Key);
				}
			}
			for (const TObjectPtr<UArcItemDefinition>& StaleItem : StaleItems)
			{
				FArcKnowledgeHandle Handle = Building.SupplyKnowledgeHandles.FindAndRemoveChecked(StaleItem);
				if (Handle.IsValid())
				{
					KnowledgeSub->ForceRemoveKnowledge(Handle);
				}
			}

			// Remove stale supply sources from market for this building
			if (FArcSettlementMarketFragment* MarketForCleanup = EntityManager.GetFragmentDataPtr<FArcSettlementMarketFragment>(Building.SettlementHandle))
			{
				for (TPair<TObjectPtr<UArcItemDefinition>, FArcResourceMarketData>& MarketPair : MarketForCleanup->PriceTable)
				{
					if (SeenItems.Contains(MarketPair.Key))
					{
						continue;
					}
					TArray<FArcMarketSupplySource>& Sources = MarketPair.Value.SupplySources;
					for (int32 SourceIdx = Sources.Num() - 1; SourceIdx >= 0; --SourceIdx)
					{
						if (Sources[SourceIdx].BuildingHandle == Entity)
						{
							Sources.RemoveAtSwap(SourceIdx);
						}
					}
				}
			}
		}
	});
}
