// Copyright Lukasz Baran. All Rights Reserved.

#include "Movement/ArcMassApplyForceProcessor.h"

#include "Mass/ExecutionContext.h"
#include "MassCommonTypes.h"
#include "MassMovementFragments.h"
#include "MassSimulationLOD.h"
#include "Math/UnrealMathUtility.h"
#include "Movement/ArcMassMoverMovementFragments.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcMassApplyForceProcessor)

DEFINE_LOG_CATEGORY_STATIC(LogArcMassApplyForce, Log, All);

UArcMassApplyForceProcessor::UArcMassApplyForceProcessor()
{
	ExecutionFlags = int32(EProcessorExecutionFlags::AllNetModes);
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::ApplyForces;
	ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::Avoidance);
}

void UArcMassApplyForceProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FMassDesiredMovementFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FArcMassForceFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FArcMassMovementLimitsFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddTagRequirement<FMassOffLODTag>(EMassFragmentPresence::None);

	EntityQuery.AddConstSharedRequirement<FMassMovementParameters>(EMassFragmentPresence::All);
}

void UArcMassApplyForceProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	const float DeltaTime = FMath::Min(0.1f, Context.GetDeltaTimeSeconds());

	EntityQuery.ForEachEntityChunk(Context, [DeltaTime](FMassExecutionContext& Context)
	{
		const TArrayView<FArcMassForceFragment> ForceList = Context.GetMutableFragmentView<FArcMassForceFragment>();
		const TArrayView<FMassDesiredMovementFragment> MovementList = Context.GetMutableFragmentView<FMassDesiredMovementFragment>();
		const TConstArrayView<FArcMassMovementLimitsFragment> LimitsList = Context.GetFragmentView<FArcMassMovementLimitsFragment>();

		const FMassMovementParameters& MovementParams = Context.GetConstSharedFragment<FMassMovementParameters>();

		for (FMassExecutionContext::FEntityIterator EntityIt = Context.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			FArcMassForceFragment& Force = ForceList[EntityIt];
			FMassDesiredMovementFragment& DesiredMovement = MovementList[EntityIt];
			const FArcMassMovementLimitsFragment& Limits = LimitsList[EntityIt];

			const FVector PreviousDesiredVelocity = DesiredMovement.DesiredVelocity;
			const FVector IntegratedDesiredVelocity = PreviousDesiredVelocity + (Force.Value * DeltaTime);

			const FVector::FReal IntegratedSizeSq = IntegratedDesiredVelocity.SizeSquared();
			const bool bSlowing = IntegratedSizeSq < PreviousDesiredVelocity.SizeSquared();
			// Per-entity limits bypass FMassMovementParameters::Update, which clamps MinSpeedAccelerating to the config MaxSpeed (MassMovementFragments.cpp:10).
			const float Threshold = FMath::Min(bSlowing ? MovementParams.MinSpeed : MovementParams.MinSpeedAccelerating, Limits.MaxSpeed);

			if (Threshold > 0.f && IntegratedSizeSq > 0.f && IntegratedSizeSq < FMath::Square(Threshold))
			{
				if (bSlowing)
				{
					DesiredMovement.DesiredVelocity = FVector::ZeroVector;

#if WITH_MASSGAMEPLAY_DEBUG
					const FMassEntityHandle Entity = Context.GetEntity(EntityIt);
					UE_LOGF(LogArcMassApplyForce, Verbose, "MinSpeed snap-down: entity %d:%d  prev=%.2f  integrated=%.2f  min=%.2f",
						Entity.Index, Entity.SerialNumber,
						(float)PreviousDesiredVelocity.Size(),
						(float)IntegratedDesiredVelocity.Size(),
						Threshold);
#endif //WITH_MASSGAMEPLAY_DEBUG
				}
				else
				{
					const FVector::FReal Scale = Threshold / FMath::Sqrt(IntegratedSizeSq);
					DesiredMovement.DesiredVelocity = IntegratedDesiredVelocity * Scale;

#if WITH_MASSGAMEPLAY_DEBUG
					const FMassEntityHandle Entity = Context.GetEntity(EntityIt);
					UE_LOGF(LogArcMassApplyForce, Verbose, "MinSpeed snap-up: entity %d:%d  prev=%.2f  integrated=%.2f  kick=%.2f",
						Entity.Index, Entity.SerialNumber,
						(float)PreviousDesiredVelocity.Size(),
						(float)IntegratedDesiredVelocity.Size(),
						Threshold);
#endif //WITH_MASSGAMEPLAY_DEBUG
				}
			}
			else
			{
				DesiredMovement.DesiredVelocity = IntegratedDesiredVelocity.GetClampedToMaxSize(Limits.MaxSpeed);
			}

			Force.Value = FVector::ZeroVector;
		}
	});
}
