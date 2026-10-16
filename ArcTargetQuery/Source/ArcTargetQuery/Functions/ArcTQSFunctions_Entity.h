// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcTQSPropertyFunction.h"
#include "Mass/EntityHandle.h"
#include "ArcTQSFunctions_Entity.generated.h"

/** Current world location of Entity, or Fallback when the entity is invalid or has no transform. */
USTRUCT(meta = (DisplayName = "Entity Location", Category = "Entity"))
struct ARCTARGETQUERY_API FArcTQSFunction_EntityLocation : public FArcTQSPropertyFunction
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Input")
	FMassEntityHandle Entity;

	UPROPERTY(EditAnywhere, Category = "Input")
	FVector Fallback = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, Category = "Output", meta = (ArcTQSOutput))
	FVector Result = FVector::ZeroVector;

	virtual void Execute(const FArcTQSFunctionContext& Context) override;
};

/** Current forward direction of Entity, or Fallback when the entity is invalid or has no transform. */
USTRUCT(meta = (DisplayName = "Entity Forward", Category = "Entity"))
struct ARCTARGETQUERY_API FArcTQSFunction_EntityForward : public FArcTQSPropertyFunction
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Input")
	FMassEntityHandle Entity;

	UPROPERTY(EditAnywhere, Category = "Input")
	FVector Fallback = FVector::ForwardVector;

	UPROPERTY(VisibleAnywhere, Category = "Output", meta = (ArcTQSOutput))
	FVector Result = FVector::ForwardVector;

	virtual void Execute(const FArcTQSFunctionContext& Context) override;
};

/** True when Entity is a valid handle to a live entity. */
USTRUCT(meta = (DisplayName = "Entity Is Valid", Category = "Entity"))
struct ARCTARGETQUERY_API FArcTQSFunction_EntityIsValid : public FArcTQSPropertyFunction
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Input")
	FMassEntityHandle Entity;

	UPROPERTY(VisibleAnywhere, Category = "Output", meta = (ArcTQSOutput))
	bool Result = false;

	virtual void Execute(const FArcTQSFunctionContext& Context) override;
};
