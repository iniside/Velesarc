// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "ArcScalableFloat.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemScalableFloat.h"
#include "Items/Fragments/ArcItemFragment.h"

#include "ArcScalableFloatItemFragment_TargetingShape.generated.h"

USTRUCT(BlueprintType, meta = (DisplayName = "Targeting Shape Stats", Category = "Targeting", ToolTip = "Level-scalable numeric dimensions for area-of-effect targeting shapes. Provides Radius (for spheres), and Length/Width/Height (for boxes). Values scale with item level via FScalableFloat curves. Pair with FArcItemFragment_TargetingShapeConfig to select the shape type."))
struct ARCCORE_API FArcScalableFloatItemFragment_TargetingShape : public FArcScalableFloatItemFragment
{
	GENERATED_BODY()

public:
	virtual ~FArcScalableFloatItemFragment_TargetingShape() override = default;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(const UArcItemDefinition* ItemDefinition, class FDataValidationContext& Context) const override;
#endif

protected:
	UPROPERTY(EditAnywhere, Category = "Sphere", meta = (ForceUnits = "cm", EnableCategories))
	FArcScalableFloat Radius = 200.f;
	ARC_ITEMSCALABLEFLOAT_STRUCT_GETTER(FArcScalableFloatItemFragment_TargetingShape, Radius);

	UPROPERTY(EditAnywhere, Category = "Box", meta = (ForceUnits = "cm", EnableCategories))
	FArcScalableFloat Length = 500.f;
	ARC_ITEMSCALABLEFLOAT_STRUCT_GETTER(FArcScalableFloatItemFragment_TargetingShape, Length);

	UPROPERTY(EditAnywhere, Category = "Box", meta = (ForceUnits = "cm", EnableCategories))
	FArcScalableFloat Width = 200.f;
	ARC_ITEMSCALABLEFLOAT_STRUCT_GETTER(FArcScalableFloatItemFragment_TargetingShape, Width);

	UPROPERTY(EditAnywhere, Category = "Box", meta = (ForceUnits = "cm", EnableCategories))
	FArcScalableFloat Height = 200.f;
	ARC_ITEMSCALABLEFLOAT_STRUCT_GETTER(FArcScalableFloatItemFragment_TargetingShape, Height);
};
