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

#include "Items/ArcItemDefinition.h"

#include "ArcCore/Items/ArcItemData.h"
#include "Items/ArcItemsArray.h"
#include "Items/ArcItemScalableFloat.h"
#include "Items/ArcItemSpec.h"

#include "AbilitySystem/ArcCoreAbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"

#include "ArcItemsStoreComponent.h"

#include "Core/ArcCoreAssetManager.h"
#include "Engine/NetConnection.h"
#include "Engine/World.h"

#include "Items/Fragments/ArcItemFragment_Tags.h"


#include "ArcItemsHelpers.h"
#include "ArcItemsSubsystem.h"
#include "Logging/StructuredLog.h"
#include "Misc/NamePermissionList.h"
#include "Net/RepLayout.h"

#if UE_WITH_IRIS

#include "Iris/ReplicationState/PropertyNetSerializerInfoRegistry.h"
#include "Net/Core/NetBitArray.h"
#endif
DEFINE_LOG_CATEGORY(LogItemEntry);


FArcItemData::FArcItemData()
	: OwnerComponent(nullptr)
	, ItemId()
	, Level(0)
{
}

FArcItemData::~FArcItemData()
{
}

UScriptStruct* FArcItemData::GetScriptStruct() const
{
	return FArcItemData::StaticStruct();
}

float FArcItemData::GetValue(const FArcScalableCurveFloat& InScalableFloat) const
{
	if (const FArcScalableFloatItemFragment* const* ScalableFragment = ScalableFloatFragments.Find(InScalableFloat.GetOwnerType()))
	{
		return InScalableFloat.GetValue(*ScalableFragment
			, Level, ScalableFloatContext);
	}
	return 0.f;
}

float FArcItemData::GetValueWithLevel(const FArcScalableCurveFloat& InScalableFloat
									   , float InLevel) const
{
	if (const FArcScalableFloatItemFragment* const* ScalableFragment = ScalableFloatFragments.Find(InScalableFloat.GetOwnerType()))
	{
		return InScalableFloat.GetValue(*ScalableFragment
			, InLevel, ScalableFloatContext);
	}
	return 0.f;
}

const TArray<const FArcItemData*>& FArcItemData::GetItemsInSockets() const
{
	if (OwnerComponent == nullptr)
	{
		UE_LOG(LogItemEntry, Warning, TEXT("GetItemsInSockets() called on FArcItemData with no OwnerComponent (Mass context). ItemId=%s"), *ItemId.ToString());
		return ItemsInSockets;
	}

	if (ItemsInSockets.Num() != AttachedItems.Num())
	{
		ItemsInSockets.Empty();
		ItemsInSockets.Reserve(AttachedItems.Num());
		TArray<const FArcItemData*> Attached = GetItemsStoreComponent()->GetItemsAttachedTo(GetItemId());
		ItemsInSockets.Append(Attached);

	}
	return ItemsInSockets;
}

const FArcItemSpec* FArcItemData::GetSpecPtr() const
{
	return &Spec;
}

const FArcItemData* FArcItemData::GetOwnerPtr() const
{
	if (OwnerComponent == nullptr)
	{
		UE_LOG(LogItemEntry, Warning, TEXT("GetOwnerPtr() called on FArcItemData with no OwnerComponent (Mass context). ItemId=%s"), *ItemId.ToString());
		return nullptr;
	}

	if (OwnerItemCachedPtr)
	{
		return OwnerItemCachedPtr;
	}

	if (OwnerId.IsValid() == false)
	{
		OwnerItemCachedPtr = nullptr;
		return nullptr;
	}

	OwnerItemCachedPtr = GetItemsStoreComponent()->GetItemPtr(OwnerId);
	return OwnerItemCachedPtr;
}

void FArcItemData::SetItemSpec(const FArcItemSpec& InSpec)
{
	Spec = InSpec;
}

FInstancedStruct FArcItemData::NewFromSpec(const FArcItemSpec& InSpec)
{
	FInstancedStruct Result = FInstancedStruct::Make<FArcItemData>();
	FArcItemData* NewEntry = Result.GetMutablePtr<FArcItemData>();

	if (InSpec.ItemId.IsValid() == false)
	{
		NewEntry->ItemId = FArcItemId::Generate();
	}
	else
	{
		NewEntry->ItemId = InSpec.ItemId;
	}

	NewEntry->ItemDefinition = InSpec.GetItemDefinition();
	NewEntry->Level = InSpec.Level;

	return Result;
}

FInstancedStruct FArcItemData::Duplicate(UArcItemsStoreComponent* InItemsStore
											, const FArcItemData* From
											, const bool bPreserveItemId)
{
	FInstancedStruct Result = FInstancedStruct::Make<FArcItemData>(*From);
	FArcItemData* Copy = Result.GetMutablePtr<FArcItemData>();

	Copy->ScalableFloatFragments = From->ScalableFloatFragments;

	Copy->AttachedToSlot = FGameplayTag::EmptyTag;
	Copy->OldAttachedToSlot = FGameplayTag::EmptyTag;

	return Result;
}

