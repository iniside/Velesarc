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
#include "ArcMacroDefines.h"
#include "AttributeSet.h"
#include "GameplayEffectComponent.h"
#include "GameplayTagContainer.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "Subsystems/WorldSubsystem.h"
#include "UObject/ObjectKey.h"

#include "ArcWorldDelegates.generated.h"

class AActor;
class UClass;
class UActorComponent;
class AArcCorePlayerState;
struct FUniqueNetIdRepl;
struct FHitResult;
struct FArcGameplayAttributeChangedData;
class UArcCoreAbilitySystemComponent;
struct FGameplayAbilitySpec;
struct FArcActiveGameplayEffectForRPC;

DECLARE_MULTICAST_DELEGATE_OneParam(FArcGenericActorComponentDelegate, UActorComponent*);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FArcGenericActorComponentDynamicDelegate, UActorComponent*, Component);

DECLARE_MULTICAST_DELEGATE_OneParam(FArcGenericActorDelegate, AActor*);
DECLARE_MULTICAST_DELEGATE_OneParam(FArcCorePlayerStateReady, AArcCorePlayerState*);
DECLARE_MULTICAST_DELEGATE_TwoParams(FArcCorePlayerStateUniqedNetId, AArcCorePlayerState*, const FUniqueNetIdRepl&);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FArcAnyGlobalHitResultChangedDynamicDelegate, const FGameplayTag&, TargetingTag, const FHitResult&, HitResult);
DECLARE_MULTICAST_DELEGATE_OneParam(FArcAnyGlobalHitResultChangedDelegate, const FHitResult&);

DECLARE_MULTICAST_DELEGATE_OneParam(FArcOnAttributeChanged, const FArcGameplayAttributeChangedData& /*AttributeData*/);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FArcOnAttributeChangedDynamic, const FGameplayAttribute&, TargetAttribute, const FArcGameplayAttributeChangedData&, AttributeData);
DECLARE_MULTICAST_DELEGATE_TwoParams(FArcAbilitySystemAbilitySpecDelegate, FGameplayAbilitySpec& /*AbilitySpec*/, UArcCoreAbilitySystemComponent* /* InASC*/);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FArcAnyAttributeChangedDynamicDelegate, UArcCoreAbilitySystemComponent*, AbilitySystem, const FGameplayAttribute&, Attribute, float, OldValue, float, NewValue);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FArcGenericGameplayEffectDynamicDelegate, UArcCoreAbilitySystemComponent*, AbilitySystem, const FActiveGameplayEffect&, ActiveGE);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FArcGenericGameplayEffectTargetDynamicDelegate, AActor*, Target, const FArcActiveGameplayEffectForRPC&, ActiveGE);

struct ARCCORE_API FArcComponentChannelKey
{
private:
	const TObjectKey<AActor> OwningActor;
	const TObjectKey<UClass> ComponentClass;

public:
	FArcComponentChannelKey(AActor* InOwningActor, UClass* InComponentClass);

	bool operator==(const FArcComponentChannelKey& Other) const
	{
		return OwningActor == Other.OwningActor && ComponentClass == Other.ComponentClass;
	}

	friend uint32 GetTypeHash(const FArcComponentChannelKey& InKey)
	{
		return HashCombineFast(GetTypeHash(InKey.OwningActor), GetTypeHash(InKey.ComponentClass));
	}
};

/**
 * 
 */
UCLASS()
class ARCCORE_API UArcWorldDelegates : public UGameInstanceSubsystem
{
	GENERATED_BODY()
	
public:
	static UArcWorldDelegates* Get(UObject* WorldObject);

	DEFINE_ARC_CHANNEL_DELEGATE(FArcComponentChannelKey, Actor, FArcGenericActorComponentDelegate, OnComponentBeginPlay);

	DEFINE_ARC_DELEGATE(FArcCorePlayerStateReady, OnPlayerStateBeginPlay);

	DEFINE_ARC_CHANNEL_CLASS_DELEGATE(UClass*, Class, FArcGenericActorComponentDelegate, OnComponentBeginPlay);
	
	DEFINE_ARC_CHANNEL_CLASS_DELEGATE(UClass*, Class, FArcGenericActorDelegate, OnActorPreRegisterAllComponents);
	
	DEFINE_ARC_DELEGATE(FArcCorePlayerStateUniqedNetId, OnUniqueNetIdSet);
	
public:
	UPROPERTY(BlueprintAssignable)
	FArcGenericActorComponentDynamicDelegate OnAnyComponentBeginPlayDynamic;
	
	UPROPERTY(BlueprintAssignable)
	FArcAnyGlobalHitResultChangedDynamicDelegate OnAnyGlobalHitResultChaged;
	
	DEFINE_ARC_DELEGATE_MAP(FGameplayTag, FArcAnyGlobalHitResultChangedDelegate, OnGlobalHitResultChanged);

	DEFINE_ARC_DELEGATE_MAP(FGameplayAttribute, FArcOnAttributeChanged, OnTargetAttributeChanged);

public:
	UPROPERTY(BlueprintAssignable)
	FArcAnyAttributeChangedDynamicDelegate OnAnyAttributeChangedDynamic;
	
public:
	UPROPERTY(BlueprintAssignable)
	FArcOnAttributeChangedDynamic OnAnyTargetAttributeChanged;

	UPROPERTY(BlueprintAssignable)
	FArcGenericGameplayEffectDynamicDelegate OnAnyGameplayEffectAddedToSelf;

	UPROPERTY(BlueprintAssignable)
	FArcGenericGameplayEffectDynamicDelegate OnAnyGameplayEffectRemovedSelf;

	UPROPERTY(BlueprintAssignable)
	FArcGenericGameplayEffectTargetDynamicDelegate OnAnyGameplayEffectAddedToTarget;

	UPROPERTY(BlueprintAssignable)
	FArcGenericGameplayEffectTargetDynamicDelegate OnAnyGameplayEffectRemovedFromTarget;
	
	DEFINE_ARC_CHANNEL_DELEGATE(AActor*, Actor, FArcAbilitySystemAbilitySpecDelegate, OnAbilityGiven);
	
protected:
	/** Don't create this Subsystem for editor worlds. Only Game and PIE. */
	//virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
};