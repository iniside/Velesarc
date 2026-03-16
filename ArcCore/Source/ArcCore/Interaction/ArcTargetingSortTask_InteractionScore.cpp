// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTargetingSortTask_InteractionScore.h"

#include "GameFramework/Actor.h"
#include "Targeting/ArcTargetingSourceContext.h"

float UArcTargetingSortTask_InteractionScore::GetScoreForTarget(const FTargetingRequestHandle& TargetingHandle, const FTargetingDefaultResultData& TargetData) const
{
	AActor* TargetActor = TargetData.HitResult.GetActor();
	if (!TargetActor)
	{
		return BIG_NUMBER;
	}

	FArcTargetingSourceContext* Ctx = FArcTargetingSourceContext::Find(TargetingHandle);
	if (!Ctx || !Ctx->SourceActor)
	{
		return BIG_NUMBER;
	}

	FVector EyeLocation;
	FRotator EyeRotation;
	Ctx->SourceActor->GetActorEyesViewPoint(EyeLocation, EyeRotation);
	const FVector LookDir = EyeRotation.Vector();

	const FVector TargetLocation = TargetActor->GetActorLocation();
	const FVector DirToTarget = (TargetLocation - EyeLocation).GetSafeNormal();
	const float Distance = FVector::Dist(EyeLocation, TargetLocation);

	const float NormalizedDistance = FMath::Clamp(Distance / MaxDistance.GetValue(), 0.f, 1.f);

	const float DotProduct = FVector::DotProduct(LookDir, DirToTarget);
	const float LookScore = 1.f - FMath::Clamp(DotProduct, 0.f, 1.f);

	return NormalizedDistance * DistanceWeight + LookScore * LookDirectionWeight;
}
