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

#include "QuickBar/ArcQuickBarComponent.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputSubsystems.h"

#include "GameFramework/Character.h"
#include "GameFramework/PlayerState.h"

#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
#include "ArcCoreUtils.h"
#include "ArcQuickBarInputBindHandling.h"
#include "ArcWorldDelegates.h"

#include "Input/ArcCoreInputComponent.h"
#include "AbilitySystem/ArcCoreAbilitySystemComponent.h"

#include "Equipment/ArcItemAttachmentComponent.h"

#include "InputMappingContext.h"
#include "Engine/World.h"
#include "Items/ArcItemsComponent.h"
#include "Items/ArcItemsStoreComponent.h"
#include "Items/ArcItemsSubsystem.h"
#include "Misc/DataValidation.h"
#include "Net/UnrealNetwork.h"
#include "Player/ArcPlayerStateExtensionComponent.h"
#include "QuickBarAction/ArcQuickBarSelectedAction.h"
#include "QuickSlot/ArcQuickSlotHandler.h"
#include "Validators/ArcQuickBarSlotValidator.h"

DEFINE_LOG_CATEGORY(LogArcQuickBar);

/**
 * @brief
 * @param Initializer/
 */
UArcQuickBarComponentBlueprint::UArcQuickBarComponentBlueprint(const FObjectInitializer& Initializer)
	: Super(Initializer)
{
}

void FArcSelectedQuickBarSlot::PreReplicatedRemove(const FArcSelectedQuickBarSlotList& SelectionMap)
{
	UArcQuickBarComponent* QuickBar = SelectionMap.QuickBar.Get();
	UArcItemsStoreComponent* SlotComponent = QuickBar->GetItemStoreComponent(BarId);
	UArcCoreAbilitySystemComponent* ArcASC = QuickBar->GetOwner()->FindComponentByClass<UArcCoreAbilitySystemComponent>();

	int32 BarIdx = QuickBar->QuickBars.IndexOfByKey(BarId);
	int32 SlotIdx = QuickBar->QuickBars[BarIdx].Slots.IndexOfByKey(SlotId);
	if (AssignedItemId.IsValid())
	{
		QuickBar->InternalTryUnbindInput(BarIdx, SlotIdx, ArcASC);
	}

	UArcQuickBarSubsystem* QuickBarSubsystem = UArcQuickBarSubsystem::Get(QuickBar);
	QuickBarSubsystem->BroadcastActorOnItemRemovedFromQuickSlot(QuickBar->GetOwner(), QuickBar, BarId, SlotId, AssignedItemId);
	QuickBarSubsystem->OnItemRemovedFromQuickSlot.Broadcast(QuickBar, BarId, SlotId, AssignedItemId);
}

void FArcSelectedQuickBarSlot::PostReplicatedAdd(const FArcSelectedQuickBarSlotList& SelectionMap)
{
	if (AssignedItemId.IsValid() == false)
	{
		return;
	}
	
	if (HandlePendingItemAdded(SelectionMap) == false)
	{
		SelectionMap.AddPendingAddQuickSlotItem(AssignedItemId, BarId, SlotId);
	}

	SelectionMap.QuickBar->SetUnlockQuickBar();
}

bool FArcSelectedQuickBarSlot::HandlePendingItemAdded(const FArcSelectedQuickBarSlotList& SelectionMap)
{
	UArcQuickBarComponent* QuickBar = SelectionMap.QuickBar.Get();
	UArcQuickBarSubsystem* QuickBarSubsystem = UArcQuickBarSubsystem::Get(QuickBar);

	UArcItemsStoreComponent* SlotComponent = QuickBar->GetItemStoreComponent(BarId);
	UArcCoreAbilitySystemComponent* ArcASC = QuickBar->GetOwner()->FindComponentByClass<UArcCoreAbilitySystemComponent>();

	if (ArcASC->GetAvatarActor() == nullptr)
	{
		return false;
	}
	
	int32 BarIdx = QuickBar->QuickBars.IndexOfByKey(BarId);
	int32 SlotIdx = QuickBar->QuickBars[BarIdx].Slots.IndexOfByKey(SlotId);

	
	if (bIsSelected && bClientSelected == false)
	{
		if (AssignedItemId.IsValid() == false)
		{
			return false;
		}
	
		const FArcItemData* SlotData = SlotComponent->GetItemPtr(AssignedItemId);

		if (SlotData)
		{
			// TODO: This need more checks, like auto select slot on bar with multiple selected slots (like spells)
			// and should not run if there is only one selected slot and it is not this one.
	
			
			bool bHandled = true;
			if (AssignedItemId.IsValid())
			{
				bHandled = QuickBar->InternalTryBindInput(BarIdx, SlotIdx, ArcASC);
			}

			if (bHandled)
			{
				for (const FInstancedStruct& IS : QuickBar->QuickBars[BarIdx].Slots[BarIdx].SelectedHandlers)
				{
					IS.GetPtr<FArcQuickSlotHandler>()->OnSlotSelected(ArcASC
						, QuickBar
						, SlotData
						, &QuickBar->QuickBars[BarIdx]
						, &QuickBar->QuickBars[BarIdx].Slots[SlotIdx]);
				}

				QuickBarSubsystem->BroadcastActorOnQuickBarSlotChanged(QuickBar->GetOwner()
					, QuickBar
					, BarId
					, SlotId
					, FGameplayTag::EmptyTag
					, FGameplayTag::EmptyTag);
			}
			
			if (AssignedItemId.IsValid() && bHandled)
			{
				bClientSelected = true;
				bClientDeselected = false;
				QuickBarSubsystem->BroadcastActorOnItemAddedToQuickSlot(QuickBar->GetOwner(), QuickBar, BarId, SlotId, AssignedItemId);
				QuickBarSubsystem->OnItemAddedToQuickSlot.Broadcast(QuickBar, BarId, SlotId, AssignedItemId);
			}
			
			return bHandled;
		}

		return false;
	}

	return false;
}

void FArcSelectedQuickBarSlot::PostReplicatedChange(const FArcSelectedQuickBarSlotList& SelectionMap)
{
	UArcQuickBarComponent* QuickBar = SelectionMap.QuickBar.Get();
	UArcItemsStoreComponent* SlotComponent = QuickBar->GetItemStoreComponent(BarId);
	UArcCoreAbilitySystemComponent* ArcASC = QuickBar->GetOwner()->FindComponentByClass<UArcCoreAbilitySystemComponent>();

	int32 BarIdx = QuickBar->QuickBars.IndexOfByKey(BarId);
    int32 SlotIdx = QuickBar->QuickBars[BarIdx].Slots.IndexOfByKey(SlotId);

	UArcQuickBarSubsystem* QuickBarSubsystem = UArcQuickBarSubsystem::Get(QuickBar);
	
	// TODO:: Quick Bar - fix client prediction, change bools to ENUM or something.
	if (bIsSelected && bClientSelected == false)
	{
		const FArcItemData* SlotData = SlotComponent->GetItemPtr(AssignedItemId);;
		if (SlotData)
		{
			

			bool bHandled = true;
			if (AssignedItemId.IsValid() && AssignedItemId != OldAssignedItemId)
			{
				bHandled = QuickBar->InternalTryBindInput(BarIdx, SlotIdx, ArcASC);
			}

			if (bHandled)
			{
				bClientSelected = true;
				bClientDeselected = false;
				
				for (const FInstancedStruct& IS : QuickBar->QuickBars[BarIdx].Slots[BarIdx].SelectedHandlers)
				{
					IS.GetPtr<FArcQuickSlotHandler>()->OnSlotSelected(ArcASC
						, QuickBar
						, SlotData
						, &QuickBar->QuickBars[BarIdx]
						, &QuickBar->QuickBars[BarIdx].Slots[SlotIdx]);
				}
			}
		}
		else
		{
			SelectionMap.AddPendingAddQuickSlotItem(AssignedItemId, BarId, SlotId);
		}
		
	}
	else if (!bIsSelected && bClientDeselected == false)
	{
        const FArcItemData* SlotData = SlotComponent->GetItemPtr(AssignedItemId);;
        for (const FInstancedStruct& IS : QuickBar->QuickBars[BarIdx].Slots[BarIdx].SelectedHandlers)
        {
        	IS.GetPtr<FArcQuickSlotHandler>()->OnSlotDeselected(ArcASC
        		, QuickBar
        		, SlotData
        		, &QuickBar->QuickBars[BarIdx]
        		, &QuickBar->QuickBars[BarIdx].Slots[SlotIdx]);
        }

		bClientSelected = false;
		bClientDeselected = true;
		
		if (AssignedItemId.IsValid() && AssignedItemId != OldAssignedItemId)
		{
			QuickBar->InternalTryUnbindInput(BarIdx, SlotIdx, ArcASC);
		}
	}
	
	if (AssignedItemId.IsValid() == false
		&& OldAssignedItemId.IsValid()
		&& AssignedItemId != OldAssignedItemId)
	{
		QuickBar->InternalTryUnbindInput(BarIdx, SlotIdx, ArcASC);
		const FArcQuickBarInputBindHandling* Handling = QuickBar->QuickBars[BarIdx].Slots[SlotIdx].InputBind.GetPtr<FArcQuickBarInputBindHandling>();
		if (Handling != nullptr)
		{
			//const FArcItemData* Data = FindSlotData(BarId, QuickSlotId);
			const FArcItemData* Data = SlotComponent->GetItemPtr(OldAssignedItemId);
		
			TArray<const FArcItemData*> Extensions = SlotComponent->GetItemsAttachedTo(Data->GetOwnerId());
			Extensions.Add(Data);
			Handling->OnRemovedFromQuickBar(ArcASC, Extensions);
		}
	}
	
	if (AssignedItemId.IsValid() && AssignedItemId != OldAssignedItemId)
	{
		QuickBarSubsystem->BroadcastActorOnItemAddedToQuickSlot(QuickBar->GetOwner(), QuickBar, BarId, SlotId, AssignedItemId);
		QuickBarSubsystem->OnItemAddedToQuickSlot.Broadcast(QuickBar, BarId, SlotId, AssignedItemId);
	}
	
	if (AssignedItemId.IsValid() == false && AssignedItemId != OldAssignedItemId)
	{
		QuickBarSubsystem->BroadcastActorOnItemRemovedFromQuickSlot(QuickBar->GetOwner(), QuickBar, BarId, SlotId, OldAssignedItemId);
		QuickBarSubsystem->OnItemRemovedFromQuickSlot.Broadcast(QuickBar, BarId, SlotId, AssignedItemId);
	}

	SelectionMap.QuickBar->SetUnlockQuickBar();
}

