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

#include "QuickBar/ArcQuickBarComponent.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/LocalPlayer.h"

#include "GameFramework/Character.h"
#include "GameFramework/PlayerState.h"

#include "Animation/AnimMontage.h"
#include "ArcCoreUtils.h"
#include "ArcQuickBarInputBindHandling.h"
#include "ArcWorldDelegates.h"

#include "Input/ArcCoreInputComponent.h"
#include "AbilitySystem/ArcCoreAbilitySystemComponent.h"

#include "Engine/World.h"
#include "Items/ArcItemsStoreComponent.h"
#include "Items/ArcItemsSubsystem.h"
#include "Items/Fragments/ArcItemFragment_GrantedAbilities.h"
#include "Misc/DataValidation.h"
#include "Net/UnrealNetwork.h"
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

void FArcSelectedQuickBarSlot::PreReplicatedRemove(const struct FArcSelectedQuickBarSlotList& InArraySerializer)
{
	UArcCoreAbilitySystemComponent* ArcASC = InArraySerializer.QuickBar->GetOwner()->FindComponentByClass<UArcCoreAbilitySystemComponent>();
	UArcQuickBarComponent* QuickBar = InArraySerializer.QuickBar.Get();
	
	UArcQuickBarSubsystem* QuickBarSubsystem = UArcQuickBarSubsystem::Get(ArcASC);
	
	int32 BarIdx = InArraySerializer.QuickBar->QuickBars.IndexOfByKey(BarId);
	int32 OldSlotIdx = InArraySerializer.QuickBar->QuickBars[BarIdx].Slots.IndexOfByKey(QuickSlotId);
	
	if (OldSlotIdx != INDEX_NONE)
	{
		const FArcQuickBar* QuickBarStruct = &InArraySerializer.QuickBar->QuickBars[BarIdx];
		const FArcQuickBarSlot* QuickSlot = &InArraySerializer.QuickBar->QuickBars[BarIdx].Slots[OldSlotIdx];

		UArcItemsStoreComponent* ItemsStoreComponent = InArraySerializer.QuickBar->GetItemStoreComponent(QuickBarStruct->BarId);
		
		const FArcItemData* OldData = InArraySerializer.QuickBar->FindQuickSlotItem(BarId, QuickSlotId);
	
		if (OldData == nullptr)
		{
			return;
		}
		
		for (const FInstancedStruct& IS : InArraySerializer.QuickBar->QuickBars[BarIdx].Slots[OldSlotIdx].SelectedHandlers)
		{
			IS.GetPtr<FArcQuickSlotHandler>()->OnSlotDeselected(ArcASC
				, InArraySerializer.QuickBar.Get()
				, OldData
				, QuickBarStruct
				, QuickSlot);
		}
		
		QuickBarSubsystem->BroadcastActorOnQuickSlotDeactivated(QuickBar->GetOwner()
		, QuickBar
		, BarId
		, QuickSlotId
		, AssignedItemId);

		QuickBarSubsystem->OnQuickSlotDeactivated.Broadcast(QuickBar, BarId, QuickSlotId, AssignedItemId);
	}
	
	QuickBarSubsystem->BroadcastActorOnQuickSlotRemoved(QuickBar->GetOwner()
			, QuickBar
			, BarId
			, QuickSlotId
			, AssignedItemId);
	
	QuickBarSubsystem->OnQuickSlotRemoved.Broadcast(QuickBar, BarId, QuickSlotId, AssignedItemId);
}

