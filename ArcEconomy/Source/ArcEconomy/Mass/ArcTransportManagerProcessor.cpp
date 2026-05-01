// Copyright Lukasz Baran. All Rights Reserved.

#include "Mass/ArcTransportManagerProcessor.h"
#include "Mass/ArcEconomyFragments.h"
#include "ArcEconomyTypes.h"
#include "ArcCraft/Mass/ArcCraftMassFragments.h"
#include "ArcCraft/Recipe/ArcRecipeIngredient.h"
#include "ArcCraft/Recipe/ArcRecipeDefinition.h"
#include "Items/ArcItemDefinition.h"
#include "Items/Fragments/ArcItemFragment_Tags.h"
#include "Core/ArcCoreAssetManager.h"
#include "Mass/EntityFragments.h"
#include "MassSignalSubsystem.h"
#include "MassStateTreeTypes.h"
#include "MassExecutionContext.h"
#include "Mass/ArcMassItemFragments.h"

UArcTransportManagerProcessor::UArcTransportManagerProcessor()
	: TransporterQuery{*this}
	, BuildingQuery{*this}
	, ConsumerBuildingQuery{*this}
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Server | EProcessorExecutionFlags::Standalone);
}

void UArcTransportManagerProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::InitializeInternal(Owner, EntityManager);

	UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
	SubscribeToSignal(*SignalSubsystem, ArcEconomy::Signals::SupplyAvailable);
	SubscribeToSignal(*SignalSubsystem, ArcEconomy::Signals::DemandPosted);
	SubscribeToSignal(*SignalSubsystem, ArcEconomy::Signals::TransporterIdle);
}

void UArcTransportManagerProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	// EntityQuery (base class) matches the signaled settlement entities
	EntityQuery.AddRequirement<FArcSettlementMarketFragment>(EMassFragmentAccess::ReadWrite);

	TransporterQuery.AddRequirement<FArcTransporterFragment>(EMassFragmentAccess::ReadWrite);
	TransporterQuery.AddRequirement<FArcEconomyNPCFragment>(EMassFragmentAccess::ReadOnly);
	TransporterQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);

	BuildingQuery.AddRequirement<FArcBuildingFragment>(EMassFragmentAccess::ReadOnly);
	BuildingQuery.AddRequirement<FArcBuildingWorkforceFragment>(EMassFragmentAccess::ReadOnly);
	BuildingQuery.AddRequirement<FArcCraftInputFragment>(EMassFragmentAccess::ReadOnly);

	ConsumerBuildingQuery.AddRequirement<FArcBuildingFragment>(EMassFragmentAccess::ReadOnly);
	ConsumerBuildingQuery.AddSharedRequirement<FArcBuildingEconomyConfig>(EMassFragmentAccess::ReadOnly);
	ConsumerBuildingQuery.AddRequirement<FArcMassItemSpecArrayFragment>(EMassFragmentAccess::ReadOnly);
}

