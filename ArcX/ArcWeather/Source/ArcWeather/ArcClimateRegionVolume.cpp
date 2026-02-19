// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcClimateRegionVolume.h"
#include "Engine/World.h"

#include "Components/BoxComponent.h"

AArcClimateRegionVolume::AArcClimateRegionVolume()
{
	BoxComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("ClimateVolume"));
	BoxComponent->SetBoxExtent(FVector(500.f, 500.f, 500.f));
	BoxComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BoxComponent->SetGenerateOverlapEvents(false);
	RootComponent = BoxComponent;
}

void AArcClimateRegionVolume::BeginPlay()
{
	Super::BeginPlay();

	UArcWeatherSubsystem* Weather = GetWorld()->GetSubsystem<UArcWeatherSubsystem>();
	if (!Weather)
	{
		return;
	}

	const FBox Bounds = BoxComponent->Bounds.GetBox();
	Weather->SetBaseClimateInBounds(Bounds, BaseClimate);
}

void AArcClimateRegionVolume::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UArcWeatherSubsystem* Weather = GetWorld()->GetSubsystem<UArcWeatherSubsystem>();
	if (!Weather)
	{
		return;
	}
	
	const FBox Bounds = BoxComponent->Bounds.GetBox();
	Weather->ClearCellsInBounds(Bounds);
	Super::EndPlay(EndPlayReason);
}
