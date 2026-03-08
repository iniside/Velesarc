// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Targeting/ArcAoETypes.h"
#include "Items/Fragments/ArcItemFragment.h"

#include "ArcItemFragment_TargetingShapeConfig.generated.h"

USTRUCT(BlueprintType, meta = (DisplayName = "Targeting Shape Config", Category = "Gameplay Ability"))
struct ARCCORE_API FArcItemFragment_TargetingShapeConfig : public FArcItemFragment
{
	GENERATED_BODY()

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(const UArcItemDefinition* ItemDefinition, class FDataValidationContext& Context) const override;
#endif

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EArcAoEShape Shape = EArcAoEShape::Sphere;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Box")
	EArcAoEBoxAlignment BoxAlignment = EArcAoEBoxAlignment::LongEdgeFacingSource;
};
