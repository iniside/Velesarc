// Copyright Lukasz Baran. All Rights Reserved.

#include "Debug/ArcRecoilDebug.h"

#include "ArcRecoilApi.h"
#include "ArcRecoilImpulse.h"
#include "ArcRecoilModule.h"
#include "Camera/PlayerCameraManager.h"
#include "CollisionQueryParams.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Fragments/ArcRecoilFragments.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "MassAgentComponent.h"
#include "MassCommonFragments.h"
#include "Mass/ExecutionContext.h"
#include "MassSubsystemAccess.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcRecoilDebug)

namespace ArcRecoilDebugProcessorPrivate
{
	constexpr float TraceDistance = 10000.0f;
	constexpr float AimLineLength = 200.0f;
	constexpr float OffsetLineLength = 200.0f;
	constexpr float ConeLength = 150.0f;
	constexpr float ReticleSphereRadius = 6.0f;

	FVector TraceAimPoint(const UWorld* World, const FVector& Start, const FVector& Direction, const AActor* IgnoreActor)
	{
		const FVector End = Start + Direction * TraceDistance;

		FHitResult Hit;
		FCollisionQueryParams Params;
		Params.AddIgnoredActor(IgnoreActor);

		if (World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params) && Hit.bBlockingHit)
		{
			return Hit.ImpactPoint;
		}

		return End;
	}

	bool ResolveLocalPlayer(const UWorld* World,
		FMassEntityHandle& OutEntity,
		APawn*& OutPawn,
		FVector& OutCameraLocation,
		FRotator& OutCameraRotation)
	{
		const APlayerController* PlayerController = World->GetFirstPlayerController();
		APawn* Pawn = PlayerController != nullptr ? PlayerController->GetPawn() : nullptr;
		if (Pawn == nullptr || PlayerController->PlayerCameraManager == nullptr)
		{
			return false;
		}

		const UMassAgentComponent* AgentComponent = Pawn->FindComponentByClass<UMassAgentComponent>();
		if (AgentComponent == nullptr)
		{
			return false;
		}

		OutEntity = AgentComponent->GetEntityHandle();
		OutPawn = Pawn;
		PlayerController->PlayerCameraManager->GetCameraViewPoint(OutCameraLocation, OutCameraRotation);

		return OutEntity.IsSet();
	}
}

UArcRecoilDebugProcessor::UArcRecoilDebugProcessor()
	: EntityQuery{*this}
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
	ProcessingPhase = EMassProcessingPhase::PostPhysics;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Client | EProcessorExecutionFlags::Standalone);
}

void UArcRecoilDebugProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FArcRecoilStateFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
}

void UArcRecoilDebugProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	const int32 DebugMode = CVarArcRecoilDebug.GetValueOnGameThread();
	if (DebugMode == 0)
	{
		return;
	}

	UWorld* World = Context.GetWorld();
	if (World == nullptr)
	{
		return;
	}

	FMassEntityHandle LocalEntity;
	APawn* LocalPawn = nullptr;
	FVector CameraLocation = FVector::ZeroVector;
	FRotator CameraRotation = FRotator::ZeroRotator;
	const bool bHasLocalPlayer = ArcRecoilDebugProcessorPrivate::ResolveLocalPlayer(
		World, LocalEntity, LocalPawn, CameraLocation, CameraRotation);

	EntityQuery.ForEachEntityChunk(Context,
		[&EntityManager, World, LocalEntity, LocalPawn, CameraLocation, CameraRotation, bHasLocalPlayer](FMassExecutionContext& Ctx)
		{
			TConstArrayView<FArcRecoilStateFragment> States = Ctx.GetFragmentView<FArcRecoilStateFragment>();
			TConstArrayView<FTransformFragment> Transforms = Ctx.GetFragmentView<FTransformFragment>();
			if (Transforms.Num() == 0)
			{
				return;
			}

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				const FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);
				const FArcRecoilStateFragment& State = States[EntityIt];
				const FTransform& Transform = Transforms[EntityIt].GetTransform();
				const FVector Origin = Transform.GetLocation();
				const FVector AimDirection = Transform.GetRotation().GetForwardVector();

				FVector2f AngularOffsetDeg = FVector2f::ZeroVector;
				float SpreadHalfAngleDeg = 0.0f;
				float SpreadExponent = 1.0f;
				if (!ArcRecoil::GetAimPerturbation(EntityManager, Entity, AngularOffsetDeg, SpreadHalfAngleDeg, SpreadExponent))
				{
					continue;
				}

				// Local stream: drawing must never consume State.Stream, or it would perturb the real shot sequence.
				FRandomStream VizStream(Entity.Index);
				const FVector OffsetDirection = ArcRecoil::PerturbAimDirection(
					AimDirection, AngularOffsetDeg, 0.0f, SpreadExponent, VizStream);

				DrawDebugLine(World, Origin, Origin + AimDirection * ArcRecoilDebugProcessorPrivate::AimLineLength,
					FColor::White, false, -1.0f, 0, 1.5f);
				DrawDebugLine(World, Origin, Origin + OffsetDirection * ArcRecoilDebugProcessorPrivate::OffsetLineLength,
					FColor::Yellow, false, -1.0f, 0, 1.5f);
				DrawDebugCone(World, Origin, OffsetDirection, ArcRecoilDebugProcessorPrivate::ConeLength,
					FMath::DegreesToRadians(SpreadHalfAngleDeg), FMath::DegreesToRadians(SpreadHalfAngleDeg), 16,
					FColor::Orange, false, -1.0f, 0, 1.0f);

				DrawDebugString(World, Origin,
					FString::Printf(TEXT("Heat=%.2f ShotIdx=%.2f |Offset|=%.2f"),
						State.Heat, State.ShotIndex, AngularOffsetDeg.Size()),
					nullptr, FColor::White, -1.0f, false, 1.0f);

				if (bHasLocalPlayer && Entity == LocalEntity)
				{
					const FVector LookPoint = ArcRecoilDebugProcessorPrivate::TraceAimPoint(
						World, CameraLocation, CameraRotation.Vector(), LocalPawn);

					const FVector PerturbedCameraDirection = ArcRecoil::PerturbAimDirection(
						CameraRotation.Vector(), AngularOffsetDeg, SpreadHalfAngleDeg, SpreadExponent, VizStream);
					const FVector ImpactPoint = ArcRecoilDebugProcessorPrivate::TraceAimPoint(
						World, CameraLocation, PerturbedCameraDirection, LocalPawn);

					DrawDebugSphere(World, LookPoint, ArcRecoilDebugProcessorPrivate::ReticleSphereRadius, 12,
						FColor::Green, false, -1.0f, 0, 1.0f);
					DrawDebugSphere(World, ImpactPoint, ArcRecoilDebugProcessorPrivate::ReticleSphereRadius, 12,
						FColor::Red, false, -1.0f, 0, 1.0f);
				}
			}
		});
}
