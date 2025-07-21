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
#include "Subsystems/WorldSubsystem.h"
#include "ArcCoreInteractionCustomData.h"
#include "Interaction/ArcCoreInteractionManager.h"
#include "ArcMacroDefines.h"
#include "Engine/ActorInstanceHandle.h"

#include "ArcCoreInteractionSubsystem.generated.h"

class AArcCoreInteractionManager;

DECLARE_MULTICAST_DELEGATE_OneParam(FArcInteractionDelegate, const FArcCoreInteractionItem&);
DECLARE_MULTICAST_DELEGATE_OneParam(FArcInteractionDataDelegate, FArcCoreInteractionCustomData*);
/**
 * This is subsystem for managing network optimized actors for interaction.
 * It can only be implemented in C++ and uses combination of AdvertiserComponent, ReplicatedCommands, Options and FastArrays
 * to optimize replication, by eliminating need for replicated actors.
 *
 * AArcCoreInteractionManager - manages replication of interaction state.
 * FArcCoreInteractionItemsArray - fast array contains states for interactable actor.
 * FArcCoreInteractionCustomData - is actual state of interaction which can be either mutated and applied to actor
 * by overriding it's virtual functions, or can be simple container for some data.
 *
 * Replicated Commands should be use to replicate how we want to mutate state when using interaction option.
 * It should always contain unique Id (FGuid) to interact with system.
 */
UCLASS()
class ARCCORE_API UArcCoreInteractionSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

	friend struct FArcCoreInteractionItem;
	friend class AArcInteractionRepresentation;
private:
	struct CustomDataMap
	{
		TMap<FName, FArcCoreInteractionCustomData*> Data;
	};
	TMap<FGuid, TWeakObjectPtr<AArcCoreInteractionManager>> InteractionToManager;
	TMap<FGuid, CustomDataMap> IdToInteractionData;

	/*
	 * TODO
	 * Interactions are represented as lightweight instances (somehow ?)
	 * and these are mappings, which should be somehow created.
	 * I guess when Lightweight instance is created ? Somewhere.
	 */
	TMap<FActorInstanceHandle, FGuid> ActorToId;
	TMap<FGuid, FActorInstanceHandle> IdToActor;

	struct PendingLocalAdd
	{
		FGuid Id;
		TArray<FArcCoreInteractionCustomData*> CustomData;
	};
	
	TArray<PendingLocalAdd> PendingLocalAdds;

	DEFINE_ARC_DELEGATE(FArcInteractionDelegate, OnInteractionAdded);
	DEFINE_ARC_DELEGATE_MAP(FGuid, FArcInteractionDelegate, OnInteractionChanged);
	DEFINE_ARC_DELEGATE_MAP(FGuid, FArcInteractionDelegate, OnInteractionRemoved);
	DEFINE_ARC_CHANNEL_DELEGATE_MAP(FGuid, DataType, UScriptStruct*, FArcInteractionDataDelegate, OnInteractionDataChanged);
	
public:
	UPROPERTY()
	TArray<TObjectPtr<AArcCoreInteractionManager>> InteractionManagers;

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	void AddInteraction(const FGuid& InId, TArray<FArcCoreInteractionCustomData*>&& InCustomData);

	void RemoveInteraction(const FGuid& InId, bool bRemoveLevelActor);

	// Adds interaction locally, does not replicate.
	// If doing this make sure to add the same Ids on Client and Server.
	// Used to add interactions, which are determistically placed in world.
	void AddInteractionLocal(const FGuid& InId, const TArray<FArcCoreInteractionCustomData*>& InCustomData);
	
	template<typename T>
	const T* GetInteractionData(const FGuid& InId) const
	{
		if (const CustomDataMap* Data = IdToInteractionData.Find(InId))
		{
			if (const FArcCoreInteractionCustomData* const* CustomData = Data->Data.Find(T::StaticStruct()->GetFName()))
			{
				const FArcCoreInteractionCustomData* Ptr = *CustomData;
				if (Ptr->GetScriptStruct()->IsChildOf(T::StaticStruct()))
				{
					return static_cast<const T*>(Ptr);
				}
			}
		}

		return nullptr;
	}

	
	FArcCoreInteractionCustomData* GetInteractionData(const FGuid& InId, UScriptStruct* InType) const
	{
		if (const CustomDataMap* Data = IdToInteractionData.Find(InId))
		{
			if (const FArcCoreInteractionCustomData* const* CustomData = Data->Data.Find(InType->GetFName()))
			{
				FArcCoreInteractionCustomData* Ptr = const_cast<FArcCoreInteractionCustomData*>(*CustomData);
				if (Ptr->GetScriptStruct()->IsChildOf(InType))
				{
					return Ptr;
				}
			}
		}

		return nullptr;
	}
	
	template<typename T>
    T* FindInteractionDataMutable(const FGuid& InId)
    {
		if (TWeakObjectPtr<AArcCoreInteractionManager>* Manager = InteractionToManager.Find(InId))
		{
			if (AArcCoreInteractionManager* Ptr = Manager->Get())
			{
				return Ptr->InteractionItems.FindInteractionDataMutable<T>(InId);
			}
		}
    
    	return nullptr;
    }

	void MarkInteractionChanged(const FGuid& InteractionId);
	
	void NotifyInteractionManagerBeginPlay(AArcCoreInteractionManager* InInteractionManager);

	UFUNCTION(BlueprintCallable, CustomThunk, BlueprintInternalUseOnly, Category = "Arc Core", meta = (CustomStructureParam = "OutData"))
	static bool GetInteractionData(const FArcCoreInteractionCustomData& Item
														  , UPARAM(meta = (BaseStruct = "ArcCoreInteractionCustomData")) UScriptStruct* InFragmentType
														  , int32& OutData);
	DECLARE_FUNCTION(execGetInteractionData);
	
protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
};