void FArcSelectedQuickBarSlot::PostReplicatedAdd(const struct FArcSelectedQuickBarSlotList& InArraySerializer)
{
	UArcQuickBarComponent* QuickBar = InArraySerializer.QuickBar.Get();

	UArcCoreAbilitySystemComponent* ArcASC = QuickBar->GetOwner()->FindComponentByClass<UArcCoreAbilitySystemComponent>();
	UArcItemsStoreComponent* ItemsStoreComponent = QuickBar->GetItemStoreComponent(BarId);
	UArcQuickBarSubsystem* QuickBarSubsystem = UArcQuickBarSubsystem::Get(QuickBar);

	const int32 BarIdx = QuickBar->QuickBars.IndexOfByKey(BarId);
	const FArcQuickBarSlot* QuickSlotPtr = QuickBar->FindQuickSlot(BarId, QuickSlotId);

	if (bIsSlotActive)
	{
		const FArcItemData* ItemData = ItemsStoreComponent->GetItemPtr(AssignedItemId);

		if (!ItemData)
		{
			InArraySerializer.PendingAddQuickSlotItems.Add( {AssignedItemId, BarId, QuickSlotId} );
			return;
		}
	
		for (const FInstancedStruct& IS : QuickSlotPtr->SelectedHandlers)
		{
			IS.GetPtr<FArcQuickSlotHandler>()->OnSlotSelected(ArcASC
				, QuickBar
				, ItemData
				, &QuickBar->QuickBars[BarIdx]
				, QuickSlotPtr);
		}
	
		QuickBarSubsystem->BroadcastActorOnQuickSlotActivated(QuickBar->GetOwner()
		, QuickBar
		, BarId
		, QuickSlotId
		, AssignedItemId);

		QuickBarSubsystem->OnQuickSlotActivated.Broadcast(QuickBar, BarId, QuickSlotId, AssignedItemId);
	}
	
	QuickBarSubsystem->BroadcastActorOnQuickSlotAdded(QuickBar->GetOwner()
			, QuickBar
			, BarId
			, QuickSlotId
			, AssignedItemId);
	
	QuickBarSubsystem->OnQuickSlotAdded.Broadcast(QuickBar, BarId, QuickSlotId, AssignedItemId);
}


UArcQuickBarSubsystem* UArcQuickBarSubsystem::Get(UObject* WorldObject)
{
	if (UWorld* World = WorldObject->GetWorld())
	{
		return World->GetGameInstance()->GetSubsystem<UArcQuickBarSubsystem>();
	}

	if (UWorld* World= Cast<UWorld>(WorldObject))
	{
		return World->GetGameInstance()->GetSubsystem<UArcQuickBarSubsystem>();
	}

	return nullptr;
}

void UArcQuickBarComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	Params.Condition = COND_OwnerOnly;

	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, ReplicatedSelectedSlots, Params);
	
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
}

const FArcItemData* UArcQuickBarComponent::FindQuickSlotItem(const FGameplayTag& BarId
															 , const FGameplayTag& QuickBarSlotId) const
{
	UArcItemsStoreComponent* SlotComp = GetItemStoreComponent(BarId);

	FArcItemId ItemId = ReplicatedSelectedSlots.FindItemId(BarId, QuickBarSlotId);
	
	return SlotComp->GetItemPtr(ItemId);
}

