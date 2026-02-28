// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcRegionVolume.h"
#include "ArcSettlementSubsystem.h"
#include "ArcRegionDefinition.h"
#include "Components/SphereComponent.h"
#include "Engine/World.h"

AArcRegionVolume::AArcRegionVolume()
{
	SphereComponent = CreateDefaultSubobject<USphereComponent>(TEXT("RegionBounds"));
	SphereComponent->SetSphereRadius(50000.0f);
	SphereComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SphereComponent->SetGenerateOverlapEvents(false);
	SphereComponent->ShapeColor = FColor::Yellow;
	RootComponent = SphereComponent;
}

void AArcRegionVolume::BeginPlay()
{
	Super::BeginPlay();

	if (!Definition)
	{
		UE_LOG(LogTemp, Warning, TEXT("AArcRegionVolume '%s' has no Definition set."), *GetName());
		return;
	}

	UArcSettlementSubsystem* Subsystem = GetWorld()->GetSubsystem<UArcSettlementSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	const float Radius = SphereComponent->GetScaledSphereRadius();
	RegionHandle = Subsystem->CreateRegion(Definition, GetActorLocation(), Radius);
}

void AArcRegionVolume::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (RegionHandle.IsValid())
	{
		if (UArcSettlementSubsystem* Subsystem = GetWorld()->GetSubsystem<UArcSettlementSubsystem>())
		{
			Subsystem->DestroyRegion(RegionHandle);
		}
		RegionHandle = FArcRegionHandle();
	}

	Super::EndPlay(EndPlayReason);
}
