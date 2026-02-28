// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassStateTreeTypes.h"
#include "GameplayTagContainer.h"
#include "MassEntityFragments.h"
#include "ArcSettlementHasKnowledgeCondition.generated.h"

USTRUCT()
struct FArcSettlementHasKnowledgeConditionInstanceData
{
	GENERATED_BODY()

	/** Tag query to match against knowledge entries. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FGameplayTagQuery TagQuery;
};

/**
 * StateTree condition: does the NPC's settlement have any knowledge matching a tag query?
 */
USTRUCT(DisplayName = "Arc Settlement Has Knowledge")
struct ARCSETTLEMENT_API FArcSettlementHasKnowledgeCondition : public FMassStateTreeConditionBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcSettlementHasKnowledgeConditionInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;

private:
	TStateTreeExternalDataHandle<FTransformFragment> TransformHandle;
};