const FArcQuickBarSlot* UArcQuickBarComponent::FindQuickSlot(const FGameplayTag& BarId
	, const FGameplayTag& QuickBarSlotId) const
{
	const int32 BarIdx = QuickBars.IndexOfByKey(BarId);
	const int32 QuickSlotIdx = QuickBars[BarIdx].Slots.IndexOfByKey(QuickBarSlotId);

	return &QuickBars[BarIdx].Slots[QuickSlotIdx];
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

			if (QuickBar.QuickSlotsMode == EArcQuickSlotsMode::AutoActivateOnly)
			{
				if (Slot.bAutoSelect)
				{
					FString Msg = FString::Printf(TEXT("QuickBar %s Slot %s has bAutoSelect set to true, but QuickSlotsMode is set to AutoActivateOnly."), *QuickBar.BarId.ToString(), *Slot.QuickBarSlotId.ToString());
					Context.AddError(FText::FromString(Msg));
					ErrorCount++;
				}
			}

			if (QuickBar.QuickSlotsMode == EArcQuickSlotsMode::Cyclable)
			{
				int32 Counter = 0;
				for (const FArcQuickBarSlot& CyclebaleSlots : QuickBar.Slots)
				{
					if (CyclebaleSlots.bAutoSelect)
					{
						Counter++;
						if (Counter > 1)
						{
							FString Msg = FString::Printf(TEXT("QuickBar %s Is set to Cyclable but there is more than one slot with bAutoSelect set to true. "), *QuickBar.BarId.ToString(), *Slot.QuickBarSlotId.ToString());
							Context.AddError(FText::FromString(Msg));
							ErrorCount++;
							break;
						}
					}
					
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

}

void UArcQuickBarComponent::InitializeComponent()
{
	Super::InitializeComponent();

	if (GetOwnerRole() < ROLE_Authority)
	{
		return;
	}
	if (Arcx::Utils::IsPlayableWorld(GetWorld()))
	{
		UArcItemsSubsystem* ItemsSubsystem = UArcItemsSubsystem::Get(this);
		// Listen for slot items only on server ?
				
		ItemsSubsystem->AddActorOnAddedToSlot(GetOwner(), FArcGenericItemSlotDelegate::FDelegate::CreateUObject(
			this, &UArcQuickBarComponent::HandlePendingItemsWhenSlotChanges
		));
			
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
}

void UArcQuickBarComponent::AddAndActivateQuickSlot(const FGameplayTag& BarId, const FGameplayTag& QuickSlotId, const FArcItemId& ItemId)
{
	UArcItemsStoreComponent* ItemsStoreComponent = GetItemStoreComponent(BarId);
	const FArcItemData* ItemData = ItemsStoreComponent->GetItemPtr(ItemId);

	int32 QuickBarIdx = QuickBars.IndexOfByKey(BarId);
	int32 QuickSlotIdx = QuickBars[QuickBarIdx].Slots.IndexOfByKey(QuickSlotId);
	
	FArcQuickBarSlot& QukcBarSlot = QuickBars[QuickBarIdx].Slots[QuickSlotIdx];
	const FArcQuickBar* QuickBar = &QuickBars[QuickBarIdx];

	ReplicatedSelectedSlots.AddQuickSlot(BarId, QuickSlotId, ItemId, ItemData->GetSlotId());
	
	UArcQuickBarSubsystem* QuickBarSubsystem = UArcQuickBarSubsystem::Get(this);

	if (QuickBar->QuickSlotsMode == EArcQuickSlotsMode::AutoActivateOnly || QukcBarSlot.bAutoSelect)
	{
		HandleSlotActivated(BarId, QuickSlotId, ItemId);	
	}
		
	QuickBarSubsystem->BroadcastActorOnQuickSlotActivated(this->GetOwner()
		, this
		, BarId
		, QuickSlotId
		, ItemData->GetItemId());
	
	QuickBarSubsystem->OnQuickSlotActivated.Broadcast(this, BarId, QuickSlotId, ItemData->GetItemId());

	MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, ReplicatedSelectedSlots, this);
}

void UArcQuickBarComponent::RemoveQuickSlot(const FGameplayTag& BarId, const FGameplayTag& QuickSlotId)
{
	UArcQuickBarSubsystem* QuickBarSubsystem = UArcQuickBarSubsystem::Get(this);

	FArcItemId ItemId = ReplicatedSelectedSlots.FindItemId(BarId, QuickSlotId);

	HandleSlotDeactivated(BarId, QuickSlotId);
	
	QuickBarSubsystem->BroadcastActorOnQuickSlotDeactivated(GetOwner()
		, this
		, BarId
		, QuickSlotId
		, ItemId);

	QuickBarSubsystem->OnQuickSlotDeactivated.Broadcast(this, BarId, QuickSlotId, ItemId);

	QuickBarSubsystem->BroadcastActorOnQuickSlotRemoved(this->GetOwner()
				, this
				, BarId
				, QuickSlotId
				, ItemId);

	QuickBarSubsystem->OnQuickSlotRemoved.Broadcast(this, BarId, QuickSlotId, ItemId);
	
	ReplicatedSelectedSlots.RemoveQuickSlot(BarId, QuickSlotId);

	MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, ReplicatedSelectedSlots, this);
}

bool UArcQuickBarComponent::HandleSlotActivated(const FGameplayTag& BarId, const FGameplayTag& QuickSlotId, const FArcItemId& ItemId)
{
	UArcCoreAbilitySystemComponent* ArcASC = GetOwner()->FindComponentByClass<UArcCoreAbilitySystemComponent>();
	
	int32 QuickBarIdx = QuickBars.IndexOfByKey(BarId);
	int32 QuickSlotIdx = QuickBars[QuickBarIdx].Slots.IndexOfByKey(QuickSlotId);

	if (QuickBarIdx == INDEX_NONE || QuickSlotIdx == INDEX_NONE)
	{
		UE_LOG(LogArcQuickBar, Warning, TEXT("HandleSlotActivated: QuickBar or QuickSlot not found for BarId: %s, QuickSlotId: %s"), *BarId.ToString(), *QuickSlotId.ToString());
		return false;
	}
	
	FArcQuickBarSlot& QukcBarSlot = QuickBars[QuickBarIdx].Slots[QuickSlotIdx];
	const FArcQuickBar* QuickBar = &QuickBars[QuickBarIdx];

	UArcItemsStoreComponent* ItemsStoreComponent = GetItemStoreComponent(BarId);
	const FArcItemData* ItemData = ItemsStoreComponent->GetItemPtr(ItemId);
	
	for (const FInstancedStruct& IS : QukcBarSlot.SelectedHandlers)
	{
		IS.GetPtr<FArcQuickSlotHandler>()->OnSlotSelected(ArcASC
			, this
			, ItemData
			, QuickBar
			, &QukcBarSlot);
	}

	InternalTryBindInput(QuickBarIdx, QuickSlotIdx, ArcASC);
	ReplicatedSelectedSlots.ActivateQuickSlot(BarId, QuickSlotId);

	MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, ReplicatedSelectedSlots, this);
	return true;
}

