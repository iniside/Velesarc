// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "Fragments/ArcMassNetId.h"
#include "ArcMassNetIdFragmentLegacy.generated.h"

USTRUCT()
struct ARCMASSREPLICATIONRUNTIME_API FArcMassEntityNetIdFragmentLegacy : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY()
	FArcMassNetId NetId;
};
