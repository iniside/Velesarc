// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/PropertyFunctions/ArcAbilityPropertyFunctionBase.h"
#include "ArcStateTreeGetAbilitySocketLocationPropertyFunction.generated.h"

struct FStateTreeExecutionContext;

USTRUCT()
struct ARCABILITYKIT_API FArcStateTreeGetAbilitySocketLocationPropertyFunctionInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Parameter")
	FName SocketName = NAME_None;

	UPROPERTY(VisibleAnywhere, Category = "Output")
	FVector Location = FVector::ZeroVector;
};

USTRUCT(meta = (DisplayName = "Get Ability Socket Location"))
struct ARCABILITYKIT_API FArcStateTreeGetAbilitySocketLocationPropertyFunction
	: public FArcAbilityPropertyFunctionBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcStateTreeGetAbilitySocketLocationPropertyFunctionInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual void Execute(FStateTreeExecutionContext& Context) const override;
};
