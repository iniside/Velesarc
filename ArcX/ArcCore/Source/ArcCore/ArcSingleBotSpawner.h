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


#include "Engine/DataAsset.h"
#include "GameFramework/Actor.h"
#include "StructUtils/InstancedStruct.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "StateTreeEvaluatorBase.h"
#include "StateTreeReference.h"

#include "ArcSingleBotSpawner.generated.h"

/*
 * Data fragment can add/modify functionality of spawned Bot. They are added AFTER bot is
 * possessed by AIController.
 */
USTRUCT(BlueprintType
	, meta = (Hidden))
struct ARCCORE_API FArcBotDataFragment
{
	GENERATED_BODY()

public:
	virtual void GiveFragment(class APawn* InPawn
							  , class AAIController* InController) const
	{
	};

	virtual ~FArcBotDataFragment()
	{
	}
};

USTRUCT(BlueprintType
	, meta = (Hidden))
struct ARCCORE_API FArcBotPawnComponentFragment
{
	GENERATED_BODY()

public:
	virtual void GiveComponent(class APawn* InPawn) const
	{
	};

	virtual ~FArcBotPawnComponentFragment()
	{
	}
};

USTRUCT(BlueprintType)
struct ARCCORE_API FArcBotPawnActorComponentFragment : public FArcBotPawnComponentFragment
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditAnywhere, Category = "Arc Core")
	TSubclassOf<class UActorComponent> Component;

public:
	virtual void GiveComponent(class APawn* InPawn) const override;

	virtual ~FArcBotPawnActorComponentFragment() override
	{
	}
};

/*
 * Run provided behavior tree on pawn.
 */
USTRUCT(BlueprintType)
struct ARCCORE_API FArcBotRunBehaviorTree : public FArcBotDataFragment
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditAnywhere, Category = "Arc Core")
	TObjectPtr<class UBehaviorTree> BehaviorTree;

public:
	FArcBotRunBehaviorTree()
		: BehaviorTree(nullptr)
	{
	};

	virtual void GiveFragment(class APawn* InPawn, class AAIController* InController) const override;

	virtual ~FArcBotRunBehaviorTree() override
	{
	}
};

class UStateTree;
USTRUCT(BlueprintType)
struct ARCCORE_API FArcBotRunStateTree : public FArcBotDataFragment
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditAnywhere, Category = AI)
	TObjectPtr<UStateTree> StateTree;;

public:
	FArcBotRunStateTree()
	{
	};

	virtual void GiveFragment(class APawn* InPawn, class AAIController* InController) const override;

	virtual ~FArcBotRunStateTree() override
	{
	}
};

USTRUCT()
struct FArcBotAttributeSet
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere
		, Category = "Arc Core")
	TSubclassOf<class UArcAttributeSet> AttributeSetClass;

	UPROPERTY(EditAnywhere
		, Category = "Arc Core"
		, meta = (RequiredAssetDataTags = "RowStructure=/Script/GameplayAbilities.AttributeMetaData"))
	TObjectPtr<class UDataTable> AttributeMetaData;

	FArcBotAttributeSet()
		: AttributeSetClass(nullptr)
		, AttributeMetaData(nullptr)
	{
	};
};

USTRUCT(BlueprintType)
struct ARCCORE_API FArcBotGiveAttributeSet : public FArcBotDataFragment
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditAnywhere
		, Category = "Arc Core")
	TArray<FArcBotAttributeSet> AttributeSets;

public:
	FArcBotGiveAttributeSet()
	{
	};

	virtual void GiveFragment(class APawn* InPawn
							  , class AAIController* InController) const override;

	virtual ~FArcBotGiveAttributeSet() override
	{
	}
};

USTRUCT(BlueprintType)
struct ARCCORE_API FArcBotGiveAbilities : public FArcBotDataFragment
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditAnywhere, Category = "Arc Core")
	TArray<TSubclassOf<class UGameplayAbility>> Abilities;

public:
	FArcBotGiveAbilities()
	{
	};

	virtual void GiveFragment(class APawn* InPawn
							  , class AAIController* InController) const override;

	virtual ~FArcBotGiveAbilities() override
	{
	}
};

USTRUCT(BlueprintType)
struct ARCCORE_API FArcBotGiveEffects : public FArcBotDataFragment
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditAnywhere, Category = "Arc Core")
	TArray<TSubclassOf<class UGameplayEffect>> GameplayEffects;

