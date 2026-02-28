// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassStateTreeTypes.h"
#include "ArcSettlementTypes.h"
#include "ArcKnowledgeEntry.h"
#include "ArcMassClaimActionTask.generated.h"

USTRUCT()
struct FArcMassClaimActionTaskInstanceData
{
	GENERATED_BODY()

	/** Handle to the action to claim (typically from a query result). */
	UPROPERTY(EditAnywhere, Category = Input)
	FArcKnowledgeHandle ActionHandle;

	/** Output: whether the claim succeeded. */
	UPROPERTY(EditAnywhere, Category = Output)
	bool bClaimSucceeded = false;
};

/**
 * StateTree task that claims an action from the knowledge system.
 * Once claimed, other NPCs won't see it as available.
 * On exit, releases the claim if not completed.
 */
USTRUCT(meta = (DisplayName = "Arc Claim Action", Category = "Arc|Settlement"))
struct ARCSETTLEMENT_API FArcMassClaimActionTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcMassClaimActionTaskInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

	/** Whether to release the claim on exit (set false if you complete the action in a follow-up state). */
	UPROPERTY(EditAnywhere, Category = "Task")
	bool bReleaseOnExit = true;

	/** Whether to mark the action as completed (remove it) on exit instead of releasing. */
	UPROPERTY(EditAnywhere, Category = "Task")
	bool bCompleteOnExit = false;

#if WITH_EDITOR
	virtual FName GetIconName() const override { return FName("StateTreeEditorStyle|Node.Task"); }
	virtual FColor GetIconColor() const override { return FColor(220, 160, 60); }
#endif
};
