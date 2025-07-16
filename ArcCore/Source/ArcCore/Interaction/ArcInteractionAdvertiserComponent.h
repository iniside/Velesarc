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

#include "StructUtils/InstancedStruct.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Info.h"
#include "Iris/ReplicationState/IrisFastArraySerializer.h"
#include "Iris/ReplicationState/Private/IrisFastArraySerializerInternal.h"
#include "Items/ArcItemSpec.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "Subsystems/WorldSubsystem.h"
#include "ArcInteractionAdvertiserComponent.generated.h"

class UArcInteractionStateChangePreset;
class USmartObjectComponent;
class APawn;
class AController;
class UArcInteractionReceiverComponent;
class UArcInteractionAdvertiserComponent;

struct FSmartObjectEventData;

/**
 * Helper component, which advertises if object can be interacted with. It can work with SmartObjects
 * then this component, will check if Smart Object can be interacted with (having slots etc), and then replicate if interaction is possible.
 *
 * If no Smart Object is preset, will check if there is interacting pawn with component, limiting interaction to max 1.
 *
 * Or everyone can interact at the same time.
 */
UCLASS(Blueprintable, ClassGroup = (Arc), meta = (BlueprintSpawnableComponent, LoadBehavior = "LazyOnDemand"))
class ARCCORE_API UArcInteractionAdvertiserComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere)
	FGuid InteractionId = FGuid();
	/**
	 * If true will set owning actor ENetDormancy::DORM_DormantAll, since we want interactables to be dormant and
	 * only replicate from time to time.
	 */
	UPROPERTY(EditAnywhere)
	bool bSetOwnerDormant = true;

	/**
	 * @brief If true interaction will be able to claim this. By either using Smart Object or setting pawn.
	 * Limiting possible Interactors at the same time.
	 *
	 * If false, everyone can interact at the same time.
	 */
	UPROPERTY(EditAnywhere)
	bool bAllowClaim = false;


	UPROPERTY(EditAnywhere)
	TObjectPtr<UArcInteractionStateChangePreset> InteractionStatePreset;

	UPROPERTY(EditAnywhere, meta = (BaseStruct = "/Script/ArcCore.ArcCoreInteractionCustomData"))
	FInstancedStruct InteractionCustomData;

	UPROPERTY(EditAnywhere, meta = (BaseStruct = "/Script/ArcCore.ArcInteractionData_LevelActor"))
	FInstancedStruct LevelActor;
	
protected:
	TWeakObjectPtr<USmartObjectComponent> SmartObjectComponent;

	UPROPERTY(Replicated)
	TObjectPtr<APawn> CurrentInteractingPawn;

	UPROPERTY(Replicated)
	bool bIsClaimed;

	UFUNCTION(BlueprintImplementableEvent)
	void OnInteractionInstigatorSet(APawn* OutPawn);

public:
	APawn* GetCurrentInteractingPawn() const
	{
		return CurrentInteractingPawn;
	}
	
	UArcInteractionAdvertiserComponent();

	virtual void OnRegister() override;
	
	virtual void PreSave(FObjectPreSaveContext SaveContext);
	
	void SetInteractionInstigator(class APawn* InPawn);

	bool IsClaimed(APawn* InPawnTrying) const;

	void OnSelected(APawn* InPawn
					, AController* InController
					, UArcInteractionReceiverComponent* InReceiver) const;

	void OnDeselected(APawn* InPawn
					  , AController* InController
					  , UArcInteractionReceiverComponent* InReceiver) const;
	
	const FGuid& GetInteractionId() const
	{
		return InteractionId;
	}
protected:
	virtual void BeginPlay() override;

	void HandleOnSmartObjectChanged(const FSmartObjectEventData& EventData, const AActor* Interactor);
};