void FArcItemData::Initialize(UArcItemsStoreComponent* ItemsStoreComponent)
{
	OwnerComponent = ItemsStoreComponent;

	ArcItemsHelper::ForEachFragment<FArcItemFragment_ItemInstanceBase>(this
			, [](const FArcItemData* ItemData, const FArcItemFragment_ItemInstanceBase* InFragment)
			{
				InFragment->OnItemInitialize(ItemData);
			});
	
	ItemDefinition = UArcCoreAssetManager::Get().GetAssetWithBundles<UArcItemDefinition>(ItemsStoreComponent
		, GetItemDefinitionId()
		, false);

	// Fallback: use the definition set directly on the spec (e.g. from tests or manually created items)
	if (ItemDefinition == nullptr && Spec.ItemDefinition != nullptr)
	{
		ItemDefinition = Spec.ItemDefinition;
	}

	if (const UArcItemDefinition* Def = GetItemDefinition())
	{
		const TSet<FArcInstancedStruct>& IS = Def->GetScalableFloatFragments();
		for (const FArcInstancedStruct& I : IS)
		{
			ScalableFloatFragments.FindOrAdd(I.GetScriptStruct()) = I.GetPtr<FArcScalableFloatItemFragment>();
		}
	}

	World = OwnerComponent ? OwnerComponent->GetWorld() : nullptr;

	ScalableFloatContext.ItemData = this;
	ScalableFloatContext.ItemsStore = OwnerComponent;
	ScalableFloatContext.ItemDefinition = ItemDefinition;
	ScalableFloatContext.Owner = OwnerComponent ? OwnerComponent->GetOwner() : nullptr;
	if (ScalableFloatContext.Owner)
	{
		ScalableFloatContext.AbilitySystemComponent = Cast<UArcCoreAbilitySystemComponent>(
			UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(ScalableFloatContext.Owner));
		if (ScalableFloatContext.AbilitySystemComponent)
		{
			ScalableFloatContext.Avatar = ScalableFloatContext.AbilitySystemComponent->GetAvatarActor();
			ScalableFloatContext.Owner = ScalableFloatContext.AbilitySystemComponent->GetOwnerActor();
		}
	}
	if (ScalableFloatContext.Avatar == nullptr)
	{
		ScalableFloatContext.Avatar = ScalableFloatContext.Owner;
	}

	if (const FArcItemFragment_Tags* Tags = ArcItemsHelper::GetFragment<FArcItemFragment_Tags>(this))
	{
		ItemAggregatedTags.Reset();
		ItemAggregatedTags.AppendTags(Tags->ItemTags);
	}

	for (FInstancedStruct& Inst : ItemInstances.Data)
	{
		if (Inst.IsValid())
		{
			FArcItemInstance* Instance = Inst.GetMutablePtr<FArcItemInstance>();
			if (Instance)
			{
				Instance->IC = OwnerComponent;
				Instance->OwningItem = OwnerItemCachedPtr;
				InstancedData.Add(Inst.GetScriptStruct(), FStructView(Inst));
			}
		}
	}
}

