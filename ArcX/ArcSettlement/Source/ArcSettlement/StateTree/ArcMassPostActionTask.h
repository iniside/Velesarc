// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassStateTreeTypes.h"
#include "ArcKnowledgeEntry.h"
#include "MassEntityFragments.h"
#include "ArcMassPostActionTask.generated.h"

USTRUCT()
struct FArcMassPostActionTaskInstanceData
{
	GENERATED_BODY()

	/** Tags describing this action (e.g., "Quest.Deliver", "Request.Escort"). */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FGameplayTagContainer Tags;

	/** Optional payload data (reward, quantity, deadline, etc.). */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FInstancedStruct ActionData;

	/** Priority for ordering (higher = more urgent). */
	UPROPERTY(EditAnywhere, Category = Parameter)
	float Priority = 0.5f;

	/** Output: handle to the posted action. */
	UPROPERTY(EditAnywhere, Category = Output)
	FArcKnowledgeHandle PostedHandle;
};

/**
 * StateTree task that posts an action (quest/request) to the knowledge system.
 * Other NPCs can query for and claim this action.
 */
USTRUCT(meta = (DisplayName = "Arc Post Action", Category = "Arc|Settlement"))
struct ARCSETTLEMENT_API FArcMassPostActionTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcMassPostActionTaskInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

	/** Whether to cancel the action when this state exits. */
	UPROPERTY(EditAnywhere, Category = "Task")
	bool bCancelOnExit = true;

#if WITH_EDITOR
	virtual FName GetIconName() const override { return FName("StateTreeEditorStyle|Node.Add"); }
	virtual FColor GetIconColor() const override { return FColor(220, 160, 60); }
#endif

private:
	TStateTreeExternalDataHandle<FTransformFragment> TransformHandle;
};