void UArcQuickBarComponent::HandleSlotDeactivated(const FGameplayTag& BarId, const FGameplayTag& QuickSlotId)
{	
	UArcCoreAbilitySystemComponent* ArcASC = GetOwner()->FindComponentByClass<UArcCoreAbilitySystemComponent>();
	
	int32 BarIdx = QuickBars.IndexOfByKey(BarId);
	int32 OldSlotIdx = QuickBars[BarIdx].Slots.IndexOfByKey(QuickSlotId);
	
	if (OldSlotIdx != INDEX_NONE)
	{
		const FArcQuickBar* QuickBar = &QuickBars[BarIdx];
		const FArcQuickBarSlot* QuickSlot = &QuickBars[BarIdx].Slots[OldSlotIdx];

		UArcItemsStoreComponent* ItemsStoreComponent = GetItemStoreComponent(QuickBar->BarId);
		
		const FArcItemData* OldData = FindQuickSlotItem(BarId, QuickSlotId);
	
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

		ReplicatedSelectedSlots.DeactivateQuickSlot(BarId, QuickSlotId);

		MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, ReplicatedSelectedSlots, this);
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

void UArcQuickBarComponent::DeselectCurrentQuickSlot(FGameplayTag InBarId)
{
	FGameplayTag SelectedQuickSlot = GetFirstActiveSlot(InBarId);
	if (!SelectedQuickSlot.IsValid())
	{
		return;
	}
	
	int32 BarIdx = QuickBars.IndexOfByKey(InBarId);
	int32 OldSlotIdx = QuickBars[BarIdx].Slots.IndexOfByKey(SelectedQuickSlot);
	
	const FArcQuickBar* QuickBar = &QuickBars[BarIdx];
	
	TSubclassOf<UArcCoreAbilitySystemComponent> C = UArcCoreAbilitySystemComponent::StaticClass(); 
	UArcCoreAbilitySystemComponent* ArcASC = Arcx::Utils::GetComponent(GetOwner(), C);
	
	FArcCycledSlotRPCData RPCData;
	/**
	 * If OldSlotIdx is valid it means there was something on old slot, so we let it clean up after itself.
	 */
	if (OldSlotIdx != INDEX_NONE)
	{	
		const FArcItemData* OldData = FindQuickSlotItem(InBarId, SelectedQuickSlot);
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
			
			RPCData.DeactivateBarIdx = BarIdx;
			RPCData.DeactivateSlotIdx = OldSlotIdx;

		}

		if (SelectedQuickSlot.IsValid())
		{
			InternalTryUnbindInput(BarIdx, OldSlotIdx, ArcASC);
			
			ReplicatedSelectedSlots.DeactivateQuickSlot(InBarId, SelectedQuickSlot);
		}

		UArcQuickBarSubsystem* QuickBarSubsystem = UArcQuickBarSubsystem::Get(this);
		QuickBarSubsystem->BroadcastActorOnQuickSlotDeactivated(GetOwner()
				, this
				, QuickBars[BarIdx].BarId
				, SelectedQuickSlot
				, OldData->GetItemId());
		
		QuickBarSubsystem->OnQuickSlotDeactivated.Broadcast(this
				, QuickBars[BarIdx].BarId
				, SelectedQuickSlot
				, OldData->GetItemId());
				
		QuickBarSubsystem->OnQuickSlotDeselected.Broadcast(this
				, QuickBars[BarIdx].BarId
				, SelectedQuickSlot
				, OldData->GetItemId());
	}
	
	if (GetOwnerRole() < ENetRole::ROLE_Authority)
	{
		ServerHandleCycledSlot(RPCData);
	}
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
		const FArcItemData* OldData = FindQuickSlotItem(BarId, OldSlotTag);
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
			
			RPCData.DeactivateBarIdx = BarIdx;
			RPCData.DeactivateSlotIdx = OldSlotIdx;

		}

		if (OldSlotTag.IsValid())
		{
			InternalTryUnbindInput(BarIdx, OldSlotIdx, ArcASC);
			
			ReplicatedSelectedSlots.DeactivateQuickSlot(BarId, OldSlotTag);
		}

		UArcQuickBarSubsystem* QuickBarSubsystem = UArcQuickBarSubsystem::Get(this);
		QuickBarSubsystem->BroadcastActorOnQuickSlotDeactivated(GetOwner()
				, this
				, QuickBars[BarIdx].BarId
				, NewQuickSlotId
				, OldData->GetItemId());
		
		QuickBarSubsystem->OnQuickSlotDeactivated.Broadcast(this
				, QuickBars[BarIdx].BarId
				, NewQuickSlotId
				, OldData->GetItemId());
	}

	/**
	 * We found new valid slot, let it call all handlers and bind inputs.
	 */
	if (NewSlotIdx != INDEX_NONE)
	{
		NewQuickSlotId = QuickBars[BarIdx].Slots[NewSlotIdx].QuickBarSlotId;

		const FArcItemData* NewData = FindQuickSlotItem(BarId, NewQuickSlotId);
		const FArcQuickBarSlot* QuickSlot = &QuickBars[BarIdx].Slots[NewSlotIdx];

		ReplicatedSelectedSlots.ActivateQuickSlot(QuickBars[BarIdx].BarId, NewQuickSlotId);
		
		for (const FInstancedStruct& IS : QuickBars[BarIdx].Slots[NewSlotIdx].SelectedHandlers)
		{
			IS.GetPtr<FArcQuickSlotHandler>()->OnSlotSelected(ArcASC
				, this
				, NewData
				, QuickBar
				, QuickSlot);
		}

		RPCData.ActivateBarIdx = BarIdx;
		RPCData.DeactivateSlotIdx = NewSlotIdx;

		InternalTryBindInput(BarIdx, NewSlotIdx, ArcASC);

		UArcQuickBarSubsystem* QuickBarSubsystem = UArcQuickBarSubsystem::Get(this);
		QuickBarSubsystem->BroadcastActorOnQuickSlotCycled(GetOwner()
				, this
				, QuickBars[BarIdx].BarId
				, NewQuickSlotId
				, NewData->GetItemId());
		
		QuickBarSubsystem->OnQuickSlotSelected.Broadcast(this
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
		if (QuickBars[BarIdx].QuickSlotsMode != EArcQuickSlotsMode::Cyclable)
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
				if (Val->IsValid(this, QuickBars[BarIdx], QuickBars[BarIdx].Slots[NewSlotIdx]) == false)
				{
					continue;
				}
			}
		}

		const FArcItemData* Data = FindQuickSlotItem(QuickBars[BarIdx].BarId
												, QuickBars[BarIdx].Slots[NewSlotIdx].QuickBarSlotId);
		
		UE_LOG(LogArcQuickBar, Log, TEXT("CycleSlotForward Iteration [SlotIdx %d] [NewSlotIdx %d]"), StartSlotIdx, NewSlotIdx);
		
		// do we want to equip, equipped weapon ?
		if (NewSlotIdx == StartSlotIdx)
		{
			UE_LOG(LogArcQuickBar, Log, TEXT("CycleSlotForward break 1 [SlotIdx %d] [NewSlotIdx %d]"), StartSlotIdx, NewSlotIdx);
			break;
		}
		
		if (Data)
		{
			// TODO Maybe also add extension delegates _RetVal ?
			if (SlotValidCondition(Data))
			{
				UE_LOG(LogArcQuickBar, Log, TEXT("CycleSlotForward break Found [SlotIdx %d] [NewSlotIdx %d]"), StartSlotIdx, NewSlotIdx);
				
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
			UE_LOG(LogArcQuickBar, Log, TEXT("CycleSlotForward break Not Found [SlotIdx %d] [NewSlotIdx %d]"), StartSlotIdx, NewSlotIdx);
			break;
		}
	}
	while (NewSlotIdx != StartSlotIdx);

	return bFoundNewSlot;
}


