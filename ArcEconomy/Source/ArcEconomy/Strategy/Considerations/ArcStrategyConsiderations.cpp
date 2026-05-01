// Copyright Lukasz Baran. All Rights Reserved.

#include "Strategy/Considerations/ArcStrategyConsiderations.h"
#include "MassEntitySubsystem.h"
#include "MassEntityView.h"
#include "Mass/ArcEconomyFragments.h"
#include "Faction/ArcFactionFragments.h"

// ============================================================================
// Helpers
// ============================================================================

namespace ArcStrategyConsiderationsInternal
{
	int32 ComputeTotalWorkforce(const FArcSettlementWorkforceFragment& Workforce)
	{
		return Workforce.WorkerCount
			+ Workforce.TransporterCount
			+ Workforce.GathererCount
			+ Workforce.CaravanCount
			+ Workforce.IdleCount;
	}

	float ComputeStorageRatio(const FArcSettlementMarketFragment& Market)
	{
		if (Market.TotalStorageCap <= 0)
		{
			return 0.0f;
		}
		return FMath::Clamp(
			static_cast<float>(Market.CurrentTotalStorage) / static_cast<float>(Market.TotalStorageCap),
			0.0f, 1.0f);
	}
}

// ============================================================================
// Settlement-Level Considerations
// ============================================================================

float FArcConsideration_SettlementSecurity::Score(
	const FArcUtilityTarget& Target, const FArcUtilityContext& Context) const
{
	if (!Context.EntityManager || !Context.QuerierEntity.IsSet())
	{
		return 0.5f;
	}

	if (!Context.EntityManager->IsEntityValid(Context.QuerierEntity))
	{
		return 0.5f;
	}

	const FMassEntityView EntityView(*Context.EntityManager, Context.QuerierEntity);
	const FArcSettlementWorkforceFragment* Workforce =
		EntityView.GetFragmentDataPtr<FArcSettlementWorkforceFragment>();

	if (!Workforce)
	{
		return 0.5f;
	}

	const int32 Total = ArcStrategyConsiderationsInternal::ComputeTotalWorkforce(*Workforce);
	if (Total == 0)
	{
		return 0.0f;
	}

	// Larger workforce = more secure. Normalize against a reasonable max of 20.
	// All current roles are non-combat, so security scales with population.
	constexpr float MaxSecurityWorkforce = 20.0f;
	return FMath::Clamp(static_cast<float>(Total) / MaxSecurityWorkforce, 0.0f, 1.0f);
}

float FArcConsideration_SettlementProsperity::Score(
	const FArcUtilityTarget& Target, const FArcUtilityContext& Context) const
{
	if (!Context.EntityManager || !Context.QuerierEntity.IsSet())
	{
		return 0.5f;
	}

	if (!Context.EntityManager->IsEntityValid(Context.QuerierEntity))
	{
		return 0.5f;
	}

	const FMassEntityView EntityView(*Context.EntityManager, Context.QuerierEntity);
	const FArcSettlementMarketFragment* Market =
		EntityView.GetFragmentDataPtr<FArcSettlementMarketFragment>();

	if (!Market)
	{
		return 0.5f;
	}

	return ArcStrategyConsiderationsInternal::ComputeStorageRatio(*Market);
}

float FArcConsideration_SettlementGrowth::Score(
	const FArcUtilityTarget& Target, const FArcUtilityContext& Context) const
{
	if (!Context.EntityManager || !Context.QuerierEntity.IsSet())
	{
		return 0.5f;
	}

	if (!Context.EntityManager->IsEntityValid(Context.QuerierEntity))
	{
		return 0.5f;
	}

	const FMassEntityView EntityView(*Context.EntityManager, Context.QuerierEntity);
	const FArcSettlementMarketFragment* Market =
		EntityView.GetFragmentDataPtr<FArcSettlementMarketFragment>();

	if (!Market)
	{
		return 0.5f;
	}

	return 1.0f - ArcStrategyConsiderationsInternal::ComputeStorageRatio(*Market);
}

