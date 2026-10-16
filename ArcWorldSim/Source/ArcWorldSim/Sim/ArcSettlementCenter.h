// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/ArchetypeCompositionTypes.h"
#include "Mass/EntityElementTypes.h"

#include "ArcSettlementCenter.generated.h"

USTRUCT()
struct ARCWORLDSIM_API FArcSettlementCenterFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Settlement", meta = (ClampMin = "1.0"))
	FVector Extent = FVector(10000.0, 10000.0, 2000.0);
};
