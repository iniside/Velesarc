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

#include "ArcCoreGameplayTags.h"
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
#include "Items/ArcItemsHelpers.h"
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
	if (BarIdx == INDEX_NONE)
	{
		return nullptr;
	}
	const int32 QuickSlotIdx = QuickBars[BarIdx].Slots.IndexOfByKey(QuickBarSlotId);
	if (QuickSlotIdx == INDEX_NONE)
	{
		return nullptr;
	}
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
							FString Msg = FString::Printf(TEXT("QuickBar %s slot %s Is set to Cyclable but there is more than one slot with bAutoSelect set to true. "), *QuickBar.BarId.ToString(), *Slot.QuickBarSlotId.ToString());
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

	if (QuickBar->QuickSlotsMode == EArcQuickSlotsMode::AutoActivateOnly || QukcBarSlot.bAutoSelect)
	{
		HandleSlotActivated(BarId, QuickSlotId, ItemId);	
	}
		
	BroadcastSlotActivated(BarId, QuickSlotId, ItemData->GetItemId());

	MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, ReplicatedSelectedSlots, this);

	if (const FArcItemFragment_QuickBarItems* Fragment = ArcItemsHelper::FindFragment<FArcItemFragment_QuickBarItems>(ItemData))
	{
		int32 ChildBarIdx = QuickBars.IndexOfByPredicate([QuickSlotId](const FArcQuickBar& QuickBar)
			{
				if (QuickBar.ParentQuickSlot == QuickSlotId)
				{
					return true;
				}
				return false;
			});
		
		if (ChildBarIdx != INDEX_NONE)
		{
			const FArcQuickBar* ChildQuickBar = &QuickBars[ChildBarIdx];
			int32 Idx = 0;
			for (const FArcQuickBarItem& Item : Fragment->ItemsToAdd)
			{
				if (Idx >= ChildQuickBar->Slots.Num())
				{
					UE_LOG(LogArcQuickBar, Warning, TEXT("AddAndActivateQuickSlot: More items (%d) than child bar slots (%d)"), Fragment->ItemsToAdd.Num(), ChildQuickBar->Slots.Num());
					break;
				}

				FArcItemSpec Spec = FArcItemSpec::NewItem(Item.Item, 1, 1);
				FArcItemId Id = ItemsStoreComponent->AddItem(Spec, FArcItemId());
				ItemsStoreComponent->AddItemToSlot(Id, FArcCoreGameplayTags::Get().Item_SlotActive);

				AddAndActivateQuickSlot(ChildQuickBar->BarId, ChildQuickBar->Slots[Idx].QuickBarSlotId, Id);
				Idx++;
			}
		}
	}
}

void UArcQuickBarComponent::RemoveQuickSlot(const FGameplayTag& BarId, const FGameplayTag& QuickSlotId)
{
	UArcQuickBarSubsystem* QuickBarSubsystem = UArcQuickBarSubsystem::Get(this);

	FArcItemId ItemId = ReplicatedSelectedSlots.FindItemId(BarId, QuickSlotId);

	HandleSlotDeactivated(BarId, QuickSlotId);

	BroadcastSlotDeactivated(BarId, QuickSlotId, ItemId);

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
	int32 BarIdx = QuickBars.IndexOfByKey(BarId);
	int32 SlotIdx = (BarIdx != INDEX_NONE) ? QuickBars[BarIdx].Slots.IndexOfByKey(QuickSlotId) : INDEX_NONE;
	return ExecuteSlotActivation(BarIdx, SlotIdx, ItemId, true);
}

void UArcQuickBarComponent::HandleSlotDeactivated(const FGameplayTag& BarId, const FGameplayTag& QuickSlotId)
{
	int32 BarIdx = QuickBars.IndexOfByKey(BarId);
	int32 SlotIdx = (BarIdx != INDEX_NONE) ? QuickBars[BarIdx].Slots.IndexOfByKey(QuickSlotId) : INDEX_NONE;
	ExecuteSlotDeactivation(BarIdx, SlotIdx, true);
}

