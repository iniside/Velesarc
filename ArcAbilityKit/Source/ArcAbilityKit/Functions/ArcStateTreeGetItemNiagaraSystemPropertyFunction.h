// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StateTreePropertyFunctionBase.h"

class UNiagaraSystem;
struct FStateTreeExecutionContext;

#include "ArcStateTreeGetItemNiagaraSystemPropertyFunction.generated.h"

USTRUCT()
struct ARCABILITYKIT_API FArcStateTreeGetItemNiagaraSystemPropertyFunctionInstanceData
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = "Output")
	TObjectPtr<UNiagaraSystem> Output = nullptr;
};

USTRUCT(meta = (DisplayName = "Get Item Niagara System"))
struct ARCABILITYKIT_API FArcStateTreeGetItemNiagaraSystemPropertyFunction
	: public FStateTreePropertyFunctionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcStateTreeGetItemNiagaraSystemPropertyFunctionInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual void Execute(FStateTreeExecutionContext& Context) const override;
};
