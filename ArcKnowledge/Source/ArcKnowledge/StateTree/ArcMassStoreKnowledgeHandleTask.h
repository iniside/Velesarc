// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassStateTreeTypes.h"
#include "ArcKnowledgeTypes.h"
#include "ArcMassStoreKnowledgeHandleTask.generated.h"

USTRUCT()
struct FArcMassStoreKnowledgeHandleTaskInstanceData
{
	GENERATED_BODY()

	/** The knowledge handle to store. */
	UPROPERTY(EditAnywhere, Category = Input)
	FArcKnowledgeHandle KnowledgeHandle;

	/** Output: the stored handle — bind this to persist across states. */
	UPROPERTY(EditAnywhere, Category = Output)
	FArcKnowledgeHandle StoredHandle;
};

/**
 * Passthrough task that copies a knowledge handle from input to output.
 * Use this to persist a claimed handle in StateTree state for permanent claims.
 */
USTRUCT(meta = (DisplayName = "Arc Store Knowledge Handle", Category = "ArcKnowledge"))
struct ARCKNOWLEDGE_API FArcMassStoreKnowledgeHandleTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcMassStoreKnowledgeHandleTaskInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FName GetIconName() const override { return FName("StateTreeEditorStyle|Node.Task"); }
	virtual FColor GetIconColor() const override { return FColor(220, 160, 60); }
#endif
};
