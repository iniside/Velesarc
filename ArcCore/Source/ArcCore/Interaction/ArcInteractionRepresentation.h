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

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/LightWeightInstanceStaticMeshManager.h"
#include "StructUtils/InstancedStruct.h"
#include "ArcInteractionRepresentation.generated.h"

class UArcInteractionStateChangePreset;

UINTERFACE(BlueprintType)
class ARCCORE_API UArcInteractionObjectInterface : public UInterface
{
	GENERATED_BODY()
};

class ARCCORE_API IArcInteractionObjectInterface
{
	GENERATED_BODY()

public:
	virtual UArcInteractionStateChangePreset* GetInteractionStatePreset() const = 0;

	virtual	void SetInteractionId(const FGuid& InId) {}

	virtual	FGuid GetInteractionId(const FHitResult& InHitResult) const = 0;

	virtual FVector GetInteractionLocation(const FHitResult& InHitResult) const = 0;
};

/*
 * Base class to use with Lightweight instance for actors which should represent interaction within world.
 * TODO: If not used, and loaded, it should return to LWI form after some time (or some other condition ?)
 */
UCLASS()
class ARCCORE_API AArcInteractionRepresentation : public AActor
{
	GENERATED_BODY()

	friend class AArcInteractionLWI;
	
	UPROPERTY(VisibleAnywhere)
	FGuid InteractionId;

	UPROPERTY(EditAnywhere)
	TSoftObjectPtr<UStaticMesh> MeshRepresentation;
	
public:
	// Sets default values for this actor's properties
	AArcInteractionRepresentation();

	virtual void PreSave(FObjectPreSaveContext SaveContext) override;
	
protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;
};

class UInstancedStaticMeshComponent;

UCLASS()
class ARCCORE_API AArcInteractionInstances : public AActor, public IArcInteractionObjectInterface
{
	GENERATED_BODY()
	
protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Instances, AdvancedDisplay, meta = (BlueprintProtected = "true", AllowPrivateAccess = "true"))
	TObjectPtr<UInstancedStaticMeshComponent> ISMComponent;

	UPROPERTY(EditAnywhere)
	TObjectPtr<UArcInteractionStateChangePreset> InteractionStatePreset;

	UPROPERTY(EditAnywhere, meta = (BaseStruct = "/Script/ArcCore.ArcCoreInteractionCustomData"))
	TArray<FInstancedStruct> InteractionCustomData;
	
	UPROPERTY()
	TMap<FGuid, int32> InteractionIdToInstanceIdx;

	UPROPERTY()
	TMap<int32, FGuid> InstanceIdxToInteractionId;
	
public:
	AArcInteractionInstances(const FObjectInitializer& ObjectInitializer);
	
	virtual void PreSave(FObjectPreSaveContext ObjectSaveContext) override;
	virtual void BeginPlay() override;

	virtual UArcInteractionStateChangePreset* GetInteractionStatePreset() const override;

	virtual	FGuid GetInteractionId(const FHitResult& InHitResult) const override;

	virtual FVector GetInteractionLocation(const FHitResult& InHitResult) const override;
	// IArcInteractionObjectInterface -- End
};

/**
 * Base class for interactions which are statically placed on level. 
 * Do not use with any kind of dynamic runtime placement !
 */
UCLASS()
class ARCCORE_API AArcInteractionPlaceable : public AActor, public IArcInteractionObjectInterface
{
	GENERATED_BODY()

protected:
	UPROPERTY(VisibleAnywhere)
	FGuid InteractionId;

	UPROPERTY(EditAnywhere)
	TObjectPtr<UArcInteractionStateChangePreset> InteractionStatePreset;

	UPROPERTY(EditAnywhere, meta = (BaseStruct = "/Script/ArcCore.ArcCoreInteractionCustomData"))
	TArray<FInstancedStruct> InteractionCustomData;

public:
	// Sets default values for this actor's properties
	AArcInteractionPlaceable();

	virtual void PreSave(FObjectPreSaveContext SaveContext) override;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// IArcInteractionObjectInterface -- Start
	virtual UArcInteractionStateChangePreset* GetInteractionStatePreset() const override;
	
	virtual	void SetInteractionId(const FGuid& InId) override;

	virtual	FGuid GetInteractionId(const FHitResult& InHitResult) const override;

	virtual FVector GetInteractionLocation(const FHitResult& InHitResult) const override;
	// IArcInteractionObjectInterface -- End
};