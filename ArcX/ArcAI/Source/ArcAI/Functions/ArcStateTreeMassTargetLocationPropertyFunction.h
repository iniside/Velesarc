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

USTRUCT()
struct FStateTreeGetMassTargetLocationFromTransformPropertyFunctionInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Parameter)
	FTransform Input = FTransform::Identity;

	UPROPERTY(EditAnywhere, Category = Parameter)
	EMassMovementAction EndOfPathIntent = EMassMovementAction::Move;
	
	UPROPERTY(EditAnywhere, Category = Output)
	FMassTargetLocation Output;;
};

USTRUCT(DisplayName = "Get Mass Target Location From Transform")
struct FStateTreeGetMassTargetLocationFromTransformPropertyFunctionI : public FStateTreePropertyFunctionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FStateTreeGetMassTargetLocationFromTransformPropertyFunctionInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual void Execute(FStateTreeExecutionContext& Context) const override;

#if WITH_EDITOR
	//virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const;
#endif
};

USTRUCT()
struct FArcStateTreeGetLocationFromTransformPropertyFunctionInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Parameter)
	FTransform Input = FTransform::Identity;

	UPROPERTY(EditAnywhere, Category = Output)
	FVector Output;;
};

USTRUCT(DisplayName = "Get Location From Transform")
struct FArcStateTreeGetLocationFromTransformPropertyFunction : public FStateTreePropertyFunctionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcStateTreeGetLocationFromTransformPropertyFunctionInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual void Execute(FStateTreeExecutionContext& Context) const override;

#if WITH_EDITOR
	//virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const;
#endif
};