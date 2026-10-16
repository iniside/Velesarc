// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "ArcTacticsInstanceHandle.generated.h"

USTRUCT(BlueprintType)
struct ARCTACTICS_API FArcTacticsInstanceHandle
{
	GENERATED_BODY()

	FArcTacticsInstanceHandle() = default;
	FArcTacticsInstanceHandle(const int32 InIndex, const uint32 InGeneration)
		: Index(InIndex), Generation(InGeneration)
	{
	}

	UPROPERTY(VisibleAnywhere, Category = "Tactics")
	int32 Index = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, Category = "Tactics")
	uint32 Generation = 0;

	bool IsValid() const
	{
		return Index != INDEX_NONE && Generation != 0;
	}

	void Reset()
	{
		Index = INDEX_NONE;
		Generation = 0;
	}

	friend bool operator==(const FArcTacticsInstanceHandle&, const FArcTacticsInstanceHandle&) = default;
};
