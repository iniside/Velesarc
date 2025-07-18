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

#include "ArcCore/Equipment/ArcItemAttachmentComponent.h"

#include "Engine/World.h"

#include "Animation/AnimInstance.h"
#include "ArcAttachmentHandler.h"
#include "GameFramework/Actor.h"

#include "ArcCoreUtils.h"
#include "ArcWorldDelegates.h"

#include "Engine/SkeletalMesh.h"
#include "Components/SkeletalMeshComponent.h"
#include "Core/ArcCoreAssetManager.h"
#include "Engine/AssetManager.h"
#include "Engine/StaticMesh.h"

#include "GameFramework/Character.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"

#include "Fragments/ArcItemFragment_AnimLayer.h"
#include "Fragments/ArcItemFragment_ItemVisualAttachment.h"
#include "Player/ArcCorePlayerState.h"

#include "Player/ArcPlayerStateExtensionComponent.h"
#include "Pawn/ArcPawnData.h"
#include "Items/ArcItemsStoreComponent.h"
#include "Items/ArcItemsSubsystem.h"

#include "Items/ArcItemDefinition.h"
#include "Items/ArcItemsHelpers.h"

DEFINE_LOG_CATEGORY(LogArcItemAttachment);

UE_DEFINE_GAMEPLAY_TAG(TAG_Invalid_Item_Slot, "SlotId.Attachment.Invalid");

void UArcItemAttachmentComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	Params.Condition = COND_SkipOwner;
	
	Params.Condition = COND_None;
	Params.RepNotifyCondition = REPNOTIFY_Always;
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass
		, LinkedAnimLayer
		, Params);

	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass
		, ReplicatedAttachments
		, Params);
	
	//DOREPLIFETIME_CONDITION(ThisClass
	//	, ReplicatedAttachments
	//	, COND_SkipOwner);
}

void FArcItemAttachment::PreReplicatedRemove(const FArcItemAttachmentContainer& InArraySerializer)
{
	InArraySerializer.Owner->HandleItemRemovedFromReplication(ItemId);
}

void FArcItemAttachment::PostReplicatedAdd(const FArcItemAttachmentContainer& InArraySerializer)
{
	OwnerArray = &InArraySerializer;
	
	InArraySerializer.Owner->HandleItemAddedFromReplication(ItemId);
}

void FArcItemAttachment::PostReplicatedChange(const FArcItemAttachmentContainer& InArraySerializer)
{
	InArraySerializer.Owner->HandleItemChangedFromReplication(ItemId);
}

bool FArcItemAttachment::operator==(const FArcItemAttachment& Other) const
{
	const bool bDifferent = ItemId == Other.ItemId
							&& OwnerId == Other.OwnerId
							&& SocketName == Other.SocketName
							&& ChangedSocket == Other.ChangedSocket
							&& SlotId == Other.SlotId
							&& OwnerSlotId == Other.OwnerSlotId
							//&& RelativeTransform == Other.RelativeTransform
							&& ChangeUpdate == Other.ChangeUpdate
							&& VisualItemDefinition == Other.VisualItemDefinition
							&& OldVisualItemDefinition == Other.OldVisualItemDefinition
							&& ItemDefinition == Other.ItemDefinition
							&& OwnerItemDefinition == Other.OwnerItemDefinition;
	//&& Other.CustomData == Other.CustomData;
	if (!bDifferent)
	{
		if (OwnerArray)
		{
			OwnerArray->ChangedItems.FindOrAdd(ItemId) = *this;	
		}
	}
	return bDifferent;
}
UClass* UArcItemAttachmentComponent::GetItemStoreClass() const
{
	return ItemSlotClass;
}

#if WITH_EDITORONLY_DATA
void UArcItemAttachmentComponent::UpdateAssetBundleData()
{
	if (UAssetManager::IsInitialized())
	{
		AssetBundleData.Reset();
		UAssetManager::Get().InitializeAssetBundlesFromMetadata(this, AssetBundleData);

		for (FArcItemAttachmentSlot& AS : StaticAttachmentSlots.Slots)
		{
			for (FInstancedStruct& IS : AS.Handlers)
			{
				if (IS.IsValid() == false)
				{
					continue;
				}
				
				UAssetManager::Get().InitializeAssetBundlesFromMetadata(IS.GetScriptStruct(), IS.GetMemory(), AssetBundleData, IS.GetScriptStruct()->GetFName());
			}
		}
	}
}
#endif

TArray<USkeletalMeshComponent*> UArcItemAttachmentComponent::FindAttachedSkeletalMeshes(const UArcItemDefinition* InItemDefinition
																						, USkeletalMesh* InSkeletalMesh) const
{
	TArray<USkeletalMeshComponent*> Found;
	Found.Reserve(8);
		
	if (const TArray<UObject*>* Objects = ObjectsAttachedFromItem.Find(InItemDefinition))
	{
		for (UObject* Obj : (*Objects))
		{
			if (USkeletalMeshComponent* SMC = Cast<USkeletalMeshComponent>(Obj))
			{
				if (SMC->GetSkeletalMeshAsset() == InSkeletalMesh)
				{
					Found.Add(SMC);
				}
			}
		}
	}

	return Found;
}

bool UArcItemAttachmentComponent::IsSocketTaken(const FGameplayTag& InSlot
												, const FName& InSocketName) const
{
	if (const TSet<FName>* Sockets = TakenSockets.Find(InSlot))
	{
		return Sockets->Contains(InSocketName);
	}

	return false;
}