void UArcQuickBarComponent::ServerHandleCycledSlot_Implementation(const FArcCycledSlotRPCData& RPCData)
{
	UArcCoreAbilitySystemComponent* ArcASC = GetOwner()->FindComponentByClass<UArcCoreAbilitySystemComponent>();

	UArcQuickBarSubsystem* QuickBarSubsystem = UArcQuickBarSubsystem::Get(this);
	
	if (RPCData.DeactivateBarIdx >= 0 && RPCData.DeactivateSlotIdx >= 0)
	{
		FGameplayTag OldBarId = QuickBars[RPCData.DeactivateBarIdx].BarId;
		FGameplayTag OldQuickSlotId = QuickBars[RPCData.DeactivateBarIdx].Slots[RPCData.DeactivateSlotIdx].QuickBarSlotId;
		
		const FArcItemData* NewData = FindQuickSlotItem(OldBarId, OldQuickSlotId);

		if (NewData)
		{
			const FArcQuickBar* QuickBar = &QuickBars[RPCData.DeactivateBarIdx];
			const FArcQuickBarSlot* QuickSlot = &QuickBars[RPCData.DeactivateBarIdx].Slots[RPCData.DeactivateSlotIdx];
			
			for (const FInstancedStruct& IS : QuickBars[RPCData.DeactivateBarIdx].Slots[RPCData.DeactivateSlotIdx].
				 SelectedHandlers)
			{
				IS.GetPtr<FArcQuickSlotHandler>()->OnSlotDeselected(ArcASC
					, this
					, NewData
					, QuickBar
					, QuickSlot);
			}
		}

		QuickBarSubsystem->BroadcastActorOnQuickSlotDeactivated(GetOwner()
				, this
				, OldBarId
				, OldQuickSlotId
				, NewData->GetItemId());
		
		QuickBarSubsystem->OnQuickSlotDeactivated.Broadcast(this
				, OldBarId
				, OldQuickSlotId
				, NewData->GetItemId());
		
		ReplicatedSelectedSlots.DeactivateQuickSlot(OldBarId, OldQuickSlotId);
	}

	if (RPCData.ActivateBarIdx >= 0 && RPCData.ActivateSlotIdx >= 0)
	{
		FGameplayTag BarId = QuickBars[RPCData.ActivateBarIdx].BarId;
		FGameplayTag QuickSlotId = QuickBars[RPCData.ActivateBarIdx].Slots[RPCData.ActivateSlotIdx].QuickBarSlotId;
		
		const FArcItemData* NewData = FindQuickSlotItem(BarId, QuickSlotId);
			
		if (NewData)
		{
			const FArcQuickBar* QuickBar = &QuickBars[RPCData.ActivateBarIdx];
			const FArcQuickBarSlot* QuickSlot = &QuickBars[RPCData.ActivateBarIdx].Slots[RPCData.ActivateSlotIdx];

			ReplicatedSelectedSlots.ActivateQuickSlot(BarId, QuickSlotId);
			
			for (const FInstancedStruct& IS : QuickBars[RPCData.ActivateBarIdx].Slots[RPCData.ActivateSlotIdx].
				 SelectedHandlers)
			{
				IS.GetPtr<FArcQuickSlotHandler>()->OnSlotSelected(ArcASC
					, this
					, NewData
					, QuickBar
					, QuickSlot);
			}

			QuickBarSubsystem->BroadcastActorOnQuickSlotActivated(GetOwner()
					, this
					, BarId
					, QuickSlotId
					, NewData->GetItemId());
		
			QuickBarSubsystem->OnQuickSlotActivated.Broadcast(this
					, BarId
					, QuickSlotId
					, NewData->GetItemId());
			
		}
		
	}
	
	InternalTryUnbindInput(RPCData.DeactivateBarIdx, RPCData.DeactivateSlotIdx, ArcASC);
	InternalTryBindInput(RPCData.ActivateBarIdx, RPCData.ActivateSlotIdx, ArcASC);
	
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
		
		const FArcItemData* Data = FindQuickSlotItem(BarId, QuickSlotId);

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
		
		const FArcItemData* Data = FindQuickSlotItem(BarId, QuickSlotId);
		
		TArray<const FArcItemData*> Extensions = SlotComp->GetItemsAttachedTo(Data->GetOwnerId());
		Extensions.Add(Data);
		return Handling->OnRemovedFromQuickBar(InArcASC, Extensions);
	}

	return false;
}

