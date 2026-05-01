// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassStateTreeTypes.h"
#include "ArcKnowledgeTypes.h"
#include "ArcMassGetClaimedAdvertisementsTask.generated.h"

USTRUCT()
struct FArcMassGetClaimedAdvertisementsTaskInstanceData
{
	GENERATED_BODY()

	/** Output: all advertisement handles currently claimed by this entity. */
	UPROPERTY(EditAnywhere, Category = Output)
	TArray<FArcKnowledgeHandle> ClaimedHandles;

	/** Output: number of claimed advertisements. */
	UPROPERTY(VisibleAnywhere, Category = Output)
	int32 ClaimedCount = 0;
};

/**
 * StateTree task that retrieves all advertisements currently claimed by the executing entity.
 * Succeeds if the entity has at least one claim, fails otherwise.
 */
USTRUCT(meta = (DisplayName = "Arc Get Claimed Advertisements", Category = "ArcKnowledge"))
struct ARCKNOWLEDGE_API FArcMassGetClaimedAdvertisementsTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcMassGetClaimedAdvertisementsTaskInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FName GetIconName() const override { return FName("StateTreeEditorStyle|Node.Task"); }
	virtual FColor GetIconColor() const override { return FColor(220, 160, 60); }
#endif
};
