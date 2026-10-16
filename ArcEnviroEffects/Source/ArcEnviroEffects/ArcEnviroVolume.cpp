// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcEnviroVolume.h"

#include "ArcEnviroEffectsSubsystem.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"

AArcEnviroVolume::AArcEnviroVolume()
{
	PrimaryActorTick.bCanEverTick = false;

	BoxComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("EnviroVolume"));
	BoxComponent->SetBoxExtent(FVector(500.f, 500.f, 500.f));
	BoxComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BoxComponent->SetGenerateOverlapEvents(false);
	RootComponent = BoxComponent;
}

bool AArcEnviroVolume::ContainsLocation(const FVector& Location) const
{
	const FVector LocalLocation = BoxComponent->GetComponentTransform().InverseTransformPosition(Location);
	const FVector Extent = BoxComponent->GetUnscaledBoxExtent();
	return FMath::Abs(LocalLocation.X) <= Extent.X
		&& FMath::Abs(LocalLocation.Y) <= Extent.Y
		&& FMath::Abs(LocalLocation.Z) <= Extent.Z;
}

TOptional<int32> AArcEnviroVolume::GetRainOverride() const
{
	return bOverrideRain ? TOptional<int32>(FMath::Clamp(RainLevel, 0, 3)) : TOptional<int32>();
}

TOptional<int32> AArcEnviroVolume::GetColdOverride() const
{
	return bOverrideCold ? TOptional<int32>(FMath::Clamp(ColdLevel, 0, 3)) : TOptional<int32>();
}

TOptional<int32> AArcEnviroVolume::GetHeatOverride() const
{
	return bOverrideHeat ? TOptional<int32>(FMath::Clamp(HeatLevel, 0, 3)) : TOptional<int32>();
}

void AArcEnviroVolume::BeginPlay()
{
	Super::BeginPlay();

	if (UArcEnviroEffectsSubsystem* Subsystem = GetWorld()->GetSubsystem<UArcEnviroEffectsSubsystem>())
	{
		Subsystem->RegisterVolume(this);
	}
}

void AArcEnviroVolume::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UArcEnviroEffectsSubsystem* Subsystem = GetWorld()->GetSubsystem<UArcEnviroEffectsSubsystem>())
	{
		Subsystem->UnregisterVolume(this);
	}

	Super::EndPlay(EndPlayReason);
}
