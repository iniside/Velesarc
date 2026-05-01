// Copyright Lukasz Baran. All Rights Reserved.

#include "StateTree/ArcTransportAcceptJobTask.h"
#include "Mass/ArcEconomyFragments.h"
#include "Knowledge/ArcEconomyKnowledgeTypes.h"
#include "ArcKnowledgeSubsystem.h"
#include "ArcKnowledgeEntry.h"
#include "ArcSettlementSubsystem.h"
#include "Items/ArcItemDefinition.h"
#include "Items/Fragments/ArcItemFragment_Tags.h"
#include "MassEntityView.h"
#include "MassStateTreeExecutionContext.h"
#include "StateTreeLinker.h"

bool FArcTransportAcceptJobTask::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(NPCFragmentHandle);
	Linker.LinkExternalData(TransporterFragmentHandle);
	Linker.LinkExternalData(TransformHandle);
	return true;
}

EStateTreeRunStatus FArcTransportAcceptJobTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FMassStateTreeExecutionContext& MassContext = static_cast<FMassStateTreeExecutionContext&>(Context);
	FArcTransportAcceptJobTaskInstanceData& InstanceData = Context.GetInstanceData(*this);

	UWorld* World = MassContext.GetWorld();
	UArcKnowledgeSubsystem* KnowledgeSub = World ? World->GetSubsystem<UArcKnowledgeSubsystem>() : nullptr;
	UArcSettlementSubsystem* SettlementSub = World ? World->GetSubsystem<UArcSettlementSubsystem>() : nullptr;
	if (!KnowledgeSub || !SettlementSub)
	{
		return EStateTreeRunStatus::Failed;
	}

	// 1. Extract demand payload from the claimed advertisement
	const FArcKnowledgeEntry* AdvertisementEntry = KnowledgeSub->GetKnowledgeEntry(InstanceData.AdvertisementHandle);
	if (!AdvertisementEntry)
	{
		return EStateTreeRunStatus::Failed;
	}

	const FArcEconomyDemandPayload* DemandPayload = AdvertisementEntry->Payload.GetPtr<FArcEconomyDemandPayload>();
	if (!DemandPayload || (!DemandPayload->ItemDefinition && DemandPayload->RequiredTags.IsEmpty()))
	{
		return EStateTreeRunStatus::Failed;
	}

	const bool bIsTagBasedDemand = !DemandPayload->ItemDefinition && !DemandPayload->RequiredTags.IsEmpty();

	const FMassEntityHandle DemandBuildingHandle = AdvertisementEntry->SourceEntity;
	UArcItemDefinition* TargetItemDef = DemandPayload->ItemDefinition;
	const FGameplayTagContainer& DemandRequiredTags = DemandPayload->RequiredTags;
	const int32 QuantityNeeded = DemandPayload->QuantityNeeded;

	// 2. Determine query radius based on caravan status
	const FArcEconomyNPCFragment& NPC = Context.GetExternalData(NPCFragmentHandle);
	const FTransformFragment& Transform = Context.GetExternalData(TransformHandle);
	const FVector Location = Transform.GetTransform().GetLocation();

	FMassEntityManager& EntityManager = MassContext.GetEntityManager();
	const FMassEntityHandle EntityHandle = MassContext.GetEntity();
	const FMassEntityView EntityView(EntityManager, EntityHandle);
	const bool bIsCaravan = EntityView.HasTag<FArcCaravanTag>();

	FMassEntityHandle BestSupplyBuilding;
	UArcItemDefinition* ResolvedItemDef = nullptr;
	int32 ResolvedQuantity = 0;

	if (!bIsCaravan)
	{
		// Local: read market fragment directly
		const FArcSettlementMarketFragment* Market =
			EntityManager.GetFragmentDataPtr<FArcSettlementMarketFragment>(NPC.SettlementHandle);
		if (!Market)
		{
			return EStateTreeRunStatus::Failed;
		}

		if (bIsTagBasedDemand)
		{
			// Scan market for any item with matching tags and supply
			float BestDist = TNumericLimits<float>::Max();
			for (const TPair<TObjectPtr<UArcItemDefinition>, FArcResourceMarketData>& MarketPair : Market->PriceTable)
			{
				const UArcItemDefinition* MarketItemDef = MarketPair.Key;
				if (!MarketItemDef || MarketPair.Value.SupplySources.IsEmpty())
				{
					continue;
				}
				const FArcItemFragment_Tags* TagsFrag = MarketItemDef->FindFragment<FArcItemFragment_Tags>();
				if (!TagsFrag || !TagsFrag->AssetTags.HasAll(DemandRequiredTags))
				{
					continue;
				}
				// Pick closest supply source
				for (const FArcMarketSupplySource& Source : MarketPair.Value.SupplySources)
				{
					if (!EntityManager.IsEntityValid(Source.BuildingHandle))
					{
						continue;
					}
					const FTransformFragment* BuildingTransform =
						EntityManager.GetFragmentDataPtr<FTransformFragment>(Source.BuildingHandle);
					if (!BuildingTransform)
					{
						continue;
					}
					const float Dist = FVector::Dist(Location, BuildingTransform->GetTransform().GetLocation());
					if (Dist < BestDist)
					{
						BestDist = Dist;
						BestSupplyBuilding = Source.BuildingHandle;
						ResolvedItemDef = const_cast<UArcItemDefinition*>(MarketItemDef);
						ResolvedQuantity = FMath::Min(QuantityNeeded, Source.Quantity);
					}
				}
			}
		}
		else
		{
			// Exact item match
			const FArcResourceMarketData* MarketData = Market->PriceTable.Find(TargetItemDef);
			if (!MarketData || MarketData->SupplySources.IsEmpty())
			{
				return EStateTreeRunStatus::Failed;
			}
			// Pick closest supply source
			float BestDist = TNumericLimits<float>::Max();
			for (const FArcMarketSupplySource& Source : MarketData->SupplySources)
			{
				if (!EntityManager.IsEntityValid(Source.BuildingHandle))
				{
					continue;
				}
				const FTransformFragment* BuildingTransform =
					EntityManager.GetFragmentDataPtr<FTransformFragment>(Source.BuildingHandle);
				if (!BuildingTransform)
				{
					continue;
				}
				const float Dist = FVector::Dist(Location, BuildingTransform->GetTransform().GetLocation());
				if (Dist < BestDist)
				{
					BestDist = Dist;
					BestSupplyBuilding = Source.BuildingHandle;
					ResolvedItemDef = TargetItemDef;
					ResolvedQuantity = FMath::Min(QuantityNeeded, Source.Quantity);
				}
			}
		}

		if (!BestSupplyBuilding.IsValid())
		{
			return EStateTreeRunStatus::Failed;
		}
	}
	else
	{
		// Caravan: use spatial knowledge query (cross-settlement)
		float QueryRadius = 100000.0f;

		FGameplayTagQuery SupplyTagQuery = FGameplayTagQuery::MakeQuery_MatchTag(ArcEconomy::Tags::TAG_Knowledge_Economy_Supply);
		TArray<FArcKnowledgeHandle> SupplyHandles;
		KnowledgeSub->QueryKnowledgeInRadius(Location, QueryRadius, SupplyHandles, SupplyTagQuery);

		if (SupplyHandles.Num() == 0)
		{
			return EStateTreeRunStatus::Failed;
		}

		float BestScore = -TNumericLimits<float>::Max();
		for (const FArcKnowledgeHandle& SupplyHandle : SupplyHandles)
		{
			const FArcKnowledgeEntry* SupplyEntry = KnowledgeSub->GetKnowledgeEntry(SupplyHandle);
			if (!SupplyEntry)
			{
				continue;
			}

			const FArcEconomySupplyPayload* SupplyPayload = SupplyEntry->Payload.GetPtr<FArcEconomySupplyPayload>();
			if (!SupplyPayload || !SupplyPayload->ItemDefinition)
			{
				continue;
			}

			if (bIsTagBasedDemand)
			{
				const FArcItemFragment_Tags* TagsFrag = SupplyPayload->ItemDefinition->FindFragment<FArcItemFragment_Tags>();
				if (!TagsFrag || !TagsFrag->AssetTags.HasAll(DemandRequiredTags))
				{
					continue;
				}
			}
			else
			{
				if (SupplyPayload->ItemDefinition != TargetItemDef)
				{
					continue;
				}
			}

			const float SellPrice = DemandPayload->OfferingPrice;
			const float BuyPrice = SupplyPayload->AskingPrice;
			const float Distance = FVector::Dist(SupplyEntry->Location, AdvertisementEntry->Location);
			const float TravelCost = Distance * SettlementSub->TravelCostScale;
			const float Score = SellPrice - BuyPrice - TravelCost;

			if (Score > BestScore)
			{
				BestScore = Score;
				BestSupplyBuilding = SupplyEntry->SourceEntity;
				ResolvedItemDef = const_cast<UArcItemDefinition*>(SupplyPayload->ItemDefinition.Get());
				ResolvedQuantity = FMath::Min(QuantityNeeded, SupplyPayload->QuantityAvailable);
			}
		}

		if (!BestSupplyBuilding.IsValid())
		{
			return EStateTreeRunStatus::Failed;
		}
	}

	// 5. Populate outputs
	InstanceData.DemandBuildingHandle = DemandBuildingHandle;
	InstanceData.SupplyBuildingHandle = BestSupplyBuilding;
	InstanceData.ItemDefinition = ResolvedItemDef;
	InstanceData.RequiredTags = DemandRequiredTags;
	InstanceData.Quantity = ResolvedQuantity;

	// 6. Update transporter fragment
	FArcTransporterFragment& Transporter = Context.GetExternalData(TransporterFragmentHandle);
	Transporter.TaskState = EArcTransporterTaskState::PickingUp;
	Transporter.TargetBuildingHandle = InstanceData.SupplyBuildingHandle;
	Transporter.TargetItemDefinition = InstanceData.ItemDefinition;
	Transporter.TargetQuantity = InstanceData.Quantity;

	return EStateTreeRunStatus::Succeeded;
}
