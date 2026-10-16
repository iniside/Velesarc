// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Simulation/ArcTownWorldSnapshot.h"
#include "UObject/Object.h"

#include "ArcTownSimulationState.generated.h"

UCLASS(DefaultToInstanced, EditInlineNew)
class ARCTOWN_API UArcTownSimulationState : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FArcTownWorldSnapshot Snapshot;
};
