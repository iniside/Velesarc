// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcCollisionHelpers.h"

#include "Engine/World.h"

bool ArcCollisionHelpers::OverlapAnySphere(
	const UWorld* World,
	const FVector& Center,
	float Radius,
	ECollisionChannel Channel,
	const FCollisionQueryParams& Params)
{
	if (!World)
	{
		return false;
	}
	return World->OverlapAnyTestByChannel(
		Center, FQuat::Identity, Channel,
		FCollisionShape::MakeSphere(Radius), Params);
}

bool ArcCollisionHelpers::OverlapBlockingSphere(
	const UWorld* World,
	const FVector& Center,
	float Radius,
	ECollisionChannel Channel,
	const FCollisionQueryParams& Params)
{
	if (!World)
	{
		return false;
	}
	return World->OverlapBlockingTestByChannel(
		Center, FQuat::Identity, Channel,
		FCollisionShape::MakeSphere(Radius), Params);
}

bool ArcCollisionHelpers::LineTrace(
	const UWorld* World,
	FHitResult& OutHit,
	const FVector& Start,
	const FVector& End,
	ECollisionChannel Channel,
	const FCollisionQueryParams& Params)
{
	if (!World)
	{
		return false;
	}
	return World->LineTraceSingleByChannel(OutHit, Start, End, Channel, Params);
}
