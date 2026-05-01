// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcKnowledgeTypes.h"
#include "MassStateTreeTypes.h"

#include "ArcKnowledgeHandleIsValidCondition.generated.h"

USTRUCT()
struct FArcKnowledgeHandleIsValidConditionInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Parameter)
	FArcKnowledgeHandle KnowledgeHandle;
};

USTRUCT(DisplayName = "Arc Knowledge Handle Is Valid")
struct ARCKNOWLEDGE_API FArcKnowledgeHandleIsValidCondition : public FMassStateTreeConditionBase
{
	GENERATED_BODY()

public:
	using FInstanceDataType = FArcKnowledgeHandleIsValidConditionInstanceData;

	FArcKnowledgeHandleIsValidCondition() = default;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;

	UPROPERTY(EditAnywhere, Category = Condition)
	bool bInvert = false;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif
};