void FArcItemData::InitializeMass(UWorld* InWorld)
{
	World = InWorld;

	// Build ItemInstances from definition fragments
	TArray<const FArcItemFragment_ItemInstanceBase*> DefInstances = ItemDefinition->GetFragmentsOfType<FArcItemFragment_ItemInstanceBase>();
	for (const FArcItemFragment_ItemInstanceBase* IIB : DefInstances)
	{
		if (!IIB->GetCreateItemInstance())
		{
			continue;
		}
		if (UScriptStruct* InstanceType = IIB->GetItemInstanceType())
		{
			FInstancedStruct NewInstance;
			NewInstance.InitializeAs(InstanceType, nullptr);

			int32 InitialDataIdx = Spec.InitialInstanceData.IndexOfByPredicate([InstanceType](const FInstancedStruct& Other)
			{
				return Other.GetScriptStruct() == InstanceType;
			});
			if (InitialDataIdx != INDEX_NONE)
			{
				NewInstance = Spec.InitialInstanceData[InitialDataIdx];
			}

			ItemInstances.Data.Add(MoveTemp(NewInstance));
		}
	}

	// Build ItemInstances from spec fragments
	TArray<const FArcItemFragment_ItemInstanceBase*> SpecFragments = Spec.GetSpecFragments();
	for (const FArcItemFragment_ItemInstanceBase* Fragment : SpecFragments)
	{
		if (!Fragment->GetCreateItemInstance())
		{
			continue;
		}
		if (UScriptStruct* InstanceType = Fragment->GetItemInstanceType())
		{
			FInstancedStruct NewInstance;
			NewInstance.InitializeAs(InstanceType, nullptr);

			int32 InitialDataIdx = Spec.InitialInstanceData.IndexOfByPredicate([InstanceType](const FInstancedStruct& Other)
			{
				return Other.GetScriptStruct() == InstanceType;
			});
			if (InitialDataIdx != INDEX_NONE)
			{
				NewInstance = Spec.InitialInstanceData[InitialDataIdx];
			}

			ItemInstances.Data.Add(MoveTemp(NewInstance));
		}
	}

	SetItemInstances(ItemInstances);

	// Build ScalableFloatFragments
	const TSet<FArcInstancedStruct>& DefScalable = ItemDefinition->GetScalableFloatFragments();
	for (const FArcInstancedStruct& I : DefScalable)
	{
		ScalableFloatFragments.FindOrAdd(I.GetScriptStruct()) = I.GetPtr<FArcScalableFloatItemFragment>();
	}

	// Build ScalableFloatContext (Mass: no owner actor / ASC)
	ScalableFloatContext.ItemData = this;
	ScalableFloatContext.ItemsStore = nullptr;
	ScalableFloatContext.ItemDefinition = ItemDefinition;

	// Build ItemAggregatedTags
	if (const FArcItemFragment_Tags* Tags = ArcItemsHelper::GetFragment<FArcItemFragment_Tags>(this))
	{
		ItemAggregatedTags.Reset();
		ItemAggregatedTags.AppendTags(Tags->ItemTags);
	}

	// Fire OnItemInitialize on all fragments
	ArcItemsHelper::ForEachFragment<FArcItemFragment_ItemInstanceBase>(this
		, [](const FArcItemData* ItemData, const FArcItemFragment_ItemInstanceBase* InFragment)
		{
			InFragment->OnItemInitialize(ItemData);
		});
}

void FArcItemData::OnItemAdded()
{
	const UArcItemDefinition* Def = GetItemDefinition();

	UArcCoreAssetManager::Get().AddLoadedAsset(Def);
	
	ArcItemsHelper::ForEachFragment<FArcItemFragment_ItemInstanceBase>(this
			, [](const FArcItemData* ItemData, const FArcItemFragment_ItemInstanceBase* InFragment)
			{
				InFragment->OnItemAdded(ItemData);
			});
}

void FArcItemData::OnItemChanged()
{
	ArcItemsHelper::ForEachFragment<FArcItemFragment_ItemInstanceBase>(this
			, [](const FArcItemData* ItemData, const FArcItemFragment_ItemInstanceBase* InFragment)
			{
				FArcItemInstance* Instance = ArcItemsHelper::FindMutableInstance(ItemData, InFragment->GetItemInstanceType());
				InFragment->OnItemChanged(ItemData);
			});
	
	if (OwnerComponent)
	{
		UArcItemsSubsystem* ItemsSubsystem = UArcItemsSubsystem::Get(OwnerComponent);
		if (ItemsSubsystem)
		{
			ItemsSubsystem->OnItemChangedDynamic.Broadcast(OwnerComponent, GetItemId());
			ItemsSubsystem->BroadcastActorOnItemChangedStore(OwnerComponent->GetOwner(), OwnerComponent, this);
			ItemsSubsystem->BroadcastActorOnItemChangedStoreMap(OwnerComponent->GetOwner(), GetItemId(), OwnerComponent, this);
		}
	}
}

void FArcItemData::OnPreRemove()
{
	ArcItemsHelper::ForEachFragment<FArcItemFragment_ItemInstanceBase>(this
			, [](const FArcItemData* ItemData, const FArcItemFragment_ItemInstanceBase* InFragment)
			{
				InFragment->OnPreRemoveItem(ItemData);
			});

	UArcCoreAssetManager::Get().RemoveLoadedAsset(GetItemDefinition());
}

void FArcItemData::Deinitialize()
{
	UArcCoreAssetManager::Get().RemoveLoadedAsset(ItemDefinition);
}

const uint8* FArcItemData::FindFragment(UScriptStruct* InStructType) const
{
	const uint8* Ptr = GetItemDefinition()->GetFragment(InStructType);
	if (Ptr != nullptr)
	{
		return Ptr;
	}
		
	for (const FArcItemData* SocketItem : GetItemsInSockets())
	{
		Ptr = SocketItem->FindFragment(InStructType);
		if (Ptr)
		{
			return Ptr;
		}
	}
	return nullptr;
}

void FArcItemData::SetItemInstances(const FArcItemInstanceArray& InInstances)
{
	InstancedData.Reset();
	for (FInstancedStruct& Inst : ItemInstances.Data)
	{
		if (Inst.IsValid())
		{
			FArcItemInstance* Instance = Inst.GetMutablePtr<FArcItemInstance>();
			if (Instance)
			{
				Instance->IC = OwnerComponent;
				Instance->OwningItem = OwnerItemCachedPtr;
				InstancedData.Add(Inst.GetScriptStruct(), FStructView(Inst));
			}
		}
	}
}