float FArcConsideration_ResourceNeed::Score(
	const FArcUtilityTarget& Target, const FArcUtilityContext& Context) const
{
	if (!Context.EntityManager || !Context.QuerierEntity.IsSet())
	{
		return 0.5f;
	}

	if (!Context.EntityManager->IsEntityValid(Context.QuerierEntity))
	{
		return 0.5f;
	}

	if (!ResourceTag.IsValid())
	{
		return 0.5f;
	}

	const FMassEntityView EntityView(*Context.EntityManager, Context.QuerierEntity);
	const FArcSettlementMarketFragment* Market =
		EntityView.GetFragmentDataPtr<FArcSettlementMarketFragment>();

	if (!Market)
	{
		return 0.5f;
	}

	// Look through PriceTable for entries where demand exceeds supply.
	// Since PriceTable is keyed by UArcItemDefinition* (not tag), we iterate
	// and match by checking if the item's tags contain ResourceTag.
	// TODO: Refine once resource-to-tag mapping is formalized. For now, use
	// aggregate demand/supply ratio across all entries as a fallback.
	float TotalDemand = 0.0f;
	float TotalSupply = 0.0f;

	for (const TPair<TObjectPtr<UArcItemDefinition>, FArcResourceMarketData>& Entry : Market->PriceTable)
	{
		TotalDemand += Entry.Value.DemandCounter;
		TotalSupply += Entry.Value.SupplyCounter;
	}

	const float TotalActivity = TotalDemand + TotalSupply;
	if (TotalActivity <= 0.0f)
	{
		return 0.5f;
	}

	// Higher demand relative to supply = higher need score.
	return FMath::Clamp(TotalDemand / TotalActivity, 0.0f, 1.0f);
}

float FArcConsideration_WorkforceAvailability::Score(
	const FArcUtilityTarget& Target, const FArcUtilityContext& Context) const
{
	if (!Context.EntityManager || !Context.QuerierEntity.IsSet())
	{
		return 0.5f;
	}

	if (!Context.EntityManager->IsEntityValid(Context.QuerierEntity))
	{
		return 0.5f;
	}

	const FMassEntityView EntityView(*Context.EntityManager, Context.QuerierEntity);
	const FArcSettlementWorkforceFragment* Workforce =
		EntityView.GetFragmentDataPtr<FArcSettlementWorkforceFragment>();

	if (!Workforce)
	{
		return 0.5f;
	}

	const int32 Total = ArcStrategyConsiderationsInternal::ComputeTotalWorkforce(*Workforce);
	if (Total == 0)
	{
		return 0.0f;
	}

	return FMath::Clamp(static_cast<float>(Workforce->IdleCount) / static_cast<float>(Total), 0.0f, 1.0f);
}

float FArcConsideration_MilitaryStrength::Score(
	const FArcUtilityTarget& Target, const FArcUtilityContext& Context) const
{
	if (!Context.EntityManager || !Context.QuerierEntity.IsSet())
	{
		return 0.5f;
	}

	if (!Context.EntityManager->IsEntityValid(Context.QuerierEntity))
	{
		return 0.5f;
	}

	const FMassEntityView EntityView(*Context.EntityManager, Context.QuerierEntity);
	const FArcSettlementWorkforceFragment* Workforce =
		EntityView.GetFragmentDataPtr<FArcSettlementWorkforceFragment>();

	if (!Workforce)
	{
		return 0.5f;
	}

	const int32 Total = ArcStrategyConsiderationsInternal::ComputeTotalWorkforce(*Workforce);
	return FMath::Clamp(
		static_cast<float>(Total) / static_cast<float>(FMath::Max(MaxExpectedWorkforce, 1)),
		0.0f, 1.0f);
}

