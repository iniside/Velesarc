// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassStateTreeTypes.h"
#include "ArcKnowledgeTypes.h"
#include "ArcKnowledgeEntry.h"
#include "ArcMassClaimAdvertisementTask.generated.h"

USTRUCT()
struct FArcMassClaimAdvertisementTaskInstanceData
{
	GENERATED_BODY()

	/** Handle to the advertisement to claim (typically from a query result). */
	UPROPERTY(EditAnywhere, Category = Input)
	FArcKnowledgeHandle AdvertisementHandle;

	/** Output: whether the claim succeeded. */
	UPROPERTY(EditAnywhere, Category = Output)
	bool bClaimSucceeded = false;
};

/**
 * StateTree task that claims an advertisement from the knowledge system.
 * Once claimed, other NPCs won't see it as available.
 * On exit, releases the claim if not completed.
 */
USTRUCT(meta = (DisplayName = "Arc Claim Advertisement", Category = "ArcKnowledge"))
struct ARCKNOWLEDGE_API FArcMassClaimAdvertisementTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcMassClaimAdvertisementTaskInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

	/** Whether to release the claim on exit (set false if you complete the advertisement in a follow-up state). */
	UPROPERTY(EditAnywhere, Category = "Task")
	bool bReleaseOnExit = true;

	/** Whether to mark the advertisement as completed (remove it) on exit instead of releasing. */
	UPROPERTY(EditAnywhere, Category = "Task")
	bool bCompleteOnExit = false;

#if WITH_EDITOR
	virtual FName GetIconName() const override { return FName("StateTreeEditorStyle|Node.Task"); }
	virtual FColor GetIconColor() const override { return FColor(220, 160, 60); }
#endif
};
