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

#include "ArcSingleActorSpawner.h"

#include "Engine/World.h"
#include "TimerManager.h"

#include "Core/ArcCoreAssetManager.h"
#include "Engine/HitResult.h"

void UArcSingleActorSpawnerLifetimeComponent::OnRegister()
{
	Super::OnRegister();
	GetOwner()->OnDestroyed.AddDynamic(this
		, &UArcSingleActorSpawnerLifetimeComponent::HandleOnOwnerDestroyed);
}

void UArcSingleActorSpawnerLifetimeComponent::HandleOnOwnerDestroyed(AActor* DestroyedActor)
{
	if (SpawnedBy.IsValid())
	{
		SpawnedBy->RespawnActor();
	}
}

// Sets default values
AArcSingleActorSpawner::AArcSingleActorSpawner()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve
	// performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;
}

// Called when the game starts or when spawned
void AArcSingleActorSpawner::BeginPlay()
{
	Super::BeginPlay();

	if (ActorData.IsNull() == true)
	{
		return;
	}

	SpawnActor();
}

void AArcSingleActorSpawner::SpawnActor()
{
	bool bHaveAuthority = HasAuthority();
	if (bHaveAuthority == false && bSpawnOnAuthority == true)
	{
		return;
	}
	if (bHaveAuthority == true && bSpawnOnAuthority == false)
	{
		return;
	}
	TArray<FSoftObjectPath> AssetToLoad;
	AssetToLoad.Add(ActorData.ToSoftObjectPath());

	TSharedPtr<FStreamableHandle> StreamableHandle;

	auto OnAssetLoaded = [this, StreamableHandle]()
	{
		LoadedData = ActorData.Get();
		FTransform SpawnTM = GetActorTransform() + SpawnTransform;
		SpawnTM.SetRotation(GetActorRotation().Quaternion());
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride =
				ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

		AActor* SpawnedActor = GetWorld()->SpawnActor<AActor>(LoadedData->ActorClass
			, SpawnTM
			, SpawnParameters);
		FBoxSphereBounds Bounds = SpawnedActor->GetRootComponent()->GetPlacementExtent();

		FHitResult OutHit;
		FVector EndLocation = SpawnTM.GetLocation() + FVector(0
								  , 0
								  , -1) * 1000;

		FCollisionQueryParams QueryParams;
		QueryParams.AddIgnoredActor(SpawnedActor);
		QueryParams.AddIgnoredActor(this);
		FCollisionResponseParams CollisionResponse;
		CollisionResponse.CollisionResponse = ECR_Block;

		GetWorld()->LineTraceSingleByChannel(OutHit
			, SpawnTM.GetLocation()
			, EndLocation
			, ECC_Visibility
			, QueryParams
			, CollisionResponse);

		if (OutHit.bBlockingHit)
		{
			FVector NewLocation = OutHit.ImpactPoint + FVector(0
									  , 0
									  , Bounds.SphereRadius + LoadedData->HeightOffset);
			SpawnedActor->SetActorLocation(NewLocation);
		}

		if (LoadedData->bAutoRespawnActor)
		{
			UArcSingleActorSpawnerLifetimeComponent* Comp = NewObject<UArcSingleActorSpawnerLifetimeComponent>(
				SpawnedActor
				, UArcSingleActorSpawnerLifetimeComponent::StaticClass()
				, UArcSingleActorSpawnerLifetimeComponent::StaticClass()->GetFName());
			Comp->RegisterComponentWithWorld(GetWorld());
			Comp->SpawnedBy = this;
		}
		// Comp->SetNetAddressable();
		// Comp->SetIsReplicated(false);

		if (LoadedData->bDestroySpawnerOnActorSpawn == true)
		{
			Destroy();
		}
	};

	StreamableHandle = UArcCoreAssetManager::Get().LoadAssetList(AssetToLoad
		, FStreamableDelegate::CreateLambda(OnAssetLoaded));
}

void AArcSingleActorSpawner::RespawnActor()
{
	if (LoadedData)
	{
		if (LoadedData->DelayBeforeRespawn > 0.f)
		{
			FTimerHandle Handle;
			if (GetWorld() != nullptr)
			{
				GetWorld()->GetTimerManager().SetTimer(Handle
					, FTimerDelegate::CreateUObject(this
						, &AArcSingleActorSpawner::SpawnActor)
					, LoadedData->DelayBeforeRespawn
					, false);
			}
		}
		else
		{
			SpawnActor();
		}
	}
}
