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

#include "AbilitySystemInterface.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagAssetInterface.h"
#include "ArcAbilitySystemActor.generated.h"

USTRUCT(BlueprintType)
struct ARCCORE_API FArcAttributeSetWithTable
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere
		, Category = "Arc Core")
	TSubclassOf<class UAttributeSet> DefaultAttributeSetClass;

	UPROPERTY(EditAnywhere
		, Category = "Arc Core")
	TObjectPtr<class UDataTable> AttributeSetTable;

	FArcAttributeSetWithTable()
		: DefaultAttributeSetClass(nullptr)
		, AttributeSetTable(nullptr)
	{
	}
};

UCLASS()
class ARCCORE_API AArcAbilitySystemActor
		: public AActor
		, public IAbilitySystemInterface
		, public IGameplayTagAssetInterface
{
	GENERATED_BODY()

protected:
	/*
	 * If set to true, AbilitySystem will be spawned along with actor.
	 * If set to false, AbilitySystem will spawn on first usage.
	 */
	UPROPERTY(EditDefaultsOnly
		, Category = "Arc Core")
	bool bSpawnAbiltySystemOnBeginPlay;

	UPROPERTY(EditDefaultsOnly
		, Category = "Arc Core")
	TArray<FArcAttributeSetWithTable> DefaultAttributeSetsClasses;

	UPROPERTY()
	mutable TObjectPtr<class UAbilitySystemComponent> AbilitySystem;

public:
	// Sets default values for this actor's properties
	AArcAbilitySystemActor();

protected:
	virtual void PreInitializeComponents() override;

	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	virtual void GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const override;
};