void UArcQuickBarComponent::ActivateBar(const FGameplayTag& InBarId)
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

	int32 BarIdx = QuickBars.IndexOfByKey(InBarId);
	if (BarIdx == INDEX_NONE)
	{
		UE_LOG(LogArcQuickBar, Warning, TEXT("UArcQuickBarComponent::ActivateBar BarId %s not found"), *InBarId.ToString());
		return;
	}
	
	/**
	 * Checks if all slots on this bar can access valid SlotData.
	 * If not, do not allow activating entire bar.
	 *
	 * TODO: Might want later to emmit some events to inform user that something is wrong.
	 */
	if(AreAllBarSlotsValid(BarIdx) == false)
	{
		UE_LOG(LogArcQuickBar, Log, TEXT("UArcQuickBarComponent::ActivateBar One of the slots does not have valid FArcItemData"))
		return;
	}
	
	ServerSelectBar(BarIdx);
	
	for (FInstancedStruct& IS : QuickBars[BarIdx].QuickBarSelectedActions)
	{
		if (FArcQuickBarSelectedAction* QBA = IS.GetMutablePtr<FArcQuickBarSelectedAction>())
		{
			QBA->QuickBarActivated(this, &QuickBars[BarIdx]);
		}
	}
	
	ActivateSlotsOnBarInternal(InBarId);
}