void FArcSelectedQuickBarSlotList::PostReplicatedChange(const TArrayView<int32>& ChangedIndices
	, int32 FinalSize)
{
	for (int32 Idx : ChangedIndices)
	{
		Items[Idx].PostReplicatedChange(*this);
	}
}

void FArcSelectedQuickBarSlotList::HandleOnItemAddedToSlot()
{
	// TODO: This need more checks, like auto select slot on bar with multiple selected slots (like spells)
	// and should not run if there is only one selected slot and it is not this one.
	TArray<PendingSlot> Copy = PendingAddQuickSlotItems;
	for (const PendingSlot& Item : Copy)
	{
		for (FArcSelectedQuickBarSlot& SelectedSlot : Items)
		{
			if (SelectedSlot.AssignedItemId == Item.ItemId && SelectedSlot.bIsSelected)
			{
				const bool bAdded = SelectedSlot.HandlePendingItemAdded(*this);
				if (bAdded)
				{
					int32 ItemIdx = PendingAddQuickSlotItems.IndexOfByKey(Item.ItemId);
					if (ItemIdx != INDEX_NONE)
					{
						PendingAddQuickSlotItems.RemoveAt(ItemIdx);
					}
				}
			}
		}	
	}
}

UArcQuickBarSubsystem* UArcQuickBarSubsystem::Get(UObject* WorldObject)
{
	if (UWorld* World = WorldObject->GetWorld())
	{
		return World->GetGameInstance()->GetSubsystem<UArcQuickBarSubsystem>();
		//return World->GetSubsystem<UArcItemsSubsystem>();
	}

	if (UWorld* World= Cast<UWorld>(WorldObject))
	{
		return World->GetGameInstance()->GetSubsystem<UArcQuickBarSubsystem>();
		//return World->GetSubsystem<UArcItemsSubsystem>();
	}

	return nullptr;
}

const FArcItemData* UArcQuickBarComponent::FindSlotData(const FGameplayTag& BarId
														, const FGameplayTag& QuickBarSlotId) const
{
	UArcItemsStoreComponent* SlotComp = GetItemStoreComponent(BarId);

	FArcItemId ItemId = ReplicatedSelectedSlots.FindItemId(BarId, QuickBarSlotId);
	if (ItemId.IsValid() == false)
	{
		ItemId = ReplicatedSelectedSlots.FindOldItemId(BarId, QuickBarSlotId);
	}
	
	return SlotComp->GetItemPtr(ItemId);
}

const FArcQuickBarSlot* UArcQuickBarComponent::FindQuickSlot(const FGameplayTag& BarId
	, const FGameplayTag& QuickBarSlotId) const
{
	const int32 BarIdx = QuickBars.IndexOfByKey(BarId);
	const int32 QuickSlotIdx = QuickBars[BarIdx].Slots.IndexOfByKey(QuickBarSlotId);

	return &QuickBars[BarIdx].Slots[QuickSlotIdx];
}

void UArcQuickBarComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	FDoRepLifetimeParams Params;
	Params.bIsPushBased = false;

	// TODO: Only replicate items to owner ?
	Params.Condition = COND_OwnerOnly;
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, ReplicatedSelectedSlots, Params);
}

// Sets default values for this component's properties
UArcQuickBarComponent::UArcQuickBarComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every
	// frame.  You can turn these features off to improve performance if you don't need
	// them.
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	bWantsInitializeComponent = true;
	// ...
}

#if WITH_EDITOR
EDataValidationResult UArcQuickBarComponent::IsDataValid(FDataValidationContext& Context) const
{
	if (Super::IsDataValid(Context) == EDataValidationResult::Invalid)
	{
		return EDataValidationResult::Invalid;	
	}

	int32 ErrorCount = 0;
	for (const FArcQuickBar& QuickBar : QuickBars)
	{
		if (QuickBar.ItemsStoreClass == nullptr)
		{
			FString Msg = FString::Printf(TEXT("QuickBar %s has no ItemsStoreClass set."), *QuickBar.BarId.ToString());
			Context.AddError(FText::FromString(Msg));
			ErrorCount++;
		}

		
		for (const FArcQuickBarSlot& Slot : QuickBar.Slots)
		{
			if (QuickBar.bCanAutoSelectOnItemAddedToSlot && Slot.ItemSlotId.IsValid() == false)
			{
				FString Msg = FString::Printf(TEXT("QuickBar %s Slot %s has bCanAutoSelectOnItemAddedToSlot set to true, but ItemSlotId is not set."), *QuickBar.BarId.ToString(), *Slot.QuickBarSlotId.ToString());
				Context.AddError(FText::FromString(Msg));
				ErrorCount++;
			}
			if (Slot.InputBind.IsValid() == false)
			{
				FString Msg = FString::Printf(TEXT("QuickBar %s Slot %s has no InputBind set."), *QuickBar.BarId.ToString(), *Slot.QuickBarSlotId.ToString());
				Context.AddError(FText::FromString(Msg));
				ErrorCount++;
			}

			if (Slot.bAutoSelect == true && Slot.bCanByCycled == false)
			{
				FString Msg = FString::Printf(TEXT("QuickBar %s Slot %s has bCanByCycled set to false, but bAutoSelect set to true."), *QuickBar.BarId.ToString(), *Slot.QuickBarSlotId.ToString());
				Context.AddError(FText::FromString(Msg));
				ErrorCount++;
			}

			if (QuickBar.QuickSlotsMode == EArcQuickSlotsMode::ActivateOnly)
			{
				if (Slot.bAutoSelect || Slot.bCanByCycled)
				{
					FString Msg = FString::Printf(TEXT("QuickBar %s Slot %s has bAutoSelect or bCanByCycled set to true, but QuickSlotsMode is set to ActivateOnly."), *QuickBar.BarId.ToString(), *Slot.QuickBarSlotId.ToString());
					Context.AddError(FText::FromString(Msg));
					ErrorCount++;
				}
			}

			if (QuickBar.QuickSlotsMode == EArcQuickSlotsMode::Cyclable)
			{
				if (Slot.bCanByCycled == false)
				{
					FString Msg = FString::Printf(TEXT("QuickBar %s Slot %s has bCanByCycled set to false, but QuickSlotsMode is set to Cyclable."), *QuickBar.BarId.ToString(), *Slot.QuickBarSlotId.ToString());
					Context.AddError(FText::FromString(Msg));
					ErrorCount++;
				}
			}
		}
	}

	if (ErrorCount > 0)
	{
		return EDataValidationResult::Invalid;
	}

	return EDataValidationResult::Valid;
}
#endif

UArcQuickBarComponent* UArcQuickBarComponent::GetQuickBar(AActor* InOwner)
{
	return InOwner->FindComponentByClass<UArcQuickBarComponent>();
}

// Called when the game starts
void UArcQuickBarComponent::BeginPlay()
{
	Super::BeginPlay();


	UArcWorldDelegates::Get(this)->BroadcastActorOnComponentBeginPlay(
		FArcComponentChannelKey(GetOwner(), GetClass())
		, this);
	// ...
}

void UArcQuickBarComponent::OnRegister()
{
	ReplicatedSelectedSlots.QuickBar = this;
	Super::OnRegister();
	if (Arcx::Utils::IsPlayableWorld(GetWorld()))
	{
		UArcItemsSubsystem* ItemsSubsystem = UArcItemsSubsystem::Get(this);
		// Listen for slot items only on server ?
		if(GetOwnerRole() < ENetRole::ROLE_Authority)
		{
			ItemsSubsystem->AddActorOnAddedToSlot(GetOwner(), FArcGenericItemSlotDelegate::FDelegate::CreateUObject(
				this, &UArcQuickBarComponent::HandlePendingItemsWhenSlotChanges
			));
	
			UArcWorldDelegates::Get(GetWorld())->AddActorOnAbilityGiven(GetOwner()
				, FArcAbilitySystemAbilitySpecDelegate::FDelegate::CreateUObject(this, &UArcQuickBarComponent::HandleOnAbilityAdded));
			return;
		}
		
		if (ItemsSubsystem) 
		{
			for (FArcQuickBar& QB : QuickBars)
			{
				if (!QB.bCanAutoSelectOnItemAddedToSlot)
				{
					continue;
				}
				
				for (FArcQuickBarSlot& QBS : QB.Slots)
				{
					if(QBS.ItemSlotId.IsValid() == true)
					{
						ItemsSubsystem->AddActorOnAddedToSlotMap(GetOwner(), QBS.ItemSlotId
								, FArcGenericItemStoreDelegate::FDelegate::CreateUObject(this
									, &UArcQuickBarComponent::HandleOnItemAddedToSlot
									, QB.BarId
									, QBS.QuickBarSlotId));
						
						ItemsSubsystem->AddActorOnRemovedFromSlotMap(GetOwner(), QBS.ItemSlotId,
								FArcGenericItemStoreDelegate::FDelegate::CreateUObject(this,
									&UArcQuickBarComponent::HandleOnItemRemovedFromSlot,
									QB.BarId,
									QBS.QuickBarSlotId));
					}
				}
			}
		}
		
		ItemsSubsystem->AddActorOnAddedToSlot(GetOwner(), FArcGenericItemSlotDelegate::FDelegate::CreateUObject(
			this, &UArcQuickBarComponent::HandlePendingItemsWhenSlotChanges
			));
		
		UArcWorldDelegates::Get(GetWorld())->AddActorOnAbilityGiven(GetOwner()
			,FArcAbilitySystemAbilitySpecDelegate::FDelegate::CreateUObject(this, &UArcQuickBarComponent::HandleOnAbilityAdded));
	}
}

