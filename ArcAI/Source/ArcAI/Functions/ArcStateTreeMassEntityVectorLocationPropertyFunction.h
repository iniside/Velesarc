#pragma once
#include "MassEQSBlueprintLibrary.h"
#include "StateTreePropertyFunctionBase.h"

#include "ArcStateTreeMassEntityVectorLocationPropertyFunction.generated.h"

/** Instance data for FArcStateTreeMassEntityVectorLocationPropertyFunction. */
USTRUCT()
struct FArcStateTreeMassEntityVectorLocationPropertyFunctionInstanceData
{
	GENERATED_BODY()

	/** Mass entity wrapper whose stored/query location will be extracted as a vector. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FMassEnvQueryEntityInfoBlueprintWrapper Input;

	/** Extracted FVector position from the entity's stored/query location. */
	UPROPERTY(EditAnywhere, Category = Output)
	FVector Output = FVector::ZeroVector;
};

/** Gets an FVector world-space location from a Mass entity wrapper using the entity's stored/query location. */
USTRUCT(DisplayName = "Get Mass Entity Vector Location", meta = (ToolTip = "Gets an FVector world-space location from a Mass entity wrapper using the entity's stored/query location."))
struct FArcStateTreeMassEntityVectorLocationPropertyFunction : public FStateTreePropertyFunctionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcStateTreeMassEntityVectorLocationPropertyFunctionInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual void Execute(FStateTreeExecutionContext& Context) const override;
};


/** Instance data for FArcStateTreeMassEntityCurrentLocationPropertyFunction. */
USTRUCT()
struct FArcStateTreeMassEntityCurrentLocationPropertyFunctionInstanceData
{
	GENERATED_BODY()

	/** Mass entity wrapper whose current transform position will be extracted as a vector. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FMassEnvQueryEntityInfoBlueprintWrapper Input;

	/** Extracted FVector position from the entity's current transform. */
	UPROPERTY(EditAnywhere, Category = Output)
	FVector Output = FVector::ZeroVector;
};

/** Gets an FVector world-space location from a Mass entity wrapper using the entity's current transform position. */
USTRUCT(DisplayName = "Get Mass Entity Current Location", meta = (ToolTip = "Gets an FVector world-space location from a Mass entity wrapper using the entity's current transform position."))
struct FArcStateTreeMassEntityCurrentLocationPropertyFunction : public FStateTreePropertyFunctionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcStateTreeMassEntityCurrentLocationPropertyFunctionInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual void Execute(FStateTreeExecutionContext& Context) const override;
};
