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

#include "ArcCoreInteractionSubsystem.h"

#include "ArcCoreInteractionManager.h"
#include "Engine/World.h"

void UArcCoreInteractionSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	ENetMode NM = InWorld.GetNetMode();
	if (NM == ENetMode::NM_Client)
	{
		return;
	}
	AArcCoreInteractionManager* InteractionActor = GetWorld()->SpawnActor<AArcCoreInteractionManager>();
	InteractionManagers.Add(InteractionActor);
}

void UArcCoreInteractionSubsystem::AddInteraction(const FGuid& InId, TArray<FArcCoreInteractionCustomData*>&& InCustomData)
{
	if (InteractionManagers.Num() > 0)
	{
		FGuid Id = InId;
		if (Id.IsValid() == false)
		{
			Id = FGuid::NewGuid();	
		}

		FArcCoreInteractionItem* InteractionItem = InteractionManagers[0]->InteractionItems.AddInteraction(Id, MoveTemp(InCustomData));
		InteractionToManager.FindOrAdd(Id) = InteractionManagers[0];

		for (const FArcCoreInteractionCustomDataInternal& Item : InteractionItem->CustomData.Data)
		{
			IdToInteractionData.FindOrAdd(Id).Data.FindOrAdd(Item.CustomData.Get()->GetScriptStruct()->GetFName()) = Item.CustomData.Get();
			
		}
		
		MARK_PROPERTY_DIRTY_FROM_NAME(AArcCoreInteractionManager, InteractionItems, InteractionManagers[0]);
	}
}

void UArcCoreInteractionSubsystem::RemoveInteraction(const FGuid& InId, bool bRemoveLevelActor)
{
	if (InteractionToManager.Contains(InId) == false)
	{
		return;
	}
	
	TWeakObjectPtr<AArcCoreInteractionManager> Manager = InteractionToManager[InId];
	Manager->InteractionItems.Remove(InId, bRemoveLevelActor);
	InteractionToManager.Remove(InId);
	IdToInteractionData.Remove(InId);
}

void UArcCoreInteractionSubsystem::AddInteractionLocal(const FGuid& InId, const TArray<FArcCoreInteractionCustomData*>& InCustomData)
{
	if (InteractionManagers.Num() > 0)
	{
		FGuid Id = InId;
		if (Id.IsValid() == false)
		{
			Id = FGuid::NewGuid();
		}

		InteractionManagers[0]->InteractionItems.AddInteractionLocal(Id, InCustomData);
		InteractionToManager.FindOrAdd(Id) = InteractionManagers[0];
		for (FArcCoreInteractionCustomData* CustomData : InCustomData)
		{
			IdToInteractionData.FindOrAdd(Id).Data.FindOrAdd(CustomData->GetScriptStruct()->GetFName()) = CustomData;
		}

		MARK_PROPERTY_DIRTY_FROM_NAME(AArcCoreInteractionManager, InteractionItems, InteractionManagers[0]);
	}
	else
	{
		PendingLocalAdds.Add({ InId, InCustomData });
	}
}

void UArcCoreInteractionSubsystem::MarkInteractionChanged(const FGuid& InteractionId)
{
	if (TWeakObjectPtr<AArcCoreInteractionManager>* Manager = InteractionToManager.Find(InteractionId))
	{
		if (AArcCoreInteractionManager* Ptr = Manager->Get())
		{
			Ptr->InteractionItems.MarkDirtyById(InteractionId);
		}
	}
}

void UArcCoreInteractionSubsystem::NotifyInteractionManagerBeginPlay(AArcCoreInteractionManager* InInteractionManager)
{
	for (const PendingLocalAdd& PendingAdd : PendingLocalAdds)
	{
		FGuid Id = PendingAdd.Id;
		if (Id.IsValid() == false)
		{
			Id = FGuid::NewGuid();
		}

		InInteractionManager->InteractionItems.AddInteractionLocal(Id, PendingAdd.CustomData);
		InteractionToManager.FindOrAdd(Id) = InInteractionManager;
		for (FArcCoreInteractionCustomData* CustomData : PendingAdd.CustomData)
		{
			IdToInteractionData.FindOrAdd(Id).Data.FindOrAdd(CustomData->GetScriptStruct()->GetFName()) = CustomData;
		}

		MARK_PROPERTY_DIRTY_FROM_NAME(AArcCoreInteractionManager, InteractionItems, InInteractionManager);
	}

	PendingLocalAdds.Empty();
}

bool UArcCoreInteractionSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	if (WorldType == EWorldType::Game || WorldType == EWorldType::PIE)
	{
		return true;
	}

	return false;
}

bool UArcCoreInteractionSubsystem::GetInteractionData(const FArcCoreInteractionCustomData& Item
								   , UPARAM(meta = (BaseStruct = "ArcCoreInteractionCustomData")) UScriptStruct* InFragmentType
								   , int32& OutFragment)
{
	checkNoEntry();
	return false;
}

DEFINE_FUNCTION(UArcCoreInteractionSubsystem::execGetInteractionData)
{
	P_GET_STRUCT(FArcCoreInteractionCustomData, Item);
	P_GET_OBJECT(UScriptStruct, InFragmentType);

	Stack.MostRecentPropertyAddress = nullptr;
	Stack.StepCompiledIn<FStructProperty>(nullptr);

	void* OutItemDataPtr = Stack.MostRecentPropertyAddress;
	FStructProperty* OutItemProp = CastField<FStructProperty>(Stack.MostRecentProperty);

	P_FINISH;
	bool bSuccess = true;

	{
		P_NATIVE_BEGIN;
		const uint8* Fragment = reinterpret_cast<const uint8*>(&Item);
		UScriptStruct* OutputStruct = OutItemProp->Struct;
		// Make sure the type we are trying to get through the blueprint node matches the
		// type of the message payload received.
		if ((OutItemProp != nullptr) && (OutItemProp->Struct != nullptr) && (Fragment != nullptr) && (
				OutItemProp->Struct == InFragmentType))
		{
			OutItemProp->Struct->CopyScriptStruct(OutItemDataPtr
				, Fragment);
			bSuccess = true;
		}
		P_NATIVE_END;
	}
	*(bool*)RESULT_PARAM = bSuccess;
}
