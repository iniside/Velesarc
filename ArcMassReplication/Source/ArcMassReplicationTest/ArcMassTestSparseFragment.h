// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/EntityElementTypes.h"
#include "ArcMassTestSparseFragment.generated.h"

USTRUCT()
struct FArcMassTestSparseFragment : public FMassSparseFragment
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Health = 0;

	UPROPERTY()
	float Speed = 0.f;
};
