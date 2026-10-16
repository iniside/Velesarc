// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Targeting/ArcAoETypes.h"
#include "Items/Fragments/ArcItemFragment.h"

#include "ArcItemFragment_TargetingShapeConfig.generated.h"

USTRUCT(BlueprintType, meta = (DisplayName = "Targeting Shape Config", Category = "Gameplay Ability", ToolTip = "Configures the geometric shape used for area-of-effect targeting. Selects between shapes (Sphere, Box, etc.) and provides shape-specific alignment settings. Pair with FArcScalableFloatItemFragment_TargetingShape to define level-scaled dimensions."))
struct ARCABILITYKIT_API FArcItemFragment_TargetingShapeConfig : public FArcItemFragment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EArcAoEShape Shape = EArcAoEShape::Sphere;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Box")
	EArcAoEBoxAlignment BoxAlignment = EArcAoEBoxAlignment::LongEdgeFacingSource;
};