bool UArcQuickBarComponent::AreAllBarSlotsValid(const int32 BarIdx) const
{
	for (int8 Idx = 0; Idx < QuickBars[BarIdx].Slots.Num(); Idx++)
	{
		const FArcItemData* SlotData = FindQuickSlotItem(QuickBars[BarIdx].BarId, QuickBars[BarIdx].Slots[Idx].QuickBarSlotId);
		if(SlotData == nullptr)
		{
			return false;
		}
	}

	return true;
}

void UArcQuickBarComponent::ActivateSlotsOnBarInternal(const FGameplayTag& InBarId)
{
	int32 QuickBarIdx = QuickBars.IndexOfByKey(InBarId);
	
	for (int32 QuickSlotIdx = 0; QuickSlotIdx < QuickBars[QuickBarIdx].Slots.Num(); QuickSlotIdx++)
	{
		FArcQuickBarSlot& QBS = QuickBars[QuickBarIdx].Slots[QuickSlotIdx];
		
		ReplicatedSelectedSlots.ActivateQuickSlot(InBarId, QBS.QuickBarSlotId);
		FArcItemId ItemId = ReplicatedSelectedSlots.FindItemId(InBarId, QBS.QuickBarSlotId);
		
		HandleSlotActivated(InBarId, QBS.QuickBarSlotId, ItemId);
	}
}

