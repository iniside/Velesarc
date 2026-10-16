#pragma once
#include "ArcScalableFloat.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemScalableFloat.h"
#include "Items/Fragments/ArcItemFragment.h"

#include "ArcRPGItemFragment_AbilityStats.generated.h"

USTRUCT()
struct ARCRPGMECHANICS_API FArcRPGItemFragment_AbilityStats : public FArcScalableFloatItemFragment
{
	GENERATED_BODY()

public:
	virtual ~FArcRPGItemFragment_AbilityStats() override = default;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cost", meta = (EnableCategories, NoPreview))
	FArcScalableFloat HealthCost;
	ARC_ITEMSCALABLEFLOAT_STRUCT_GETTER(FArcRPGItemFragment_AbilityStats, HealthCost);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cost", meta = (EnableCategories, NoPreview))
	FArcScalableFloat StaminaCost;
	ARC_ITEMSCALABLEFLOAT_STRUCT_GETTER(FArcRPGItemFragment_AbilityStats, StaminaCost);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cost", meta = (EnableCategories, NoPreview))
	FArcScalableFloat EnergyCost;
	ARC_ITEMSCALABLEFLOAT_STRUCT_GETTER(FArcRPGItemFragment_AbilityStats, EnergyCost);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cost", meta = (EnableCategories, NoPreview))
	FArcScalableFloat EntropyGenerated;
	ARC_ITEMSCALABLEFLOAT_STRUCT_GETTER(FArcRPGItemFragment_AbilityStats, EntropyGenerated);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cost", meta = (EnableCategories, NoPreview, ToolTip = "Entropy the cast consumes: needs Entropy to hold the amount, paid through RemoveEntropy. Flat, no channeling factor. An item with both Entropy Cost and Entropy Generated is a config error."))
	FArcScalableFloat EntropyCost;
	ARC_ITEMSCALABLEFLOAT_STRUCT_GETTER(FArcRPGItemFragment_AbilityStats, EntropyCost);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cost", meta = (EnableCategories, NoPreview))
	FArcScalableFloat CascadeMultiplier = 1.f;
	ARC_ITEMSCALABLEFLOAT_STRUCT_GETTER(FArcRPGItemFragment_AbilityStats, CascadeMultiplier);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cost", meta = (EnableCategories, NoPreview))
	FArcScalableFloat SoftCooldownDuration = 0.f;
	ARC_ITEMSCALABLEFLOAT_STRUCT_GETTER(FArcRPGItemFragment_AbilityStats, SoftCooldownDuration);
};