FGameplayTag UArcQuickBarComponent::FindQuickSlotForItem(const FArcItemId& InItemId)
{
	TPair<FGameplayTag, FGameplayTag> BarAndQuickSlot = ReplicatedSelectedSlots.FindItemBarAndSlot(InItemId);
	
	return BarAndQuickSlot.Value;
}

FGameplayTag UArcQuickBarComponent::FindSubQuickBarForSlot(const FGameplayTag& InQuickSlot)
{
	int32 BarIdx = QuickBars.IndexOfByPredicate([InQuickSlot](const FArcQuickBar& QuickBar)
		{
			if (QuickBar.ParentQuickSlot == InQuickSlot)
			{
				return true;
			}
			return false;
		});
	
	if (BarIdx != INDEX_NONE)
	{
		return QuickBars[BarIdx].BarId;
	}
	
	return FGameplayTag::EmptyTag;
}

void UArcQuickBarComponent::ActivateQuickBar(const FGameplayTag& InBarId)
{
	int32 BarIdx = QuickBars.IndexOfByKey(InBarId);
	
	for (const FArcQuickBarSlot& QuickSlot : QuickBars[BarIdx].Slots)
	{
		FArcItemId ItemId = GetItemId(InBarId, QuickSlot.QuickBarSlotId);
		HandleSlotActivated(InBarId, QuickSlot.QuickBarSlotId, ItemId);
		BroadcastSlotActivated(InBarId, QuickSlot.QuickBarSlotId, ItemId);
	}
}

void UArcQuickBarComponent::DeactivateQuickBar(const FGameplayTag& InBarId)
{
	int32 BarIdx = QuickBars.IndexOfByKey(InBarId);
	
	for (const FArcQuickBarSlot& QuickSlot : QuickBars[BarIdx].Slots)
	{
		FArcItemId ItemId = GetItemId(InBarId, QuickSlot.QuickBarSlotId);
		HandleSlotDeactivated(InBarId, QuickSlot.QuickBarSlotId);
		BroadcastSlotDeactivated(InBarId, QuickSlot.QuickBarSlotId, ItemId);
	}
}

FGameplayTag UArcQuickBarComponent::CycleSlotForward(const FGameplayTag& BarId
													 , const FGameplayTag& CurrentSlotId
													 , TFunction<bool(const FArcItemData*)> SlotValidCondition)
{
	return CycleSlotInternal(BarId, CurrentSlotId, 1, MoveTemp(SlotValidCondition));
}

FGameplayTag UArcQuickBarComponent::CycleSlotBackward(const FGameplayTag& BarId
													  , const FGameplayTag& CurrentSlotId
													  , TFunction<bool(const FArcItemData*)> SlotValidCondition)
{
	return CycleSlotInternal(BarId, CurrentSlotId, -1, MoveTemp(SlotValidCondition));
}

FGameplayTag UArcQuickBarComponent::CycleSlotInternal(const FGameplayTag& BarId
													  , const FGameplayTag& CurrentSlotId
													  , int32 Direction
													  , TFunction<bool(const FArcItemData*)> SlotValidCondition)
{
	int32 BarIdx = INDEX_NONE;
	if (!InternalCanCycle(BarId, BarIdx))
	{
		return FGameplayTag::EmptyTag;
	}

	int32 StartSlotIdx = QuickBars[BarIdx].Slots.IndexOfByKey(CurrentSlotId);
	int32 OldSlotIdx = StartSlotIdx;
	int32 NewSlotIdx = StartSlotIdx;

	UE_LOG(LogArcQuickBar
		, Log
		, TEXT("CycleSlot Direction=%d [SlotIdx %d]")
		, Direction
		, StartSlotIdx);

	if (InternalCycleSlot(StartSlotIdx, BarIdx, Direction, SlotValidCondition, NewSlotIdx))
	{
		return InternalSelectCycledSlot(BarId, BarIdx, OldSlotIdx, NewSlotIdx);
	}
	return FGameplayTag::EmptyTag;
}