float FArcConsideration_DiplomaticRelation::Score(
	const FArcUtilityTarget& Target, const FArcUtilityContext& Context) const
{
	if (!Context.EntityManager || !Context.QuerierEntity.IsSet())
	{
		return 0.5f;
	}

	if (!Context.EntityManager->IsEntityValid(Context.QuerierEntity))
	{
		return 0.5f;
	}

	if (!Target.EntityHandle.IsSet() || !Context.EntityManager->IsEntityValid(Target.EntityHandle))
	{
		return 0.5f;
	}

	// Determine the querier's faction handle.
	// If querier is a settlement, follow FArcSettlementFactionFragment to get the faction.
	// If querier is already a faction entity, use it directly.
	FMassEntityHandle QuerierFaction;

	const FMassEntityView QuerierView(*Context.EntityManager, Context.QuerierEntity);
	const FArcSettlementFactionFragment* SettlementFaction =
		QuerierView.GetFragmentDataPtr<FArcSettlementFactionFragment>();

	if (SettlementFaction && SettlementFaction->OwningFaction.IsSet())
	{
		QuerierFaction = SettlementFaction->OwningFaction;
	}
	else
	{
		// Assume querier is a faction entity itself.
		QuerierFaction = Context.QuerierEntity;
	}

	if (!Context.EntityManager->IsEntityValid(QuerierFaction))
	{
		return 0.5f;
	}

	// Determine the target's faction handle similarly.
	FMassEntityHandle TargetFaction;

	const FMassEntityView TargetView(*Context.EntityManager, Target.EntityHandle);
	const FArcSettlementFactionFragment* TargetSettlementFaction =
		TargetView.GetFragmentDataPtr<FArcSettlementFactionFragment>();

	if (TargetSettlementFaction && TargetSettlementFaction->OwningFaction.IsSet())
	{
		TargetFaction = TargetSettlementFaction->OwningFaction;
	}
	else
	{
		TargetFaction = Target.EntityHandle;
	}

	// Read diplomacy from the querier's faction.
	const FMassEntityView FactionView(*Context.EntityManager, QuerierFaction);
	const FArcFactionDiplomacyFragment* Diplomacy =
		FactionView.GetFragmentDataPtr<FArcFactionDiplomacyFragment>();

	if (!Diplomacy)
	{
		return 0.5f;
	}

	const EArcDiplomaticStance Stance = Diplomacy->GetStanceToward(TargetFaction);

	// Allied = 4, map to 0-1 range.
	return FMath::Clamp(static_cast<float>(Stance) / 4.0f, 0.0f, 1.0f);
}

float FArcConsideration_ThreatProximity::Score(
	const FArcUtilityTarget& Target, const FArcUtilityContext& Context) const
{
	// TODO: Requires ArcKnowledge spatial query integration.
	// When wired, query for hostile entities within QueryRadius of the settlement location
	// and return a normalized threat score.
	return 0.0f;
}

// ============================================================================
// Faction-Level Considerations
// ============================================================================

float FArcConsideration_FactionWealth::Score(
	const FArcUtilityTarget& Target, const FArcUtilityContext& Context) const
{
	if (!Context.EntityManager || !Context.QuerierEntity.IsSet())
	{
		return 0.5f;
	}

	if (!Context.EntityManager->IsEntityValid(Context.QuerierEntity))
	{
		return 0.5f;
	}

	const FMassEntityView FactionView(*Context.EntityManager, Context.QuerierEntity);
	const FArcFactionSettlementsFragment* Settlements =
		FactionView.GetFragmentDataPtr<FArcFactionSettlementsFragment>();

	if (!Settlements || Settlements->OwnedSettlements.Num() == 0)
	{
		return 0.0f;
	}

	float TotalStorageRatio = 0.0f;
	int32 ValidCount = 0;

	for (const FMassEntityHandle& SettlementHandle : Settlements->OwnedSettlements)
	{
		if (!SettlementHandle.IsSet() || !Context.EntityManager->IsEntityValid(SettlementHandle))
		{
			continue;
		}

		const FMassEntityView SettlementView(*Context.EntityManager, SettlementHandle);
		const FArcSettlementMarketFragment* Market =
			SettlementView.GetFragmentDataPtr<FArcSettlementMarketFragment>();

		if (Market)
		{
			TotalStorageRatio += ArcStrategyConsiderationsInternal::ComputeStorageRatio(*Market);
			++ValidCount;
		}
	}

	if (ValidCount == 0)
	{
		return 0.0f;
	}

	return FMath::Clamp(TotalStorageRatio / static_cast<float>(ValidCount), 0.0f, 1.0f);
}