void UArcQuickBarComponent::InitializeComponent()
{
	Super::InitializeComponent();

	UArcPlayerStateExtensionComponent* PSExt = UArcPlayerStateExtensionComponent::Get(GetOwner());

	if (PSExt != nullptr)
	{
		PSExt->AddOnPawnReady(FArcPlayerPawnReady::FDelegate::CreateUObject(
			this, &UArcQuickBarComponent::HandleOnPlayerPawnReady
		));
	}
}

void UArcQuickBarComponent::HandleOnPlayerPawnReady(APawn* InPawn)
{
	if (GetNetMode() == ENetMode::NM_Client)
	{
		ReplicatedSelectedSlots.HandleOnItemAddedToSlot();
	}
}

FGameplayTag UArcQuickBarComponent::CycleSlotForward(const FGameplayTag& BarId
												  , const FGameplayTag& CurrentSlotId
												  , TFunction<bool(const FArcItemData*)> SlotValidCondition)
{
	int32 BarIdx = INDEX_NONE;
	if (InternalCanCycle(BarId, BarIdx) == false)
	{
		return FGameplayTag::EmptyTag;
	}
	
	int32 StartSlotIdx = QuickBars[BarIdx].Slots.IndexOfByKey(CurrentSlotId);

	int32 OldSlotIdx = StartSlotIdx;

	const int32 SlotNum = QuickBars[BarIdx].Slots.Num();

	UArcCoreAbilitySystemComponent* ArcASC = GetOwner()->FindComponentByClass<UArcCoreAbilitySystemComponent>();

	bool bFoundNewSlot = false;
	int32 NewSlotIdx = StartSlotIdx;
	
	UE_LOG(LogArcQuickBar
		, Log
		, TEXT("CycleSlotForward Start [SlotIdx %d] [NewSlotIdx %d]")
		, StartSlotIdx
		, NewSlotIdx);

	bFoundNewSlot = InternalCycleSlot(StartSlotIdx
									  , BarIdx
									  , 1
									  , SlotValidCondition
									  , NewSlotIdx);
	
	if (bFoundNewSlot)
	{
		return InternalSelectCycledSlot(BarId
			, BarIdx
			, OldSlotIdx
			, ArcASC
			, NewSlotIdx);
	}
	return FGameplayTag::EmptyTag;
}

FGameplayTag UArcQuickBarComponent::CycleSlotBackward(const FGameplayTag& BarId
													  , const FGameplayTag& CurrentSlotId
													  , TFunction<bool(const FArcItemData*)> SlotValidCondition)
{
	int32 BarIdx = INDEX_NONE;
	if (InternalCanCycle(BarId, BarIdx) == false)
	{
		return FGameplayTag::EmptyTag;
	}
	
	int32 StartSlotIdx = QuickBars[BarIdx].Slots.IndexOfByKey(CurrentSlotId);

	int32 OldSlotIdx = StartSlotIdx;

	const int32 SlotNum = QuickBars[BarIdx].Slots.Num();

	UArcCoreAbilitySystemComponent* ArcASC = GetOwner()->FindComponentByClass<UArcCoreAbilitySystemComponent>();

	bool bFoundNewSlot = false;
	int32 NewSlotIdx = StartSlotIdx;
	
	UE_LOG(LogArcQuickBar
		, Log
		, TEXT("CycleSlotForward Start [SlotIdx %d] [NewSlotIdx %d]")
		, StartSlotIdx
		, NewSlotIdx);

	bFoundNewSlot = InternalCycleSlot(StartSlotIdx
									  , BarIdx
									  , -1
									  , SlotValidCondition
									  , NewSlotIdx);
	
	if (bFoundNewSlot)
	{
		return InternalSelectCycledSlot(BarId
			, BarIdx
			, OldSlotIdx
			, ArcASC
			, NewSlotIdx);
	}
	return FGameplayTag::EmptyTag;
}

bool UArcQuickBarComponent::InternalCanCycle(const FGameplayTag& BarId
											 , int32& BarIdx)
{
	if (GetNetMode() == ENetMode::NM_DedicatedServer)
	{
		return false;
	}
	
	if (GetOwnerRole() < ENetRole::ROLE_Authority)
	{
		if (LockSlotCycle > 0)
		{
			return false;
		}
	}
	
	const APlayerController* PC = Cast<APlayerController>(GetOwner());
	if (PC == nullptr)
	{
		if (const APlayerState* PS = Cast<APlayerState>(GetOwner()))
		{
			PC = PS->GetPlayerController();
		}
	}

	if (PC == nullptr)
	{
		return false;
	}

	/**
	 * We don't want to accidently Cycle slots on servers.
	 */
	if (PC->IsLocalController() == false)
	{
		return false;
	}

	BarIdx = QuickBars.IndexOfByKey(BarId);
	if (BarIdx == INDEX_NONE)
	{
		return false;
	}
	return true;
}


FGameplayTag UArcQuickBarComponent::InternalSelectCycledSlot(const FGameplayTag& BarId
	, const int32 BarIdx
	, const int32 OldSlotIdx
	, UArcCoreAbilitySystemComponent* ArcASC
	, const int32 NewSlotIdx)
{
	if (GetOwnerRole() < ENetRole::ROLE_Authority)
	{
		LockSlotCycle = 1;	
	}
	
	UArcItemsStoreComponent* SlotComp = Arcx::Utils::GetComponent(GetOwner(), GetItemsStoreClass(BarIdx));
	
	/**
	 * Data we will send to server, with changed being made on client, to redo them.
	 */
	FArcCycledSlotRPCData RPCData;
	FGameplayTag NewQuickSlotId;
	FGameplayTag OldSlotTag;
	if (OldSlotIdx != INDEX_NONE)
	{
		OldSlotTag = QuickBars[BarIdx].Slots[OldSlotIdx].QuickBarSlotId;
	}
	if (NewSlotIdx != INDEX_NONE)
	{
		NewQuickSlotId = QuickBars[BarIdx].Slots[NewSlotIdx].QuickBarSlotId;
	}

	const FArcQuickBar* QuickBar = &QuickBars[BarIdx];
	/**
	 * If OldSlotIdx is valid it means there was something on old slot, so we let it clean up after itself.
	 */
	if (OldSlotIdx != INDEX_NONE)
	{	
		const FArcItemData* OldData = FindSlotData(BarId, OldSlotTag);
		const FArcQuickBarSlot* QuickSlot = &QuickBars[BarIdx].Slots[OldSlotIdx];
		
		if (OldData)
		{
			for (const FInstancedStruct& IS : QuickBars[BarIdx].Slots[OldSlotIdx].SelectedHandlers)
			{
				IS.GetPtr<FArcQuickSlotHandler>()->OnSlotDeselected(ArcASC
					, this
					, OldData
					, QuickBar
					, QuickSlot);
			}
			
			RPCData.DeselectBarIdx = BarIdx;
			RPCData.DeselectSlotIdx = OldSlotIdx;

		}

		if (OldSlotTag.IsValid())
		{
			if (InternalTryUnbindInput(BarIdx, OldSlotIdx, ArcASC))
			{
				RPCData.bUnbindInput = true;
			}
			
			ReplicatedSelectedSlots.SetQuickSlotDeselected(BarId, OldSlotTag);
			ReplicatedSelectedSlots.SetQuickSlotClientDeselected(QuickBars[BarIdx].BarId, NewQuickSlotId);
		}
	}

	/**
	 * We found new valid slot, let it call all handlers and bind inputs.
	 */
	if (NewSlotIdx != INDEX_NONE)
	{
		NewQuickSlotId = QuickBars[BarIdx].Slots[NewSlotIdx].QuickBarSlotId;

		const FArcItemData* NewData = FindSlotData(BarId, NewQuickSlotId);
		const FArcQuickBarSlot* QuickSlot = &QuickBars[BarIdx].Slots[NewSlotIdx];

		ReplicatedSelectedSlots.AddQuickSlot(QuickBars[BarIdx].BarId, NewQuickSlotId);
		ReplicatedSelectedSlots.SetQuickSlotSelected(QuickBars[BarIdx].BarId, NewQuickSlotId);
		ReplicatedSelectedSlots.SetQuickSlotClientSelected(QuickBars[BarIdx].BarId, NewQuickSlotId);
		
		ReplicatedSelectedSlots.SetItemSlot(QuickBars[BarIdx].BarId, NewQuickSlotId, NewData->GetSlotId());
		ReplicatedSelectedSlots.SetAssignedItemId(QuickBars[BarIdx].BarId, NewQuickSlotId, NewData->GetItemId());
		
		RPCData.SelectedItemSlot = NewData->GetSlotId();
		RPCData.SelectedItemId = NewData->GetItemId();
		
		// Lazy add to item slot mapping, as some systems depends on this id being set..
		//ItemSlotMapping.FindOrAdd(QuickBars[BarIdx].BarId).Slots
		//	.FindOrAdd(QuickBars[BarIdx].Slots[NewSlotIdx].QuickBarSlotId) = NewData->GetItemId();
		
		for (const FInstancedStruct& IS : QuickBars[BarIdx].Slots[NewSlotIdx].SelectedHandlers)
		{
			IS.GetPtr<FArcQuickSlotHandler>()->OnSlotSelected(ArcASC
				, this
				, NewData
				, QuickBar
				, QuickSlot);
		}

		RPCData.OldBarIdx = BarIdx;
		RPCData.OldSlotIdx = OldSlotIdx;
		RPCData.NewBarIdx = BarIdx;
		RPCData.NewSlotIdx = NewSlotIdx;

		if (InternalTryBindInput(BarIdx, NewSlotIdx, ArcASC))
		{
			RPCData.bBindInput = true;
		}

		UArcQuickBarSubsystem* QuickBarSubsystem = UArcQuickBarSubsystem::Get(this);
		QuickBarSubsystem->BroadcastActorOnQuickBarSlotChanged(this->GetOwner()
					, this
					, QuickBars[BarIdx].BarId
					, NewQuickSlotId
					, BarId
					, OldSlotTag);
		
		QuickBarSubsystem->OnQuickBarSlotChanged.Broadcast(this
				, QuickBars[BarIdx].BarId
				, NewQuickSlotId
				, NewData->GetItemId());
	}


	
	if (GetOwnerRole() < ENetRole::ROLE_Authority)
	{
		ServerHandleCycledSlot(RPCData);
	}
	
	return NewQuickSlotId;
}

