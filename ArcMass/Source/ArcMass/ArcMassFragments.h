// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcMassGameplayTagContainerFragment.h"
#include "UObject/Object.h"
#include "ArcMassFragments.generated.h"

USTRUCT()
struct ARCMASS_API FArcMassLastUpdateTimeFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY()
	double LastUpdateTime = 0.0;
};

USTRUCT()
struct ARCMASS_API FArcAgentCapsuleFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	float HalfHeight = 88.f;
};