void FArcItemData::NotifyFragmentsSlotAdded(const FGameplayTag& InSlotId)
{
	ArcItemsHelper::ForEachFragment<FArcItemFragment_ItemInstanceBase>(this
		, [InSlotId](const FArcItemData* ItemData, const FArcItemFragment_ItemInstanceBase* InFragment)
		{
			InFragment->OnItemAddedToSlot(ItemData, InSlotId);
		});

	TArray<const FArcItemData*> LocalAttachedItems = GetItemsStoreComponent()->GetAttachedItems(GetItemId());
	for (const FArcItemData* Item : LocalAttachedItems)
	{
		ArcItemsHelper::ForEachFragment<FArcItemFragment_ItemInstanceBase>(Item
			, [](const FArcItemData* ItemData, const FArcItemFragment_ItemInstanceBase* InFragment)
			{
				InFragment->OnItemAddedToSlot(ItemData, ItemData->GetAttachSlot());
			});
	}
}

void FArcItemData::NotifyFragmentsSlotRemoved(const FGameplayTag& InSlotId)
{
	ArcItemsHelper::ForEachFragment<FArcItemFragment_ItemInstanceBase>(this
		, [InSlotId](const FArcItemData* ItemData, const FArcItemFragment_ItemInstanceBase* InFragment)
		{
			InFragment->OnItemRemovedFromSlot(ItemData, InSlotId);
		});

	TArray<const FArcItemData*> LocalAttachedItems = GetItemsStoreComponent()->GetAttachedItems(GetItemId());
	for (const FArcItemData* Item : LocalAttachedItems)
	{
		ArcItemsHelper::ForEachFragment<FArcItemFragment_ItemInstanceBase>(Item
			, [](const FArcItemData* ItemData, const FArcItemFragment_ItemInstanceBase* InFragment)
			{
				InFragment->OnItemRemovedFromSlot(ItemData, ItemData->GetAttachSlot());
			});
	}
}

void FArcItemData::AddToSlot(const FGameplayTag& InSlotId)
{
	if (GetItemsStoreComponent()->HasAuthority())
	{
		Slot = InSlotId;
		NotifyFragmentsSlotAdded(InSlotId);
	}
	else
	{
		if (bAddedToSlot == false)
		{
			bAddedToSlot = true;
			bRemoveFromSlot = false;
			ArcItemsHelper::ForEachFragment<FArcItemFragment_ItemInstanceBase>(this
				, [InSlotId](const FArcItemData* ItemData, const FArcItemFragment_ItemInstanceBase* InFragment)
				{
					InFragment->OnItemAddedToSlot(ItemData, InSlotId);
				});
		}

		TArray<const FArcItemData*> LocalAttachedItems = GetItemsStoreComponent()->GetAttachedItems(GetItemId());
		for (const FArcItemData* Item : LocalAttachedItems)
		{
			if (Item->bAddedToSlot == false)
			{
				const_cast<FArcItemData*>(Item)->bAddedToSlot = true;
				const_cast<FArcItemData*>(Item)->bRemoveFromSlot = false;
				ArcItemsHelper::ForEachFragment<FArcItemFragment_ItemInstanceBase>(Item
					, [](const FArcItemData* ItemData, const FArcItemFragment_ItemInstanceBase* InFragment)
					{
						InFragment->OnItemAddedToSlot(ItemData, ItemData->GetAttachSlot());
					});
			}
		}
	}
}

void FArcItemData::RemoveFromSlot(const FGameplayTag& InSlotId)
{
	if (GetItemsStoreComponent()->HasAuthority())
	{
		NotifyFragmentsSlotRemoved(InSlotId);

		OldSlot = Slot;
		Slot = FGameplayTag::EmptyTag;
	}
	else
	{
		if (bRemoveFromSlot == false)
		{
			bAddedToSlot = false;
			bRemoveFromSlot = true;
			ArcItemsHelper::ForEachFragment<FArcItemFragment_ItemInstanceBase>(this
				, [InSlotId](const FArcItemData* ItemData, const FArcItemFragment_ItemInstanceBase* InFragment)
				{
					InFragment->OnItemRemovedFromSlot(ItemData, InSlotId);
				});
		}

		TArray<const FArcItemData*> LocalAttachedItems = GetItemsStoreComponent()->GetAttachedItems(GetItemId());
		for (const FArcItemData* Item : LocalAttachedItems)
		{
			if (Item->bRemoveFromSlot == false)
			{
				const_cast<FArcItemData*>(Item)->bAddedToSlot = false;
				const_cast<FArcItemData*>(Item)->bRemoveFromSlot = true;
				ArcItemsHelper::ForEachFragment<FArcItemFragment_ItemInstanceBase>(Item
					, [](const FArcItemData* ItemData, const FArcItemFragment_ItemInstanceBase* InFragment)
					{
						InFragment->OnItemRemovedFromSlot(ItemData, ItemData->GetAttachSlot());
					});
			}
		}
	}
}

