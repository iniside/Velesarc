// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/ArchetypeCompositionTypes.h"
#include "MassLogging.h"
#include "ArcMassTestPayloadFragment.generated.h"

USTRUCT(meta = (Hidden))
struct FArcMassTestPayloadFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY()
	uint32 Payload = 0;
};
