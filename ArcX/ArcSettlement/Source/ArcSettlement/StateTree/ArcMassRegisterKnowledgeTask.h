// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassStateTreeTypes.h"
#include "ArcKnowledgeEntry.h"
#include "MassEntityFragments.h"
#include "ArcMassRegisterKnowledgeTask.generated.h"

USTRUCT()
struct FArcMassRegisterKnowledgeTaskInstanceData
{
	GENERATED_BODY()

	/** Tags describing what this knowledge is about. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FGameplayTagContainer Tags;

	/** Optional payload data. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FInstancedStruct Payload;

	/** Initial relevance. */
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = 0.0, ClampMax = 1.0))
	float Relevance = 1.0f;

	/** Output: handle to the registered entry (for later update/removal). */
	UPROPERTY(EditAnywhere, Category = Output)
	FArcKnowledgeHandle RegisteredHandle;
};

/**
 * StateTree task that registers a knowledge entry at the entity's location and settlement.
 * Use for: "I found ore here", "I need iron", "danger at this location".
 */
USTRUCT(meta = (DisplayName = "Arc Register Knowledge", Category = "Arc|Settlement"))
struct ARCSETTLEMENT_API FArcMassRegisterKnowledgeTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcMassRegisterKnowledgeTaskInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

	/** Whether to remove the knowledge entry when this state exits. */
	UPROPERTY(EditAnywhere, Category = "Task")
	bool bRemoveOnExit = false;

#if WITH_EDITOR
	virtual FName GetIconName() const override { return FName("StateTreeEditorStyle|Node.Add"); }
	virtual FColor GetIconColor() const override { return FColor(100, 200, 100); }
#endif

private:
	TStateTreeExternalDataHandle<FTransformFragment> TransformHandle;
};
