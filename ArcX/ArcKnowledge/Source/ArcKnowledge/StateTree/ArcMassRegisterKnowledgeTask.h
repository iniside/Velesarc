// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassStateTreeTypes.h"
#include "ArcKnowledgeEntry.h"
#include "MassEntityFragments.h"
#include "ArcMassRegisterKnowledgeTask.generated.h"

class UArcKnowledgeEntryDefinition;

USTRUCT()
struct FArcMassRegisterKnowledgeTaskInstanceData
{
	GENERATED_BODY()

	/** Tags describing what this knowledge is about. Ignored when Definition is set. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FGameplayTagContainer Tags;

	/** Optional payload data. Ignored when Definition is set. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FInstancedStruct Payload;

	/** Initial relevance. Used only for inline registration (no Definition). */
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = 0.0, ClampMax = 1.0))
	float Relevance = 1.0f;

	/** Output: handle to the registered entry (for later update/removal).
	  * When using a Definition, this is the first (primary) handle. */
	UPROPERTY(EditAnywhere, Category = Output)
	FArcKnowledgeHandle RegisteredHandle;

	/** All handles registered by this task (for cleanup when using Definition). */
	TArray<FArcKnowledgeHandle> AllRegisteredHandles;
};

/**
 * StateTree task that registers a knowledge entry at the entity's location.
 * Supports two modes:
 *   1. Inline: specify Tags/Payload/Relevance directly (instance data)
 *   2. Definition: reference a UArcKnowledgeEntryDefinition data asset
 * When Definition is set, its entries are registered instead of the inline parameters.
 */
USTRUCT(meta = (DisplayName = "Arc Register Knowledge", Category = "ArcKnowledge"))
struct ARCKNOWLEDGE_API FArcMassRegisterKnowledgeTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcMassRegisterKnowledgeTaskInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

	/** Optional data asset defining predefined knowledge entries to register.
	  * When set, the definition's entries are registered instead of inline Tags/Payload. */
	UPROPERTY(EditAnywhere, Category = "Task")
	TObjectPtr<UArcKnowledgeEntryDefinition> Definition;

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
