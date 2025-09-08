#pragma once
#include "MassNavigationTypes.h"
#include "StateTreePropertyFunctionBase.h"

#include "ArcStateTreeMassTargetLocationPropertyFunction.generated.h"

USTRUCT()
struct FStateTreeGetMassTargetLocationPropertyFunctionInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Parameter)
	FVector Input = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category = Parameter)
	EMassMovementAction EndOfPathIntent = EMassMovementAction::Move;
	
	UPROPERTY(EditAnywhere, Category = Output)
	FMassTargetLocation Output;;
};

USTRUCT(DisplayName = "Get Mass Target Location")
struct FArcStateTreeMassTargetLocationPropertyFunction : public FStateTreePropertyFunctionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FStateTreeGetMassTargetLocationPropertyFunctionInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual void Execute(FStateTreeExecutionContext& Context) const override;

#if WITH_EDITOR
	//virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const;
#endif
};
