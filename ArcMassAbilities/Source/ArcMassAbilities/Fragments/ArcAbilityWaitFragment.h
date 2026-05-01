// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Mass/EntityElementTypes.h"
#include "ArcAbilityWaitFragment.generated.h"

USTRUCT()
struct FArcAbilityWaitFragment : public FMassSparseFragment
{
	GENERATED_BODY()

	float Duration = 0.f;
	float ElapsedTime = 0.f;
};

USTRUCT()
struct FArcAbilityWaitTag : public FMassSparseTag
{
	GENERATED_BODY()
};