void UArcTransportManagerProcessor::SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ArcTransportManager);

	struct FIdleTransporter
	{
		FMassEntityHandle Entity;
		FVector Location;
	};

	struct FTransportJob
	{
		FMassEntityHandle DemandBuilding;
		UArcItemDefinition* ItemDefinition = nullptr;
		int32 Quantity = 0;
		FMassEntityHandle SupplyBuilding;
		FVector SupplyLocation = FVector::ZeroVector;
	};

	// Collect unique signaled settlement handles
	TSet<FMassEntityHandle> SignaledSettlements;
	EntityQuery.ForEachEntityChunk(Context, [&SignaledSettlements](FMassExecutionContext& Ctx)
	{
		const int32 NumEntities = Ctx.GetNumEntities();
		for (int32 Idx = 0; Idx < NumEntities; ++Idx)
		{
			SignaledSettlements.Add(Ctx.GetEntity(Idx));
		}
	});

	if (SignaledSettlements.IsEmpty())
	{
		return;
	}

	// For each settlement, collect idle transporters
	// Clear the signal entity collection so TransporterQuery iterates all matching archetypes
	// (the signaled collection only contains settlement entities which don't match TransporterQuery)
	Context.ClearEntityCollection();

	TMap<FMassEntityHandle, TArray<FIdleTransporter>> IdleTransportersBySettlement;
	TransporterQuery.ForEachEntityChunk(Context, [&SignaledSettlements, &IdleTransportersBySettlement](FMassExecutionContext& Ctx)
	{
		const TConstArrayView<FArcTransporterFragment> Transporters = Ctx.GetFragmentView<FArcTransporterFragment>();
		const TConstArrayView<FArcEconomyNPCFragment> NPCs = Ctx.GetFragmentView<FArcEconomyNPCFragment>();
		const TConstArrayView<FTransformFragment> Transforms = Ctx.GetFragmentView<FTransformFragment>();
		const int32 NumEntities = Ctx.GetNumEntities();

		for (int32 Idx = 0; Idx < NumEntities; ++Idx)
		{
			const FArcEconomyNPCFragment& NPC = NPCs[Idx];
			if (NPC.Role != EArcEconomyNPCRole::Transporter)
			{
				continue;
			}

			if (!SignaledSettlements.Contains(NPC.SettlementHandle))
			{
				continue;
			}

			const FArcTransporterFragment& Transporter = Transporters[Idx];
			if (Transporter.TaskState != EArcTransporterTaskState::Idle)
			{
				continue;
			}

			FIdleTransporter IdleEntry;
			IdleEntry.Entity = Ctx.GetEntity(Idx);
			IdleEntry.Location = Transforms[Idx].GetTransform().GetLocation();
			IdleTransportersBySettlement.FindOrAdd(NPC.SettlementHandle).Add(IdleEntry);
		}
	});

	if (IdleTransportersBySettlement.IsEmpty())
	{
		return;
	}

	// For each settlement, collect fulfillable demand jobs from buildings
	TMap<FMassEntityHandle, TArray<FTransportJob>> JobsBySettlement;
	for (const FMassEntityHandle& SettlementHandle : SignaledSettlements)
	{
		if (!IdleTransportersBySettlement.Contains(SettlementHandle))
		{
			continue;
		}

		if (!EntityManager.IsEntityValid(SettlementHandle))
		{
			continue;
		}

		const FMassEntityView SettlementView(EntityManager, SettlementHandle);
		const FArcSettlementMarketFragment& Market = SettlementView.GetFragmentData<FArcSettlementMarketFragment>();

		BuildingQuery.ForEachEntityChunk(Context, [&SettlementHandle, &Market, &JobsBySettlement, &EntityManager](FMassExecutionContext& Ctx)
		{
			const TConstArrayView<FArcBuildingFragment> Buildings = Ctx.GetFragmentView<FArcBuildingFragment>();
			const TConstArrayView<FArcBuildingWorkforceFragment> Workforces = Ctx.GetFragmentView<FArcBuildingWorkforceFragment>();
			const int32 NumEntities = Ctx.GetNumEntities();

			for (int32 Idx = 0; Idx < NumEntities; ++Idx)
			{
				const FArcBuildingFragment& Building = Buildings[Idx];
				if (Building.SettlementHandle != SettlementHandle)
				{
					continue;
				}

				const FArcBuildingWorkforceFragment& Workforce = Workforces[Idx];
				const FMassEntityHandle BuildingEntity = Ctx.GetEntity(Idx);

				for (const FArcBuildingSlotData& Slot : Workforce.Slots)
				{
					if (Slot.HaltDuration <= 0.0f || !Slot.DesiredRecipe)
					{
						continue;
					}

					UArcRecipeDefinition* Recipe = Slot.DesiredRecipe;
					const int32 IngredientCount = Recipe->GetIngredientCount();

					for (int32 IngIdx = 0; IngIdx < IngredientCount; ++IngIdx)
					{
						// Check if this ingredient slot has an active demand (knowledge handle is valid)
						if (!Slot.DemandKnowledgeHandles.IsValidIndex(IngIdx) || !Slot.DemandKnowledgeHandles[IngIdx].IsValid())
						{
							continue;
						}

						// Resolve item definition from ingredient
						UArcItemDefinition* ItemDef = nullptr;

						const FArcRecipeIngredient_ItemDef* ItemDefIngredient = Recipe->GetIngredient<FArcRecipeIngredient_ItemDef>(IngIdx);
						if (ItemDefIngredient && ItemDefIngredient->ItemDefinitionId.IsValid())
						{
							ItemDef = UArcCoreAssetManager::GetAsset<UArcItemDefinition>(ItemDefIngredient->ItemDefinitionId.AssetId);
						}
						else
						{
							// Tag-based ingredient: search the market for any item with matching tags
							const FArcRecipeIngredient_Tags* TagIngredient = Recipe->GetIngredient<FArcRecipeIngredient_Tags>(IngIdx);
							if (TagIngredient)
							{
								for (const TPair<TObjectPtr<UArcItemDefinition>, FArcResourceMarketData>& MarketPair : Market.PriceTable)
								{
									UArcItemDefinition* CandidateDef = MarketPair.Key;
									if (!CandidateDef)
									{
										continue;
									}

									const FArcItemFragment_Tags* TagsFrag = CandidateDef->FindFragment<FArcItemFragment_Tags>();
									if (TagsFrag && TagsFrag->ItemTags.HasAll(TagIngredient->RequiredTags))
									{
										// Check available supply
										const FArcResourceMarketData& ResData = MarketPair.Value;
										for (const FArcMarketSupplySource& Source : ResData.SupplySources)
										{
											const int32 Available = Source.Quantity - Source.ReservedQuantity;
											if (Available > 0)
											{
												ItemDef = CandidateDef;
												break;
											}
										}
										if (ItemDef)
										{
											break;
										}
									}
								}
							}
						}

						if (!ItemDef)
						{
							continue;
						}

						// Check if there is supply available for this item
						const FArcResourceMarketData* ResData = Market.PriceTable.Find(ItemDef);
						if (!ResData)
						{
							continue;
						}

						for (const FArcMarketSupplySource& Source : ResData->SupplySources)
						{
							const int32 Available = Source.Quantity - Source.ReservedQuantity;
							if (Available <= 0)
							{
								continue;
							}

							// Resolve supply building location
							FVector SupplyLoc = FVector::ZeroVector;
							if (EntityManager.IsEntityValid(Source.BuildingHandle))
							{
								const FMassEntityView SupplyView(EntityManager, Source.BuildingHandle);
								const FArcBuildingFragment& SupplyBuilding = SupplyView.GetFragmentData<FArcBuildingFragment>();
								SupplyLoc = SupplyBuilding.BuildingLocation;
							}

							const FArcRecipeIngredient* BaseIngredient = Recipe->GetIngredientBase(IngIdx);
							const int32 NeededQuantity = BaseIngredient ? BaseIngredient->Amount : 1;
							const int32 JobQuantity = FMath::Min(NeededQuantity, Available);

							FTransportJob Job;
							Job.DemandBuilding = BuildingEntity;
							Job.ItemDefinition = ItemDef;
							Job.Quantity = JobQuantity;
							Job.SupplyBuilding = Source.BuildingHandle;
							Job.SupplyLocation = SupplyLoc;
							JobsBySettlement.FindOrAdd(SettlementHandle).Add(Job);
							break; // one source per ingredient
						}
					}
				}
			}
		});

		// --- Consumption-based demand (consumer buildings without recipes) ---
		ConsumerBuildingQuery.ForEachEntityChunk(Context, [&SettlementHandle, &Market, &JobsBySettlement, &EntityManager](FMassExecutionContext& Ctx)
		{
			const FArcBuildingEconomyConfig& EconomyConfig = Ctx.GetSharedFragment<FArcBuildingEconomyConfig>();
			if (EconomyConfig.ConsumptionNeeds.IsEmpty())
			{
				return;
			}

			const TConstArrayView<FArcBuildingFragment> Buildings = Ctx.GetFragmentView<FArcBuildingFragment>();
			const TConstArrayView<FArcMassItemSpecArrayFragment> ItemArrays = Ctx.GetFragmentView<FArcMassItemSpecArrayFragment>();
			const int32 NumEntities = Ctx.GetNumEntities();

			for (int32 Idx = 0; Idx < NumEntities; ++Idx)
			{
				const FArcBuildingFragment& Building = Buildings[Idx];
				if (Building.SettlementHandle != SettlementHandle)
				{
					continue;
				}

				const FArcMassItemSpecArrayFragment& ItemArrayFrag = ItemArrays[Idx];
				const FMassEntityHandle BuildingEntity = Ctx.GetEntity(Idx);

				for (int32 NeedIdx = 0; NeedIdx < EconomyConfig.ConsumptionNeeds.Num(); ++NeedIdx)
				{
					// Only process needs with active demand handles
					if (!Building.ConsumptionDemandHandles.IsValidIndex(NeedIdx) || !Building.ConsumptionDemandHandles[NeedIdx].IsValid())
					{
						continue;
					}

					const FArcBuildingConsumptionEntry& Need = EconomyConfig.ConsumptionNeeds[NeedIdx];

					// Count current stock to compute deficit
					int32 CurrentStock = 0;
					for (const FArcItemSpec& Spec : ItemArrayFrag.Items)
					{
						if (Need.Item && Spec.ItemDefinition == Need.Item)
						{
							CurrentStock += Spec.Amount;
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
					if (Deficit <= 0)
					{
						continue;
					}

					// Resolve item definition
					UArcItemDefinition* ItemDef = Need.Item;

					if (!ItemDef && Need.ItemTag.IsValid())
					{
						// Tag-based: search market for matching item with available supply
						for (const TPair<TObjectPtr<UArcItemDefinition>, FArcResourceMarketData>& MarketPair : Market.PriceTable)
						{
							UArcItemDefinition* CandidateDef = MarketPair.Key;
							if (!CandidateDef)
							{
								continue;
							}

							const FArcItemFragment_Tags* TagsFrag = CandidateDef->FindFragment<FArcItemFragment_Tags>();
							if (TagsFrag && TagsFrag->AssetTags.HasTag(Need.ItemTag))
							{
								const FArcResourceMarketData& ResData = MarketPair.Value;
								for (const FArcMarketSupplySource& Source : ResData.SupplySources)
								{
									const int32 Available = Source.Quantity - Source.ReservedQuantity;
									if (Available > 0)
									{
										ItemDef = CandidateDef;
										break;
									}
								}
								if (ItemDef)
								{
									break;
								}
							}
						}
					}

					if (!ItemDef)
					{
						continue;
					}

					// Find supply source
					const FArcResourceMarketData* ResData = Market.PriceTable.Find(ItemDef);
					if (!ResData)
					{
						continue;
					}

					for (const FArcMarketSupplySource& Source : ResData->SupplySources)
					{
						const int32 Available = Source.Quantity - Source.ReservedQuantity;
						if (Available <= 0)
						{
							continue;
						}

						FVector SupplyLoc = FVector::ZeroVector;
						if (EntityManager.IsEntityValid(Source.BuildingHandle))
						{
							const FMassEntityView SupplyView(EntityManager, Source.BuildingHandle);
							const FArcBuildingFragment& SupplyBuilding = SupplyView.GetFragmentData<FArcBuildingFragment>();
							SupplyLoc = SupplyBuilding.BuildingLocation;
						}

						const int32 JobQuantity = FMath::Min(Deficit, Available);

						FTransportJob Job;
						Job.DemandBuilding = BuildingEntity;
						Job.ItemDefinition = ItemDef;
						Job.Quantity = JobQuantity;
						Job.SupplyBuilding = Source.BuildingHandle;
						Job.SupplyLocation = SupplyLoc;
						JobsBySettlement.FindOrAdd(SettlementHandle).Add(Job);
						break; // one source per need
					}
				}
			}
		});
	}

	UMassSignalSubsystem* SignalSubsystem = Context.GetWorld()->GetSubsystem<UMassSignalSubsystem>();
	TArray<FMassEntityHandle> AssignedTransporters;

	// Match jobs to idle transporters by closest distance to supply building
	for (TPair<FMassEntityHandle, TArray<FTransportJob>>& SettlementPair : JobsBySettlement)
	{
		const FMassEntityHandle& SettlementHandle = SettlementPair.Key;
		TArray<FTransportJob>& Jobs = SettlementPair.Value;
		TArray<FIdleTransporter>* IdlePtr = IdleTransportersBySettlement.Find(SettlementHandle);
		if (!IdlePtr || IdlePtr->IsEmpty())
		{
			continue;
		}

		TArray<FIdleTransporter>& IdleTransporters = *IdlePtr;

		for (FTransportJob& Job : Jobs)
		{
			if (IdleTransporters.IsEmpty())
			{
				break;
			}

			// Find closest idle transporter to the supply building
			int32 BestIdx = INDEX_NONE;
			double BestDistSq = TNumericLimits<double>::Max();

			for (int32 TIdx = 0; TIdx < IdleTransporters.Num(); ++TIdx)
			{
				const double DistSq = FVector::DistSquared(IdleTransporters[TIdx].Location, Job.SupplyLocation);
				if (DistSq < BestDistSq)
				{
					BestDistSq = DistSq;
					BestIdx = TIdx;
				}
			}

			if (BestIdx == INDEX_NONE)
			{
				continue;
			}

			const FMassEntityHandle TransporterEntity = IdleTransporters[BestIdx].Entity;
			IdleTransporters.RemoveAtSwap(BestIdx);

			// Write the job to the transporter fragment
			if (!EntityManager.IsEntityValid(TransporterEntity))
			{
				continue;
			}

			FMassEntityView TransporterView(EntityManager, TransporterEntity);
			FArcTransporterFragment& TransporterFrag = TransporterView.GetFragmentData<FArcTransporterFragment>();
			TransporterFrag.TaskState = EArcTransporterTaskState::PickingUp;
			TransporterFrag.SourceBuildingHandle = Job.SupplyBuilding;
			TransporterFrag.DestinationBuildingHandle = Job.DemandBuilding;
			TransporterFrag.TargetBuildingHandle = Job.SupplyBuilding;
			TransporterFrag.TargetItemDefinition = Job.ItemDefinition;
			TransporterFrag.TargetQuantity = Job.Quantity;

			AssignedTransporters.Add(TransporterEntity);

			// Increment ReservedQuantity on the matched supply source
			if (EntityManager.IsEntityValid(SettlementHandle))
			{
				FMassEntityView SettlementView(EntityManager, SettlementHandle);
				FArcSettlementMarketFragment& Market = SettlementView.GetFragmentData<FArcSettlementMarketFragment>();
				FArcResourceMarketData* ResData = Market.PriceTable.Find(Job.ItemDefinition);
				if (ResData)
				{
					for (FArcMarketSupplySource& Source : ResData->SupplySources)
					{
						if (Source.BuildingHandle == Job.SupplyBuilding)
						{
							Source.ReservedQuantity += Job.Quantity;
							break;
						}
					}
				}
			}
		}
	}

	// Signal assigned transporters so their StateTrees re-evaluate
	if (SignalSubsystem && AssignedTransporters.Num() > 0)
	{
		SignalSubsystem->SignalEntities(UE::Mass::Signals::NewStateTreeTaskRequired, AssignedTransporters);
	}
}
