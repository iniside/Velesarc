// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "ArcMassTestPayloadFragment.generated.h"

USTRUCT()
struct FArcMassTestPayloadFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY()
	uint32 Payload = 0;
};
