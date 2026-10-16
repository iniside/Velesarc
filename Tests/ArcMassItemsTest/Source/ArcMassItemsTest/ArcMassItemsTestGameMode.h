// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameMode/ArcMassGameMode.h"

#include "ArcMassItemsTestGameMode.generated.h"

UCLASS(Hidden, NotPlaceable)
class ARCMASSITEMSTEST_API AArcMassItemsTestGameMode : public AArcMassGameMode
{
	GENERATED_BODY()

public:
	AArcMassItemsTestGameMode();
};
