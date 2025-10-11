#pragma once
#include "MassEQSBlueprintLibrary.h"
#include "StateTreePropertyFunctionBase.h"

#include "ArcStateTreeMassEntityVectorLocationPropertyFunction.generated.h"

USTRUCT()
struct FArcStateTreeMassEntityVectorLocationPropertyFunctionInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Parameter)
	FMassEnvQueryEntityInfoBlueprintWrapper Input;

	UPROPERTY(EditAnywhere, Category = Output)
	FVector Output;;
};

USTRUCT(DisplayName = "Get Mass Entity Vector Location")
struct FArcStateTreeMassEntityVectorLocationPropertyFunction : public FStateTreePropertyFunctionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcStateTreeMassEntityVectorLocationPropertyFunctionInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual void Execute(FStateTreeExecutionContext& Context) const override;

#if WITH_EDITOR
	//virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const;
#endif
};

