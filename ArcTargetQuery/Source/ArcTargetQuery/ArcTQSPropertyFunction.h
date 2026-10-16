// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/EntityHandle.h"
#include "ArcTQSPropertyFunction.generated.h"

struct FMassEntityManager;
class UWorld;

struct ARCTARGETQUERY_API FArcTQSFunctionContext
{
	FMassEntityManager* EntityManager = nullptr;
	UWorld* World = nullptr;
	FMassEntityHandle Self;
};

USTRUCT(meta = (Hidden))
struct ARCTARGETQUERY_API FArcTQSPropertyFunction
{
	GENERATED_BODY()

	virtual ~FArcTQSPropertyFunction() = default;

	virtual void Execute(const FArcTQSFunctionContext& Context)
	{
	}

	UPROPERTY()
	FGuid ID;
};

namespace ArcTQS
{
#if WITH_EDITOR
	ARCTARGETQUERY_API bool IsFunctionOutputProperty(const FProperty& Property);
	ARCTARGETQUERY_API const FProperty* FindFunctionOutputProperty(const UStruct* FunctionStruct, int32* OutNumOutputs = nullptr);
#endif
}
