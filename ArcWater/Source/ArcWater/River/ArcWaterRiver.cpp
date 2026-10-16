// Copyright Lukasz Baran. All Rights Reserved.

#include "River/ArcWaterRiver.h"

#include "Components/PostProcessComponent.h"

#include "River/ArcWaterRiverComponent.h"
#include "River/ArcWaterRiverSplineModifier.h"

AArcWaterRiver::AArcWaterRiver()
{
	PrimaryActorTick.bCanEverTick = false;

	RiverComponent = CreateDefaultSubobject<UArcWaterRiverComponent>(TEXT("RiverComponent"));
	SetRootComponent(RiverComponent);

	UnderwaterPostProcess = CreateDefaultSubobject<UPostProcessComponent>(TEXT("UnderwaterPostProcess"));
	UnderwaterPostProcess->SetupAttachment(RiverComponent);
	UnderwaterPostProcess->bUnbound = true;
	UnderwaterPostProcess->bEnabled = false;
	UnderwaterPostProcess->BlendWeight = 1.0f;

	TerrainModifier = CreateDefaultSubobject<UArcWaterRiverSplineModifier>(TEXT("TerrainModifier"));
	TerrainModifier->SetupAttachment(RiverComponent);
}

void AArcWaterRiver::PostRegisterAllComponents()
{
	Super::PostRegisterAllComponents();

	if (RiverComponent != nullptr)
	{
		RiverComponent->SetUnderwaterPostProcess(UnderwaterPostProcess);
	}
}
