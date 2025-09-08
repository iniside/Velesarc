#pragma once
#include "ArcStateTreeMassTargetLocationPropertyFunction.h"
#include "MassEQSBlueprintLibrary.h"

#include "ArcStateTreeMassEntityLocationPropertyFunction.generated.h"

USTRUCT()
struct FStateTreeGetMassEntityLocationPropertyFunctionInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Parameter)
	FMassEnvQueryEntityInfoBlueprintWrapper Input;

	UPROPERTY(EditAnywhere, Category = Parameter)
	EMassMovementAction EndOfPathIntent = EMassMovementAction::Move;
	
	UPROPERTY(EditAnywhere, Category = Output)
	FMassTargetLocation Output;;
};

USTRUCT(DisplayName = "Get Mass Entity Location")
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
