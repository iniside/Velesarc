// Copyright Lukasz Baran. All Rights Reserved.

#include "Movement/ArcMassMoverMovementTrait.h"

#include "Engine/World.h"
#include "Mass/EntityUtils.h"
#include "MassCommonFragments.h"
#include "MassEntityTemplateRegistry.h"
#include "MassNavigationFragments.h"
#include "MassWorldUtils.h"
#include "Movement/ArcMassMoverMovementFragments.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcMassMoverMovementTrait)

void UArcMassMoverMovementTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);

	BuildContext.RequireFragment<FAgentRadiusFragment>();
	BuildContext.RequireFragment<FTransformFragment>();

	BuildContext.AddFragment<FMassVelocityFragment>();
	BuildContext.AddFragment<FArcMassForceFragment>();
	BuildContext.AddFragment<FMassDesiredMovementFragment>();

	const FMassMovementParameters MovementValidated = Movement.GetValidated();
	BuildContext.AddConstSharedFragment(EntityManager.GetOrCreateConstSharedFragment(MovementValidated));

	FArcMassMovementLimitsFragment& Limits = BuildContext.AddFragment_GetRef<FArcMassMovementLimitsFragment>();
	Limits.MaxSpeed = MovementValidated.MaxSpeed;
	Limits.MaxAcceleration = MovementValidated.MaxAcceleration;
	Limits.SpeedScale = 1.f;
	Limits.BrakingDeceleration = Braking.BrakingDeceleration;
	Limits.BrakingLag = Braking.BrakingLag;

	BuildContext.AddConstSharedFragment(EntityManager.GetOrCreateConstSharedFragment(Braking));
	BuildContext.AddFragment<FArcMassStandBrakeFragment>();

	if (Movement.bIsCodeDrivenMovement)
	{
		BuildContext.AddTag<FMassCodeDrivenMovementTag>();
	}

	BuildContext.AddFragment<FMassMoveTargetFragment>();
	BuildContext.AddFragment<FMassSteeringFragment>();
	BuildContext.AddFragment<FMassStandingSteeringFragment>();
	BuildContext.AddFragment<FMassGhostLocationFragment>();

	BuildContext.AddConstSharedFragment(EntityManager.GetOrCreateConstSharedFragment(MovingSteering));
	BuildContext.AddConstSharedFragment(EntityManager.GetOrCreateConstSharedFragment(StandingSteering));

	BuildContext.AddFragment<FMassNavigationEdgesFragment>();

	const FMassMovingAvoidanceParameters MovingAvoidanceValidated = MovingAvoidance.GetValidated();
	BuildContext.AddConstSharedFragment(EntityManager.GetOrCreateConstSharedFragment(MovingAvoidanceValidated));

	const FMassStandingAvoidanceParameters StandingAvoidanceValidated = StandingAvoidance.GetValidated();
	BuildContext.AddConstSharedFragment(EntityManager.GetOrCreateConstSharedFragment(StandingAvoidanceValidated));
}
