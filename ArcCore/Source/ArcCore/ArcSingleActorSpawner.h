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
#include "ArcSingleActorSpawner.generated.h"

UCLASS()
class ARCCORE_API UArcSingleActorSpawnData : public UDataAsset
{
	GENERATED_BODY()
	friend class AArcSingleActorSpawner;
	friend class UArcSingleActorSpawnerLifetimeComponent;

protected:
	UPROPERTY(EditAnywhere
		, Category = "Arc Core")
	TSubclassOf<AActor> ActorClass;

	UPROPERTY(EditAnywhere
		, Category = "Arc Core")
	bool bDestroySpawnerOnActorSpawn = false;

	UPROPERTY(EditAnywhere
		, Category = "Arc Core")
	bool bSnapToFloor = true;

	UPROPERTY(EditAnywhere
		, Category = "Arc Core"
		, meta = (EditCondition = "bSnapToFloor", EditConditionHides))
	float HeightOffset = 0;

	UPROPERTY(EditAnywhere
		, Category = "Arc Core"
		, meta = (EditCondition = "bDestroySpawnerOnActorSpawn==false", EditConditionHides))
	bool bAutoRespawnActor = false;

	UPROPERTY(EditAnywhere
		, Category = "Arc Core"
		, meta = (EditCondition = "bDestroySpawnerOnActorSpawn==false", EditConditionHides))
	float DelayBeforeRespawn = 1.0f;
};

/*
 * Actor Component added  to spawned to actor, to track it's lifetime.
 */
UCLASS()
class ARCCORE_API UArcSingleActorSpawnerLifetimeComponent : public UActorComponent
{
	GENERATED_BODY()
	friend class AArcSingleActorSpawner;

protected:
	TWeakObjectPtr<class AArcSingleActorSpawner> SpawnedBy;

	virtual void OnRegister() override;

	UFUNCTION()
	void HandleOnOwnerDestroyed(AActor* DestroyedActor);
};

UCLASS()
class ARCCORE_API AArcSingleActorSpawner : public AActor
{
	GENERATED_BODY()
	friend class UArcSingleActorSpawnerLifetimeComponent;

protected:
	UPROPERTY(EditAnywhere
		, Category = "Arc Core")
	TSoftObjectPtr<UArcSingleActorSpawnData> ActorData;

	UPROPERTY(EditAnywhere
		, Category = "Arc Core")
	bool bSpawnOnAuthority = true;

	UPROPERTY(EditAnywhere
		, Category = "Arc Core"
		, meta = (MakeEditWidget))
	FTransform SpawnTransform;

	UPROPERTY()
	TObjectPtr<UArcSingleActorSpawnData> LoadedData = nullptr;

public:
	// Sets default values for this actor's properties
	AArcSingleActorSpawner();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	void SpawnActor();

	void RespawnActor();
};