float FArcConsideration_FactionDominance::Score(
	const FArcUtilityTarget& Target, const FArcUtilityContext& Context) const
{
	if (!Context.EntityManager || !Context.QuerierEntity.IsSet())
	{
		return 0.5f;
	}

	if (!Context.EntityManager->IsEntityValid(Context.QuerierEntity))
	{
		return 0.5f;
	}

	const FMassEntityView FactionView(*Context.EntityManager, Context.QuerierEntity);
	const FArcFactionSettlementsFragment* Settlements =
		FactionView.GetFragmentDataPtr<FArcFactionSettlementsFragment>();

	if (!Settlements)
	{
		return 0.0f;
	}

	return FMath::Clamp(
		static_cast<float>(Settlements->OwnedSettlements.Num()) / static_cast<float>(FMath::Max(MaxExpectedSettlements, 1)),
		0.0f, 1.0f);
}

float FArcConsideration_FactionStability::Score(
	const FArcUtilityTarget& Target, const FArcUtilityContext& Context) const
{
	if (!Context.EntityManager || !Context.QuerierEntity.IsSet())
	{
		return 0.5f;
	}

	if (!Context.EntityManager->IsEntityValid(Context.QuerierEntity))
	{
		return 0.5f;
	}

	const FMassEntityView FactionView(*Context.EntityManager, Context.QuerierEntity);
	const FArcFactionDiplomacyFragment* Diplomacy =
		FactionView.GetFragmentDataPtr<FArcFactionDiplomacyFragment>();

	if (!Diplomacy)
	{
		return 1.0f; // No diplomacy data = no wars = stable.
	}

	int32 WarCount = 0;
	for (const TPair<FMassEntityHandle, EArcDiplomaticStance>& Entry : Diplomacy->Stances)
	{
		if (Entry.Value == EArcDiplomaticStance::Hostile)
		{
			++WarCount;
		}
	}

	const float WarPenalty = static_cast<float>(WarCount) / 3.0f;
	return 1.0f - FMath::Clamp(WarPenalty, 0.0f, 1.0f);
}

float FArcConsideration_FactionExpansion::Score(
	const FArcUtilityTarget& Target, const FArcUtilityContext& Context) const
{
	// TODO: Requires ArcKnowledge query for unclaimed resource nodes / territory.
	// When wired, evaluate expansion opportunities and return normalized score.
	return 0.5f;
}

float FArcConsideration_RelativeStrength::Score(
	const FArcUtilityTarget& Target, const FArcUtilityContext& Context) const
{
	if (!Context.EntityManager || !Context.QuerierEntity.IsSet())
	{
		return 0.5f;
	}

	if (!Context.EntityManager->IsEntityValid(Context.QuerierEntity))
	{
		return 0.5f;
	}

	if (!Target.EntityHandle.IsSet() || !Context.EntityManager->IsEntityValid(Target.EntityHandle))
	{
		return 0.5f;
	}

	// Get own settlement count.
	const FMassEntityView OwnView(*Context.EntityManager, Context.QuerierEntity);
	const FArcFactionSettlementsFragment* OwnSettlements =
		OwnView.GetFragmentDataPtr<FArcFactionSettlementsFragment>();

	const int32 OwnCount = OwnSettlements ? OwnSettlements->OwnedSettlements.Num() : 0;

	// Determine target faction. Target may be a settlement or a faction entity.
	FMassEntityHandle TargetFaction;

	const FMassEntityView TargetView(*Context.EntityManager, Target.EntityHandle);
	const FArcSettlementFactionFragment* TargetSettlementFaction =
		TargetView.GetFragmentDataPtr<FArcSettlementFactionFragment>();

	if (TargetSettlementFaction && TargetSettlementFaction->OwningFaction.IsSet())
	{
		TargetFaction = TargetSettlementFaction->OwningFaction;
	}
	else
	{
		TargetFaction = Target.EntityHandle;
	}

	int32 TargetCount = 0;
	if (Context.EntityManager->IsEntityValid(TargetFaction))
	{
		const FMassEntityView TargetFactionView(*Context.EntityManager, TargetFaction);
		const FArcFactionSettlementsFragment* TargetSettlements =
			TargetFactionView.GetFragmentDataPtr<FArcFactionSettlementsFragment>();

		TargetCount = TargetSettlements ? TargetSettlements->OwnedSettlements.Num() : 0;
	}

	const int32 TotalCount = OwnCount + TargetCount;
	if (TotalCount == 0)
	{
		return 0.5f;
	}

	return FMath::Clamp(static_cast<float>(OwnCount) / static_cast<float>(TotalCount), 0.0f, 1.0f);
}
