// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UtilityAI/ArcUtilityConsideration.h"
#include "ArcStrategyConsiderations.generated.h"

// ============================================================================
// Settlement-Level Considerations
// ============================================================================

/**
 * Security proxy based on workforce size.
 * Small settlements are less secure. Returns inverse proportion to total workforce.
 */
USTRUCT(BlueprintType, DisplayName = "Settlement Security")
struct ARCECONOMY_API FArcConsideration_SettlementSecurity : public FArcUtilityConsideration
{
	GENERATED_BODY()

	virtual float Score(const FArcUtilityTarget& Target, const FArcUtilityContext& Context) const override;
};

/**
 * Storage utilization as prosperity proxy.
 * Higher storage ratio = more prosperous.
 */
USTRUCT(BlueprintType, DisplayName = "Settlement Prosperity")
struct ARCECONOMY_API FArcConsideration_SettlementProsperity : public FArcUtilityConsideration
{
	GENERATED_BODY()

	virtual float Score(const FArcUtilityTarget& Target, const FArcUtilityContext& Context) const override;
};

/**
 * Inverse of storage fullness — room to grow.
 * Low storage utilization = high growth potential.
 */
USTRUCT(BlueprintType, DisplayName = "Settlement Growth")
struct ARCECONOMY_API FArcConsideration_SettlementGrowth : public FArcUtilityConsideration
{
	GENERATED_BODY()

	virtual float Score(const FArcUtilityTarget& Target, const FArcUtilityContext& Context) const override;
};

/**
 * Resource need based on market price table demand.
 * Looks up a specific resource tag in the settlement's PriceTable.
 */
USTRUCT(BlueprintType, DisplayName = "Resource Need")
struct ARCECONOMY_API FArcConsideration_ResourceNeed : public FArcUtilityConsideration
{
	GENERATED_BODY()

	/** Gameplay tag identifying the resource to evaluate. */
	UPROPERTY(EditAnywhere, Category = "Consideration")
	FGameplayTag ResourceTag;

	virtual float Score(const FArcUtilityTarget& Target, const FArcUtilityContext& Context) const override;
};

/**
 * Idle worker ratio as availability measure.
 * High idle count relative to total = workers available.
 */
USTRUCT(BlueprintType, DisplayName = "Workforce Availability")
struct ARCECONOMY_API FArcConsideration_WorkforceAvailability : public FArcUtilityConsideration
{
	GENERATED_BODY()

	virtual float Score(const FArcUtilityTarget& Target, const FArcUtilityContext& Context) const override;
};

/**
 * Military strength proxy using total workforce.
 * Larger workforce = more potential military. Normalized against a configurable max.
 */
USTRUCT(BlueprintType, DisplayName = "Military Strength")
struct ARCECONOMY_API FArcConsideration_MilitaryStrength : public FArcUtilityConsideration
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Consideration", meta = (ClampMin = 1))
	int32 MaxExpectedWorkforce = 20;

	virtual float Score(const FArcUtilityTarget& Target, const FArcUtilityContext& Context) const override;
};

/**
 * Diplomatic relation toward a target entity.
 * Follows settlement -> faction -> diplomacy chain. Returns stance / 4.0 (Allied = 1.0).
 */
USTRUCT(BlueprintType, DisplayName = "Diplomatic Relation")
struct ARCECONOMY_API FArcConsideration_DiplomaticRelation : public FArcUtilityConsideration
{
	GENERATED_BODY()

	virtual float Score(const FArcUtilityTarget& Target, const FArcUtilityContext& Context) const override;
};

/**
 * Threat proximity placeholder. Requires ArcKnowledge integration.
 * Currently returns 0.0 (no threat detected).
 */
USTRUCT(BlueprintType, DisplayName = "Threat Proximity")
struct ARCECONOMY_API FArcConsideration_ThreatProximity : public FArcUtilityConsideration
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Consideration", meta = (ClampMin = 1.0))
	float QueryRadius = 10000.0f;

	virtual float Score(const FArcUtilityTarget& Target, const FArcUtilityContext& Context) const override;
};

// ============================================================================
// Faction-Level Considerations
// ============================================================================

/**
 * Average storage utilization across all owned settlements.
 */
USTRUCT(BlueprintType, DisplayName = "Faction Wealth")
struct ARCECONOMY_API FArcConsideration_FactionWealth : public FArcUtilityConsideration
{
	GENERATED_BODY()

	virtual float Score(const FArcUtilityTarget& Target, const FArcUtilityContext& Context) const override;
};

/**
 * Settlement count relative to expected maximum. More settlements = more dominant.
 */
USTRUCT(BlueprintType, DisplayName = "Faction Dominance")
struct ARCECONOMY_API FArcConsideration_FactionDominance : public FArcUtilityConsideration
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Consideration", meta = (ClampMin = 1))
	int32 MaxExpectedSettlements = 10;

	virtual float Score(const FArcUtilityTarget& Target, const FArcUtilityContext& Context) const override;
};

/**
 * Stability based on hostile diplomatic relations.
 * More wars = less stable.
 */
USTRUCT(BlueprintType, DisplayName = "Faction Stability")
struct ARCECONOMY_API FArcConsideration_FactionStability : public FArcUtilityConsideration
{
	GENERATED_BODY()

	virtual float Score(const FArcUtilityTarget& Target, const FArcUtilityContext& Context) const override;
};

/**
 * Expansion opportunity placeholder. Requires ArcKnowledge query for unclaimed resources.
 * Currently returns 0.5 (neutral).
 */
USTRUCT(BlueprintType, DisplayName = "Faction Expansion")
struct ARCECONOMY_API FArcConsideration_FactionExpansion : public FArcUtilityConsideration
{
	GENERATED_BODY()

	virtual float Score(const FArcUtilityTarget& Target, const FArcUtilityContext& Context) const override;
};

/**
 * Relative strength vs target faction based on settlement count ratio.
 * Returns 0.5 when equal.
 */
USTRUCT(BlueprintType, DisplayName = "Relative Strength")
struct ARCECONOMY_API FArcConsideration_RelativeStrength : public FArcUtilityConsideration
{
	GENERATED_BODY()

	virtual float Score(const FArcUtilityTarget& Target, const FArcUtilityContext& Context) const override;
};