void FArcItemData::ChangeSlot(const FGameplayTag& InNewSlot)
{
	if (GetItemsStoreComponent()->HasAuthority())
	{
		OldSlot = Slot;
		Slot = InNewSlot;	
	}

	//bAddedToSlot = false;
	//bRemoveFromSlot = false;
	
	ArcItemsHelper::ForEachFragment<FArcItemFragment_ItemInstanceBase>(this
			, [this](const FArcItemData* ItemData, const FArcItemFragment_ItemInstanceBase* InFragment)
			{
				InFragment->OnItemChangedSlot(ItemData, Slot, OldSlot);
			});
}

void FArcItemData::AttachToItem(const FArcItemId& InOwnerItem, const FGameplayTag& InAttachSlot)
{
	FArcItemData* OwnerItemData = OwnerComponent->GetItemPtr(InOwnerItem);

	ENetRole Role = OwnerComponent->GetOwnerRole();
	if (Role == ROLE_Authority)
	{
		OldOwnerId = OwnerId;
		OwnerId = InOwnerItem;
		//Slot = InAttachSlot;
		OldAttachedToSlot = AttachedToSlot;
		AttachedToSlot = InAttachSlot;
		GetOwnerPtr();
	}
	
	const TSet<FArcInstancedStruct>& IS = GetItemDefinition()->GetScalableFloatFragments();
	for (const FArcInstancedStruct& I : IS)
	{
		if (OwnerItemData->ScalableFloatFragments.Contains(I.GetScriptStruct()) == false)
		{
			OwnerItemData->ScalableFloatFragments.Add(I.GetScriptStruct()
				, I.GetPtr<FArcScalableFloatItemFragment>());
		}
		else
		{
			UE_LOG(LogItemEntry
				, Log
				, TEXT("%s already added to ScalableFloats stats")
				, *I.GetScriptStruct()->GetName());
		}
	}

	UArcItemsSubsystem* ItemsSubsystem = UArcItemsSubsystem::Get(OwnerComponent);
	
	OwnerItemData->AttachedItems.AddUnique(GetItemId());
	
	ArcItemsHelper::ForEachFragment<FArcItemFragment_ItemInstanceBase>(this
				, [OwnerItemData](const FArcItemData* ItemData, const FArcItemFragment_ItemInstanceBase* InFragment)
				{
					InFragment->OnItemAttachedTo(ItemData, OwnerItemData);
				});

	if (OwnerItemData->GetSlotId().IsValid() && Role == ROLE_Authority)
	{
		ArcItemsHelper::ForEachFragment<FArcItemFragment_ItemInstanceBase>(this
				, [](const FArcItemData* ItemData, const FArcItemFragment_ItemInstanceBase* InFragment)
				{
					InFragment->OnItemAddedToSlot(ItemData, ItemData->GetAttachSlot());
				});
	}
	else if (OwnerItemData->GetSlotId().IsValid())
	{
		if (bAddedToSlot == false)
		{
			bAddedToSlot = true;
			bRemoveFromSlot = false;
			ArcItemsHelper::ForEachFragment<FArcItemFragment_ItemInstanceBase>(this
					, [](const FArcItemData* ItemData, const FArcItemFragment_ItemInstanceBase* InFragment)
					{
						InFragment->OnItemAddedToSlot(ItemData, ItemData->GetAttachSlot());
					});
		}
	}
	
	
	if (ItemsSubsystem)
	{
		ItemsSubsystem->BroadcastActorOnAddedToSocket(GetItemsStoreComponent()->GetOwner()
			, GetItemsStoreComponent(), OwnerItemData->Slot, OwnerItemData, AttachedToSlot, this);
	}
	
	if (Role == ROLE_Authority)
	{
		OwnerComponent->MarkItemDirtyById(GetItemId());
		OwnerComponent->MarkItemDirtyById(InOwnerItem);	
	}

	if (Role == ROLE_Authority)
	{
		OnItemChanged();
		OwnerItemData->OnItemChanged();
	}
}

void FArcItemData::CleanupOwnerOnDetach(FArcItemData* OwnerData)
{
	const TSet<FArcInstancedStruct>& IS = GetItemDefinition()->GetScalableFloatFragments();
	for (const FArcInstancedStruct& I : IS)
	{
		OwnerData->ScalableFloatFragments.Remove(I.GetScriptStruct());
	}

	int32 ItemIdx = OwnerData->AttachedItems.IndexOfByPredicate([this](const FArcItemId& InItemId)
	{
		return InItemId == GetItemId();
	});

	if (ItemIdx != INDEX_NONE)
	{
		OwnerData->AttachedItems.RemoveAt(ItemIdx);
	}
}