bool UArcQuickBarComponent::InternalCycleSlot(const int32 StartSlotIdx
											  , const int32 BarIdx
											  , const int32 Direction
											  , TFunction<bool(const FArcItemData*)> SlotValidCondition
											  , int32& OutNewSlotIdx)
{
	bool bFoundNewSlot = false;
	int32 NewSlotIdx = StartSlotIdx;
	const int32 SlotNum = QuickBars[BarIdx].Slots.Num();
	OutNewSlotIdx = NewSlotIdx;
	
	do
	{
		int32 Test = (NewSlotIdx + Direction);
		NewSlotIdx = Test % SlotNum;

		/**
		 * We can set slot on bar to be explictly ignored during cycling.
		 */
		if (QuickBars[BarIdx].Slots[NewSlotIdx].bCanByCycled == false)
		{
			continue;
		}

		/**
		 * Run Validators to check if we can even cycle to to this slot.
		 */
		for (const FInstancedStruct& Validator : QuickBars[BarIdx].Slots[NewSlotIdx].Validators)
		{
			if (const FArcQuickBarSlotValidator* Val = Validator.GetPtr<FArcQuickBarSlotValidator>())
			{
				if (Val->IsValid(this
						, QuickBars[BarIdx]
						, QuickBars[BarIdx].Slots[NewSlotIdx]) == false)
				{
					continue;
				}
			}
		}

		const FArcItemData* Data = FindSlotData(QuickBars[BarIdx].BarId
												, QuickBars[BarIdx].Slots[NewSlotIdx].QuickBarSlotId);
		
		UE_LOG(LogArcQuickBar
			, Log
			, TEXT("CycleSlotForward Iteration [SlotIdx %d] [NewSlotIdx %d]")
			, StartSlotIdx
			, NewSlotIdx);
		
		// do we want to equip, equipped weapon ?
		if (NewSlotIdx == StartSlotIdx)
		{
			UE_LOG(LogArcQuickBar
				, Log
				, TEXT("CycleSlotForward break 1 [SlotIdx %d] [NewSlotIdx %d]")
				, StartSlotIdx
				, NewSlotIdx);
			
			break;
		}
		
		if (Data)
		{
			// TODO Maybe also add extension delegates _RetVal ?
			if (SlotValidCondition(Data))
			{
				UE_LOG(LogArcQuickBar
					, Log
					, TEXT("CycleSlotForward break Found [SlotIdx %d] [NewSlotIdx %d]")
					, StartSlotIdx
					, NewSlotIdx);
				
				bFoundNewSlot = true;
				OutNewSlotIdx = NewSlotIdx;
				break;
			}
		}

		/**
		 * If we somehow got to this point, we cycled over everything and found not valid data for other slots.
		 */
		if (StartSlotIdx < 0 && NewSlotIdx >= (SlotNum - 1))
		{
			UE_LOG(LogArcQuickBar
				, Log
				, TEXT("CycleSlotForward break Not Found [SlotIdx %d] [NewSlotIdx %d]")
				, StartSlotIdx
				, NewSlotIdx);
			// this means we did not have any weapon equipped and did not find any to
			// equip.
			break;
		}
	}
	while (NewSlotIdx != StartSlotIdx);

	return bFoundNewSlot;
}

void UArcQuickBarComponent::SetBarSlotSelected(const FGameplayTag& InBarId
											   , const FGameplayTag& InQuickSlotId
											   , const bool bReplicate)
{
	UArcCoreAbilitySystemComponent* ArcASC = GetOwner()->FindComponentByClass<UArcCoreAbilitySystemComponent>();
	UArcItemsStoreComponent* SlotComp = Arcx::Utils::GetComponent(GetOwner(), GetItemsStoreClass(InBarId));

	TArray<FGameplayTag> ActiveTag = ReplicatedSelectedSlots.GetAllActiveSlots(InBarId);

	if (ActiveTag.Contains(InQuickSlotId))
	{
		return;
	}
	
	int32 BarIdx = QuickBars.IndexOfByKey(InBarId);
	int32 NewSlotIdx = QuickBars[BarIdx].Slots.IndexOfByKey(InQuickSlotId);

	if (NewSlotIdx != INDEX_NONE)
	{
		const FArcItemData* SlotData = FindSlotData(InBarId, InQuickSlotId);
		
		if (SlotData == nullptr)
		{
			UE_LOG(LogArcQuickBar, Log, TEXT("UArcQuickBarComponent::SetBarSlotSelected SlotData is invalid [Bar %s] [QuickSlot %s]")
				, *InBarId.ToString()
				, *InQuickSlotId.ToString())

			return;
		}

		const FArcQuickBar* QuickBar = &QuickBars[BarIdx];
		const FArcQuickBarSlot* QuickSlot = &QuickBars[BarIdx].Slots[NewSlotIdx];
		
		ReplicatedSelectedSlots.AddQuickSlot(InBarId, InQuickSlotId);
		ReplicatedSelectedSlots.SetQuickSlotSelected(InBarId, InQuickSlotId);
		ReplicatedSelectedSlots.SetQuickSlotClientSelected(InBarId, InQuickSlotId);
		
		for (const FInstancedStruct& IS : QuickBars[BarIdx].Slots[NewSlotIdx].SelectedHandlers)
		{
			IS.GetPtr<FArcQuickSlotHandler>()->OnSlotSelected(ArcASC
				, this
				, SlotData
				, QuickBar
				, QuickSlot);
		}
		
		InternalTryBindInput(BarIdx, NewSlotIdx, ArcASC);
		
		if (bReplicate && GetOwnerRole() < ENetRole::ROLE_Authority)
		{
			ServerSetBarSlotSelected(BarIdx, NewSlotIdx);
		}
	}

	UArcQuickBarSubsystem* QuickBarSubsystem = UArcQuickBarSubsystem::Get(this);
	QuickBarSubsystem->BroadcastActorOnQuickBarSlotChanged(this->GetOwner()
				, this
				, InBarId
				, InQuickSlotId
				, FGameplayTag::EmptyTag
				, FGameplayTag::EmptyTag);
}

void UArcQuickBarComponent::ServerSetBarSlotSelected_Implementation(int8 BarIdx
	, int8 NewSlotIdx)
{
	if (QuickBars.IsValidIndex(BarIdx) == false)
	{
		return;
	}

	if (QuickBars[BarIdx].Slots.IsValidIndex(NewSlotIdx) == false)
	{
		return;
	}
	
	SetBarSlotSelected(QuickBars[BarIdx].BarId, QuickBars[BarIdx].Slots[NewSlotIdx].QuickBarSlotId, false);
}

void UArcQuickBarComponent::SetBarSlotDeselected(const FGameplayTag& InBarId
												 , const FGameplayTag& InQuickSlotId
												 , const bool bReplicateToServer)
{
	InternalSetBarSlotDeselected(InBarId, InQuickSlotId, bReplicateToServer);
}

void UArcQuickBarComponent::ServerSetBarSlotDeselected_Implementation(int8 BarIdx
	, int8 NewSlotIdx)
{
	if (QuickBars.IsValidIndex(BarIdx) == false)
	{
		return;
	}

	if (QuickBars[BarIdx].Slots.IsValidIndex(NewSlotIdx) == false)
	{
		return;
	}
	
	SetBarSlotDeselected(QuickBars[BarIdx].BarId, QuickBars[BarIdx].Slots[NewSlotIdx].QuickBarSlotId, false);
}

void UArcQuickBarComponent::InternalSetBarSlotDeselected(const FGameplayTag& InBarId
														 , const FGameplayTag& InQuickSlotId
														 , const bool bReplicateToServer)
{
	UArcCoreAbilitySystemComponent* ArcASC = GetOwner()->FindComponentByClass<UArcCoreAbilitySystemComponent>();
	UArcItemsStoreComponent* SlotComp = Arcx::Utils::GetComponent(GetOwner(), GetItemsStoreClass(InBarId));
	
	int32 BarIdx = QuickBars.IndexOfByKey(InBarId);
	int32 DeselectedSlotIdx = QuickBars[BarIdx].Slots.IndexOfByKey(InQuickSlotId);
		
	if (DeselectedSlotIdx != INDEX_NONE)
	{
		const FArcItemData* OldData = FindSlotData(InBarId, InQuickSlotId);
			
		if (OldData == nullptr)
		{
			return;
		}

		const FArcQuickBar* QuickBar = &QuickBars[BarIdx];
		const FArcQuickBarSlot* QuickSlot = &QuickBars[BarIdx].Slots[DeselectedSlotIdx];

		ReplicatedSelectedSlots.AddQuickSlot(InBarId, InQuickSlotId);
		ReplicatedSelectedSlots.SetQuickSlotDeselected(InBarId, InQuickSlotId);
		ReplicatedSelectedSlots.SetQuickSlotClientDeselected(InBarId, InQuickSlotId);
		
		for (const FInstancedStruct& IS : QuickBars[BarIdx].Slots[DeselectedSlotIdx].SelectedHandlers)
		{
			IS.GetPtr<FArcQuickSlotHandler>()->OnSlotDeselected(ArcASC
				, this
				, OldData
				, QuickBar
				, QuickSlot);
		}
		
		InternalTryUnbindInput(BarIdx, DeselectedSlotIdx, ArcASC);
		
		if (bReplicateToServer == true && GetOwnerRole() < ENetRole::ROLE_Authority)
		{
			ServerSetBarSlotDeselected(BarIdx, DeselectedSlotIdx);
		}

		UArcQuickBarSubsystem* QuickBarSubsystem = UArcQuickBarSubsystem::Get(this);
		QuickBarSubsystem->BroadcastActorOnQuickBarSlotChanged(this->GetOwner()
					, this
					, FGameplayTag::EmptyTag
					, FGameplayTag::EmptyTag
					, InBarId
					, InQuickSlotId);
	}
}

