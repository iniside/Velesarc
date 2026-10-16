// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "MassEntityTraitBase.h"
#include "MassMovementFragments.h"
#include "Steering/MassSteeringFragments.h"
#include "Avoidance/MassAvoidanceFragments.h"
#include "Movement/ArcMassMoverMovementFragments.h"

#include "ArcMassMoverMovementTrait.generated.h"

UCLASS(meta = (DisplayName = "Arc Mover Movement"))
class ARCMASSGAMEPLAYFRAMEWORK_API UArcMassMoverMovementTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

protected:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;

	UPROPERTY(Category = "Movement", EditAnywhere)
	FMassMovementParameters Movement;

	UPROPERTY(Category = "Movement", EditAnywhere)
	FArcMassBrakingParameters Braking;

	UPROPERTY(Category = "Steering", EditAnywhere, meta = (EditInline))
	FMassMovingSteeringParameters MovingSteering;

	UPROPERTY(Category = "Steering", EditAnywhere, meta = (EditInline))
	FMassStandingSteeringParameters StandingSteering;

	UPROPERTY(Category = "Avoidance", EditAnywhere)
	FMassMovingAvoidanceParameters MovingAvoidance;

	UPROPERTY(Category = "Avoidance", EditAnywhere)
	FMassStandingAvoidanceParameters StandingAvoidance;
};
