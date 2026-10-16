#pragma once
#include "ArcStateTreeMassTargetLocationPropertyFunction.h"
#include "MassEQSBlueprintLibrary.h"
#include "ArcMass/ArcMassEntityHandleWrapper.h"

#include "ArcStateTreeMassEntityLocationPropertyFunction.generated.h"

/** Instance data for FArcStateTreeMassEntityLocationPropertyFunction. */
USTRUCT()
struct FStateTreeGetMassEntityLocationPropertyFunctionInstanceData
{
	GENERATED_BODY()

	/** Mass entity wrapper whose stored/query location will be used. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FMassEnvQueryEntityInfoBlueprintWrapper Input;

	/** Action the entity should perform when it reaches the end of its path (e.g., Move or Stand). */
	UPROPERTY(EditAnywhere, Category = Parameter)
	EMassMovementAction EndOfPathIntent = EMassMovementAction::Move;

	/** Resulting FMassTargetLocation built from the entity's stored location and end-of-path intent. */
	UPROPERTY(EditAnywhere, Category = Output)
	FMassTargetLocation Output;;
};

/** Gets an FMassTargetLocation from a Mass entity wrapper using the entity's stored/query location. */
USTRUCT(DisplayName = "Get Mass Entity Location", meta = (ToolTip = "Gets an FMassTargetLocation from a Mass entity wrapper using the entity's stored/query location."))
struct FArcStateTreeMassEntityLocationPropertyFunction : public FStateTreePropertyFunctionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FStateTreeGetMassEntityLocationPropertyFunctionInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual void Execute(FStateTreeExecutionContext& Context) const override;

#if WITH_EDITOR
	//virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const;
#endif
};

/** Instance data for FArcStateTreeMassEntityCurrentPositionPropertyFunction. */
USTRUCT()
struct FStateTreeGetMassEntityCurrentPositionPropertyFunctionInstanceData
{
	GENERATED_BODY()

	/** Mass entity wrapper whose current transform position will be read. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FMassEnvQueryEntityInfoBlueprintWrapper Input;

	/** Action the entity should perform when it reaches the end of its path (e.g., Move or Stand). */
	UPROPERTY(EditAnywhere, Category = Parameter)
	EMassMovementAction EndOfPathIntent = EMassMovementAction::Move;

	/** Resulting FMassTargetLocation built from the entity's current transform position and end-of-path intent. */
	UPROPERTY(EditAnywhere, Category = Output)
	FMassTargetLocation Output;;
};

/** Gets an FMassTargetLocation from a Mass entity wrapper using the entity's current transform position. */
USTRUCT(DisplayName = "Get Mass Entity Current Position", meta = (ToolTip = "Gets an FMassTargetLocation from a Mass entity wrapper using the entity's current transform position."))
struct FArcStateTreeMassEntityCurrentPositionPropertyFunction : public FStateTreePropertyFunctionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FStateTreeGetMassEntityCurrentPositionPropertyFunctionInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual void Execute(FStateTreeExecutionContext& Context) const override;

#if WITH_EDITOR
	//virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const;
#endif
};

USTRUCT()
struct FArcStateTreeGetLocationFromMassEntityPropertyFunctionInstanceData
{
	GENERATED_BODY()

	/** Mass entity wrapper whose stored/query location will be used. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FMassEntityHandle Input;

	/** Action the entity should perform when it reaches the end of its path (e.g., Move or Stand). */
	UPROPERTY(EditAnywhere, Category = Parameter)
	EMassMovementAction EndOfPathIntent = EMassMovementAction::Move;

	/** Resulting FMassTargetLocation built from the entity's stored location and end-of-path intent. */
	UPROPERTY(EditAnywhere, Category = Output)
	FMassTargetLocation Output;;
};

/** Gets an FMassTargetLocation from a Mass entity wrapper using the entity's stored/query location. */
USTRUCT(DisplayName = "Arc Get Location From Mass Entity", meta = (ToolTip = "Gets an FMassTargetLocation from a Mass entity wrapper using the entity's stored/query location."))
struct FArcStateTreeGetLocationFromMassEntityPropertyFunction : public FStateTreePropertyFunctionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcStateTreeGetLocationFromMassEntityPropertyFunctionInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual void Execute(FStateTreeExecutionContext& Context) const override;

#if WITH_EDITOR
	//virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const;
#endif
};

USTRUCT()
struct FArcEntityWrapperFromMassEntityPropertyFunctionInstanceData
{
	GENERATED_BODY()

	/** Mass entity wrapper whose stored/query location will be used. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FMassEntityHandle Input;

	/** Resulting FMassTargetLocation built from the entity's stored location and end-of-path intent. */
	UPROPERTY(EditAnywhere, Category = Output)
	FArcMassEntityHandleWrapper Output;;
};

/** Gets an FMassTargetLocation from a Mass entity wrapper using the entity's stored/query location. */
USTRUCT(DisplayName = "Arc Entity Wrapper From Mass Entity")
struct FArcEntityWrapperFromMassEntityPropertyFunction : public FStateTreePropertyFunctionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcEntityWrapperFromMassEntityPropertyFunctionInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual void Execute(FStateTreeExecutionContext& Context) const override;

#if WITH_EDITOR
	//virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const;
#endif
};