void FArcItemData::DetachFromItem()
{
	ENetRole Role = OwnerComponent->GetOwnerRole();

	// TODO:: Attach from Old and New Attachment or just from Old or New ?
	if (OldOwnerId.IsValid())
	{
		FArcItemData* OldOwnerData = OwnerComponent->GetItemPtr(OldOwnerId);
		if (OldOwnerData)
		{
			CleanupOwnerOnDetach(OldOwnerData);

			OldAttachedToSlot = AttachedToSlot;
			AttachedToSlot = FGameplayTag::EmptyTag;

			OwnerComponent->MarkItemDirtyById(OldOwnerData->GetItemId());
			OwnerComponent->MarkItemDirtyById(GetItemId());

			if (Role == ROLE_Authority)
			{
				OnItemChanged();
				OldOwnerData->OnItemChanged();
			}

			if (OldOwnerData->GetSlotId().IsValid())
			{
				ArcItemsHelper::ForEachFragment<FArcItemFragment_ItemInstanceBase>(this
					, [](const FArcItemData* ItemData, const FArcItemFragment_ItemInstanceBase* InFragment)
					{
						InFragment->OnItemRemovedFromSlot(ItemData, ItemData->GetAttachSlot());
					});
			}
		}
	}

	FArcItemData* OwnerData = OwnerComponent->GetItemPtr(OwnerId);
	if (OwnerData == nullptr)
	{
		if (Role == ROLE_Authority)
		{
			AttachedToSlot = FGameplayTag::EmptyTag;
			// If owner doesn't exists anymore we don't care about cleaning it.
			OwnerId.Reset();

			OldOwnerId = OwnerId;
		}

		return;
	}

	CleanupOwnerOnDetach(OwnerData);
	
	if (Role == ROLE_Authority)
	{
		OldOwnerId = OwnerId;
		OwnerId.Reset();
		OldAttachedToSlot = AttachedToSlot;
		AttachedToSlot = FGameplayTag::EmptyTag;
	
		OwnerComponent->MarkItemDirtyById(OwnerData->GetItemId());
		OwnerComponent->MarkItemDirtyById(GetItemId());
	
		OnItemChanged();
		OwnerData->OnItemChanged();
	}

	if (OwnerData->GetSlotId().IsValid() && Role == ROLE_Authority)
	{
		ArcItemsHelper::ForEachFragment<FArcItemFragment_ItemInstanceBase>(this
				, [](const FArcItemData* ItemData, const FArcItemFragment_ItemInstanceBase* InFragment)
				{
					InFragment->OnItemRemovedFromSlot(ItemData, ItemData->GetAttachSlot());
				});
	}
	else if (OwnerData->GetSlotId().IsValid())
	{
		if (bRemoveFromSlot == false)
		{
			bAddedToSlot = false;
			bRemoveFromSlot = true;
			ArcItemsHelper::ForEachFragment<FArcItemFragment_ItemInstanceBase>(this
				, [](const FArcItemData* ItemData, const FArcItemFragment_ItemInstanceBase* InFragment)
				{
					InFragment->OnItemRemovedFromSlot(ItemData, ItemData->GetAttachSlot());
				});
		}
	}
	
	ArcItemsHelper::ForEachFragment<FArcItemFragment_ItemInstanceBase>(this
			, [OwnerData](const FArcItemData* ItemData, const FArcItemFragment_ItemInstanceBase* InFragment)
			{
				InFragment->OnItemDetachedFrom(ItemData, OwnerData);
			});

	if (Role == ROLE_Authority)
	{
		
		if (OldOwnerId.IsValid())
		{
			OwnerComponent->MarkItemDirtyById(OwnerData->GetItemId());
			OwnerComponent->MarkItemDirtyById(GetItemId());	
		}
	}

	OwnerItemCachedPtr = nullptr;
}

void FArcItemData::PreReplicatedRemove(const FArcItemsArray& InArraySerializer)
{
	UE_LOGFMT(LogArcItems, Log, "PreRepllicatedRemove {0} Id {1}", GetItemDefinition()->GetName(), ItemId.ToString());
	OnPreRemove();
	UArcItemsSubsystem* ItemsSubsystem = UArcItemsSubsystem::Get(OwnerComponent);
	ItemsSubsystem->BroadcastActorOnItemRemovedFromStoreMap(InArraySerializer.Owner->GetOwner(), ItemId, OwnerComponent, this);
	
	if (OwnerId.IsValid())
	{
		const FArcItemData* OwnerEntry = OwnerComponent->GetItemPtr(OwnerId);
		// there is still chance owner have not replicated if both items were added at the
		// same time.
		if (OwnerEntry != nullptr)
		{
			// we don't care about tag slot here, it should be already replicated from
			// server.
			DetachFromItem();
		}
	}

	InArraySerializer.Owner->PreRemove(this);
	
	
	ItemsSubsystem->BroadcastActorOnItemRemovedFromStoreMap(OwnerComponent->GetOwner(), ItemId, OwnerComponent, this);
	ItemsSubsystem->BroadcastActorOnItemRemovedFromStore(OwnerComponent->GetOwner(), OwnerComponent, this);
	ItemsSubsystem->BroadcastOnItemRemovedFromStore(OwnerComponent, this);
	
	UArcCoreAssetManager::Get().RemoveLoadedAsset(ItemDefinition);
	Spec = FArcItemSpec();

	GetItemsStoreComponent()->RemovePendingItem(ItemId);
}

