// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcTQSPropertyFunction.h"
#include "GameplayTagContainer.h"
#include "Mass/EntityHandle.h"
#include "ArcTQSFunctions_Gameplay.generated.h"

/** Where Perceiver last perceived Entity, by sight or else hearing, or Fallback when it is not perceived. */
USTRUCT(meta = (DisplayName = "Perceived Last Location", Category = "Perception"))
struct ARCAI_API FArcTQSFunction_PerceivedLastLocation : public FArcTQSPropertyFunction
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Input")
	FMassEntityHandle Perceiver;

	UPROPERTY(EditAnywhere, Category = "Input")
	FMassEntityHandle Entity;

	UPROPERTY(EditAnywhere, Category = "Input")
	FVector Fallback = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, Category = "Output", meta = (ArcTQSOutput))
	FVector Result = FVector::ZeroVector;

	virtual void Execute(const FArcTQSFunctionContext& Context) override;
};

/** Location the querier remembers under Key, or Fallback when the key is absent or of another type, the querier has no memory or there is no querier. */
USTRUCT(meta = (DisplayName = "Memory Location", Category = "Memory"))
struct ARCAI_API FArcTQSFunction_MemoryLocation : public FArcTQSPropertyFunction
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Input", meta = (Categories = "AI.Memory", ArcMemoryAccess = "Read", ArcMemoryType = "/Script/CoreUObject.Vector"))
	FGameplayTag Key;

	UPROPERTY(EditAnywhere, Category = "Input")
	FVector Fallback = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, Category = "Output", meta = (ArcTQSOutput))
	FVector Result = FVector::ZeroVector;

	virtual void Execute(const FArcTQSFunctionContext& Context) override;
};

/** Entity the querier remembers under Key, or Fallback when the key is absent or of another type, the querier has no memory or there is no querier. */
USTRUCT(meta = (DisplayName = "Memory Entity", Category = "Memory"))
struct ARCAI_API FArcTQSFunction_MemoryEntity : public FArcTQSPropertyFunction
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Input", meta = (Categories = "AI.Memory", ArcMemoryAccess = "Read", ArcMemoryType = "/Script/MassCore.MassEntityHandle"))
	FGameplayTag Key;

	UPROPERTY(EditAnywhere, Category = "Input")
	FMassEntityHandle Fallback;

	UPROPERTY(VisibleAnywhere, Category = "Output", meta = (ArcTQSOutput))
	FMassEntityHandle Result;

	virtual void Execute(const FArcTQSFunctionContext& Context) override;
};

/** Float the querier remembers under Key, or Fallback when the key is absent or of another type, the querier has no memory or there is no querier. */
USTRUCT(meta = (DisplayName = "Memory Float", Category = "Memory"))
struct ARCAI_API FArcTQSFunction_MemoryFloat : public FArcTQSPropertyFunction
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Input", meta = (Categories = "AI.Memory", ArcMemoryAccess = "Read", ArcMemoryType = "/Script/ArcAI.ArcMemoryFloat"))
	FGameplayTag Key;

	UPROPERTY(EditAnywhere, Category = "Input")
	float Fallback = 0.0f;

	UPROPERTY(VisibleAnywhere, Category = "Output", meta = (ArcTQSOutput))
	float Result = 0.0f;

	virtual void Execute(const FArcTQSFunctionContext& Context) override;
};

/** Bool the querier remembers under Key, or Fallback when the key is absent or of another type, the querier has no memory or there is no querier. */
USTRUCT(meta = (DisplayName = "Memory Bool", Category = "Memory"))
struct ARCAI_API FArcTQSFunction_MemoryBool : public FArcTQSPropertyFunction
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Input", meta = (Categories = "AI.Memory", ArcMemoryAccess = "Read", ArcMemoryType = "/Script/ArcAI.ArcMemoryBool"))
	FGameplayTag Key;

	UPROPERTY(EditAnywhere, Category = "Input")
	bool bFallback = false;

	UPROPERTY(VisibleAnywhere, Category = "Output", meta = (ArcTQSOutput))
	bool bResult = false;

	virtual void Execute(const FArcTQSFunctionContext& Context) override;
};

/** Tag the querier remembers under Key, or Fallback when the key is absent or of another type, the querier has no memory or there is no querier. */
USTRUCT(meta = (DisplayName = "Memory Tag", Category = "Memory"))
struct ARCAI_API FArcTQSFunction_MemoryTag : public FArcTQSPropertyFunction
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Input", meta = (Categories = "AI.Memory", ArcMemoryAccess = "Read", ArcMemoryType = "/Script/GameplayTags.GameplayTag"))
	FGameplayTag Key;

	UPROPERTY(EditAnywhere, Category = "Input")
	FGameplayTag Fallback;

	UPROPERTY(VisibleAnywhere, Category = "Output", meta = (ArcTQSOutput))
	FGameplayTag Result;

	virtual void Execute(const FArcTQSFunctionContext& Context) override;
};