bool UArcItemAttachmentComponent::DoesSlotHaveAttachedActor(const FGameplayTag& InSlot) const
{
	int32 Idx = ReplicatedAttachments.IndexOfByPredicate([InSlot, this] (const FArcItemAttachment& Item)
	{
		return (Item.SlotId == InSlot);
	});

	if (Idx == INDEX_NONE)
	{
		return false;
	}

	AActor* ActorCheck = FindFirstAttachedObject<AActor>(ReplicatedAttachments.Items[Idx].ItemDefinition);

	return ActorCheck != nullptr;
}

ACharacter* UArcItemAttachmentComponent::FindCharacter() const
{
	if (APlayerState* PS = GetOwner<APlayerState>())
	{
		return PS->GetPawn<ACharacter>();
	}

	return GetOwner<ACharacter>();
}

void UArcItemAttachmentComponent::SetVisualItemAttachment(UArcItemDefinition* InItemDefinition, const FArcItemId& ForItem)
{
	APlayerState* PS = Cast<APlayerState>(GetOwner());
	if (PS == nullptr)
	{
		return;
	}


	UArcItemsStoreComponent* SlotComp = Arcx::Utils::GetComponent<UArcItemsStoreComponent>(PS, ItemSlotClass);
	if (SlotComp == nullptr)
	{
		return;
	}

	FArcItemData* ItemData = SlotComp->GetItemPtr(ForItem);
	if (ItemData == nullptr)
	{
		return;
	}

	UArcCoreAssetManager& AM = UArcCoreAssetManager::Get();
	FArcItemInstance_ItemVisualAttachment* VisualInstance = ArcItems::FindMutableInstance<FArcItemInstance_ItemVisualAttachment>(ItemData);
	VisualInstance->VisualItem = AM.GetPrimaryAssetIdForObject(InItemDefinition);

	int32 Idx = ReplicatedAttachments.IndexOfByKey(ForItem);
	if (Idx == INDEX_NONE)
	{
		return;
	}
	ReplicatedAttachments.Items[Idx].OldVisualItemDefinition = ReplicatedAttachments.Items[Idx].VisualItemDefinition;
	ReplicatedAttachments.Items[Idx].VisualItemDefinition = InItemDefinition;
		
	ReplicatedAttachments.ItemChanged(ForItem);
}

void UArcItemAttachmentComponent::ResetVisualItemAttachment(const FArcItemId& ForItem)
{
	check(false);
}

void UArcItemAttachmentComponent::AddAttachedItem(const FArcItemAttachment& InAttachment)
{
	int32 Idx = ReplicatedAttachments.AddItemUnique(InAttachment);
	if (Idx == INDEX_NONE)
	{
		return;
	}
	
	MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, ReplicatedAttachments, this);
	TakenSockets.FindOrAdd(InAttachment.SlotId).Add(InAttachment.SocketName);

	//if (GetNetMode() != ENetMode::NM_DedicatedServer)
	//{
	//	FArcItemAttachment* LocalOwnerAttachment = const_cast<FArcItemAttachment*>(GetAttachment(InAttachment.OwnerId));
	//	if (LocalOwnerAttachment != nullptr)
	//	{
	//		const FArcItemFragment_ItemAttachmentSlots* Fragment = InAttachment.OwnerItemDefinition->FindFragment<FArcItemFragment_ItemAttachmentSlots>();
	//		if (Fragment != nullptr)
	//		{
	//			for (const FInstancedStruct& IS : Fragment->Handlers)
	//			{
	//				IS.GetPtr<FArcAttachmentHandler>()->HandleItemAttach(this, InAttachment.ItemId, InAttachment.OwnerId);
	//			}
	//			return;				
	//		}
	//	}
	//}
}

void UArcItemAttachmentComponent::AddAttachedItem(const FArcItemData* InItem
	, const FArcItemData* InOwnerItem
	, const FName& AttachSocket
	, UArcItemDefinition* InVisualItem)
{
	FArcItemAttachment Attached;
	Attached.SlotId = InItem->GetSlotId();
	Attached.OwnerSlotId = InOwnerItem != nullptr ? InOwnerItem->GetSlotId() : FGameplayTag::EmptyTag;
	Attached.ItemId = InItem->GetItemId();
	Attached.OwnerId = InOwnerItem != nullptr ? InOwnerItem->GetItemId() : FArcItemId::InvalidId;
	Attached.SocketName = AttachSocket;
	Attached.VisualItemDefinition = InVisualItem;
	Attached.ItemDefinition = InItem->GetItemDefinition();
	Attached.OwnerItemDefinition = InOwnerItem != nullptr ? InOwnerItem->GetItemDefinition() : nullptr;

	AddAttachedItem(Attached);
}

// Sets default values for this component's properties
UArcItemAttachmentComponent::UArcItemAttachmentComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every
	// frame.  You can turn these features off to improve performance if you don't need
	// them.
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	bWantsInitializeComponent = true;
	// ...
}

UArcItemAttachmentComponent* UArcItemAttachmentComponent::FindItemAttachmentComponent(AActor* InActor)
{
	return InActor != nullptr ? InActor->FindComponentByClass<UArcItemAttachmentComponent>() : nullptr;
}

