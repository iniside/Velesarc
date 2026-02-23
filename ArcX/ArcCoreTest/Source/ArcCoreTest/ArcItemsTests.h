/**
 * This file is part of Velesarc
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
#include "GameFramework/Actor.h"
#include "Items/ArcItemDefinition.h"
#include "Items/ArcItemsComponent.h"
#include "AttributeSet.h"
#include "GameplayEffectExtension.h"
#include "AbilitySystem/ArcAttributeSet.h"
#include "AbilitySystem/ArcAttributesTypes.h"
#include "AbilitySystem/ArcCoreAbilitySystemComponent.h"
#include "Items/ArcItemsStoreComponent.h"

#include "ArcItemsTests.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogArcTestLog, Log, All);

UCLASS()
class UArcTest_ItemWithSlots : public UArcItemDefinition
{
	GENERATED_BODY()
public:
	UArcTest_ItemWithSlots();
};

UCLASS()
class UArcCoreTestAttributeSet : public UArcAttributeSet
{
	GENERATED_BODY()
public:
	UPROPERTY(SaveGame)
	FGameplayAttributeData TestAttributeA;

	UPROPERTY(SaveGame)
	FGameplayAttributeData TestAttributeB;

	UPROPERTY(SaveGame)
	FGameplayAttributeData TestAttributeC;

	UPROPERTY(SaveGame)
	FGameplayAttributeData TestAttributeD;

	UPROPERTY(SaveGame)
	FGameplayAttributeData TestAttributeE;
	
	ARC_ATTRIBUTE_ACCESSORS(UArcCoreTestAttributeSet, TestAttributeA);
	ARC_ATTRIBUTE_ACCESSORS(UArcCoreTestAttributeSet, TestAttributeB);
	ARC_ATTRIBUTE_ACCESSORS(UArcCoreTestAttributeSet, TestAttributeC);
	ARC_ATTRIBUTE_ACCESSORS(UArcCoreTestAttributeSet, TestAttributeD);
	ARC_ATTRIBUTE_ACCESSORS(UArcCoreTestAttributeSet, TestAttributeE);
	
	UArcCoreTestAttributeSet() {};
};

UCLASS()
class UArcTestItemsStoreComponent : public UArcItemsStoreComponent
{
	GENERATED_BODY()

public:
	
};

class UArcQuickBarComponent;

UCLASS()
class AArcItemsTestActor : public AActor
	, public IAbilitySystemInterface
{
public:
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

private:
	GENERATED_BODY()
public:
	void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(Replicated)
	int32 ReplicatedInt;
	
public:	
	UPROPERTY()
	TObjectPtr<UArcItemsStoreComponent> ItemsStore;
	
	UPROPERTY()
	TObjectPtr<UArcTestItemsStoreComponent> ItemsStore2;

	UPROPERTY()
	TObjectPtr<UArcTestItemsStoreComponent> ItemsStoreNotReplicated;
	
	UPROPERTY()
	TObjectPtr<UArcCoreAbilitySystemComponent> AbilitySystemComponent3;

	UPROPERTY()
	TObjectPtr<UArcQuickBarComponent> QuickBarComponent;
	
	AArcItemsTestActor();
};