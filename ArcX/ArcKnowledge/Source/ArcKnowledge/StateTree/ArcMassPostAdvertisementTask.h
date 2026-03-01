// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassStateTreeTypes.h"
#include "ArcKnowledgeEntry.h"
#include "MassEntityFragments.h"
#include "ArcMassPostAdvertisementTask.generated.h"

USTRUCT()
struct FArcMassPostAdvertisementTaskInstanceData
{
	GENERATED_BODY()

	/** Tags describing this advertisement (e.g., "Quest.Deliver", "Request.Escort"). */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FGameplayTagContainer Tags;

	/** Optional payload data (reward, quantity, deadline, etc.). */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FInstancedStruct Payload;

	/** Optional behavioral instruction — how to fulfill this advertisement. */
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (BaseStruct = "/Script/ArcKnowledge.ArcAdvertisementInstruction", ExcludeBaseStruct))
	FInstancedStruct Instruction;

	/** Priority for ordering (higher = more urgent). */
	UPROPERTY(EditAnywhere, Category = Parameter)
	float Priority = 0.5f;

	/** Output: handle to the posted advertisement. */
	UPROPERTY(EditAnywhere, Category = Output)
	FArcKnowledgeHandle PostedHandle;
};

/**
 * StateTree task that posts an advertisement to the knowledge system.
 * Other NPCs can query for and claim this advertisement.
 */
USTRUCT(meta = (DisplayName = "Arc Post Advertisement", Category = "ArcKnowledge"))
struct ARCKNOWLEDGE_API FArcMassPostAdvertisementTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcMassPostAdvertisementTaskInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

	/** Whether to remove the advertisement when this state exits. */
	UPROPERTY(EditAnywhere, Category = "Task")
	bool bRemoveOnExit = true;

#if WITH_EDITOR
	virtual FName GetIconName() const override { return FName("StateTreeEditorStyle|Node.Add"); }
	virtual FColor GetIconColor() const override { return FColor(220, 160, 60); }
#endif

private:
	TStateTreeExternalDataHandle<FTransformFragment> TransformHandle;
};