void UArcItemAttachmentComponent::OnRegister()
{
	ReplicatedAttachments.Owner = this;
	
	if (Arcx::Utils::IsPlayableWorld(GetWorld()))
	{
		if (GetOwner()->HasAuthority())
		{
			UArcItemsSubsystem* ItemsSubsystem = UArcItemsSubsystem::Get(this);
			if (ItemsSubsystem != nullptr)
			{
				ItemsSubsystem->AddActorOnAddedToSlot(GetOwner(), FArcGenericItemSlotDelegate::FDelegate::CreateUObject(
					this, &UArcItemAttachmentComponent::HandleItemAddedToSlot
				));

				ItemsSubsystem->AddActorOnRemovedFromSlot(GetOwner(), FArcGenericItemSlotDelegate::FDelegate::CreateUObject(
					this, &UArcItemAttachmentComponent::HandleItemRemovedFromSlot
				));
			
				ItemsSubsystem->AddActorOnAddedToSocket(GetOwner(), FArcGenericItemSocketSlotDelegate::FDelegate::CreateUObject(
					this, &UArcItemAttachmentComponent::HandleOnItemAttachedToSocket
				));
			}
		}
	}
	
	Super::OnRegister();
}

void UArcItemAttachmentComponent::InitializeComponent()
{
	Super::InitializeComponent();
	
	UArcPlayerStateExtensionComponent* PSExt = UArcPlayerStateExtensionComponent::Get(GetOwner());

	if (PSExt != nullptr)
	{
		PSExt->AddOnPawnReady(FArcPlayerPawnReady::FDelegate::CreateUObject(
			this, &UArcItemAttachmentComponent::HandleOnPlayerPawnReady
		));
	}

	for (FArcItemAttachmentSlot& AttachSlot : StaticAttachmentSlots.Slots)
	{
		for (FInstancedStruct& ISHandler : AttachSlot.Handlers)
		{
			if (FArcAttachmentHandler* Handler = ISHandler.GetMutablePtr<FArcAttachmentHandler>())
			{
				Handler->Initialize(this);
			}
		}
	}
}

// Called when the game starts
void UArcItemAttachmentComponent::BeginPlay()
{
	ReplicatedAttachments.Owner = this;
	Super::BeginPlay();
	UE_LOG(LogArcItemAttachment, Log, TEXT("UArcItemAttachmentComponent::BeginPlay Owner %s"), *GetNameSafe(GetOwner()));
	UArcWorldDelegates::Get(this)->BroadcastActorOnComponentBeginPlay(
		FArcComponentChannelKey(GetOwner(), GetClass())
		, this);
	// ...
}

FName UArcItemAttachmentComponent::GetItemSocket(FArcItemId InItemId) const
{
	for (const FArcItemAttachment& AttachmentContainer : ReplicatedAttachments.Items)
	{
		if (AttachmentContainer.ItemId == InItemId)
		{
			return AttachmentContainer.SocketName;
		}
	}
	return NAME_None;
}

AActor* UArcItemAttachmentComponent::GetAttachedActor(FArcItemId InItemId) const
{
	int32 Idx = ReplicatedAttachments.IndexOfByPredicate([InItemId, this] (const FArcItemAttachment& Item)
		{
			return (Item.ItemId == InItemId);
		});

	if (Idx == INDEX_NONE)
	{
		return nullptr;
	}

	AActor* ActorCheck = FindFirstAttachedObject<AActor>(ReplicatedAttachments.Items[Idx].ItemDefinition);

	return ActorCheck;
}

void UArcItemAttachmentComponent::AttachItemToSocket(const FArcItemId& InItem
													 , FName InSocketName
													 , FName SceneComponentTag
													 , const FTransform& RelativeTransform)
{
	const int32 Idx = ReplicatedAttachments.IndexOfByKey(InItem);
	if (Idx == INDEX_NONE)
	{
		return;
	}

	APlayerState* PS = Cast<APlayerState>(GetOwner());
	ACharacter* Character = PS->GetPawn<ACharacter>();
	if (Character == nullptr)
	{
		return;
	}

	ReplicatedAttachments[Idx].ChangedSocket = InSocketName;
	ReplicatedAttachments[Idx].ChangeSceneComponentTag = SceneComponentTag;
	HandleAttachSocketChanged(ReplicatedAttachments[Idx].ItemId);
	ReplicatedAttachments.ItemChanged(InItem);
}

void UArcItemAttachmentComponent::DetachItemFromSocket(const FArcItemId& InItem)
{
	const int32 Idx = ReplicatedAttachments.IndexOfByKey(InItem);
	if (Idx == INDEX_NONE)
	{
		return;
	}

	APlayerState* PS = Cast<APlayerState>(GetOwner());
	ACharacter* Character = PS->GetPawn<ACharacter>();
	if (Character == nullptr)
	{
		return;
	}

	ReplicatedAttachments[Idx].ChangedSocket = NAME_None;
	ReplicatedAttachments[Idx].ChangeSceneComponentTag = NAME_None;
	HandleAttachSocketChanged(ReplicatedAttachments[Idx].ItemId);
	ReplicatedAttachments.ItemChanged(InItem);
}