bool UArcQuickBarComponent::InternalTryBindInput(const int32 BarIdx
	, const int32 QuickSlotIdx
	, UArcCoreAbilitySystemComponent* InArcASC)
{
	const FArcQuickBarInputBindHandling* Handling = QuickBars[BarIdx].Slots[QuickSlotIdx].InputBind.GetPtr<FArcQuickBarInputBindHandling>();
	if (Handling != nullptr)
	{
		UArcItemsStoreComponent* SlotComp = Arcx::Utils::GetComponent(GetOwner(), GetItemsStoreClass(BarIdx));
		const FGameplayTag& BarId = QuickBars[BarIdx].BarId;
		const FGameplayTag& QuickSlotId = QuickBars[BarIdx].Slots[QuickSlotIdx].QuickBarSlotId;
		
		const FArcItemData* Data = FindSlotData(BarId, QuickSlotId);

		TArray<const FArcItemData*> Extensions = SlotComp->GetItemsAttachedTo(Data->GetItemId());
		Extensions.Add(Data);
		return Handling->OnAddedToQuickBar(InArcASC, Extensions);
	}

	return true;
}

bool UArcQuickBarComponent::InternalTryUnbindInput(const int32 BarIdx
	, const int32 QuickSlotIdx
	, UArcCoreAbilitySystemComponent* InArcASC)
{
	const FArcQuickBarInputBindHandling* Handling = QuickBars[BarIdx].Slots[QuickSlotIdx].InputBind.GetPtr<FArcQuickBarInputBindHandling>();
	if (Handling != nullptr)
	{
		UArcItemsStoreComponent* SlotComp = Arcx::Utils::GetComponent(GetOwner(), GetItemsStoreClass(BarIdx));
		const FGameplayTag& BarId = QuickBars[BarIdx].BarId;
		const FGameplayTag& QuickSlotId = QuickBars[BarIdx].Slots[QuickSlotIdx].QuickBarSlotId;
		
		const FArcItemData* Data = FindSlotData(BarId, QuickSlotId);
		
		TArray<const FArcItemData*> Extensions = SlotComp->GetItemsAttachedTo(Data->GetOwnerId());
		Extensions.Add(Data);
		return Handling->OnRemovedFromQuickBar(InArcASC, Extensions);
	}

	return false;
}

void UArcQuickBarComponent::SelectBar(const FGameplayTag& InBarId)
{
	if (GetNetMode() == ENetMode::NM_DedicatedServer)
	{
		return;
	}

	// We only care about it on client.
	if (GetOwnerRole() < ENetRole::ROLE_Authority)
	{
		if (LockQuickBar > 0)
		{
			return;
		}
		LockQuickBar = 1;
	}
	
	for (int8 Idx = 0; Idx < QuickBars.Num(); Idx++)
	{
		if (QuickBars[Idx].BarId == InBarId)
		{
			/**
			 * Checks if all slots on this bar can access valid SlotData.
			 * If not, do not allow activating entire bar.
			 *
			 * TODO: Might want later to emmit some events to inform user that something is wrong.
			 */
			if(AreAllBarSlotsValid(Idx) == false)
			{
				UE_LOG(LogArcQuickBar, Log, TEXT("UArcQuickBarComponent::SelectBar One of the slots does not have valid FArcItemData"))
				return;
			}
			
			ServerSelectBar(Idx);
			
			for (FInstancedStruct& IS : QuickBars[Idx].QuickBarSelectedActions)
			{
				if (FArcQuickBarSelectedAction* QBA = IS.GetMutablePtr<FArcQuickBarSelectedAction>())
				{
					QBA->QuickBarActivated(this
						, &QuickBars[Idx]);
				}
			}
			
			ActivateSlotsOnBarInternal(InBarId);
			break;
		}
	}
}

bool UArcQuickBarComponent::AreAllBarSlotsValid(const int32 BarIdx) const
{
	for (int8 Idx = 0; Idx < QuickBars[BarIdx].Slots.Num(); Idx++)
	{
		const FArcItemData* SlotData = FindSlotData(QuickBars[BarIdx].BarId, QuickBars[BarIdx].Slots[Idx].QuickBarSlotId);
		if(SlotData == nullptr)
		{
			return false;
		}
	}

	return true;
}

void UArcQuickBarComponent::ActivateSlotsOnBarInternal(const FGameplayTag& InBarId)
{
	UArcCoreAbilitySystemComponent* ArcASC = GetOwner()->FindComponentByClass<UArcCoreAbilitySystemComponent>();
	UArcItemsStoreComponent* SlotComp = Arcx::Utils::GetComponent(GetOwner(), GetItemsStoreClass(InBarId));
	UArcQuickBarSubsystem* QuickBarSubsystem = UArcQuickBarSubsystem::Get(this);
	
	int32 QuickBarIdx = QuickBars.IndexOfByKey(InBarId);
	
	for (int32 QuickSlotIdx = 0; QuickSlotIdx < QuickBars[QuickBarIdx].Slots.Num(); QuickSlotIdx++)
	{
		FArcQuickBarSlot& QBS = QuickBars[QuickBarIdx].Slots[QuickSlotIdx];
		
		ReplicatedSelectedSlots.AddQuickSlot(InBarId, QBS.QuickBarSlotId)
			.SetQuickSlotSelected(InBarId, QBS.QuickBarSlotId)
			.SetQuickSlotClientSelected(InBarId, QBS.QuickBarSlotId);
		
		const FArcItemData* NewData = FindSlotData(InBarId, QBS.QuickBarSlotId);

		const FArcQuickBar* QuickBar = &QuickBars[QuickBarIdx];
		const FArcQuickBarSlot* QuickSlot = &QuickBars[QuickBarIdx].Slots[QuickSlotIdx];
		// Consider slot selectable only if there is valid data for it.
		if (NewData)
		{
			for (const FInstancedStruct& IS : QBS.SelectedHandlers)
			{
				IS.GetPtr<FArcQuickSlotHandler>()->OnSlotSelected(ArcASC
					, this
					, NewData
					, QuickBar
					, QuickSlot);
			}

			InternalTryBindInput(QuickBarIdx, QuickSlotIdx, ArcASC);

			
			QuickBarSubsystem->BroadcastActorOnQuickBarSlotChanged(this->GetOwner()
						, this
						, InBarId
						, QBS.QuickBarSlotId
						, FGameplayTag::EmptyTag
						, FGameplayTag::EmptyTag);
		}
	}
}

void UArcQuickBarComponent::DeselectBar(const FGameplayTag& InBarId)
{
	if (GetNetMode() == ENetMode::NM_DedicatedServer)
	{
		return;
	}

	// We only care about it on client.
	if (GetOwnerRole() < ENetRole::ROLE_Authority)
	{
		if (LockQuickBar > 0)
		{
			return;
		}
		LockQuickBar = 1;
	}
	
	for (int8 Idx = 0; Idx < QuickBars.Num(); Idx++)
	{
		if (QuickBars[Idx].BarId == InBarId)
		{
			ServerDeselectBar(Idx);
			for (FInstancedStruct& IS : QuickBars[Idx].QuickBarSelectedActions)
			{
				if (FArcQuickBarSelectedAction* QBA = IS.GetMutablePtr<FArcQuickBarSelectedAction>())
				{
					QBA->QuickBarActivated(this
						, &QuickBars[Idx]);
				}
			}
			
			DeactivateSlotsOnBarInternal(InBarId);
			break;
		}
	}
}

void UArcQuickBarComponent::DeactivateSlotsOnBarInternal(const FGameplayTag& InBarId)
{
	UArcCoreAbilitySystemComponent* ArcASC = GetOwner()->FindComponentByClass<UArcCoreAbilitySystemComponent>();
	UArcItemsStoreComponent* SlotComp = Arcx::Utils::GetComponent(GetOwner(), GetItemsStoreClass(InBarId));
	UArcQuickBarSubsystem* QuickBarSubsystem = UArcQuickBarSubsystem::Get(this);
	
	int32 QuickBarIdx = QuickBars.IndexOfByKey(InBarId);
	
	for (int32 QuickSlotIdx = 0; QuickSlotIdx < QuickBars[QuickBarIdx].Slots.Num(); QuickSlotIdx++)
	{
		FArcQuickBarSlot& QBS = QuickBars[QuickBarIdx].Slots[QuickSlotIdx];
		const FArcItemData* OldData = FindSlotData(InBarId, QBS.QuickBarSlotId);

		// Consider slot selectable only if there is valid data for it.
		if (OldData)
		{
			const FArcQuickBar* QuickBar = &QuickBars[QuickBarIdx];
			const FArcQuickBarSlot* QuickSlot = &QuickBars[QuickBarIdx].Slots[QuickSlotIdx];
			
			for (const FInstancedStruct& IS : QBS.SelectedHandlers)
			{
				IS.GetPtr<FArcQuickSlotHandler>()->OnSlotDeselected(ArcASC
					, this
					, OldData
					, QuickBar
					, QuickSlot);
			}

			InternalTryUnbindInput(QuickBarIdx, QuickSlotIdx, ArcASC);

			QuickBarSubsystem->BroadcastActorOnQuickBarSlotChanged(this->GetOwner()
				, this
				, FGameplayTag::EmptyTag
				, FGameplayTag::EmptyTag
				, InBarId
				, QBS.QuickBarSlotId);
		}
	}
}

