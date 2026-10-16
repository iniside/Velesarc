// Copyright Lukasz Baran. All Rights Reserved.

#include "Town/ArcTownAttractorVolume.h"

#include "Components/BoxComponent.h"

AArcTownAttractorVolume::AArcTownAttractorVolume()
{
	PrimaryActorTick.bCanEverTick = false;

	VolumeBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("VolumeBounds"));
	SetRootComponent(VolumeBounds);
	VolumeBounds->SetBoxExtent(FVector(1000.0, 1000.0, 500.0));
	VolumeBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	VolumeBounds->SetCanEverAffectNavigation(false);
}

#if WITH_EDITOR

EDataValidationResult AArcTownAttractorVolume::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	if (DistrictTags.IsEmpty())
	{
		Context.AddError(FText::FromString(TEXT("DistrictTags must not be empty.")));
		Result = EDataValidationResult::Invalid;
	}

	if (!FMath::IsFinite(Strength) || Strength < 0.0f)
	{
		Context.AddError(FText::FromString(TEXT("Strength must be finite and non-negative.")));
		Result = EDataValidationResult::Invalid;
	}

	return Result;
}

#endif