void UArcItemAttachmentComponent::LinkAnimLayer(const FGameplayTag& InSlot)
{
	APlayerState* PS = Cast<APlayerState>(GetOwner());
	if (PS == nullptr)
	{
		return;
	}

	ACharacter* Character = PS->GetPawn<ACharacter>();
	if (Character == nullptr)
	{
		return;
	}

	UArcItemsStoreComponent* SlotComp = Arcx::Utils::GetComponent<UArcItemsStoreComponent>(PS
		, ItemSlotClass);

	const FArcItemData* ItemData = SlotComp->GetItemFromSlot(InSlot);
	if (ItemData == nullptr)
	{
		return;
	}

	const FArcItemFragment_AnimLayer* Fragment_AnimLayer = ArcItems::GetFragment<FArcItemFragment_AnimLayer>(ItemData);
	if (Fragment_AnimLayer)
	{
		for (const TSoftClassPtr<class UAnimInstance>& AnimLayer : Fragment_AnimLayer->AnimLayerToLink)
		{
			Character->GetMesh()->GetAnimInstance()->LinkAnimClassLayers(AnimLayer.LoadSynchronous());

			UAnimInstance* LinkedInstance = Character->GetMesh()->GetAnimInstance()->GetLinkedAnimLayerInstanceByClass(AnimLayer.LoadSynchronous());
			if (IArcAnimLayerItemInterface* LinkedLayer = Cast<IArcAnimLayerItemInterface>(LinkedInstance))
			{
				LinkedLayer->SetSourceItemDefinition(ItemData->GetItemDefinition());
			}
		}

		LinkedAnimLayer.SourceItemDefinition = ItemData->GetItemDefinition();
		LinkedAnimLayer.AnimLayers = Fragment_AnimLayer->AnimLayerToLink;
		LinkedAnimLayer.OwningItem = ItemData->GetItemId();
		
		MARK_PROPERTY_DIRTY_FROM_NAME(UArcItemAttachmentComponent
			, LinkedAnimLayer
			, this);
	}
}

void UArcItemAttachmentComponent::LinkAnimLayer(const FArcItemId& InItemId)
{
	APlayerState* PS = Cast<APlayerState>(GetOwner());
	if (PS == nullptr)
	{
		return;
	}

	ACharacter* Character = PS->GetPawn<ACharacter>();
	if (Character == nullptr)
	{
		return;
	}

	UArcItemsStoreComponent* SlotComp = Arcx::Utils::GetComponent<UArcItemsStoreComponent>(PS
		, ItemSlotClass);

	const FArcItemData* ItemData = SlotComp->GetItemPtr(InItemId);
	if (ItemData == nullptr)
	{
		return;
	}
	
	const FArcItemFragment_AnimLayer* Fragment_AnimLayer = ArcItems::GetFragment<FArcItemFragment_AnimLayer>(ItemData);
	if (Fragment_AnimLayer)
	{
		for (const TSoftClassPtr<class UAnimInstance>& AnimLayer : Fragment_AnimLayer->AnimLayerToLink)
		{
			UAnimInstance* LinkedInstance = Character->GetMesh()->GetAnimInstance()->GetLinkedAnimLayerInstanceByClass(AnimLayer.LoadSynchronous());
			if (IArcAnimLayerItemInterface* LinkedLayer = Cast<IArcAnimLayerItemInterface>(LinkedInstance))
			{
				LinkedLayer->SetSourceItemDefinition(ItemData->GetItemDefinition());
			}
			Character->GetMesh()->GetAnimInstance()->LinkAnimClassLayers(AnimLayer.LoadSynchronous());
		}

		LinkedAnimLayer.SourceItemDefinition = ItemData->GetItemDefinition();
		LinkedAnimLayer.AnimLayers = Fragment_AnimLayer->AnimLayerToLink;
		LinkedAnimLayer.OwningItem = ItemData->GetItemId();
		
		MARK_PROPERTY_DIRTY_FROM_NAME(UArcItemAttachmentComponent
			, LinkedAnimLayer
			, this);
	}
}

void UArcItemAttachmentComponent::UnlinkAnimLayer(const FGameplayTag& InSlot)
{
	AArcCorePlayerState* PS = Cast<AArcCorePlayerState>(GetOwner());
	if (PS == nullptr)
	{
		return;
	}

	ACharacter* Character = PS->GetPawn<ACharacter>();
	if (Character == nullptr)
	{
		return;
	}

	UArcItemsStoreComponent* SlotComp = Arcx::Utils::GetComponent<UArcItemsStoreComponent>(PS
		, ItemSlotClass);

	const FArcItemData* ItemData = SlotComp->GetItemFromSlot(InSlot);
	if (ItemData == nullptr)
	{
		return;
	}

	const FArcItemFragment_AnimLayer* Fragment_AnimLayer = ArcItems::GetFragment<FArcItemFragment_AnimLayer>(ItemData);
	if (Fragment_AnimLayer)
	{
		for (const TSoftClassPtr<class UAnimInstance>& AnimLayer : Fragment_AnimLayer->AnimLayerToLink)
		{
			UAnimInstance* Layer = Character->GetMesh()->GetAnimInstance()->GetLinkedAnimLayerInstanceByClass(
				AnimLayer.LoadSynchronous());

			// seems like it unlink anything it wants..
			if (Layer)
			{
				Character->GetMesh()->GetAnimInstance()->UnlinkAnimClassLayers(AnimLayer.LoadSynchronous());
			}
		}
		
		LinkedAnimLayer.AnimLayers.Empty();
		LinkedAnimLayer.OwningItem = ItemData->GetItemId();

		const UArcPawnData* PawnData = PS->GetPawnData<UArcPawnData>();
		const FArcPawnInitializationFragment_SetAnimBlueprint* DefaultAnimLayers = PawnData->FindInitializationFragment<FArcPawnInitializationFragment_SetAnimBlueprint>();
		if(DefaultAnimLayers)
		{
			for (const TSoftClassPtr<class UAnimInstance>& AnimLayer : DefaultAnimLayers->GetAnimLayers())
			{
				// seems like it unlink anything it wants..
				Character->GetMesh()->GetAnimInstance()->LinkAnimClassLayers(AnimLayer.LoadSynchronous());
				LinkedAnimLayer.AnimLayers.Add(AnimLayer);
				
			}	
		}
		
		MARK_PROPERTY_DIRTY_FROM_NAME(UArcItemAttachmentComponent
			, LinkedAnimLayer
			, this);
	}
}

