// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "ArcScalableFloat.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemScalableFloat.h"
#include "Items/Fragments/ArcItemFragment.h"

#include "ArcScalableFloatItemFragment_TargetingTrace.generated.h"

USTRUCT(BlueprintType, meta = (DisplayName = "Targeting Trace Stats", Category = "Targeting"))
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
