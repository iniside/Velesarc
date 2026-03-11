// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "ArcScalableFloat.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemScalableFloat.h"
#include "Items/Fragments/ArcItemFragment.h"

#include "ArcScalableFloatItemFragment_TargetingTrace.generated.h"

USTRUCT(BlueprintType, meta = (DisplayName = "Targeting Trace Stats", Category = "Targeting", ToolTip = "Level-scalable numeric parameters for line-trace targeting. Provides Distance (max trace range in cm). Values scale with item level via FScalableFloat curves. Use for ranged weapons, ray-cast abilities, or any trace-based targeting system."))
struct ARCCORE_API FArcScalableFloatItemFragment_TargetingTrace : public FArcScalableFloatItemFragment
{
	GENERATED_BODY()

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(const UArcItemDefinition* ItemDefinition, class FDataValidationContext& Context) const override;
#endif

protected:
	UPROPERTY(EditAnywhere, Category = "Targeting", meta = (ForceUnits = "cm", EnableCategories))
	FArcScalableFloat Distance = 10000.f;
	ARC_ITEMSCALABLEFLOAT_STRUCT_GETTER(FArcScalableFloatItemFragment_TargetingTrace, Distance);
};