FGameplayTag UArcQuickBarComponent::CycleBarForward(const FGameplayTag& InOldBarId)
{
	int32 StartBarIdx = QuickBars.IndexOfByKey(InOldBarId);
	int32 NewBarIdx = StartBarIdx;
	
	const int32 BarsNum = QuickBars.Num();

	UArcCoreAbilitySystemComponent* ArcASC = GetOwner()->FindComponentByClass<UArcCoreAbilitySystemComponent>();

	bool bFoundNewSlot = false;

	do
	{
		int32 Test = (NewBarIdx + 1);
		NewBarIdx = Test % BarsNum;

		/**
		 * We can set slot on bar to be explictly ignored during cycling.
		 */
		if (QuickBars[NewBarIdx].QuickSlotsMode == EArcQuickSlotsMode::Cyclable)
		{
			continue;
		}

		if (AreAllBarSlotsValid(NewBarIdx) == false)
		{
			continue;
		}
		
		
		if (NewBarIdx == StartBarIdx)
		{
			break;
		}
		
		bFoundNewSlot = true;

		/**
		 * If we somehow got to this point, we cycled over everything and found not valid data for other slots.
		 */
		if (StartBarIdx < 0 && NewBarIdx >= (BarsNum - 1))
		{
			// this means we did not have any weapon equipped and did not find any to
			// equip.
			break;
		}
	}
	while (NewBarIdx != StartBarIdx);

	if(bFoundNewSlot == true)
	{
		return QuickBars[NewBarIdx].BarId;
	}
	
	return FGameplayTag::EmptyTag;
}

FGameplayTag UArcQuickBarComponent::GetFirstActiveSlot(const FGameplayTag& InBarId) const
{
	TArray<FGameplayTag> ActiveSlots = GetActiveSlots(InBarId);
	if (ActiveSlots.Num() > 0)
	{
		return ActiveSlots[0];
	}
	return FGameplayTag::EmptyTag;
}

void UArcQuickBarComponent::AddItemToBarOrRegisterDelegate(const FGameplayTag& InBarId
	, const FGameplayTag& InQuickSlotId
	, const FArcItemId& ItemId)
{
	// We only care about it on client.
	if (GetOwnerRole() < ENetRole::ROLE_Authority)
	{
		if (LockQuickBar > 0)
		{
			return;
		}
		LockQuickBar = 1;
	}
	
	UArcItemsStoreComponent* ItemSlotComponent = GetItemStoreComponent(InBarId);

	ReplicatedSelectedSlots.AddQuickSlot(InBarId, InQuickSlotId);
	ReplicatedSelectedSlots.SetAssignedItemId(InBarId, InQuickSlotId, ItemId);
	
	const FArcItemData* SlotData = ItemSlotComponent->GetItemPtr(ItemId);

	UArcItemsSubsystem* ItemsSubsystem = UArcItemsSubsystem::Get(this);
	
	if (SlotData == nullptr)
	{
		return;
	}

	ReplicatedSelectedSlots.SetItemSlot(InBarId, InQuickSlotId, SlotData->GetSlotId());
	FArcSelectedQuickBarSlot* QuickBarSlot = ReplicatedSelectedSlots.Find(InBarId, InQuickSlotId);
	QuickBarSlot->bClientSelected = true;
	
	int32 BarIdx = QuickBars.IndexOfByKey(InBarId);
	int32 NewSlotIdx = QuickBars[BarIdx].Slots.IndexOfByKey(InQuickSlotId);

	if (NewSlotIdx != INDEX_NONE)
	{
		const FArcQuickBar* QuickBar = &QuickBars[BarIdx];
		const FArcQuickBarSlot* QuickSlot = &QuickBars[BarIdx].Slots[NewSlotIdx];

		bool bIsSelectable = false;
		if (QuickBar->QuickSlotsMode == EArcQuickSlotsMode::Cyclable)
		{
			if (QuickBar->bCanAutoSelectOnItemAddedToSlot)
			{
				if (ReplicatedSelectedSlots.FindFirstSelectedSlot(InQuickSlotId).IsValid() == false)
				{
					bIsSelectable = true;
				};
			}
		}
		else if (QuickBar->QuickSlotsMode == EArcQuickSlotsMode::ActivateOnly)
		{
			bIsSelectable = true;
		}
		if (bIsSelectable)
		{
			ReplicatedSelectedSlots.SetQuickSlotSelected(InBarId, InQuickSlotId);
		}
		
		UArcCoreAbilitySystemComponent* ArcASC = GetOwner()->FindComponentByClass<UArcCoreAbilitySystemComponent>();
		
		for (const FInstancedStruct& IS : QuickBars[BarIdx].Slots[NewSlotIdx].SelectedHandlers)
		{
			IS.GetPtr<FArcQuickSlotHandler>()->OnSlotSelected(ArcASC
				, this
				, SlotData
				, QuickBar
				, QuickSlot);
		}

		InternalTryBindInput(BarIdx, NewSlotIdx, ArcASC);
	}

	UArcQuickBarSubsystem* QuickBarSubsystem = UArcQuickBarSubsystem::Get(this);
	
	QuickBarSubsystem->BroadcastActorOnItemAddedToQuickSlot(this->GetOwner()
				, this
				, InBarId
				, InQuickSlotId
				, ItemId);

	QuickBarSubsystem->OnItemAddedToQuickSlot.Broadcast(this, InBarId, InQuickSlotId, ItemId);
	
	QuickBarSubsystem->BroadcastActorOnQuickBarSlotChanged(this->GetOwner()
				, this
				, InBarId
				, InQuickSlotId
				, FGameplayTag::EmptyTag
				, FGameplayTag::EmptyTag);
}

void UArcQuickBarComponent::RemoveItemFromBarByItemId(const FArcItemId& InItemId)
{
	TPair<FGameplayTag, FGameplayTag> QuickBarPair = ReplicatedSelectedSlots.FindItemBarAndSlot(InItemId);
	if (QuickBarPair.Key.IsValid() && QuickBarPair.Value.IsValid())
	{
		RemoveItemFromBar(QuickBarPair.Key, QuickBarPair.Value);
	}
}

void UArcQuickBarComponent::RemoveItemFromBar(const FGameplayTag& InBarId
											  , const FGameplayTag& InQuickSlotId)
{
	//if (GetNetMode() == ENetMode::NM_DedicatedServer)
	//{
	//	return;
	//}

	// We only care about it on client.
	if (GetOwnerRole() < ENetRole::ROLE_Authority)
	{
		if (LockQuickBar > 0)
		{
			return;
		}
		LockQuickBar = 1;
	}
	
	//if (BarSlotItem* BSI = ItemSlotMapping.Find(InBarId))
	{
		//if (FArcItemId* ItemId =  BSI->Slots.Find(InQuickSlotId))
		{
			UArcCoreAbilitySystemComponent* ArcASC = GetOwner()->FindComponentByClass<UArcCoreAbilitySystemComponent>();
			
			int32 BarIdx = QuickBars.IndexOfByKey(InBarId);
			int32 OldSlotIdx = QuickBars[BarIdx].Slots.IndexOfByKey(InQuickSlotId);
		
			if (OldSlotIdx != INDEX_NONE)
			{
				const FArcQuickBar* QuickBar = &QuickBars[BarIdx];
				const FArcQuickBarSlot* QuickSlot = &QuickBars[BarIdx].Slots[OldSlotIdx];

				UArcItemsStoreComponent* ItemsStoreComponent = GetItemStoreComponent(QuickBar->BarId);
				
				const FArcItemData* OldData = FindSlotData(InBarId, InQuickSlotId);
			
				if (OldData == nullptr)
				{
					return;
				}
				
				for (const FInstancedStruct& IS : QuickBars[BarIdx].Slots[OldSlotIdx].SelectedHandlers)
				{
					IS.GetPtr<FArcQuickSlotHandler>()->OnSlotDeselected(ArcASC
						, this
						, OldData
						, QuickBar
						, QuickSlot);
				}	
		
				InternalTryUnbindInput(BarIdx, OldSlotIdx, ArcASC);

				UArcQuickBarSubsystem* QuickBarSubsystem = UArcQuickBarSubsystem::Get(this);
	
				QuickBarSubsystem->BroadcastActorOnQuickBarSlotChanged(this->GetOwner()
							, this
							, FGameplayTag::EmptyTag
							, FGameplayTag::EmptyTag
							, InBarId
							, InQuickSlotId);
			}
		}
		
		ReplicatedSelectedSlots.RemoveAssignedItemId(InBarId, InQuickSlotId);
		ReplicatedSelectedSlots.RemoveItemSlot(InBarId, InQuickSlotId);
		MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, ReplicatedSelectedSlots, this);
		GetOwner()->ForceNetUpdate();

		UArcQuickBarSubsystem* QuickBarSubsystem = UArcQuickBarSubsystem::Get(this);
		QuickBarSubsystem->BroadcastActorOnItemRemovedFromQuickSlot(GetOwner(), this, InBarId, InQuickSlotId, FArcItemId::InvalidId);
		QuickBarSubsystem->OnItemRemovedFromQuickSlot.Broadcast(this, InBarId, InQuickSlotId, FArcItemId::InvalidId);
		//ReplicatedSelectedSlots.RemoveQuickSlot(InBarId, InQuickSlotId);
		//BSI->Slots.Remove(InQuickSlotId);
	}
}

