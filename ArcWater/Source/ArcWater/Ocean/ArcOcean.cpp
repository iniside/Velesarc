// Copyright Lukasz Baran. All Rights Reserved.

#include "Ocean/ArcOcean.h"

#include "Components/PostProcessComponent.h"

#include "Ocean/ArcOceanComponent.h"

AArcOcean::AArcOcean()
{
	PrimaryActorTick.bCanEverTick = false;

	OceanComponent = CreateDefaultSubobject<UArcOceanComponent>(TEXT("OceanComponent"));
	SetRootComponent(OceanComponent);

	UnderwaterPostProcess = CreateDefaultSubobject<UPostProcessComponent>(TEXT("UnderwaterPostProcess"));
	UnderwaterPostProcess->SetupAttachment(OceanComponent);
	UnderwaterPostProcess->bUnbound = true;
	UnderwaterPostProcess->bEnabled = false;
	UnderwaterPostProcess->BlendWeight = 1.0f;
}

void AArcOcean::PostRegisterAllComponents()
{
	Super::PostRegisterAllComponents();

	if (OceanComponent != nullptr)
	{
		OceanComponent->SetUnderwaterPostProcess(UnderwaterPostProcess);
	}
}

void AArcOcean::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	LockTransform();
}

#if WITH_EDITOR

void AArcOcean::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	LockTransform();
}

void AArcOcean::PostEditMove(bool bFinished)
{
	Super::PostEditMove(bFinished);
	LockTransform();
}

#endif // WITH_EDITOR

void AArcOcean::LockTransform()
{
	SetActorRotation(FRotator::ZeroRotator);
	SetActorScale3D(FVector::OneVector);
}
