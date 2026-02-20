// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcTQSTypes.h"
#include "ArcTQSStep.h"
#include "StructUtils/InstancedStruct.h"
#include "Engine/DataAsset.h"
#include "ArcTQSQueryDefinition.generated.h"

/**
 * Data asset that defines a target query. Contains a generator and an ordered pipeline of steps.
 * Can be shared across State Tree tasks or used inline.
 */
UCLASS(BlueprintType, Blueprintable, DefaultToInstanced, EditInlineNew)
class ARCAI_API UArcTQSQueryDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	// The generator that produces the initial target pool
	UPROPERTY(EditAnywhere, Category = "Generator", meta = (BaseStruct = "/Script/ArcAI.ArcTQSGenerator"))
	FInstancedStruct Generator;

	// Ordered pipeline of filter/score steps, executed sequentially
	UPROPERTY(EditAnywhere, Category = "Steps", meta = (BaseStruct = "/Script/ArcAI.ArcTQSStep"))
	TArray<FInstancedStruct> Steps;

	// How to select from the final scored pool
	UPROPERTY(EditAnywhere, Category = "Selection")
	EArcTQSSelectionMode SelectionMode = EArcTQSSelectionMode::HighestScore;

	// For TopN mode: how many results to return
	UPROPERTY(EditAnywhere, Category = "Selection", meta = (EditCondition = "SelectionMode == EArcTQSSelectionMode::TopN", ClampMin = 1))
	int32 TopN = 5;

	// For AllPassing mode: minimum score threshold
	UPROPERTY(EditAnywhere, Category = "Selection", meta = (EditCondition = "SelectionMode == EArcTQSSelectionMode::AllPassing"))
	float MinPassingScore = 0.0f;
};