void UArcItemAttachmentComponent::CallbackItemAttached(const FArcItemId InItemId)
{
	if (const FArcItemId* OwnerItemId = PendingAttachments.Find(InItemId))
	{
		FArcItemId ItemId = PendingAttachments[InItemId];

		int32 ChildAttachIdx = ReplicatedAttachments.IndexOfByKey(ItemId);
		if (ChildAttachIdx == INDEX_NONE)
		{
			return;
		}
		
		const FArcItemAttachment& ChildItemAttachment = ReplicatedAttachments[ChildAttachIdx];

		const FArcItemAttachment* LocalOwnerAttachment = GetAttachment(ChildItemAttachment.OwnerId);
		if (LocalOwnerAttachment != nullptr)
		{
			UObject* OwnerAttachedObject = FindFirstAttachedObject(ChildItemAttachment.OwnerItemDefinition);
			if (OwnerAttachedObject == nullptr)
			{
				return;
			}
			
			const FArcItemFragment_ItemAttachmentSlots* Fragment = ChildItemAttachment.OwnerItemDefinition->FindFragment<FArcItemFragment_ItemAttachmentSlots>();
			if (Fragment == nullptr)
			{
				return;				
			}

			for (const FArcItemAttachmentSlot& ItemAttachmentSlot : Fragment->AttachmentSlots)
			{
				if (ItemAttachmentSlot.SlotId == ChildItemAttachment.SlotId)
				{
					for (const FInstancedStruct& IS : ItemAttachmentSlot.Handlers)
					{
						IS.GetPtr<FArcAttachmentHandler>()->HandleItemAttach(this, ChildItemAttachment.ItemId, ChildItemAttachment.OwnerId);
					}
				}
			}
		}
	}
}

void UArcItemAttachmentComponent::HandleItemAddedFromReplication(const FArcItemId InItemId)
{
	// TODO: When OwnerSlotId is set, it means we are attached to something other in ReplicatedAttachments.
	// We are not handling that case now. It will require handling items being replicated out of order.

	int32 AttachIdx = ReplicatedAttachments.IndexOfByKey(InItemId);

	// Reusing the same function on slot replication so it can be invalid.
	if (AttachIdx == INDEX_NONE)
	{
		return;
	}

	const FArcItemAttachment& ItemAttachment = ReplicatedAttachments[AttachIdx];
	UObject* AlreadyAttachedObject = FindFirstAttachedObject(ItemAttachment.ItemDefinition);
	if(AlreadyAttachedObject != nullptr)
	{
		return;
	}

	if (FindCharacter() == nullptr)
	{
		return;
	}
	
	
	if (ItemAttachment.OwnerId.IsValid())
	{
		if (ReplicatedAttachments.Contains(ItemAttachment.OwnerId) == false)
		{
			AddPendingAttachment(ItemAttachment.OwnerId, ItemAttachment.ItemId);
			return;
		}
	}

	
	UArcItemsSubsystem* ItemsSubsystem = UArcItemsSubsystem::Get(this);

	UArcItemAttachmentSubsystem* IAS = GetWorld()->GetGameInstance()->GetSubsystem<UArcItemAttachmentSubsystem>();
	IAS->OnItemAttachedDynamic.Broadcast(this, ItemAttachment.ItemDefinition.Get(), ItemAttachment.SlotId);
	
	if (ItemAttachment.OwnerId.IsValid() == true)
	{
		int32 OwnerAttachIdx = ReplicatedAttachments.IndexOfByKey(ItemAttachment.OwnerId);
		if (OwnerAttachIdx == INDEX_NONE)
		{
			PendingAttachments.FindOrAdd(ItemAttachment.OwnerId) = InItemId;
			return;
		}
		else
		{
			const FArcItemAttachment* LocalOwnerAttachment = GetAttachment(ItemAttachment.OwnerId);
			if (LocalOwnerAttachment != nullptr)
			{
				const FArcItemFragment_ItemAttachmentSlots* Fragment = ItemAttachment.OwnerItemDefinition->FindFragment<FArcItemFragment_ItemAttachmentSlots>();
				if (Fragment != nullptr)
				{
					for (const FArcItemAttachmentSlot& ItemAttachmentSlot : Fragment->AttachmentSlots)
					{
						if (ItemAttachmentSlot.SlotId == ItemAttachment.SlotId)
						{
							for (const FInstancedStruct& IS : ItemAttachmentSlot.Handlers)
							{
								IS.GetPtr<FArcAttachmentHandler>()->HandleItemAttach(this, ItemAttachment.ItemId, ItemAttachment.OwnerId);
							}
						}
					}
					
					ItemsSubsystem->BroadcastActorOnItemAttached(GetOwner(), this, &ItemAttachment);
					return;				
				}
			}
		}
	}
	
	for (const FArcItemAttachmentSlot& Slot : StaticAttachmentSlots.Slots)
	{
		if (Slot.SlotId == ItemAttachment.SlotId)
		{
			for (const FInstancedStruct& IS : Slot.Handlers)
			{
				IS.GetPtr<FArcAttachmentHandler>()->HandleItemAttach(this, InItemId, ItemAttachment.OwnerId);
			}
			ItemsSubsystem->BroadcastActorOnItemAttached(GetOwner(), this, &ItemAttachment);
			return;
		}
	}

	if (const FArcItemId* OwnerItemId = PendingAttachments.Find(InItemId))
	{
		FArcItemId ItemId = PendingAttachments[InItemId];

		int32 ChildAttachIdx = ReplicatedAttachments.IndexOfByKey(ItemId);
		const FArcItemAttachment& ChildItemAttachment = ReplicatedAttachments[ChildAttachIdx];

		const FArcItemAttachment* LocalOwnerAttachment = GetAttachment(ChildItemAttachment.OwnerId);
		if (LocalOwnerAttachment != nullptr)
		{
			const FArcItemFragment_ItemAttachmentSlots* Fragment = ChildItemAttachment.OwnerItemDefinition->FindFragment<FArcItemFragment_ItemAttachmentSlots>();
			if (Fragment != nullptr)
			{
				for (const FArcItemAttachmentSlot& ItemAttachmentSlot : Fragment->AttachmentSlots)
				{
					if (ItemAttachmentSlot.SlotId == ItemAttachment.SlotId)
					{
						for (const FInstancedStruct& IS : ItemAttachmentSlot.Handlers)
						{
							IS.GetPtr<FArcAttachmentHandler>()->HandleItemAttach(this, ChildItemAttachment.ItemId, ChildItemAttachment.OwnerId);
						}
					}
				}
				
				ItemsSubsystem->BroadcastActorOnItemAttached(GetOwner(), this, &ItemAttachment);
				return;				
			}
		}
	}
}

