// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/EntityElementTypes.h"

#include "ArcEntityBuilderTestTypes.generated.h"

USTRUCT(meta = (Hidden))
struct FArcEntityBuilderTestRegularFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Counter = 0;
};

USTRUCT(meta = (Hidden))
struct FArcEntityBuilderTestSparseFragment : public FMassSparseFragment
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Health = 0;

	UPROPERTY()
	float Speed = 0.0f;
};

USTRUCT(meta = (Hidden))
struct FArcEntityBuilderTestRegularTag : public FMassTag
{
	GENERATED_BODY()
};

USTRUCT(meta = (Hidden))
struct FArcEntityBuilderTestSparseTag : public FMassSparseTag
{
	GENERATED_BODY()
};