void UArcQuickBarComponent::DeselectCurrentQuickSlot(FGameplayTag InBarId)
{
	FGameplayTag SelectedSlot = GetFirstActiveSlot(InBarId);
	if (!SelectedSlot.IsValid())
	{
		return;
	}

	int32 BarIdx = QuickBars.IndexOfByKey(InBarId);
	int32 OldSlotIdx = QuickBars[BarIdx].Slots.IndexOfByKey(SelectedSlot);

	FArcItemId ItemId = ReplicatedSelectedSlots.FindItemId(InBarId, SelectedSlot);
	FArcCycledSlotRPCData RPCData;

	if (ExecuteSlotDeactivation(BarIdx, OldSlotIdx, true))
	{
		RPCData.DeactivateBarIdx = BarIdx;
		RPCData.DeactivateSlotIdx = OldSlotIdx;

		BroadcastSlotDeactivated(InBarId, SelectedSlot, ItemId);

		UArcQuickBarSubsystem* Sub = UArcQuickBarSubsystem::Get(this);
		Sub->OnQuickSlotDeselected.Broadcast(this, InBarId, SelectedSlot, ItemId);
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
	, const int32 NewSlotIdx)
{
	if (GetOwnerRole() < ENetRole::ROLE_Authority)
	{
		LockSlotCycle = 1;
	}

	FArcCycledSlotRPCData RPCData;
	FGameplayTag NewQuickSlotId;

	// Deactivate old slot
	if (OldSlotIdx != INDEX_NONE)
	{
		FGameplayTag OldSlotTag = QuickBars[BarIdx].Slots[OldSlotIdx].QuickBarSlotId;
		FArcItemId OldItemId = ReplicatedSelectedSlots.FindItemId(BarId, OldSlotTag);

		if (ExecuteSlotDeactivation(BarIdx, OldSlotIdx, true))
		{
			RPCData.DeactivateBarIdx = BarIdx;
			RPCData.DeactivateSlotIdx = OldSlotIdx;
			BroadcastSlotDeactivated(BarId, OldSlotTag, OldItemId);
		}
	}

	// Activate new slot
	if (NewSlotIdx != INDEX_NONE)
	{
		NewQuickSlotId = QuickBars[BarIdx].Slots[NewSlotIdx].QuickBarSlotId;
		FArcItemId NewItemId = ReplicatedSelectedSlots.FindItemId(BarId, NewQuickSlotId);

		if (ExecuteSlotActivation(BarIdx, NewSlotIdx, NewItemId, true))
		{
			RPCData.ActivateBarIdx = BarIdx;
			RPCData.ActivateSlotIdx = NewSlotIdx;

			// Cycle-specific broadcasts (not part of core activate)
			const FArcItemData* NewData = FindQuickSlotItem(BarId, NewQuickSlotId);
			if (NewData)
			{
				UArcQuickBarSubsystem* Sub = UArcQuickBarSubsystem::Get(this);
				Sub->BroadcastActorOnQuickSlotCycled(GetOwner(), this, BarId, NewQuickSlotId, NewData->GetItemId());
				Sub->OnQuickSlotSelected.Broadcast(this, BarId, NewQuickSlotId, NewData->GetItemId());
			}
		}
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
		NewSlotIdx = ((NewSlotIdx + Direction) % SlotNum + SlotNum) % SlotNum;

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
	// Deactivate old slot on server (no input unbinding — input is client-only)
	if (RPCData.DeactivateBarIdx >= 0 && RPCData.DeactivateSlotIdx >= 0)
	{
		FGameplayTag OldBarId = QuickBars[RPCData.DeactivateBarIdx].BarId;
		FGameplayTag OldSlotId = QuickBars[RPCData.DeactivateBarIdx].Slots[RPCData.DeactivateSlotIdx].QuickBarSlotId;
		FArcItemId OldItemId = ReplicatedSelectedSlots.FindItemId(OldBarId, OldSlotId);

		ExecuteSlotDeactivation(RPCData.DeactivateBarIdx, RPCData.DeactivateSlotIdx, false);
		BroadcastSlotDeactivated(OldBarId, OldSlotId, OldItemId);
	}

	// Activate new slot on server (no input binding — input is client-only)
	if (RPCData.ActivateBarIdx >= 0 && RPCData.ActivateSlotIdx >= 0)
	{
		FGameplayTag BarId = QuickBars[RPCData.ActivateBarIdx].BarId;
		FGameplayTag SlotId = QuickBars[RPCData.ActivateBarIdx].Slots[RPCData.ActivateSlotIdx].QuickBarSlotId;
		FArcItemId ItemId = ReplicatedSelectedSlots.FindItemId(BarId, SlotId);

		ExecuteSlotActivation(RPCData.ActivateBarIdx, RPCData.ActivateSlotIdx, ItemId, false);
		BroadcastSlotActivated(BarId, SlotId, ItemId);
	}

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

bool UArcQuickBarComponent::ExecuteSlotActivation(int32 BarIdx, int32 SlotIdx
	, const FArcItemId& ItemId, bool bBindInput)
{
	if (BarIdx == INDEX_NONE || SlotIdx == INDEX_NONE)
	{
		return false;
	}

	const FArcQuickBar* QB = &QuickBars[BarIdx];
	const FArcQuickBarSlot* QS = &QB->Slots[SlotIdx];

	UArcItemsStoreComponent* Store = GetItemStoreComponent(QB->BarId);
	const FArcItemData* Data = Store ? Store->GetItemPtr(ItemId) : nullptr;
	if (!Data)
	{
		return false;
	}

	UArcCoreAbilitySystemComponent* ASC = GetOwner()->FindComponentByClass<UArcCoreAbilitySystemComponent>();

	for (const FInstancedStruct& IS : QS->SelectedHandlers)
	{
		IS.GetPtr<FArcQuickSlotHandler>()->OnSlotSelected(ASC, this, Data, QB, QS);
	}

	if (bBindInput)
	{
		InternalTryBindInput(BarIdx, SlotIdx, ASC);
	}

	ReplicatedSelectedSlots.ActivateQuickSlot(QB->BarId, QS->QuickBarSlotId);
	MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, ReplicatedSelectedSlots, this);
	return true;
}

bool UArcQuickBarComponent::ExecuteSlotDeactivation(int32 BarIdx, int32 SlotIdx, bool bUnbindInput)
{
	if (BarIdx == INDEX_NONE || SlotIdx == INDEX_NONE)
	{
		return false;
	}

	const FArcQuickBar* QB = &QuickBars[BarIdx];
	const FArcQuickBarSlot* QS = &QB->Slots[SlotIdx];

	const FArcItemData* Data = FindQuickSlotItem(QB->BarId, QS->QuickBarSlotId);
	if (!Data)
	{
		return false;
	}

	UArcCoreAbilitySystemComponent* ASC = GetOwner()->FindComponentByClass<UArcCoreAbilitySystemComponent>();

	for (const FInstancedStruct& IS : QS->SelectedHandlers)
	{
		IS.GetPtr<FArcQuickSlotHandler>()->OnSlotDeselected(ASC, this, Data, QB, QS);
	}

	if (bUnbindInput)
	{
		InternalTryUnbindInput(BarIdx, SlotIdx, ASC);
	}

	ReplicatedSelectedSlots.DeactivateQuickSlot(QB->BarId, QS->QuickBarSlotId);
	MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, ReplicatedSelectedSlots, this);
	return true;
}

void UArcQuickBarComponent::BroadcastSlotActivated(const FGameplayTag& BarId
	, const FGameplayTag& QuickSlotId
	, const FArcItemId& ItemId)
{
	UArcQuickBarSubsystem* Sub = UArcQuickBarSubsystem::Get(this);
	Sub->BroadcastActorOnQuickSlotActivated(GetOwner(), this, BarId, QuickSlotId, ItemId);
	Sub->OnQuickSlotActivated.Broadcast(this, BarId, QuickSlotId, ItemId);
}

void UArcQuickBarComponent::BroadcastSlotDeactivated(const FGameplayTag& BarId
	, const FGameplayTag& QuickSlotId
	, const FArcItemId& ItemId)
{
	UArcQuickBarSubsystem* Sub = UArcQuickBarSubsystem::Get(this);
	Sub->BroadcastActorOnQuickSlotDeactivated(GetOwner(), this, BarId, QuickSlotId, ItemId);
	Sub->OnQuickSlotDeactivated.Broadcast(this, BarId, QuickSlotId, ItemId);
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

		if (Data)
		{
			TArray<const FArcItemData*> Extensions = SlotComp->GetItemsAttachedTo(Data->GetItemId());
			Extensions.Add(Data);
			return Handling->OnAddedToQuickBar(InArcASC, Extensions);	
		}
		
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
		if (Data)
		{
			TArray<const FArcItemData*> Extensions = SlotComp->GetItemsAttachedTo(Data->GetItemId());
			Extensions.Add(Data);
			return Handling->OnRemovedFromQuickBar(InArcASC, Extensions);
		}
	}

	return true;
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
	int32 BarIdx = QuickBars.IndexOfByKey(InBarId);

	for (int32 i = 0; i < QuickBars[BarIdx].Slots.Num(); i++)
	{
		FArcItemId ItemId = ReplicatedSelectedSlots.FindItemId(InBarId, QuickBars[BarIdx].Slots[i].QuickBarSlotId);
		ExecuteSlotActivation(BarIdx, i, ItemId, true);
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
	
	int32 BarIdx = QuickBars.IndexOfByKey(InBarId);
	if (BarIdx == INDEX_NONE)
	{
		UE_LOG(LogArcQuickBar, Warning, TEXT("UArcQuickBarComponent::DeactivateBar BarId %s not found"), *InBarId.ToString());
		return;
	}

	ServerDeselectBar(BarIdx);
	for (FInstancedStruct& IS : QuickBars[BarIdx].QuickBarSelectedActions)
	{
		if (FArcQuickBarSelectedAction* QBA = IS.GetMutablePtr<FArcQuickBarSelectedAction>())
		{
			QBA->QuickBarDeactivated(this, &QuickBars[BarIdx]);
		}
	}

	DeactivateSlotsOnBarInternal(InBarId);
}

void UArcQuickBarComponent::DeactivateSlotsOnBarInternal(const FGameplayTag& InBarId)
{
	int32 BarIdx = QuickBars.IndexOfByKey(InBarId);

	for (int32 i = 0; i < QuickBars[BarIdx].Slots.Num(); i++)
	{
		ExecuteSlotDeactivation(BarIdx, i, true);
	}
}

FGameplayTag UArcQuickBarComponent::CycleBarForward(const FGameplayTag& InOldBarId)
{
	int32 StartBarIdx = QuickBars.IndexOfByKey(InOldBarId);
	int32 NewBarIdx = StartBarIdx;
	
	const int32 BarsNum = QuickBars.Num();

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
			QBA->QuickBarDeactivated(this, &QuickBars[BarIdx]);
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

const UArcItemDefinition* UArcQuickBarComponent::GetItemDefinition(const FGameplayTag& InBarId, const FGameplayTag& InQuickSlotId) const
{
	FArcItemId ItemId = ReplicatedSelectedSlots.FindItemId(InBarId, InQuickSlotId);
	
	UArcItemsStoreComponent* ItemsStore = GetItemStoreComponent(InBarId);
	if (!ItemsStore)
	{
		return nullptr;
	}
	
	FArcItemData* ItemData = ItemsStore->GetItemPtr(ItemId);
	
	if (!ItemData)
	{
		return nullptr;
	}
	
	return ItemData->GetItemDefinition();
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
