// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Mass/EntityHandle.h"

#include "ArcCueTypes.h"
#include "Cues/ArcCueSourceContext.h"

#include "ArcCueStateTreeContext.generated.h"

class UArcCueDefinition;

namespace ArcAbilityKit::CueStateTree
{
	inline const FName ContextName(TEXT("CueContext"));
}

USTRUCT(BlueprintType)
struct ARCABILITYKIT_API FArcCueStateTreeContext
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cue")
	FArcCueHandle CueHandle;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cue")
	TObjectPtr<const UArcCueDefinition> CueDefinition = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cue")
	FMassEntityHandle OwnerEntity;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cue")
	FMassEntityHandle SourceEntity;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cue")
	FMassEntityHandle TargetEntity;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cue")
	FTransform Location = FTransform::Identity;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cue")
	float Magnitude = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cue")
	FGameplayTagContainer Tags;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cue")
	bool bHasSourceContext = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cue")
	FArcCueSourceContext SourceContext;
};
