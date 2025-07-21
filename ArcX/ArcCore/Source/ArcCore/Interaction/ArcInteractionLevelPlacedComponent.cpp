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

#include "ArcInteractionLevelPlacedComponent.h"
#include "ArcCore/Interaction/ArcInteractionReceiverComponent.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"

#include "AbilitySystemComponent.h"
#include "ArcCoreInteractionSubsystem.h"
#include "ArcInteractionStateChange.h"
#include "ArcInteractionStateChangePreset.h"
#include "SmartObjectComponent.h"
#include "SmartObjectSubsystem.h"
#include "GameFramework/PlayerState.h"
#include "Items/ArcItemsComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "GameFeatures/GameFeatureAction_WorldActionBase.h"
#include "UObject/ObjectSaveContext.h"

UArcInteractionLevelPlacedComponent::UArcInteractionLevelPlacedComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	LevelActorData = FInstancedStruct::Make<FArcInteractionData_LevelActor>();
}

void UArcInteractionLevelPlacedComponent::PreSave(FObjectPreSaveContext SaveContext)
{
	if (GetOwner())
	{
		if (GetOwner()->GetLevel())
		{
			if (InteractionId.IsValid() == false)
			{
				InteractionId = FGuid::NewGuid();
				return;
			}		
		}
		else
		{
			InteractionId.Invalidate();	
		}
	}
	else
	{
		InteractionId.Invalidate();	
	}
	

	Super::PreSave(SaveContext);
}

void UArcInteractionLevelPlacedComponent::BeginPlay()
{
	Super::BeginPlay();
	
	if (GetOwner()->HasAuthority())
	{
		TArray<FArcCoreInteractionCustomData*> Datas;
		Datas.Reserve(InteractionCustomData.Num() + 1);
		{
			FArcInteractionData_LevelActor* Data = LevelActorData.GetMutablePtr<FArcInteractionData_LevelActor>();
			Data->LevelActor = GetOwner();
			Data->LevelActorLocal = GetOwner();
			
			void* Allocated = FMemory::Malloc(LevelActorData.GetScriptStruct()->GetCppStructOps()->GetSize()
						, LevelActorData.GetScriptStruct()->GetCppStructOps()->GetAlignment());
			LevelActorData.GetScriptStruct()->GetCppStructOps()->Construct(Allocated);
			LevelActorData.GetScriptStruct()->CopyScriptStruct(Allocated, LevelActorData.GetMemory());
			
			Datas.Add(static_cast<FArcCoreInteractionCustomData*>(Allocated));
		}
		for (FInstancedStruct& CustomData : InteractionCustomData)
		{
			void* Allocated = FMemory::Malloc(CustomData.GetScriptStruct()->GetCppStructOps()->GetSize()
						, CustomData.GetScriptStruct()->GetCppStructOps()->GetAlignment());
			CustomData.GetScriptStruct()->GetCppStructOps()->Construct(Allocated);
			CustomData.GetScriptStruct()->CopyScriptStruct(Allocated, CustomData.GetMemory());
			
			Datas.Add(static_cast<FArcCoreInteractionCustomData*>(Allocated));
		}

		GetWorld()->GetSubsystem<UArcCoreInteractionSubsystem>()->AddInteraction(InteractionId, MoveTemp(Datas));
	}
}

FVector UArcInteractionLevelPlacedComponent::GetInteractionLocation(const FHitResult& InHitResult) const
{
	return GetOwner()->GetActorLocation();
}

UArcInteractionDynamicSpawnedComponent::UArcInteractionDynamicSpawnedComponent()
{
}

void UArcInteractionDynamicSpawnedComponent::BeginPlay()
{
	Super::BeginPlay();

	if (GetOwner())
	{
		if (InteractionId.IsValid() == false)
		{
			InteractionId = FGuid::NewGuid();
			return;
		}
	}
	else
	{
		InteractionId.Invalidate();	
	}
}

FVector UArcInteractionDynamicSpawnedComponent::GetInteractionLocation(const FHitResult& InHitResult) const
{
	return GetOwner()->GetActorLocation();
}

