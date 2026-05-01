// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "Fragments/ArcMassNetId.h"
#include "ArcMassNetIdFragment.generated.h"

USTRUCT()
struct ARCMASSREPLICATIONRUNTIME_API FArcMassEntityNetIdFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY()
	FArcMassNetId NetId;
};