void UArcItemAttachmentComponent::HandleItemRemovedFromReplication(const FArcItemId InItemId)
{
	int32 AttachIdx = ReplicatedAttachments.IndexOfByKey(InItemId);
	
	if (AttachIdx == INDEX_NONE)
	{
		return;
	}

	UArcItemsSubsystem* ItemsSubsystem = UArcItemsSubsystem::Get(this);
	
	FArcItemAttachment& ItemAttachment = ReplicatedAttachments[AttachIdx];

	ItemsSubsystem->BroadcastActorOnItemDetached(GetOwner(), this, &ItemAttachment);

	//if (AActor* AttachedActor = FindFirstAttachedObject<AActor>(ReplicatedAttachments.Items[AttachIdx].ItemDefinition))
	//{
	//	AttachedActor->SetHidden(true);
	//	AttachedActor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	//	AttachedActor->Destroy();
	//	AttachedActor = nullptr;
	//}
	//else if (USceneComponent* AttachedComponent = FindFirstAttachedObject<USceneComponent>(ReplicatedAttachments.Items[AttachIdx].ItemDefinition))
	//{
	//	AttachedComponent->SetHiddenInGame(true);
	//	AttachedComponent->SetVisibility(false);
	//	AttachedComponent->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	//	AttachedComponent->DestroyComponent();
	//	AttachedComponent = nullptr;
	//}

	
	UArcItemAttachmentSubsystem* IAS = GetWorld()->GetGameInstance()->GetSubsystem<UArcItemAttachmentSubsystem>();
	IAS->OnItemDetachedDynamic.Broadcast(this, ItemAttachment.ItemDefinition.Get(), ItemAttachment.SlotId);
	
	for (const FArcItemAttachmentSlot& AttachmentSlot : StaticAttachmentSlots.Slots)
	{
		if (AttachmentSlot.SlotId == ItemAttachment.SlotId)
		{
			for (const FInstancedStruct& InstancedStruct : AttachmentSlot.Handlers)
			{
				const FArcAttachmentHandler* AttachmentHandler = InstancedStruct.GetPtr<FArcAttachmentHandler>();
				
				AttachmentHandler->HandleItemDetach(this
					, ItemAttachment.ItemId
					, ItemAttachment.OwnerId);
			}
		}
	}
	
	RemoveAttachedForItem(ItemAttachment.ItemDefinition);
}

void UArcItemAttachmentComponent::HandleItemChangedFromReplication(const FArcItemId InItemId)
{
	int32 AttachIdx = ReplicatedAttachments.IndexOfByKey(InItemId);
	
	if (AttachIdx == INDEX_NONE)
	{
		return;
	}

	UArcItemsSubsystem* ItemsSubsystem = UArcItemsSubsystem::Get(this);
	
	FArcItemAttachment& ItemAttachment = ReplicatedAttachments[AttachIdx];

	for (const FArcItemAttachmentSlot& Slot : StaticAttachmentSlots.Slots)
	{
		if (Slot.SlotId == ItemAttachment.SlotId)
		{
			for (const FInstancedStruct& IS : Slot.Handlers)
			{
				IS.GetPtr<FArcAttachmentHandler>()->HandleItemAttachmentChanged(this, ItemAttachment);
			}
		}
	}

	HandleAttachSocketChanged(InItemId);
	
	ItemsSubsystem->BroadcastActorOnItemAttachmentChanged(GetOwner(), this, &ItemAttachment);
}

