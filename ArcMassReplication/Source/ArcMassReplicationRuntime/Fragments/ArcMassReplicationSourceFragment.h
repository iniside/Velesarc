// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "ArcMassReplicationSourceFragment.generated.h"

USTRUCT()
struct ARCMASSREPLICATIONRUNTIME_API FArcMassReplicationSourceFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	uint32 ConnectionId = 0;

	FIntVector2 CachedCell = FIntVector2(TNumericLimits<int32>::Max(), TNumericLimits<int32>::Max());
};
