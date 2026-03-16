// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StateTreeConsiderationBase.h"
#include "ArcAreaAssignmentConsideration.generated.h"

USTRUCT()
struct FArcAreaAssignmentConsiderationInstanceData
{
	GENERATED_BODY()
};

/**
 * StateTree utility consideration: returns 1.0 if the entity is assigned to an area slot, 0.0 if not.
 * Reads FArcAreaAssignmentFragment::IsAssigned().
 */
USTRUCT(meta = (DisplayName = "Arc Area Assignment", Category = "Arc|Area|NPC"))
struct ARCAREA_API FArcAreaAssignmentConsideration : public FStateTreeConsiderationCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcAreaAssignmentConsiderationInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

protected:
	virtual float GetScore(FStateTreeExecutionContext& Context) const override;

public:
	/** If true, returns 1.0 when NOT assigned (for FindWork state). */
	UPROPERTY(EditAnywhere, Category = "Default")
	bool bInvert = false;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif
};
