// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/EntityHandle.h"
#include "UObject/PrimaryAssetId.h"

#include "Abilities/ArcAbilityHandle.h"
#include "Effects/ArcActiveEffectHandle.h"

#include "ArcCueSourceContext.generated.h"

class UArcAbilityDefinition;
class UArcEffectDefinition;

USTRUCT(BlueprintType)
struct ARCABILITYKIT_API FArcCueSourceContext
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cue")
	TObjectPtr<const UArcAbilityDefinition> SourceAbilityDefinition = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cue")
	FPrimaryAssetId SourceItemDefinition;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cue")
	FArcAbilityHandle SourceAbilityHandle;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cue")
	FMassEntityHandle SourceAbilityOwnerEntity;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cue")
	TObjectPtr<const UArcEffectDefinition> SourceEffectDefinition = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cue")
	FArcActiveEffectHandle SourceEffectHandle;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cue")
	FMassEntityHandle SourceEffectOwnerEntity;
};
