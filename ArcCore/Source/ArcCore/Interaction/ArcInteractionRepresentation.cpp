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

#include "ArcInteractionRepresentation.h"

#include "ArcCoreInteractionSubsystem.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/LightWeightInstanceBlueprintFunctionLibrary.h"
#include "UObject/ObjectSaveContext.h"

// Sets default values
AArcInteractionRepresentation::AArcInteractionRepresentation()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;
}

void AArcInteractionRepresentation::PreSave(FObjectPreSaveContext SaveContext)
{
	Super::PreSave(SaveContext);

	if (GetLevel())
	{
		if (InteractionId.IsValid() == false)
		{
			InteractionId = FGuid::NewGuid();
		}
	}
}

// Called when the game starts or when spawned
void AArcInteractionRepresentation::BeginPlay()
{
	Super::BeginPlay();
	if (Owner == nullptr)
	{
		//FActorInstanceHandle Handle = ULightWeightInstanceBlueprintFunctionLibrary::ConvertActorToLightWeightInstance(this);
		//GetWorld()->GetSubsystem<UArcCoreInteractionSubsystem>()->ActorToId.FindOrAdd(Handle) = InteractionId;
		//GetWorld()->GetSubsystem<UArcCoreInteractionSubsystem>()->IdToActor.FindOrAdd(InteractionId) = Handle;
	}
	
	UArcCoreInteractionSubsystem* IS = GetWorld()->GetSubsystem<UArcCoreInteractionSubsystem>();
	if (IS)
	{
		
	}
}

// Called every frame
void AArcInteractionRepresentation::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

AArcInteractionInstances::AArcInteractionInstances(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bReplicates = false;

	ISMComponent = CreateDefaultSubobject<UInstancedStaticMeshComponent>("ISMComponent");
	RootComponent = ISMComponent;
}

void AArcInteractionInstances::PreSave(FObjectPreSaveContext ObjectSaveContext)
{
	Super::PreSave(ObjectSaveContext);
	if (GetOwner() == nullptr)
	{
		InteractionIdToInstanceIdx.Empty();
		InstanceIdxToInteractionId.Empty();

		for (int32 Idx = 0; Idx < ISMComponent->PerInstanceSMData.Num(); Idx++)
		{
			FGuid NewId = FGuid::NewGuid();
			InteractionIdToInstanceIdx.FindOrAdd(NewId) = Idx;
			InstanceIdxToInteractionId.FindOrAdd(Idx) = NewId;
		}
	}
}

void AArcInteractionInstances::BeginPlay()
{
	Super::BeginPlay();
	TArray<FArcCoreInteractionCustomData*> CustomData;
	UArcCoreInteractionSubsystem* InteractionSubsystem = GetWorld()->GetSubsystem<UArcCoreInteractionSubsystem>();
	for (const TPair<FGuid, int32>& Pair : InteractionIdToInstanceIdx)
	{
		InteractionSubsystem->AddInteractionLocal(Pair.Key, CustomData);
	}
}

UArcInteractionStateChangePreset* AArcInteractionInstances::GetInteractionStatePreset() const
{
	return InteractionStatePreset;
}

FGuid AArcInteractionInstances::GetInteractionId(const FHitResult& InHitResult) const
{
	return InstanceIdxToInteractionId[InHitResult.Item];
}

FVector AArcInteractionInstances::GetInteractionLocation(const FHitResult& InHitResult) const
{
	FTransform OutTransform;
	ISMComponent->GetInstanceTransform(InHitResult.Item, OutTransform, true);
	return OutTransform.GetLocation();
}

AArcInteractionPlaceable::AArcInteractionPlaceable()
{
	bReplicates = false;

	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;
}

void AArcInteractionPlaceable::PreSave(FObjectPreSaveContext SaveContext)
{
	if (InteractionId.IsValid() == false)
	{
		InteractionId = FGuid::NewGuid();
	}

	Super::PreSave(SaveContext);
}

void AArcInteractionPlaceable::BeginPlay()
{
	Super::BeginPlay();

	TArray<FArcCoreInteractionCustomData*> CustomData;
	for (FInstancedStruct& IS : InteractionCustomData)
	{
		void* Allocated = FMemory::Malloc(IS.GetScriptStruct()->GetCppStructOps()->GetSize()
			, IS.GetScriptStruct()->GetCppStructOps()->GetAlignment());
		IS.GetScriptStruct()->GetCppStructOps()->Construct(Allocated);
		IS.GetScriptStruct()->CopyScriptStruct(Allocated, IS.GetMemory());
		FArcCoreInteractionCustomData* Ptr = static_cast<FArcCoreInteractionCustomData*>(Allocated);

		CustomData.Add(Ptr);
	}
	UArcCoreInteractionSubsystem* InteractionSubsystem = GetWorld()->GetSubsystem<UArcCoreInteractionSubsystem>();
	InteractionSubsystem->AddInteractionLocal(InteractionId, CustomData);
}

UArcInteractionStateChangePreset* AArcInteractionPlaceable::GetInteractionStatePreset() const
{
	return InteractionStatePreset;
}

void AArcInteractionPlaceable::SetInteractionId(const FGuid& InId)
{
	InteractionId = InId;
}

FGuid AArcInteractionPlaceable::GetInteractionId(const FHitResult& InHitResult) const
{
	return InteractionId;
}

FVector AArcInteractionPlaceable::GetInteractionLocation(const FHitResult& InHitResult) const
{
	return GetActorLocation();
}
