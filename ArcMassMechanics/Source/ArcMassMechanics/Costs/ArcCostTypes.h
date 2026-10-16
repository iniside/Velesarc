// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/ArcMassAbilityCost.h"
#include "Attributes/ArcAttribute.h"
#include "StructUtils/InstancedStruct.h"
#include "ArcCostTypes.generated.h"

namespace ArcCosts
{
	ARCMASSMECHANICS_API bool CheckResource(const FArcMassAbilityCostContext& Context, const FArcAttributeRef& Resource, const FArcAttributeRef& Meta, float Required, FArcCostCheckResult* OutResult);
	ARCMASSMECHANICS_API void PayIntoMeta(const FArcMassAbilityCostContext& Context, const FArcAttributeRef& Meta, float Amount);
}

USTRUCT(BlueprintType, meta = (DisplayName = "Cost Set", ToolTip = "Checks every cost before any is applied and reports the first failing one. Applies all costs in order."))
struct ARCMASSMECHANICS_API FArcCost_Set : public FArcMassAbilityCost
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Cost", meta = (BaseStruct = "/Script/ArcMassAbilities.ArcMassAbilityCost", ExcludeBaseStruct))
	TArray<FInstancedStruct> Costs;

	virtual bool CheckCost(const FArcMassAbilityCostContext& Context, FArcCostCheckResult* OutResult = nullptr) const override;
	virtual void ApplyCost(const FArcMassAbilityCostContext& Context) const override;
};
