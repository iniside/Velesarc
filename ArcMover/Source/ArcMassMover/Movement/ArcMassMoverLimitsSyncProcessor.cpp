// Copyright Lukasz Baran. All Rights Reserved.

#include "Movement/ArcMassMoverLimitsSyncProcessor.h"

#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "DefaultMovementSet/NavMoverComponent.h"
#include "GameFramework/Actor.h"
#include "Mass/ExecutionContext.h"
#include "MassCommonTypes.h"
#include "MassMovementFragments.h"
#include "MassNavigationFragments.h"
#include "MoverComponent.h"
#include "MoverMassTranslators.h"
#include "Mover/ArcMovementModifiers.h"
#include "Mover/ArcMoverTypes.h"
#include "Mover/ArcWalkGait.h"
#include "Movement/ArcMassMoverMovementFragments.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcMassMoverLimitsSyncProcessor)

UArcMassMoverLimitsSyncProcessor::UArcMassMoverLimitsSyncProcessor()
{
	ExecutionFlags = int32(EProcessorExecutionFlags::AllNetModes);
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::SyncWorldToMass;
	ExecutionOrder.ExecuteBefore.Add(UE::Mass::ProcessorGroupNames::Tasks);
	ExecutionOrder.ExecuteBefore.Add(UE::Mass::ProcessorGroupNames::Avoidance);
	bRequiresGameThreadExecution = true;
}

void UArcMassMoverLimitsSyncProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FArcMassMovementLimitsFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FNavMoverComponentWrapperFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddConstSharedRequirement<FMassMovementParameters>(EMassFragmentPresence::All);
	EntityQuery.AddConstSharedRequirement<FArcMassBrakingParameters>(EMassFragmentPresence::All);
}

void UArcMassMoverLimitsSyncProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	EntityQuery.ForEachEntityChunk(Context, [](FMassExecutionContext& Context)
	{
		const TArrayView<FArcMassMovementLimitsFragment> LimitsList = Context.GetMutableFragmentView<FArcMassMovementLimitsFragment>();
		const TConstArrayView<FNavMoverComponentWrapperFragment> NavMoverList = Context.GetFragmentView<FNavMoverComponentWrapperFragment>();
		const TConstArrayView<FMassMoveTargetFragment> MoveTargetList = Context.GetFragmentView<FMassMoveTargetFragment>();
		const FMassMovementParameters& MovementParams = Context.GetConstSharedFragment<FMassMovementParameters>();
		const FArcMassBrakingParameters& BrakingParams = Context.GetConstSharedFragment<FArcMassBrakingParameters>();

		for (FMassExecutionContext::FEntityIterator EntityIt = Context.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			FArcMassMovementLimitsFragment& Limits = LimitsList[EntityIt];

			const UMoverComponent* Mover = nullptr;
			if (const UNavMoverComponent* NavMover = NavMoverList[EntityIt].Component.Get())
			{
				if (const AActor* Owner = NavMover->GetOwner())
				{
					Mover = Owner->FindComponentByClass<UMoverComponent>();
				}
			}

			const IArcWalkGaitProvider* Provider = Mover ? ArcMover::FindWalkGaitProvider(*Mover) : nullptr;
			if (!Provider)
			{
				Limits.MaxSpeed = MovementParams.MaxSpeed;
				Limits.MaxAcceleration = MovementParams.MaxAcceleration;
				Limits.SpeedScale = 1.f;
				Limits.BrakingDeceleration = BrakingParams.BrakingDeceleration;
				Limits.BrakingLag = BrakingParams.BrakingLag;
				continue;
			}

			const float SpeedScale = ArcMover::GetActiveSpeedScale(*Mover);
			const float LegacyMaxSpeed = Provider->GetBaseMaxSpeed() * SpeedScale;
			const UCharacterMoverComponent* CharacterMover = Cast<UCharacterMoverComponent>(Mover);
			const bool bCrouching = CharacterMover && CharacterMover->IsCrouching();
			const float CurrentSpeedXY = static_cast<float>(Mover->GetVelocity().Size2D());
			const float DesiredSpeed = MoveTargetList[EntityIt].DesiredSpeed.Get() * SpeedScale;

			const FArcWalkGaitSettings GaitSettings = Provider->GetGaitSettings();
			const EArcMoverGaitType Gait = ArcWalkGait::GaitForSpeed(GaitSettings, DesiredSpeed, bCrouching, LegacyMaxSpeed);
			const FArcWalkLimits WalkLimits = ArcWalkGait::ResolveWalkLimits(GaitSettings, Gait, bCrouching, true, LegacyMaxSpeed
				, Provider->GetDecelerationSmoothingTime(), CurrentSpeedXY);

			Limits.MaxSpeed = ArcWalkGait::MaxAvailableSpeed(GaitSettings, bCrouching, LegacyMaxSpeed);
			Limits.MaxAcceleration = WalkLimits.Acceleration;
			Limits.SpeedScale = SpeedScale;
			Limits.BrakingDeceleration = WalkLimits.Deceleration;
			Limits.BrakingLag = Provider->GetBrakingLag();
		}
	});
}