void UArcQuickBarComponent::HandleItemAddedFromReplication(UArcItemsStoreComponent* InItemsStore, const FArcItemData* InItem)
{
	if (InItemsStore->GetOwner() != GetOwner())
	{
		return;
	}
	
	/**
	 * If we are on dedicated server we don't want to call it.
	 * The RPCs will take care of replicating relevelant pieces back to server,
	 * Since QuickBar is mostly Client Authorative we don't want servers/authority to guess what is happening.
	 *
	 * Either way you should make sure it simply won't ever be called in wrong context.
	 */
	ENetMode NM = GetNetMode();
	if (NM == ENetMode::NM_DedicatedServer)
	{
		return;
	}

	/**
	 * Do we need this heavy iteration to just find item, which we already got anyway ?
	 * We don't want to have more indirection than needed, we need to iterate
	 * to find that item is actually added to quick bar, and which Bar and QuickSlot exactly.
	 */
	//for (const TPair<FGameplayTag, BarSlotItem>& ASB : ItemSlotMapping)
	//{
	//	for (const TPair<FGameplayTag, FArcItemId>& S: ASB.Value.Slots)
	//	{
	//		if(S.Value == InItem->GetItemId())
	//		{
	//			UArcCoreAbilitySystemComponent* ArcASC = GetOwner()->FindComponentByClass<UArcCoreAbilitySystemComponent>();
	//			UArcItemsStoreComponent* SlotComp = Arcx::Utils::GetComponent(GetOwner()
	//				, ItemsStoreClass);
	//
	//			const FGameplayTag& InBarId = ASB.Key;
	//			const FGameplayTag& InQuickSlotId = S.Key;
	//			
	//			TArray<FGameplayTag>* ActiveTag = ActiveSlotOnBar.Find(InBarId);
	//
	//			if (ActiveTag != nullptr && ActiveTag->Contains(InQuickSlotId))
	//			{
	//				return;
	//			}
	//
	//			if (GetOwnerRole() < ENetRole::ROLE_Authority)
	//			{
	//				ServerAddItemToBar(InBarId, InQuickSlotId, InItem->GetItemId());
	//			}
	//			
	//			const FArcItemData* SlotData = FindSlotData(InBarId, InQuickSlotId);
	//
	//			int32 BarIdx = QuickBars.IndexOfByKey(InBarId);
	//			int32 NewSlotIdx = QuickBars[BarIdx].Slots.IndexOfByKey(InQuickSlotId);
	//
	//			if (NewSlotIdx != INDEX_NONE)
	//			{
	//				ActiveSlotOnBar.FindOrAdd(InBarId).AddUnique(InQuickSlotId);
	//
	//				const FArcQuickBar* QuickBar = &QuickBars[BarIdx];
	//				const FArcQuickBarSlot* QuickSlot = &QuickBars[BarIdx].Slots[NewSlotIdx];
	//				
	//				for (const FInstancedStruct& IS : QuickBars[BarIdx].Slots[NewSlotIdx].SelectedHandlers)
	//				{
	//					IS.GetPtr<FArcQuickSlotHandler>()->OnSlotSelected(ArcASC
	//						, this
	//						, SlotData
	//						, QuickBar
	//						, QuickSlot);
	//				}
	//
	//				InternalTryBindInput(BarIdx, NewSlotIdx, ArcASC);
	//			}
	//
	//			OnItemAddedToQuickSlotDelegate.Broadcast(InBarId, InQuickSlotId, SlotData->GetItemId());
	//			
	//			OnQuickBarSlotChangedDelegate.Broadcast(InBarId
	//				, InQuickSlotId
	//				, FGameplayTag::EmptyTag
	//				, FGameplayTag::EmptyTag);
	//		}
	//	}
	//}
	//
	//UArcItemsSubsystem* ItemsSubsystem = UArcItemsSubsystem::Get(this);
	//ItemsSubsystem->RemoveActorOnItemAddedToSlotMap(GetOwner(), InItem->GetItemId(), WaitItemInitializeDelegateHandles[InItem->GetItemId()]);
}

void UArcQuickBarComponent::ClientUnlockQuickBar_Implementation()
{
	UE_LOG(LogArcQuickBar
	, Log
	, TEXT("UArcQuickBarComponent::ClientUnlockQuickBar"));
	LockQuickBar = 0;
}

void UArcQuickBarComponent::ServerHandleCycledSlot_Implementation(const FArcCycledSlotRPCData& RPCData)
{
	UArcCoreAbilitySystemComponent* ArcASC = GetOwner()->FindComponentByClass<UArcCoreAbilitySystemComponent>();

	UArcQuickBarSubsystem* QuickBarSubsystem = UArcQuickBarSubsystem::Get(this);
	
	if (RPCData.NewBarIdx >= 0 && RPCData.OldSlotIdx >= 0)
	{
		FGameplayTag OldQuickSlotId = RPCData.OldSlotIdx != INDEX_NONE
								 ? QuickBars[RPCData.NewBarIdx].Slots[RPCData.OldSlotIdx].QuickBarSlotId
								 : FGameplayTag::EmptyTag;
		QuickBarSubsystem->BroadcastActorOnQuickBarSlotChanged(this->GetOwner()
				, this
				, QuickBars[RPCData.NewBarIdx].BarId
				, QuickBars[RPCData.NewBarIdx].Slots[RPCData.NewSlotIdx].QuickBarSlotId
				, QuickBars[RPCData.NewBarIdx].BarId
				,  OldQuickSlotId);
	}
	
	if (RPCData.DeselectBarIdx >= 0 && RPCData.DeselectSlotIdx >= 0)
	{
		FGameplayTag OldBarId = QuickBars[RPCData.NewBarIdx].BarId;
		FGameplayTag OldQuickSlotId = QuickBars[RPCData.NewBarIdx].Slots[RPCData.NewSlotIdx].QuickBarSlotId;
		
		const FArcItemData* NewData = FindSlotData(OldBarId, OldQuickSlotId);

		if (NewData)
		{
			const FArcQuickBar* QuickBar = &QuickBars[RPCData.DeselectBarIdx];
			const FArcQuickBarSlot* QuickSlot = &QuickBars[RPCData.DeselectBarIdx].Slots[RPCData.DeselectSlotIdx];
			
			for (const FInstancedStruct& IS : QuickBars[RPCData.DeselectBarIdx].Slots[RPCData.DeselectSlotIdx].
				 SelectedHandlers)
			{
				IS.GetPtr<FArcQuickSlotHandler>()->OnSlotDeselected(ArcASC
					, this
					, NewData
					, QuickBar
					, QuickSlot);
			}
		}
		ReplicatedSelectedSlots.SetQuickSlotDeselected(OldBarId, OldQuickSlotId);
	}

	if (RPCData.NewBarIdx >= 0 && RPCData.NewSlotIdx >= 0)
	{
		FGameplayTag BarId = QuickBars[RPCData.NewBarIdx].BarId;
		FGameplayTag QuickSlotId = QuickBars[RPCData.NewBarIdx].Slots[RPCData.NewSlotIdx].QuickBarSlotId;
		
		const FArcItemData* NewData = FindSlotData(BarId, QuickSlotId);
			
		if (NewData)
		{
			// Lazy add to item slot mapping, as some systems depends on this id being set..
			//ItemSlotMapping.FindOrAdd(QuickBars[RPCData.NewBarIdx].BarId).Slots
			//	.FindOrAdd(QuickBars[RPCData.NewBarIdx].Slots[RPCData.NewSlotIdx].QuickBarSlotId) = NewData->GetItemId();
			
			const FArcQuickBar* QuickBar = &QuickBars[RPCData.NewBarIdx];
			const FArcQuickBarSlot* QuickSlot = &QuickBars[RPCData.NewBarIdx].Slots[RPCData.NewSlotIdx];

			ReplicatedSelectedSlots.AddQuickSlot(BarId, QuickSlotId);
			ReplicatedSelectedSlots.SetQuickSlotSelected(BarId, QuickSlotId);
;			ReplicatedSelectedSlots.SetItemSlot(BarId, QuickSlotId, RPCData.SelectedItemSlot);
			ReplicatedSelectedSlots.SetAssignedItemId(BarId, QuickSlotId, RPCData.SelectedItemId);
			
			for (const FInstancedStruct& IS : QuickBars[RPCData.NewBarIdx].Slots[RPCData.NewSlotIdx].
				 SelectedHandlers)
			{
				IS.GetPtr<FArcQuickSlotHandler>()->OnSlotSelected(ArcASC
					, this
					, NewData
					, QuickBar
					, QuickSlot);
			}
		}
		
	}
	
	if (RPCData.bUnbindInput)
	{
		InternalTryUnbindInput(RPCData.OldBarIdx, RPCData.OldSlotIdx, ArcASC);
	}
	if (RPCData.bBindInput)
	{
		InternalTryBindInput(RPCData.NewBarIdx, RPCData.NewSlotIdx, ArcASC);
	}

	// We want to send it regardless. May consider using replicated variable instead.
	ClientConfirmSlotCycled();
}

void UArcQuickBarComponent::ClientConfirmSlotCycled_Implementation()
{
	UE_LOG(LogArcQuickBar
			, Log
			, TEXT("UArcQuickBarComponent::ClientConfirmSlotCycled"));
	
	LockSlotCycle = 0;
	
	UArcQuickBarSubsystem* QuickBarSubsystem = UArcQuickBarSubsystem::Get(this);
	QuickBarSubsystem->BroadcastActorOnSlotCycleConfirmed(this->GetOwner(), this);	
}

