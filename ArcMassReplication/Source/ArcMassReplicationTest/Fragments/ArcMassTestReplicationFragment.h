// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "ArcMassTestReplicationFragment.generated.h"

USTRUCT()
struct FArcMassTestReplicationFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY()
	uint32 Payload = 0;
};
