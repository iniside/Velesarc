// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/EntityHandle.h"
#include "Subsystems/WorldSubsystem.h"

#include "ArcGameDirectorSubsystem.generated.h"

class UArcMassEntityConfigAsset;

UCLASS()
class ARCWORLDSIM_API UArcGameDirectorSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	int32 SpawnPopulation(
		FMassEntityHandle Settlement,
		UArcMassEntityConfigAsset* Config,
		int32 Count);
};
