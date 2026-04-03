// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassStateTreeTypes.h"
#include "ArcKnowledgeTypes.h"
#include "ArcMassUnclaimAdvertisementTask.generated.h"

USTRUCT()
struct FArcMassUnclaimAdvertisementTaskInstanceData
{
	GENERATED_BODY()

	/** Handle to the advertisement to unclaim. */
	UPROPERTY(EditAnywhere, Category = Input)
	FArcKnowledgeHandle AdvertisementHandle;

	/** Output: whether the unclaim succeeded. */
	UPROPERTY(EditAnywhere, Category = Output)
	bool bSuccess = false;
};

/**
 * StateTree task that unclaims (cancels the claim on) an advertisement.
 * Any entity can unclaim any advertisement — no ownership check.
 */
USTRUCT(meta = (DisplayName = "Arc Unclaim Advertisement", Category = "ArcKnowledge"))
struct ARCKNOWLEDGE_API FArcMassUnclaimAdvertisementTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcMassUnclaimAdvertisementTaskInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FName GetIconName() const override { return FName("StateTreeEditorStyle|Node.Task"); }
	virtual FColor GetIconColor() const override { return FColor(220, 160, 60); }
#endif
};
