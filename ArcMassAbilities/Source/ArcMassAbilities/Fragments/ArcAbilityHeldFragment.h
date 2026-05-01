// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "GameplayTagContainer.h"
#include "Mass/EntityElementTypes.h"
#include "ArcAbilityHeldFragment.generated.h"

USTRUCT()
struct FArcAbilityHeldFragment : public FMassSparseFragment
{
	GENERATED_BODY()

	FGameplayTag InputTag;
	float MinHeldDuration = 0.f;
	float HeldDuration = 0.f;
	bool bIsHeld = false;
	bool bWasHeld = false;
	bool bThresholdReached = false;
};

USTRUCT()
struct FArcAbilityHeldTag : public FMassSparseTag
{
	GENERATED_BODY()
};
