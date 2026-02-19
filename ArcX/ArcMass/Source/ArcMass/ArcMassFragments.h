// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcMassGameplayTagContainerFragment.h"
#include "UObject/Object.h"
#include "ArcMassFragments.generated.h"

USTRUCT()
struct ARCMASS_API FArcMassHealthFragment : public FMassFragment
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere)
	float CurrentHealth = 100.f;
	
	UPROPERTY(EditAnywhere)
	float MaxHealth = 100.f;
};

USTRUCT()
struct ARCMASS_API FArcMassLastUpdateTimeFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY()
	double LastUpdateTime = 0.0;
};