void UArcItemAttachmentComponent::HandleAttachSocketChanged(const FArcItemId& InItemId)
{
	int32 AttachIdx = ReplicatedAttachments.IndexOfByKey(InItemId);
	
	if (AttachIdx == INDEX_NONE)
	{
		return;
	}

	FArcItemAttachment& ItemAttachment = ReplicatedAttachments[AttachIdx];
	
	const FName NewSocket = ReplicatedAttachments.Items[AttachIdx].ChangedSocket != NAME_None ? ReplicatedAttachments.Items[AttachIdx].ChangedSocket : ReplicatedAttachments.Items[AttachIdx].SocketName;
	const FName ComponentTag = ReplicatedAttachments.Items[AttachIdx].ChangeSceneComponentTag != NAME_None ? ReplicatedAttachments.Items[AttachIdx].ChangeSceneComponentTag : ReplicatedAttachments[AttachIdx].SocketComponentTag;

	USceneComponent* AttachedTo = nullptr;

	if (ItemAttachment.OwnerItemDefinition == nullptr)
	{
		AttachedTo = FindCharacter()->GetMesh();
		if (!ComponentTag.IsNone())
		{
			AttachedTo = FindCharacter()->FindComponentByTag<USceneComponent>(ComponentTag);
		}
	}
	else if (ItemAttachment.OwnerItemDefinition != nullptr)
	{
		AttachedTo = FindFirstAttachedObject<USceneComponent>(ItemAttachment.OwnerItemDefinition);	
	}

	if (AttachedTo)
	{
		if (AActor* AttachedActor = FindFirstAttachedObject<AActor>(ReplicatedAttachments.Items[AttachIdx].ItemDefinition))
		{
			AttachedActor->AttachToComponent(AttachedTo
				, FAttachmentTransformRules::SnapToTargetNotIncludingScale
				, NewSocket);
		}
		else if (USceneComponent* AttachedComponent = FindFirstAttachedObject<USceneComponent>(ReplicatedAttachments.Items[AttachIdx].ItemDefinition))
		{
			AttachedComponent->AttachToComponent(AttachedTo
				, FAttachmentTransformRules::SnapToTargetNotIncludingScale
				, NewSocket);
		}
	}
}

void UArcItemAttachmentComponent::HandleItemAddedToSlot(UArcItemsStoreComponent* InItemSlots
														, const FGameplayTag& SlotId
														, const FArcItemData* Item)
{
	if (InItemSlots->GetOwner() != GetOwner())
	{
		return;
	}
	
	if (InItemSlots->GetClass() != ItemSlotClass)
	{
		return;
	}

	if (FindCharacter() == nullptr)
	{
		return;
	}

	if (Item->GetOwnerId().IsValid())
	{
		const FArcItemData* OwnerItemData = InItemSlots->GetItemPtr(Item->GetOwnerId());
		
		const FArcItemFragment_ItemAttachmentSlots* Fragment = ArcItems::FindFragment<FArcItemFragment_ItemAttachmentSlots>(OwnerItemData);
		if (Fragment)
		{
			for (const FArcItemAttachmentSlot& ItemAttachmentSlot : Fragment->AttachmentSlots)
			{
				if (ItemAttachmentSlot.SlotId == Item->GetSlotId())
				{
					for (const FInstancedStruct& IS : ItemAttachmentSlot.Handlers)
					{
						IS.GetPtr<FArcAttachmentHandler>()->HandleItemAddedToSlot(this, Item, OwnerItemData);
					}
				}
			}
			
			return;
		}
	}
	
	for (const FArcItemAttachmentSlot& AttachmentSlot : StaticAttachmentSlots.Slots)
	{
		if (AttachmentSlot.SlotId == SlotId)
		{
			for (const FInstancedStruct& InstancedStruct : AttachmentSlot.Handlers)
			{
				const FArcAttachmentHandler* AttachmentHandler = InstancedStruct.GetPtr<FArcAttachmentHandler>();
				AttachmentHandler->HandleItemAddedToSlot(this
					, Item
					, nullptr);
			}
		}
	}
	
	if (GetNetMode() != ENetMode::NM_DedicatedServer)
	{
		HandleItemAddedFromReplication(Item->GetItemId());
	}
}

void UArcItemAttachmentComponent::HandleOnItemAttachedToSocket(UArcItemsStoreComponent* ItemSlotComponent
	, const FGameplayTag& ItemSlot
	, const FArcItemData* OwnerItemData
	, const FGameplayTag& ItemSocketSlot
	, const FArcItemData* SocketItemData)
{
	if (ItemSlotComponent->GetOwner() != GetOwner())
	{
		return;
	}
	
	if (ItemSlotComponent->GetClass() != ItemSlotClass)
	{
		return;
	}

	if (FindCharacter() == nullptr)
	{
		return;
	}

	if (SocketItemData->GetOwnerId().IsValid())
	{
		//const FArcItemData* OwnerItemData = ItemSlotComponent->GetItemPtr(SocketItemData->GetOwnerId());
		
		const FArcItemFragment_ItemAttachmentSlots* Fragment = ArcItems::FindFragment<FArcItemFragment_ItemAttachmentSlots>(OwnerItemData);
		if (Fragment)
		{
			for (const FArcItemAttachmentSlot& ItemAttachmentSlot : Fragment->AttachmentSlots)
			{
				if (ItemAttachmentSlot.SlotId == SocketItemData->GetSlotId())
				{
					for (const FInstancedStruct& IS : ItemAttachmentSlot.Handlers)
					{
						IS.GetPtr<FArcAttachmentHandler>()->HandleItemAddedToSlot(this, SocketItemData, OwnerItemData);
					}
				}
			}
			
			return;
		}
	}
	
	//for (const FArcItemAttachmentSlot& AttachmentSlot : AttachmentSlots)
	//{
	//	if (AttachmentSlot.SlotId == SlotId)
	//	{
	//		for (const FInstancedStruct& InstancedStruct : AttachmentSlot.Handlers)
	//		{
	//			const FArcAttachmentHandler* AttachmentHandler = InstancedStruct.GetPtr<FArcAttachmentHandler>();
	//			AttachmentHandler->HandleItemAddedToSlot(this
	//				, Item
	//				, nullptr);
	//		}
	//	}
	//}
	
	if (GetNetMode() != ENetMode::NM_DedicatedServer)
	{
		HandleItemAddedFromReplication(SocketItemData->GetItemId());
	}
}

