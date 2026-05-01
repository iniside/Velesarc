// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/EntityHandle.h"
#include "Abilities/ArcAbilityHandle.h"
#include "ArcAbilityContext.generated.h"

class UArcAbilityDefinition;

USTRUCT()
struct ARCMASSABILITIES_API FArcAbilityContext
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere)
	FArcAbilityHandle Handle;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<const UArcAbilityDefinition> Definition = nullptr;

	UPROPERTY(VisibleAnywhere)
	FMassEntityHandle SourceEntity;
};
