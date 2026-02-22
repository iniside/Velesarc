// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcBuildSnappingMethod.h"

#include "Components/StaticMeshComponent.h"

// -------------------------------------------------------------------
// FArcBuildSnappingMethod (base)
// -------------------------------------------------------------------

bool FArcBuildSnappingMethod::ComputeSnappedTransform(
	const FHitResult& HitResult,
	UStaticMeshComponent* PreviewMesh,
	UStaticMeshComponent* TargetMesh,
	const FGameplayTagContainer& TargetTags,
	FTransform& OutTransform) const
{
	return false;
}

// -------------------------------------------------------------------
// FArcBuildSnappingMethod_Socket
// -------------------------------------------------------------------

bool FArcBuildSnappingMethod_Socket::ComputeSnappedTransform(
	const FHitResult& HitResult,
	UStaticMeshComponent* PreviewMesh,
	UStaticMeshComponent* TargetMesh,
	const FGameplayTagContainer& TargetTags,
	FTransform& OutTransform) const
{
	if (!TargetMesh || !PreviewMesh)
	{
		return false;
	}

	if (!SnappingRequiredTags.IsEmpty() && !TargetTags.HasAll(SnappingRequiredTags))
	{
		return false;
	}

	TArray<FName> TargetSockets = TargetMesh->GetAllSocketNames();
	TArray<FName> MySockets = PreviewMesh->GetAllSocketNames();

	if (TargetSockets.Num() == 0 || MySockets.Num() == 0)
	{
		return false;
	}

	// Find closest socket on target to hit point.
	float SqDistance = FLT_MAX;
	FName TargetSocketName;

	for (const FName& TargetSocket : TargetSockets)
	{
		FVector TargetSocketLocation = TargetMesh->GetSocketLocation(TargetSocket);
		float NewSqDistance = FVector::DistSquared(TargetSocketLocation, HitResult.ImpactPoint);

		if (NewSqDistance < SqDistance)
		{
			SqDistance = NewSqDistance;
			TargetSocketName = TargetSocket;
		}
	}

	if (!TargetSocketName.IsValid())
	{
		return false;
	}

	FVector TargetSocketLocation = TargetMesh->GetSocketLocation(TargetSocketName);
	FVector TargetSocketDir = TargetMesh->GetSocketRotation(TargetSocketName).Vector();

	// Find closest socket on preview to the target socket.
	SqDistance = FLT_MAX;
	FName MySocketName;

	for (const FName& MySocket : MySockets)
	{
		if (bMatchSocketsByName && MySocket != TargetSocketName)
		{
			continue;
		}

		FVector MySocketLocation = PreviewMesh->GetSocketLocation(MySocket);
		float NewSqDistance = FVector::DistSquared(TargetSocketLocation, MySocketLocation);

		if (NewSqDistance < SqDistance)
		{
			SqDistance = NewSqDistance;
			MySocketName = MySocket;
		}
	}

	if (!MySocketName.IsValid())
	{
		return false;
	}

	AActor* PreviewActor = PreviewMesh->GetOwner();
	if (!PreviewActor)
	{
		return false;
	}

	FVector MySocketLocation = PreviewMesh->GetSocketLocation(MySocketName);
	float Dist = FVector::Dist(PreviewActor->GetActorLocation(), MySocketLocation);
	FVector NewLocation = TargetSocketLocation + (TargetSocketDir * Dist);

	OutTransform.SetLocation(NewLocation);
	OutTransform.SetRotation(PreviewActor->GetActorQuat());
	return true;
}

// -------------------------------------------------------------------
// FArcBuildSnappingMethod_BoundingBox
// -------------------------------------------------------------------

bool FArcBuildSnappingMethod_BoundingBox::ComputeSnappedTransform(
	const FHitResult& HitResult,
	UStaticMeshComponent* PreviewMesh,
	UStaticMeshComponent* TargetMesh,
	const FGameplayTagContainer& TargetTags,
	FTransform& OutTransform) const
{
	if (!TargetMesh || !PreviewMesh)
	{
		return false;
	}

	const FBoxSphereBounds TargetBounds = TargetMesh->Bounds;
	const FBoxSphereBounds PreviewBounds = PreviewMesh->Bounds;

	const FVector& ImpactPoint = HitResult.ImpactPoint;
	const FVector& ImpactNormal = HitResult.ImpactNormal;

	const FVector TargetCenter = TargetBounds.Origin;
	const FVector TargetExtent = TargetBounds.BoxExtent;
	const FVector PreviewExtent = PreviewBounds.BoxExtent;

	// Find which face of the target box the impact normal is closest to.
	FVector SnapOffset = FVector::ZeroVector;
	float BestDot = -1.0f;

	const FVector Axes[] = {
		FVector::ForwardVector, FVector::RightVector, FVector::UpVector,
		-FVector::ForwardVector, -FVector::RightVector, -FVector::UpVector
	};

	for (const FVector& Axis : Axes)
	{
		float Dot = FVector::DotProduct(ImpactNormal.GetSafeNormal(), Axis);
		if (Dot > BestDot)
		{
			BestDot = Dot;
			// Offset along this axis by target extent + preview extent.
			int32 AxisIdx = FMath::Abs(Axis.X) > 0.5f ? 0 : (FMath::Abs(Axis.Y) > 0.5f ? 1 : 2);
			float TotalOffset = TargetExtent[AxisIdx] + PreviewExtent[AxisIdx];
			SnapOffset = Axis * TotalOffset;
		}
	}

	FVector SnappedLocation = TargetCenter + SnapOffset;
	OutTransform.SetLocation(SnappedLocation);
	OutTransform.SetRotation(FQuat::Identity);
	return true;
}
