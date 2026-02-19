// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcMassSmartObjectTypes.h"
#include "MassSmartObjectRequest.h"
#include "StateTreePropertyFunctionBase.h"
#include "UObject/Object.h"
#include "ArcCandidateSlotsFromSmartObjectItemFunction.generated.h"

/**
 * 
 */
USTRUCT()
struct FArcCandidateSlotsFromSmartObjectItemFunctionInstanceData
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, Category = Parameter)
	FArcMassSmartObjectItem Input;
	
	UPROPERTY(EditAnywhere, Category = Output)
	FMassSmartObjectCandidateSlots CandidateSlots;
};

USTRUCT(DisplayName = "Arc Candidate Slots From Smart Object Item")
struct FArcCandidateSlotsFromSmartObjectItemFunction : public FStateTreePropertyFunctionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcCandidateSlotsFromSmartObjectItemFunctionInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual void Execute(FStateTreeExecutionContext& Context) const override;

#if WITH_EDITOR
	//virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const;
#endif
};