void FArcItemData::PostReplicatedAdd(const FArcItemsArray& InArraySerializer)
{
	OwnerComponent = InArraySerializer.Owner;
	
	SetItemInstances(ItemInstances);

	ItemDefinition = UArcCoreAssetManager::Get().GetAssetWithBundles<UArcItemDefinition>(InArraySerializer.Owner
		, GetItemDefinitionId()
		, false);
	
	if (ItemDefinition == nullptr)
	{
		return;
	}
	
	UArcItemsSubsystem* ItemsSubsystem = UArcItemsSubsystem::Get(OwnerComponent);
	
	//TODO Replicate if duplicated ?
	Initialize(InArraySerializer.Owner);

	GetItemsStoreComponent()->RemovePendingItem(ItemId);

	OnItemAdded();
	
	if (Slot.IsValid() && Slot != FGameplayTag::EmptyTag && OwnerId.IsValid() == false)
	{
		AddToSlot(Slot);

		ItemsSubsystem->OnItemAddedToSlotDynamic.Broadcast(GetItemsStoreComponent(), GetSlotId(), GetItemId());
		ItemsSubsystem->BroadcastActorOnAddedToSlot(GetItemsStoreComponent()->GetOwner(), GetItemsStoreComponent(), GetSlotId(), this);
		ItemsSubsystem->BroadcastActorOnItemAddedToSlotMap(GetItemsStoreComponent()->GetOwner(), ItemId, GetItemsStoreComponent(), this);
		ItemsSubsystem->BroadcastActorOnAddedToSlotMap(GetItemsStoreComponent()->GetOwner(), GetSlotId(), GetItemsStoreComponent(), this);
	}
	
	
	// If we have valid owner Id, we are extension to other item.
	if (OwnerId.IsValid())
	{
		const FArcItemData* OwnerEntry = OwnerComponent->GetItemPtr(OwnerId);
		// there is still chance owner have not replicated if both items were added at the
		// same time.
		if (OwnerEntry != nullptr)
		{
			AttachToItem(OwnerId, AttachedToSlot);
		}
		else
		{
			WaitOwnerItemAdded = ItemsSubsystem->AddActorOnItemAddedToStoreMap(OwnerComponent->GetOwner(), OwnerId
				, FArcGenericItemStoreDelegate::FDelegate::CreateLambda([this](UArcItemsStoreComponent* InItemsStore, const FArcItemData* InOwnerItem)
				{
					UArcItemsSubsystem* ItemsSubsystem = UArcItemsSubsystem::Get(InItemsStore);
					if (InOwnerItem)
					{
						AttachToItem(InOwnerItem->GetItemId(), AttachedToSlot);	
					}
					
					ItemsSubsystem->RemoveActorOnItemAddedToStoreMap(InItemsStore->GetOwner(), InOwnerItem->GetItemId(), WaitOwnerItemAdded);
				}));
		}
	}

	ItemsSubsystem->BroadcastOnItemAddedToStore(OwnerComponent, this);
	ItemsSubsystem->OnItemAddedToStoreDynamic.Broadcast(OwnerComponent, GetItemId());
	ItemsSubsystem->BroadcastActorOnItemAddedToStore(OwnerComponent->GetOwner(), OwnerComponent, this);
	ItemsSubsystem->BroadcastActorOnItemAddedToStoreMap(OwnerComponent->GetOwner(), ItemId, OwnerComponent, this);
	
	InArraySerializer.Owner->PostAdd(this);
}