public:
	FArcBotGiveEffects()
	{
	};

	virtual void GiveFragment(class APawn* InPawn, class AAIController* InController) const override;

	virtual ~FArcBotGiveEffects() override
	{
	}
};

UCLASS()
class ARCCORE_API UArcBotData : public UDataAsset
{
	GENERATED_BODY()
	friend class UArcSingleBotSpawner;
	friend class UArcAsyncSpawnSingleBot;

protected:
	UPROPERTY(EditAnywhere
		, BlueprintReadOnly
		, Category = "Arc Core")
	float DelayBeforeRespawn = 1.f;

	UPROPERTY(EditAnywhere
		, Category = "Arc Core")
	TSoftClassPtr<class APawn> PawnClass;

	UPROPERTY(EditAnywhere
		, Category = "Arc Core")
	TSubclassOf<class AController> ControllerClass;

	UPROPERTY(EditAnywhere
		, Category = "Arc Core"
		, meta = (BaseStruct = "/Script/ArcCore.ArcBotDataFragment", ShowTreeView, ExcludeBaseStruct))
	TArray<FInstancedStruct> DataFragments;

	/*
	 * Component which will be given to pawn BEFORE it finishes spawning. They should be
	 * added before PreInitializeComponenets() is called.
	 */
	UPROPERTY(EditAnywhere
		, Category = "Arc Core"
		, meta = (BaseStruct = "/Script/ArcCore.ArcBotPawnComponentFragment", ShowTreeView, ExcludeBaseStruct))
	TArray<FInstancedStruct> PawnComponentsToGive;

	// UPROPERTY(EditAnywhere, Category = "Data", meta = (BaseStruct =
	// "ArcBotPawnComponentFragment")) TArray<FInstancedStruct>
	// ControllerComponentsToGive;

#if WITH_EDITOR
	/**
	 * @return		Returns Valid if this object has data validation rules set up for it
	 * and the data for this object is valid. Returns Invalid if it does not pass the
	 * rules. Returns NotValidated if no rules are set for this object.
	 */
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif // WITH_EDITOR
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FArcBotLifetimeDynamicMulticast);

/*
 * Actor Component added  to spawned to actor, to track it's lifetime.
 */
UCLASS(BlueprintType
	, ClassGroup = (ArcCore)
	, meta = (BlueprintSpawnableComponent))
class ARCCORE_API UArcBotSpawnerLifetimeComponent : public UActorComponent
{
	GENERATED_BODY()
	friend class UArcSingleBotSpawner;

protected:
	UPROPERTY(BlueprintAssignable)
	FArcBotLifetimeDynamicMulticast OnBotDestroyed;

	TWeakObjectPtr<class UArcSingleBotSpawner> SpawnedBy;

	virtual void OnRegister() override;

	UFUNCTION()
	void HandleOnOwnerDestroyed(AActor* DestroyedActor);
};

/*
 * Right now spawn single bot, no respawn.
 */
UCLASS(ClassGroup = (ArcCore)
	, meta = (BlueprintSpawnableComponent))
class ARCCORE_API UArcSingleBotSpawner : public UActorComponent
{
	GENERATED_BODY()
	friend class UArcBotSpawnerLifetimeComponent;

protected:
	/*
	 * Bot To Spawn.
	 */
	UPROPERTY(EditAnywhere
		, Category = "Arc Core")
	TSoftObjectPtr<class UArcBotData> BotData;

public:
	// Sets default values for this actor's properties
	UArcSingleBotSpawner();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	void OnBotDataLoaded();

	void RespawnBot();
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FArcAsyncSpawnSingleBotDynamicMulticast
	, class APawn*
	, Pawn
	, class AAIController*
	, Controller);

UCLASS(MinimalAPI)
class UArcAsyncSpawnSingleBot : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

protected:
	UPROPERTY(BlueprintAssignable)
	FArcAsyncSpawnSingleBotDynamicMulticast OnSpawned;

	TSoftObjectPtr<class UArcBotData> BotData;
	FTransform Transform;
	UWorld* World = nullptr;

public:
	UArcAsyncSpawnSingleBot();

	UFUNCTION(BlueprintCallable
		, meta = (WorldContext = "WorldContextObject", BlueprintInternalUseOnly = "true"))
	static UArcAsyncSpawnSingleBot* SpawnSingleBot(UObject* WorldContextObject
												   , TSoftObjectPtr<class UArcBotData> InBotData
												   , const FTransform& InTransform);

	virtual void Activate() override;

	void OnBotDataLoaded();
};