void UArcQuickBarComponent::DeactivateBar(const FGameplayTag& InBarId)
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
					QBA->QuickBarActivated(this, &QuickBars[Idx]);
				}
			}
			
			DeactivateSlotsOnBarInternal(InBarId);
			break;
		}
	}
}

void UArcQuickBarComponent::DeactivateSlotsOnBarInternal(const FGameplayTag& InBarId)
{
	int32 QuickBarIdx = QuickBars.IndexOfByKey(InBarId);
	
	for (int32 QuickSlotIdx = 0; QuickSlotIdx < QuickBars[QuickBarIdx].Slots.Num(); QuickSlotIdx++)
	{
		FArcQuickBarSlot& QBS = QuickBars[QuickBarIdx].Slots[QuickSlotIdx];

		ReplicatedSelectedSlots.DeactivateQuickSlot(InBarId, QBS.QuickBarSlotId);
		HandleSlotDeactivated(InBarId, QBS.QuickBarSlotId);
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

void UArcQuickBarComponent::ServerDeselectBar_Implementation(int8 BarIdx)
{
	for (FInstancedStruct& IS : QuickBars[BarIdx].QuickBarSelectedActions)
	{
		if (FArcQuickBarSelectedAction* QBA = IS.GetMutablePtr<FArcQuickBarSelectedAction>())
		{
			QBA->QuickBarActivated(this, &QuickBars[BarIdx]);
		}
	}
	
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

void UArcQuickBarComponent::HandleOnItemAddedToSlot(UArcItemsStoreComponent* InItemsStore
													, const FArcItemData* InItem
													, FGameplayTag BarId
													, FGameplayTag InQuickSlotId)
{
	if(InItemsStore->GetOwner() != GetOwner())
	{
		return;
	}

	AddAndActivateQuickSlot(BarId, InQuickSlotId, InItem->GetItemId());
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

	RemoveQuickSlot(BarId, InQuickSlotId);
}

void UArcQuickBarComponent::HandlePendingItemsWhenSlotChanges(UArcItemsStoreComponent* ItemSlotComponent
	, const FGameplayTag& ItemSlot
	, const FArcItemData* ItemData)
{
	ReplicatedSelectedSlots.HandlePendingAddedQuickSlots();
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

		const bool bValid = SlotComp->BP_GetItem(ItemId, OutItem);
		return bValid;
	}
	
	return false;
}
