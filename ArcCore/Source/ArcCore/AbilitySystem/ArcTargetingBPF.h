/**
 * This file is part of ArcX.
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or –
 * as soon as they will be approved by the European Commission – later versions
 * of the EUPL (the "License");
 *
 * You may not use this work except in compliance with the License.
 * You may get a copy of the License at:
 *
 * https://eupl.eu/
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 */

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "Types/TargetingSystemTypes.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "ArcTargetingBPF.generated.h"

/**
 *
 */
UCLASS()
class ARCCORE_API UArcTargetingBPF : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable
			, Category = "Arc Core|Targeting|Ability Target Handle"
			, meta = (BlueprintThreadSafe))
	static FGameplayAbilityTargetDataHandle SingleTargetHitFromHitArray(class UGameplayAbility* SourceAbility
																	, const TArray<FHitResult>& InHits);

	/*
	 * Get first actor from provided Target Data.
	 **/
	UFUNCTION(BlueprintPure, Category = "Arc Core|Targeting")
	static AActor* GetActorFromTargetData(const FGameplayAbilityTargetDataHandle& TargetData);

	UFUNCTION(BlueprintPure, Category = "Arc Core|Targeting")
	static FGameplayAbilityTargetDataHandle HitResultArray(const TArray<FHitResult>& HitResults);

	UFUNCTION(BlueprintPure, Category = "Arc Core|Targeting")
	static FGameplayAbilityTargetDataHandle MakeProjectileTargetData(FTargetingRequestHandle InHandle);

	template<typename T>
	static const T* FindTargetData(const FGameplayAbilityTargetDataHandle& InHandle)
	{
		for (const TSharedPtr<FGameplayAbilityTargetData>& Data : InHandle.Data)
		{
			if (Data && Data->GetScriptStruct() == T::StaticStruct())
			{
				return static_cast<const T*>(Data);
			}
		}
		
		return nullptr;
	}

	UFUNCTION(BlueprintPure, Category = "Arc Core|Targeting")
	static FGameplayAbilityTargetDataHandle MakeActorSpawnLocation(FTargetingRequestHandle InHandle);
};

USTRUCT(BlueprintType)
struct FArcGameplayAbilityTargetData_ProjectileTargetHit : public FGameplayAbilityTargetData
{
	GENERATED_USTRUCT_BODY()

	FArcGameplayAbilityTargetData_ProjectileTargetHit()
	{ }

	FArcGameplayAbilityTargetData_ProjectileTargetHit(FHitResult&& InHitResult, const FVector& InOrigin)
		: HitResult(MoveTemp(InHitResult))
		, Origin(InOrigin)
	{ }

	// -------------------------------------

	virtual TArray<TWeakObjectPtr<AActor> >	GetActors() const override
	{
		TArray<TWeakObjectPtr<AActor> >	Actors;
		if (HitResult.HasValidHitObjectHandle())
		{
			Actors.Push(HitResult.HitObjectHandle.FetchActor());
		}
		return Actors;
	}

	// SetActors() will not work here because the actor "array" is drawn from the hit result data, and changing that doesn't make sense.

	// -------------------------------------

	virtual bool HasHitResult() const override
	{
		return true;
	}

	virtual const FHitResult* GetHitResult() const override
	{
		return &HitResult;
	}

	virtual bool HasOrigin() const override
	{
		return true;
	}

	virtual FTransform GetOrigin() const override
	{
		return FTransform(FQuat::Identity, Origin);
	}

	virtual bool HasEndPoint() const override
	{
		return true;
	}

	virtual FVector GetEndPoint() const override
	{
		return HitResult.Location;
	}

	virtual void ReplaceHitWith(AActor* NewHitActor, const FHitResult* NewHitResult)
	{
		bHitReplaced = true;

		HitResult = FHitResult();
		if (NewHitResult != nullptr)
		{
			HitResult = *NewHitResult;
		}
	}

	// -------------------------------------

	/** Hit result that stores data */
	UPROPERTY()
	FHitResult	HitResult;

	UPROPERTY()
	FVector Origin = FVector::BackwardVector;
	
	UPROPERTY(NotReplicated)
	bool bHitReplaced = false;

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FArcGameplayAbilityTargetData_ProjectileTargetHit::StaticStruct();
	}
};

USTRUCT(BlueprintType)
struct FArcGameplayAbilityTargetData_Origin : public FGameplayAbilityTargetData
{
	GENERATED_USTRUCT_BODY()

	FArcGameplayAbilityTargetData_Origin()
	{ }

	FArcGameplayAbilityTargetData_Origin(const FVector& InOrigin)
		: Origin(InOrigin)
	{ }

	// -------------------------------------

	virtual TArray<TWeakObjectPtr<AActor> >	GetActors() const override
	{
		return TArray<TWeakObjectPtr<AActor> >();
	}

	// SetActors() will not work here because the actor "array" is drawn from the hit result data, and changing that doesn't make sense.

	// -------------------------------------

	virtual bool HasHitResult() const override
	{
		return false;
	}

	virtual bool HasOrigin() const override
	{
		return true;
	}

	virtual FTransform GetOrigin() const override
	{
		return FTransform(FQuat::Identity, Origin);
	}

	virtual bool HasEndPoint() const override
	{
		return false;
	}

	UPROPERTY()
	FVector Origin = FVector::BackwardVector;

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FArcGameplayAbilityTargetData_Origin::StaticStruct();
	}
};