void UArcItemAttachmentComponent::HandleItemRemovedFromSlot(UArcItemsStoreComponent* InItemSlots
															, const FGameplayTag& SlotId
															, const FArcItemData* Item)
{
	if (InItemSlots->GetOwner() != GetOwner())
	{
		return;
	}
	if (InItemSlots->GetClass() != ItemSlotClass)
	{
		return;
	}

	if (GetNetMode() != ENetMode::NM_DedicatedServer)
	{
		HandleItemRemovedFromReplication(Item->GetItemId());
	}
	
	for (const FArcItemAttachmentSlot& AttachmentSlot : StaticAttachmentSlots.Slots)
	{
		if (AttachmentSlot.SlotId == SlotId)
		{
			for (const FInstancedStruct& InstancedStruct : AttachmentSlot.Handlers)
			{
				const FArcAttachmentHandler* AttachmentHandler = InstancedStruct.GetPtr<FArcAttachmentHandler>();
				
				AttachmentHandler->HandleItemRemovedFromSlot(this
					, Item
					, AttachmentSlot.SlotId
					, nullptr);
			}
		}
	}
	
	ReplicatedAttachments.RemoveItem(Item->GetItemId());
}

void UArcItemAttachmentComponent::OnRep_LinkedAnimLayer(const FArcLinkedAnimLayer& OldLinkedLayer)
{
	APlayerState* PS = Cast<APlayerState>(GetOwner());
	if (PS == nullptr)
	{
		return;
	}

	ACharacter* Character = PS->GetPawn<ACharacter>();
	if (Character == nullptr)
	{
		return;
	}

	//if(OldLinkedLayer.AnimLayers.Num() > 0)
	//{
	//	for (const TSubclassOf<class UAnimInstance>& AI : OldLinkedLayer.AnimLayers)
	//	{
	//		C->GetMesh()->GetAnimInstance()->UnlinkAnimClassLayers(AI);
	//	}
	//}

	if (LinkedAnimLayer.AnimLayers.Num() > 0)
	{
		for (const TSoftClassPtr<class UAnimInstance>& AI : LinkedAnimLayer.AnimLayers)
		{
			Character->GetMesh()->GetAnimInstance()->LinkAnimClassLayers(AI.LoadSynchronous());
			UAnimInstance* LinkedInstance = Character->GetMesh()->GetAnimInstance()->GetLinkedAnimLayerInstanceByClass(AI.LoadSynchronous());
			if (IArcAnimLayerItemInterface* LinkedLayer = Cast<IArcAnimLayerItemInterface>(LinkedInstance))
			{
				LinkedLayer->SetSourceItemDefinition(LinkedAnimLayer.SourceItemDefinition);
			}
		}
	}
}

void UArcItemAttachmentComponent::HandleOnPlayerPawnReady(APawn* InPawn)
{
	if (CurrentPawn.Get() == InPawn)
	{
		return;
	}

	if (CurrentPawn.IsValid() == false)
	{
		CurrentPawn = InPawn;
	}

	// if got to this point it probabaly means old was destroyed and we need new pawn.. I
	// guess ?
	UArcItemsStoreComponent* ItemSlot = Arcx::Utils::GetComponent<UArcItemsStoreComponent>(GetOwner(), ItemSlotClass);

	for (const FArcItemAttachment& AttachmentContainer : ReplicatedAttachments.Items)
	{
		if (FindFirstAttachedObject(AttachmentContainer.ItemDefinition))
		{
			continue;
		}
		
		HandleItemAddedFromReplication(AttachmentContainer.ItemId);
	}
}

const UArcItemDefinition* UArcItemAttachmentComponent::GetItemDefinitionFromAttachment(AActor* Owner, FGameplayTag SlotId)
{
	if (!Owner)
	{
		return nullptr;
	}
	
	UArcItemAttachmentComponent* ItemAttachmentComponent = Owner->FindComponentByClass<UArcItemAttachmentComponent>();
	if (ItemAttachmentComponent == nullptr)
	{
		ACharacter* Character = Cast<ACharacter>(Owner);
		if (Character)
		{
			APlayerState* PS = Character->GetPlayerState();
			if (PS)
			{
				ItemAttachmentComponent = PS->FindComponentByClass<UArcItemAttachmentComponent>();
			}
		}
	}

	if (ItemAttachmentComponent == nullptr)
	{
		return nullptr;
	}

	for (const FArcItemAttachment& Item : ItemAttachmentComponent->ReplicatedAttachments.Items)
	{
		if (Item.SlotId == SlotId)
		{
			if (Item.ItemDefinition)
			{
				return Item.ItemDefinition.Get();
			}
		}
	}
	
	return nullptr;
}
