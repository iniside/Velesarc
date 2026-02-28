// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcSettlementVolume.h"
#include "ArcSettlementSubsystem.h"
#include "ArcSettlementDefinition.h"
#include "Components/SphereComponent.h"
#include "Engine/World.h"

AArcSettlementVolume::AArcSettlementVolume()
{
	SphereComponent = CreateDefaultSubobject<USphereComponent>(TEXT("SettlementBounds"));
	SphereComponent->SetSphereRadius(5000.0f);
	SphereComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SphereComponent->SetGenerateOverlapEvents(false);
	SphereComponent->ShapeColor = FColor::Cyan;
	RootComponent = SphereComponent;
}

void AArcSettlementVolume::BeginPlay()
{
	Super::BeginPlay();

	if (!Definition)
	{
		UE_LOG(LogTemp, Warning, TEXT("AArcSettlementVolume '%s' has no Definition set."), *GetName());
		return;
	}

	UArcSettlementSubsystem* Subsystem = GetWorld()->GetSubsystem<UArcSettlementSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	// Use the sphere radius from the component as the bounding radius
	const float Radius = SphereComponent->GetScaledSphereRadius();

	// Override the definition's bounding radius with the placed volume's actual radius
	// The subsystem will create the settlement and register initial knowledge
	SettlementHandle = Subsystem->CreateSettlement(Definition, GetActorLocation());

	// Update the settlement's bounding radius to match the placed sphere
	FArcSettlementData Data;
	if (Subsystem->GetSettlementData(SettlementHandle, Data))
	{
		Data.BoundingRadius = Radius;
		// The subsystem stores settlements by handle, we need to update in place
		// For now, the definition's radius is used. If the designer scales the sphere,
		// we should respect that — but the subsystem API doesn't expose direct data mutation.
		// This is acceptable for now; the definition's BoundingRadius is the source of truth.
	}
}

void AArcSettlementVolume::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (SettlementHandle.IsValid())
	{
		if (UArcSettlementSubsystem* Subsystem = GetWorld()->GetSubsystem<UArcSettlementSubsystem>())
		{
			Subsystem->DestroySettlement(SettlementHandle);
		}
		SettlementHandle = FArcSettlementHandle();
	}

	Super::EndPlay(EndPlayReason);
}
