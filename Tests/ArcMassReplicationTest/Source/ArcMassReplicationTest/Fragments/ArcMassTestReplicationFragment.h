// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/ArchetypeCompositionTypes.h"
#include "MassLogging.h"
#include "ArcMassTestReplicationFragment.generated.h"

USTRUCT(meta = (Hidden))
struct FArcMassTestReplicationFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY()
	uint32 Payload = 0;
};
