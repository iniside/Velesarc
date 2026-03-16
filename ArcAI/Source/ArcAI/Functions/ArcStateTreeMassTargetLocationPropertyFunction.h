#pragma once
#include "MassNavigationTypes.h"
#include "StateTreePropertyFunctionBase.h"

#include "ArcStateTreeMassTargetLocationPropertyFunction.generated.h"

/** Instance data for FArcStateTreeMassTargetLocationPropertyFunction. */
USTRUCT()
struct FStateTreeGetMassTargetLocationPropertyFunctionInstanceData
{
	GENERATED_BODY()

	/** World-space position to use as the target location. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FVector Input = FVector::ZeroVector;

	/** Action the entity should perform when it reaches the end of its path (e.g., Move or Stand). */
	UPROPERTY(EditAnywhere, Category = Parameter)
	EMassMovementAction EndOfPathIntent = EMassMovementAction::Move;

	/** Resulting FMassTargetLocation built from the input position and end-of-path intent. */
	UPROPERTY(EditAnywhere, Category = Output)
	FMassTargetLocation Output;;
};

/** Converts an FVector position and an end-of-path intent into an FMassTargetLocation for Mass navigation. */
USTRUCT(DisplayName = "Get Mass Target Location", meta = (ToolTip = "Converts an FVector position and an end-of-path intent into an FMassTargetLocation for Mass navigation."))
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

/** Instance data for FStateTreeGetMassTargetLocationFromTransformPropertyFunctionI. */
USTRUCT()
struct FStateTreeGetMassTargetLocationFromTransformPropertyFunctionInstanceData
{
	GENERATED_BODY()

	/** Transform whose translation component is used as the target location. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FTransform Input = FTransform::Identity;

	/** Action the entity should perform when it reaches the end of its path (e.g., Move or Stand). */
	UPROPERTY(EditAnywhere, Category = Parameter)
	EMassMovementAction EndOfPathIntent = EMassMovementAction::Move;

	/** Resulting FMassTargetLocation built from the transform's translation and end-of-path intent. */
	UPROPERTY(EditAnywhere, Category = Output)
	FMassTargetLocation Output;;
};

/** Converts an FTransform (using its translation) and an end-of-path intent into an FMassTargetLocation for Mass navigation. */
USTRUCT(DisplayName = "Get Mass Target Location From Transform", meta = (ToolTip = "Converts an FTransform (using its translation) and an end-of-path intent into an FMassTargetLocation for Mass navigation."))
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

/** Instance data for FArcStateTreeGetLocationFromTransformPropertyFunction. */
USTRUCT()
struct FArcStateTreeGetLocationFromTransformPropertyFunctionInstanceData
{
	GENERATED_BODY()

	/** Transform from which the translation component will be extracted. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FTransform Input = FTransform::Identity;

	/** Extracted FVector location (translation) from the input transform. */
	UPROPERTY(EditAnywhere, Category = Output)
	FVector Output = FVector::ZeroVector;
};

/** Extracts the FVector translation from an FTransform. */
USTRUCT(DisplayName = "Get Location From Transform", meta = (ToolTip = "Extracts the FVector translation from an FTransform."))
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