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

#include "ArcInteractionRepresentation.h"
#include "StructUtils/InstancedStruct.h"
#include "Components/ActorComponent.h"
#include "Engine/CancellableAsyncAction.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Info.h"
#include "Iris/ReplicationState/IrisFastArraySerializer.h"
#include "Iris/ReplicationState/Private/IrisFastArraySerializerInternal.h"
#include "Items/ArcItemSpec.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "Subsystems/WorldSubsystem.h"
#include "ArcInteractionLevelPlacedComponent.generated.h"

class UArcInteractionStateChangePreset;
class USmartObjectComponent;
class APawn;
class AController;
class UArcInteractionReceiverComponent;

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
class ARCCORE_API UArcInteractionLevelPlacedComponent : public UActorComponent
	, public IArcInteractionObjectInterface
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere)
	FGuid InteractionId = FGuid();

	UPROPERTY(EditAnywhere)
	TObjectPtr<UArcInteractionStateChangePreset> InteractionStatePreset;

	UPROPERTY(VisibleAnywhere)
	FInstancedStruct LevelActorData;
	
	UPROPERTY(EditAnywhere, meta = (BaseStruct = "/Script/ArcCore.ArcCoreInteractionCustomData"))
	TArray<FInstancedStruct> InteractionCustomData;

public:
	UArcInteractionLevelPlacedComponent();
	
protected:
	virtual void PreSave(FObjectPreSaveContext SaveContext) override;
	
	virtual void BeginPlay() override;

public:
	// IArcInteractionObjectInterface - Start
	virtual UArcInteractionStateChangePreset* GetInteractionStatePreset() const override { return InteractionStatePreset; };

	virtual	void SetInteractionId(const FGuid& InId) override {}

	UFUNCTION(BlueprintPure, Category = "Arc Core | Interaction")
	virtual	FGuid GetInteractionId(const FHitResult& InHitResult) const override { return InteractionId; };

	virtual FVector GetInteractionLocation(const FHitResult& InHitResult) const override;
	
	// IArcInteractionObjectInterface - End
};

UCLASS(Blueprintable, ClassGroup = (Arc), meta = (BlueprintSpawnableComponent, LoadBehavior = "LazyOnDemand"))
class ARCCORE_API UArcInteractionDynamicSpawnedComponent : public UActorComponent
	, public IArcInteractionObjectInterface
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere)
	FGuid InteractionId = FGuid();

	UPROPERTY(EditAnywhere)
	TObjectPtr<UArcInteractionStateChangePreset> InteractionStatePreset;

	UPROPERTY(VisibleAnywhere)
	FInstancedStruct LevelActorData;
	
	UPROPERTY(EditAnywhere, meta = (BaseStruct = "/Script/ArcCore.ArcCoreInteractionCustomData"))
	TArray<FInstancedStruct> InteractionCustomData;

public:
	UArcInteractionDynamicSpawnedComponent();
	
protected:
	virtual void BeginPlay() override;

public:
	// IArcInteractionObjectInterface - Start
	virtual UArcInteractionStateChangePreset* GetInteractionStatePreset() const override { return InteractionStatePreset; };

	virtual	void SetInteractionId(const FGuid& InId) override { InteractionId = InId; }
	
	virtual	FGuid GetInteractionId(const FHitResult& InHitResult) const override { return InteractionId; };

	virtual FVector GetInteractionLocation(const FHitResult& InHitResult) const override;
	
	// IArcInteractionObjectInterface - End
};

struct FArcCoreInteractionCustomData;
/**
 * Proxy object pin will be hidden in K2Node_GameplayMessageAsyncAction. Is used to get a reference to the object triggering the delegate for the follow up call of 'GetPayload'.
 *
 * @param ActualChannel		The actual message channel that we received Payload from (will always start with Channel, but may be more specific if partial matches were enabled)
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FArcInteractionDataChangedDynamicDelegate, UArcAsyncAction_ListenInteractionDataChanged*, ProxyObject);

UCLASS(BlueprintType, meta=(HasDedicatedAsyncNode))
class ARCCORE_API UArcAsyncAction_ListenInteractionDataChanged : public UCancellableAsyncAction
{
	GENERATED_BODY()

public:
	/**
	 * Asynchronously waits for a gameplay message to be broadcast on the specified channel.
	 *
	 * @param Channel			The message channel to listen for
	 * @param PayloadType		The kind of message structure to use (this must match the same type that the sender is broadcasting)
	 * @param MatchType			The rule used for matching the channel with broadcasted messages
	 */
	UFUNCTION(BlueprintCallable, Category = Messaging, meta = (WorldContext = "WorldContextObject", BlueprintInternalUseOnly = "true"))
	static UArcAsyncAction_ListenInteractionDataChanged* ListenInteractionDataChanged(UObject* WorldContextObject, FGuid InInteractionId
		,UPARAM(meta = (BaseStruct = "ArcCoreInteractionCustomData")) UScriptStruct* PayloadType);

	/**
	 * Attempt to copy the payload received from the broadcasted gameplay message into the specified wildcard.
	 * The wildcard's type must match the type from the received message.
	 *
	 * @param OutPayload	The wildcard reference the payload should be copied into
	 * @return				If the copy was a success
	 */
	UFUNCTION(BlueprintCallable, CustomThunk, Category = "Messaging", meta = (CustomStructureParam = "OutPayload"))
	bool GetPayload(UPARAM(ref) int32& OutPayload);

	DECLARE_FUNCTION(execGetPayload);

	virtual void Activate() override;
	virtual void SetReadyToDestroy() override;

public:
	/** Called when a message is broadcast on the specified channel. Use GetPayload() to request the message payload. */
	UPROPERTY(BlueprintAssignable)
	FArcInteractionDataChangedDynamicDelegate OnMessageReceived;

private:
	void HandleMessageReceived(FArcCoreInteractionCustomData* Payload);

private:
	FDelegateHandle ListenerHandle;
	const void* ReceivedMessagePayloadPtr = nullptr;
	FGuid InteractionId;
	TWeakObjectPtr<UWorld> WorldPtr;
	TWeakObjectPtr<UScriptStruct> MessageStructType = nullptr;
};