UArcAsyncAction_ListenInteractionDataChanged* UArcAsyncAction_ListenInteractionDataChanged::ListenInteractionDataChanged(UObject* WorldContextObject, FGuid InInteractionId, UScriptStruct* PayloadType)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!World)
	{
		return nullptr;
	}

	UArcAsyncAction_ListenInteractionDataChanged* Action = NewObject<UArcAsyncAction_ListenInteractionDataChanged>();
	Action->WorldPtr = World;
	Action->InteractionId = InInteractionId;
	Action->MessageStructType = PayloadType;
	Action->RegisterWithGameInstance(World);

	return Action;
}

void UArcAsyncAction_ListenInteractionDataChanged::Activate()
{
	if (UWorld* World = WorldPtr.Get())
	{
		if (UArcCoreInteractionSubsystem* Subsystem = World->GetSubsystem<UArcCoreInteractionSubsystem>())
		{
			ListenerHandle = Subsystem->AddDataTypeOnInteractionDataChangedMap(InteractionId, MessageStructType.Get()
				, FArcInteractionDataDelegate::FDelegate::CreateUObject(this, &UArcAsyncAction_ListenInteractionDataChanged::HandleMessageReceived));

			FArcCoreInteractionCustomData* Ptr = Subsystem->GetInteractionData(InteractionId, MessageStructType.Get());
			if (Ptr)
			{
				HandleMessageReceived(Ptr);
			}
			return;
		}
	}

	SetReadyToDestroy();
}

void UArcAsyncAction_ListenInteractionDataChanged::SetReadyToDestroy()
{
	if (UWorld* World = WorldPtr.Get())
	{
		if (UArcCoreInteractionSubsystem* Subsystem = World->GetSubsystem<UArcCoreInteractionSubsystem>())
		{
			Subsystem->RemoveDataTypeOnInteractionDataChangedMap(InteractionId, MessageStructType.Get(), ListenerHandle);
		}
	}

	Super::SetReadyToDestroy();
}

bool UArcAsyncAction_ListenInteractionDataChanged::GetPayload(int32& OutPayload)
{
	checkNoEntry();
	return false;
}

DEFINE_FUNCTION(UArcAsyncAction_ListenInteractionDataChanged::execGetPayload)
{
	Stack.MostRecentPropertyAddress = nullptr;
	Stack.StepCompiledIn<FStructProperty>(nullptr);
	void* MessagePtr = Stack.MostRecentPropertyAddress;
	FStructProperty* StructProp = CastField<FStructProperty>(Stack.MostRecentProperty);
	P_FINISH;

	bool bSuccess = false;

	// Make sure the type we are trying to get through the blueprint node matches the type of the message payload received.
	if ((StructProp != nullptr) && (StructProp->Struct != nullptr) && (MessagePtr != nullptr) && (StructProp->Struct == P_THIS->MessageStructType.Get()) && (P_THIS->ReceivedMessagePayloadPtr != nullptr))
	{
		StructProp->Struct->CopyScriptStruct(MessagePtr, P_THIS->ReceivedMessagePayloadPtr);
		bSuccess = true;
	}

	*(bool*)RESULT_PARAM = bSuccess;
}

void UArcAsyncAction_ListenInteractionDataChanged::HandleMessageReceived(FArcCoreInteractionCustomData* Payload)
{
	if (MessageStructType.IsValid() && Payload->GetScriptStruct()->IsChildOf(MessageStructType.Get()))
	{
		ReceivedMessagePayloadPtr = Payload;

		OnMessageReceived.Broadcast(this);

		ReceivedMessagePayloadPtr = nullptr;
	}

	if (!OnMessageReceived.IsBound())
	{
		// If the BP object that created the async node is destroyed, OnMessageReceived will be unbound after calling the broadcast.
		// In this case we can safely mark this receiver as ready for destruction.
		// Need to support a more proactive mechanism for cleanup FORT-340994
		SetReadyToDestroy();
	}
}