void FArcItemData::PostReplicatedChange(const FArcItemsArray& InArraySerializer)
{
	SetItemInstances(ItemInstances);

	GetItemsStoreComponent()->RemovePendingItem(ItemId);
	
	ArcItemsHelper::ForEachFragment<FArcItemFragment_ItemInstanceBase>(this
			, [](const FArcItemData* ItemData, const FArcItemFragment_ItemInstanceBase* InFragment)
			{
				InFragment->OnItemChanged(ItemData);
			});
	
	UArcItemsSubsystem* ItemsSubsystem = UArcItemsSubsystem::Get(OwnerComponent);

	// Clear up old attachment if old owner is valid.
	if (OldOwnerId.IsValid())
	{
		const FArcItemData* OwnerEntry = OwnerComponent->GetItemPtr(OldOwnerId);
		// there is still chance owner have not replicated if both items were added at the
		// same time.
		if (OwnerEntry != nullptr)
		{
			// we don't care about tag slot here, it should be already replicated from
			// server.
			DetachFromItem();
		}
	}
	
	if (OwnerId.IsValid())
	{
		FArcItemData* OwnerEntry = OwnerComponent->GetItemPtr(OwnerId);
		// there is still chance owner have not replicated if both items were added at the
		// same time.
		if (OwnerEntry != nullptr)
		{
			AttachToItem(OwnerId, AttachedToSlot);
			OwnerEntry->OnItemChanged();
		}
		else
		{
			WaitOwnerItemAdded = ItemsSubsystem->AddActorOnItemAddedToStoreMap(OwnerComponent->GetOwner(), OwnerId
				, FArcGenericItemStoreDelegate::FDelegate::CreateLambda([this](UArcItemsStoreComponent* InItemsStore
					, const FArcItemData* InOwnerItem)
				{
					UArcItemsSubsystem* ItemsSubsystem = UArcItemsSubsystem::Get(InItemsStore);
					if (InOwnerItem)
					{
						AttachToItem(InOwnerItem->GetItemId(), AttachedToSlot);	
					}
					ItemsSubsystem->RemoveActorOnItemAddedToStoreMap(OwnerComponent->GetOwner(), InOwnerItem->GetItemId(), WaitOwnerItemAdded);
				}));
		}
	}

	if (OldSlot.IsValid()
		&& Slot.IsValid()
		&& OldSlot != Slot)
	{
		UE_LOGFMT(LogArcItems, Log, "ChangeSlot {0} OldSlot {1} NewSlot {2}", GetItemDefinition()->GetName(), OldSlot.ToString(), Slot.ToString());
		ChangeSlot(Slot);
		ItemsSubsystem->BroadcastActorOnSlotChanged(GetItemsStoreComponent()->GetOwner(), GetItemsStoreComponent(), Slot, this);
		ItemsSubsystem->BroadcastActorOnItemSlotChangedMap(GetItemsStoreComponent()->GetOwner(), GetItemId(), GetItemsStoreComponent(), this);
	}
	else if (Slot.IsValid() && Slot != FGameplayTag::EmptyTag && OwnerId.IsNone())
	{
		if (bAddedToSlot == false)
		{
			UE_LOGFMT(LogArcItems, Log, "AddToSlot {0} NewSlot {1}", GetItemDefinition()->GetName(), Slot.ToString());
			AddToSlot(Slot);

			ItemsSubsystem->OnItemAddedToSlotDynamic.Broadcast(GetItemsStoreComponent(), GetSlotId(), GetItemId());
			ItemsSubsystem->BroadcastActorOnAddedToSlot(GetItemsStoreComponent()->GetOwner(), GetItemsStoreComponent(), GetSlotId(), this);
			ItemsSubsystem->BroadcastActorOnItemAddedToSlotMap(GetItemsStoreComponent()->GetOwner(), ItemId, GetItemsStoreComponent(), this);
			ItemsSubsystem->BroadcastActorOnAddedToSlotMap(GetItemsStoreComponent()->GetOwner(), GetSlotId(), GetItemsStoreComponent(),this);
			bAddedToSlot = true;
			bRemoveFromSlot = false;
		}
	}
	else if (OwnerId.IsNone() && OldSlot.IsValid() && (Slot.IsValid() == false || Slot == FGameplayTag::EmptyTag))
	{
		if (bRemoveFromSlot == false)
		{
			bRemoveFromSlot = true;
			bAddedToSlot = false;

			UE_LOGFMT(LogArcItems, Log, "RemoveToSlot {0} NewSlot {1}", GetItemDefinition()->GetName(), Slot.ToString());
			RemoveFromSlot(Slot);

			ItemsSubsystem->OnItemRemovedFromSlotDynamic.Broadcast(GetItemsStoreComponent(), GetSlotId(), GetItemId());
			ItemsSubsystem->BroadcastActorOnRemovedFromSlot(GetItemsStoreComponent()->GetOwner(), GetItemsStoreComponent(), GetSlotId(), this);
			ItemsSubsystem->BroadcastActorOnRemovedFromSlotMap(GetItemsStoreComponent()->GetOwner(), GetSlotId(), GetItemsStoreComponent(), this);
		}
	}
	
	OnItemChanged();
}

const FPrimaryAssetId& FArcItemData::GetItemDefinitionId() const
{
	return Spec.ItemDefinitionId;
}

const UArcItemDefinition* FArcItemData::GetItemDefinition() const
{
	if (ItemDefinition == nullptr && OwnerComponent != nullptr)
	{
		ItemDefinition = UArcCoreAssetManager::Get().GetAssetWithBundles<UArcItemDefinition>(OwnerComponent
			, GetItemDefinitionId()
			, false);
	}
	return ItemDefinition;
}

const FGameplayTagContainer& FArcItemData::GetItemAggregatedTags() const
{
	return ItemAggregatedTags;
}