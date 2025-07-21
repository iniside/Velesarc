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



#include "ArcCoreInteractionCustomData.h"

#include "ArcCoreInteractionSubsystem.h"
#include "ArcInteractionLevelPlacedComponent.h"
#include "ArcInteractionRepresentation.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

#if WITH_EDITORONLY_DATA
void FArcInteractionData_ItemSpecs::PostSerialize(const FArchive& Ar)
{
	uint32 Id = 0;
	for (int32 Idx = 0; Idx < ItemSpec.Num(); Idx++)
	{
		ItemSpec[Idx].Id.Id = Id;
		Id++;
	}
}
#endif

void FArcInteractionData_LevelActor::PostReplicatedAdd(const FArcCoreInteractionItemsArray& InArraySerializer, const FArcCoreInteractionItem* InItem)
{
	LevelActorLocal = LevelActor;
}

void FArcInteractionData_LevelActor::PreReplicatedRemove(const FArcCoreInteractionItemsArray& InArraySerializer, const FArcCoreInteractionItem* InItem)
{
	if (bRemoveLevelActorOnRemove && LevelActor && LevelActor == LevelActorLocal)
    {
    	LevelActor->SetHidden(true);
    	LevelActor->Destroy();
		return;
    }
    	
	if (bRemoveLevelActorOnRemove && LevelActor)
	{
		LevelActor->SetHidden(true);
		LevelActor->Destroy();
	}
	if (bRemoveLevelActorOnRemove && LevelActorLocal)
	{
		LevelActorLocal->SetHidden(true);
		LevelActorLocal->Destroy();
	}
}

void FArcInteractionData_SpawnedActor::PostReplicatedAdd(const FArcCoreInteractionItemsArray& InArraySerializer, const FArcCoreInteractionItem* InItem)
{
	UClass* Cls = ActorClass.LoadSynchronous();
	SpawnedActor = InArraySerializer.Owner->GetWorld()->SpawnActor(Cls, &Location);

	TScriptInterface<IArcInteractionObjectInterface> Interface = SpawnedActor;
	
	Interface->SetInteractionId(InItem->Id);
}

void FArcInteractionData_SpawnedActor::PreReplicatedRemove(const FArcCoreInteractionItemsArray& InArraySerializer, const FArcCoreInteractionItem* InItem)
{
	if (SpawnedActor)
	{
		SpawnedActor->Destroy();
	}
}

void FArcCoreInteractionItem::PreRemove(const FArcCoreInteractionItemsArray& InArraySerializer)
{
	for (int32 Idx = 0; Idx < CustomData.Data.Num(); Idx++)
	{
		CustomData.Data[Idx].CustomData->PreReplicatedRemove(InArraySerializer, this);
	}
}

void FArcCoreInteractionItem::OnAdded(const FArcCoreInteractionItemsArray& InArraySerializer)
{
	for (int32 Idx = 0; Idx < CustomData.Data.Num(); Idx++)
	{
		for (UScriptStruct* Struct : ChangedCustomData)
		{
			if (CustomData.Data[Idx].CustomData->GetScriptStruct() == Struct)
			{
				UArcCoreInteractionSubsystem* Subsystem = InArraySerializer.Owner->GetWorld()->GetSubsystem<UArcCoreInteractionSubsystem>();
				Subsystem->BroadcastDataTypeOnInteractionDataChangedMap(Id, CustomData.Data[Idx].CustomData->GetScriptStruct()
					, CustomData.Data[Idx].CustomData.Get());
			}		
		}
		CustomData.Data[Idx].CustomData->PostReplicatedAdd(InArraySerializer, this);
	}
}

void FArcCoreInteractionItem::OnChanged(const FArcCoreInteractionItemsArray& InArraySerializer)
{
	for (UScriptStruct* Struct : ChangedCustomData)
	{
		for (int32 Idx = 0; Idx < CustomData.Data.Num(); Idx++)
		{
			if (CustomData.Data[Idx].CustomData->GetScriptStruct() == Struct)
			{
				CustomData.Data[Idx].CustomData->PostReplicatedChange(InArraySerializer, this);
				UArcCoreInteractionSubsystem* Subsystem = InArraySerializer.Owner->GetWorld()->GetSubsystem<UArcCoreInteractionSubsystem>();
				Subsystem->BroadcastDataTypeOnInteractionDataChangedMap(Id, CustomData.Data[Idx].CustomData->GetScriptStruct()
					, CustomData.Data[Idx].CustomData.Get());
			}
		}	
	}
}

void FArcCoreInteractionItem::PreReplicatedRemove(const FArcCoreInteractionItemsArray& InArraySerializer)
{
	PreRemove(InArraySerializer);
	UArcCoreInteractionSubsystem* IS = InArraySerializer.Owner->GetWorld()->GetSubsystem<UArcCoreInteractionSubsystem>();
	IS->IdToInteractionData.Remove(Id);
}

void FArcCoreInteractionItem::PostReplicatedAdd(const FArcCoreInteractionItemsArray& InArraySerializer)
{
	UArcCoreInteractionSubsystem* IS = InArraySerializer.Owner->GetWorld()->GetSubsystem<UArcCoreInteractionSubsystem>();
	
	for (int32 Idx = 0; Idx < CustomData.Data.Num(); Idx++)
	{
		IS->IdToInteractionData.FindOrAdd(Id).Data.FindOrAdd(CustomData.Data[Idx].CustomData->GetScriptStruct()->GetFName())
		 = CustomData.Data[Idx].CustomData.Get(); 
		CustomData.Data[Idx].CustomData->PostReplicatedAdd(InArraySerializer, this);

		for (UScriptStruct* Struct : ChangedCustomData)
		{
			if (CustomData.Data[Idx].CustomData->GetScriptStruct() == Struct)
			{
				UArcCoreInteractionSubsystem* Subsystem = InArraySerializer.Owner->GetWorld()->GetSubsystem<UArcCoreInteractionSubsystem>();
				Subsystem->BroadcastDataTypeOnInteractionDataChangedMap(Id, CustomData.Data[Idx].CustomData->GetScriptStruct()
					, CustomData.Data[Idx].CustomData.Get());
			}		
		}
	}
}

void FArcCoreInteractionItem::PostReplicatedChange(const FArcCoreInteractionItemsArray& InArraySerializer)
{
	for (UScriptStruct* Struct : ChangedCustomData)
	{
		for (int32 Idx = 0; Idx < CustomData.Data.Num(); Idx++)
		{
			if (CustomData.Data[Idx].CustomData->GetScriptStruct() == Struct)
			{
				CustomData.Data[Idx].CustomData->PostReplicatedChange(InArraySerializer, this);
				UArcCoreInteractionSubsystem* Subsystem = InArraySerializer.Owner->GetWorld()->GetSubsystem<UArcCoreInteractionSubsystem>();
				Subsystem->BroadcastDataTypeOnInteractionDataChangedMap(Id, CustomData.Data[Idx].CustomData->GetScriptStruct()
					, CustomData.Data[Idx].CustomData.Get());
			}
		}	
	}
	
	//for (int32 Idx = 0; Idx < CustomData.Data.Num(); Idx++)
	//{
	//	CustomData.Data[Idx].CustomData->PostReplicatedChange(InArraySerializer, this);
	//}
}