void UArcQuickBarComponent::ServerDeselectBar_Implementation(int8 BarIdx)
{
	for (FInstancedStruct& IS : QuickBars[BarIdx].QuickBarSelectedActions)
	{
		if (FArcQuickBarSelectedAction* QBA = IS.GetMutablePtr<FArcQuickBarSelectedAction>())
		{
			QBA->QuickBarActivated(this
				, &QuickBars[BarIdx]);
		}
	}
	
	ClientUnlockQuickBar();
	DeactivateSlotsOnBarInternal(QuickBars[BarIdx].BarId);
}

void UArcQuickBarComponent::ServerSelectBar_Implementation(int8 BarIdx)
{
	for (FInstancedStruct& IS : QuickBars[BarIdx].QuickBarSelectedActions)
	{
		if (FArcQuickBarSelectedAction* QBA = IS.GetMutablePtr<FArcQuickBarSelectedAction>())
		{
			QBA->QuickBarActivated(this
				, &QuickBars[BarIdx]);
		}
	}
	
	ClientUnlockQuickBar();
	ActivateSlotsOnBarInternal(QuickBars[BarIdx].BarId);
}

UArcItemsStoreComponent* UArcQuickBarComponent::GetItemStoreComponent(const FArcQuickBar& InQuickBar) const
{
	UActorComponent* AC = GetOwner()->GetComponentByClass(GetItemsStoreClass(InQuickBar));

	return Cast<UArcItemsStoreComponent>(AC);
}

UArcItemsStoreComponent* UArcQuickBarComponent::GetItemStoreComponent(const FGameplayTag& InQuickBarId) const
{
	UActorComponent* AC = GetOwner()->GetComponentByClass(GetItemsStoreClass(InQuickBarId));

	return Cast<UArcItemsStoreComponent>(AC);
}

UArcItemsStoreComponent* UArcQuickBarComponent::GetItemStoreComponent(const int32 InQuickBarId) const
{
	UActorComponent* AC = GetOwner()->GetComponentByClass(GetItemsStoreClass(InQuickBarId));

	return Cast<UArcItemsStoreComponent>(AC);
}

void UArcQuickBarComponent::HandleOnAbilityAdded(FGameplayAbilitySpec& AbilitySpec, UArcCoreAbilitySystemComponent* InASC)
{
	if (GetNetMode() == ENetMode::NM_Client)
	{
		ReplicatedSelectedSlots.HandleOnItemAddedToSlot();	
	}
}

void UArcQuickBarComponent::HandleOnItemAddedToSlot(UArcItemsStoreComponent* InItemsStore
													, const FArcItemData* InItem
													, FGameplayTag BarId
													, FGameplayTag InQuickSlotId)
{
	if(InItemsStore->GetOwner() != GetOwner())
	{
		return;
	}

	if (GetNetMode() == ENetMode::NM_Client)
	{
		ReplicatedSelectedSlots.HandleOnItemAddedToSlot();	
	}
	
	UArcItemsStoreComponent* SlotComponent = GetItemStoreComponent(BarId);
	UArcCoreAbilitySystemComponent* ArcASC = GetOwner()->FindComponentByClass<UArcCoreAbilitySystemComponent>();
	
	int32 BarIdx = QuickBars.IndexOfByKey(BarId);
	int32 SlotIdx = QuickBars[BarIdx].Slots.IndexOfByKey(InQuickSlotId);

	const FArcItemData* SlotData = SlotComponent->GetItemPtr(InItem->GetItemId());

	ReplicatedSelectedSlots.AddQuickSlot(BarId, InQuickSlotId);
	ReplicatedSelectedSlots.SetItemSlot(BarId, InQuickSlotId, SlotData->GetSlotId());
	ReplicatedSelectedSlots.SetAssignedItemId(BarId, InQuickSlotId, InItem->GetItemId());
	
	// TODO: This need more checks, like auto select slot on bar with multiple selected slots (like spells)
	// and should not run if there is only one selected slot and it is not this one.
	if (QuickBars[BarIdx].Slots[SlotIdx].bAutoSelect || QuickBars[BarIdx].QuickSlotsMode == EArcQuickSlotsMode::ActivateOnly)
	{
		ReplicatedSelectedSlots.SetQuickSlotSelected(BarId, InQuickSlotId);
		
		for (const FInstancedStruct& IS : QuickBars[BarIdx].Slots[BarIdx].SelectedHandlers)
		{
			IS.GetPtr<FArcQuickSlotHandler>()->OnSlotSelected(ArcASC
				, this
				, SlotData
				, &QuickBars[BarIdx]
				, &QuickBars[BarIdx].Slots[SlotIdx]);
		}

		ReplicatedSelectedSlots.SetQuickSlotSelected(BarId, InQuickSlotId);
		InternalTryBindInput(BarIdx, SlotIdx, ArcASC);
	}
	
	UArcQuickBarSubsystem* QuickBarSubsystem = UArcQuickBarSubsystem::Get(this);

	QuickBarSubsystem->BroadcastActorOnItemAddedToQuickSlot(this->GetOwner()
		, this
		, BarId
		, InQuickSlotId
		, InItem->GetItemId());
	
	QuickBarSubsystem->OnItemAddedToQuickSlot.Broadcast(this, BarId, InQuickSlotId, InItem->GetItemId());
}

void UArcQuickBarComponent::HandleOnItemRemovedFromSlot(UArcItemsStoreComponent* InItemsStore
														, const FArcItemData* InItem
														, FGameplayTag BarId
														, FGameplayTag InQuickSlotId)
{
	if(InItemsStore->GetOwner() != GetOwner())
	{
		return;
	}
	
	UArcItemsStoreComponent* SlotComponent = GetItemStoreComponent(BarId);
	UArcCoreAbilitySystemComponent* ArcASC = GetOwner()->FindComponentByClass<UArcCoreAbilitySystemComponent>();
	
	int32 BarIdx = QuickBars.IndexOfByKey(BarId);
	int32 SlotIdx = QuickBars[BarIdx].Slots.IndexOfByKey(InQuickSlotId);

	if (QuickBars[BarIdx].Slots[SlotIdx].bAutoSelect && QuickBars[BarIdx].QuickSlotsMode == EArcQuickSlotsMode::ActivateOnly)
	{
		ReplicatedSelectedSlots.SetQuickSlotDeselected(BarId, InQuickSlotId);
		
		const FArcItemData* SlotData = FindSlotData(QuickBars[BarIdx].BarId
										, QuickBars[BarIdx].Slots[SlotIdx].QuickBarSlotId);
		
		for (const FInstancedStruct& IS : QuickBars[BarIdx].Slots[BarIdx].SelectedHandlers)
		{
			IS.GetPtr<FArcQuickSlotHandler>()->OnSlotDeselected(ArcASC
				, this
				, SlotData
				, &QuickBars[BarIdx]
				, &QuickBars[BarIdx].Slots[SlotIdx]);
		}

		InternalTryUnbindInput(BarIdx, SlotIdx, ArcASC);
	}
	const FArcItemData* SlotData = FindSlotData(QuickBars[BarIdx].BarId
										, QuickBars[BarIdx].Slots[SlotIdx].QuickBarSlotId);
	UArcQuickBarSubsystem* QuickBarSubsystem = UArcQuickBarSubsystem::Get(this);

	QuickBarSubsystem->BroadcastActorOnItemRemovedFromQuickSlot(this->GetOwner()
		, this
		, BarId
		, InQuickSlotId
		, SlotData->GetItemId());
	
	ReplicatedSelectedSlots.RemoveAssignedItemId(BarId, InQuickSlotId);
	ReplicatedSelectedSlots.RemoveItemSlot(BarId, InQuickSlotId);
}

void UArcQuickBarComponent::HandlePendingItemsWhenSlotChanges(UArcItemsStoreComponent* ItemSlotComponent
	, const FGameplayTag& ItemSlot
	, const FArcItemData* ItemData)
{
	ReplicatedSelectedSlots.HandleOnItemAddedToSlot();
}

void UArcQuickBarComponent::HandlePendingItemsOnAbiilityGiven(FGameplayAbilitySpec& AbilitySpec
	, UArcCoreAbilitySystemComponent* InASC)
{
	ReplicatedSelectedSlots.HandleOnItemAddedToSlot();
}

FArcItemId UArcQuickBarComponent::GetItemId(const FGameplayTag& InBarId
											, const FGameplayTag& InQuickSlotId) const
{
	return ReplicatedSelectedSlots.FindItemId(InBarId, InQuickSlotId);
}

UArcItemsStoreComponent* UArcQuickBarComponent::BP_GetItemStoreComponent(const FGameplayTag& InQuickBarId) const
{
	return GetItemStoreComponent(InQuickBarId);
}

bool UArcQuickBarComponent::BP_GetQuickBar(UArcQuickBarComponent* QuickBarComp
	, FGameplayTag InBarId
	, FArcQuickBar& QuickBar)
{
	if (!QuickBarComp)
	{
		return false;
	}

	int32 BarIdx = QuickBarComp->QuickBars.IndexOfByKey(InBarId);
	if (BarIdx != INDEX_NONE)
	{
		QuickBar = QuickBarComp->QuickBars[BarIdx];
		return true;
	}

	return false;
}

bool UArcQuickBarComponent::BP_GetItemFromSelectedSlot(FGameplayTag QuickBar
	, FArcItemDataHandle& OutItem)
{
	FGameplayTag ActiveSlot = ReplicatedSelectedSlots.FindFirstSelectedSlot(QuickBar);
	if (ActiveSlot.IsValid())
	{
		UArcItemsStoreComponent* SlotComp = GetItemStoreComponent(QuickBar);

		FArcItemId ItemId = ReplicatedSelectedSlots.FindItemId(QuickBar, ActiveSlot);
		if (ItemId.IsValid() == false)
		{
			ItemId = ReplicatedSelectedSlots.FindOldItemId(QuickBar, ActiveSlot);
		}

		const bool bValid = SlotComp->BP_GetItem(ItemId, OutItem);
		return bValid;
	}
	
	return false;
}
