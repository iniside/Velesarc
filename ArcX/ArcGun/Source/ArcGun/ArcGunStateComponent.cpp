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

#include "ArcGunStateComponent.h"

#include "ArcAnimNotify_Unholster.h"
#include "ArcGunActor.h"


#include "ArcGunFireMode.h"
#include "ArcGunInstanceBase.h"
#include "ArcGunItemInstance.h"
#include "ArcGunRecoilInstance.h"
#include "ArcWorldDelegates.h"
#include "DisplayDebugHelpers.h"
#include "Camera/ArcPlayerCameraManager.h"
#include "Camera/CameraComponent.h"
#include "Components/GameFrameworkComponentManager.h"
#include "Engine/Canvas.h"
#include "Equipment/ArcItemAttachmentComponent.h"
#include "Fragments/ArcItemFragment_GunStats.h"
#include "Player/ArcCorePlayerController.h"
#include "Player/ArcPlayerStateExtensionComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/HUD.h"
#include "Pawn/ArcPawnExtensionComponent.h"

#include "Items/ArcItemDefinition.h"
#include "Items/ArcItemData.h"

#include "Items/ArcItemsComponent.h"
#include "Items/ArcItemsSubsystem.h"
#include "Net/UnrealNetwork.h"
#include "QuickBar/ArcQuickBarComponent.h"

#include "Items/ArcItemDefinition.h"
#include "Items/ArcItemsHelpers.h"

#include "Equipment/ArcItemAttachmentComponent.h"
#include "Items/Fragments/ArcItemFragment_AbilityEffectsToApply.h"

#include "ArcCoreUtils.h"
#include "AbilitySystem/ArcCoreAbilitySystemComponent.h"
#include "AbilitySystem/ArcCoreGameplayAbility.h"
#include "AbilitySystem/ArcTargetingBPF.h"
#include "AbilitySystem/Targeting/ArcTargetingObject.h"
#include "GameFramework/GameplayControlRotationComponent.h"
#include "Targeting/ArcTargetingSourceContext.h"
#include "TargetingSystem/TargetingSubsystem.h"

DEFINE_LOG_CATEGORY(LogArcGunStateComponent);

const FName UArcGunStateComponent::NAME_GunComponentReady = TEXT("GunComponentReady");

void UArcGunStateComponent::OnRep_SelectedGun(const FArcSelectedGun& Old)
{
	if (SelectedGun.WeaponItemHandle.IsValid() == false)
	{
		SelectedGunRecoil.Reset();
		SelectedGunFireMode.Reset();
		SelectedGunInstance.Reset();
		return;
	}

	// TODO:: This is not gonna work on sim proxies (??)
	// But I don't need item, just item def.
	// Or I can replicate entire ItemPtr.
	const FArcItemData* ItemData = GetItemsComponent()->GetItemPtr(SelectedGun.WeaponItemHandle);

	if (SelectedGun.WeaponItemDef == nullptr)
	{
		SelectedGunRecoil.Reset();
		SelectedGunFireMode.Reset();
		SelectedGunInstance.Reset();
		return;	
	}
	
	if (SelectedGunFireMode.IsValid() == true)
	{
		SelectedGunFireMode.Reset();
	}

	const FArcItemFragment_GunFireMode* FireMode = SelectedGun.WeaponItemDef->FindFragment<FArcItemFragment_GunFireMode>();
	if (FireMode != nullptr)
	{
		// TODO Move it to SelectedGun.
		SelectedGunFireMode = FireMode->FireModeAsset->FireMode;
	}

	if (SelectedGunInstance.IsValid() == true)
	{
		SelectedGunInstance.Reset();
	}

	const FArcItemFragment_GunInstancePreset* GunInstance = SelectedGun.WeaponItemDef->FindFragment<FArcItemFragment_GunInstancePreset>();
	if (GunInstance != nullptr)
	{
		SelectedGunInstance = GunInstance->InstanceAsset->GunInstance;
	}
	
	UArcItemAttachmentComponent* IAC = GetOwner()->FindComponentByClass<UArcItemAttachmentComponent>();
	
	SelectedGun.GunActor = Cast<AArcGunActor>(IAC->GetAttachedActor(SelectedGun.WeaponItemHandle));
	if (SelectedGun.GunActor.IsValid())
	{
		SelectedGun.GunActor->Initialize(SelectedGun.WeaponItemHandle);
	}
}

void UArcGunStateComponent::OnRep_GunState(const FArcGunState& OldWeaponState)
{
	if(GunState.bIsFiring)
	{
		OnStartFireDelegate.Broadcast(SelectedGun);
		if (FArcGunFireMode* FireMode = SelectedGunFireMode.GetMutablePtr<FArcGunFireMode>())
		{
			const bool bPreFired = FireMode->FireStart(GunState.RequestingAbility, this);
			if (bPreFired)
			{
				PreFire();
			}
		}
	}
	else
	{
		OnStopFireDelegate.Broadcast(SelectedGun);
		if (FArcGunFireMode* FireMode = SelectedGunFireMode.GetMutablePtr<FArcGunFireMode>())
		{
			FireMode->FireStop(GunState.RequestingAbility, this);
		}
	}
}

void UArcGunStateComponent::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	FDoRepLifetimeParams SharedParams;
	SharedParams.bIsPushBased = true;
	SharedParams.RepNotifyCondition = ELifetimeRepNotifyCondition::REPNOTIFY_Always;
	//
	SharedParams.Condition = COND_SkipOwner;
	DOREPLIFETIME_WITH_PARAMS_FAST(UArcGunStateComponent, GunState, SharedParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(UArcGunStateComponent, SelectedGun, SharedParams);
	
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
}

// Sets default values for this component's properties
UArcGunStateComponent::UArcGunStateComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	//PrimaryComponentTick.TickInterval = 0.033f;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;

	
	bWantsInitializeComponent = true;
	
	// ...
}

UArcItemsStoreComponent* UArcGunStateComponent::GetItemsComponent() const
{
	if (ItemsComponent == nullptr)
	{
		ItemsComponent = Arcx::Utils::GetComponent(GetOwner(), ItemSlotComponentClass);
	}

	return ItemsComponent;
}

// Called when the game starts
void UArcGunStateComponent::BeginPlay()
{
	Super::BeginPlay();
	
	SelectedGun.WCOwner = this;

	UArcWorldDelegates::Get(this)->BroadcastActorOnComponentBeginPlay(
		FArcComponentChannelKey(GetOwner(), UArcGunStateComponent::StaticClass())
		, this);
}

void UArcGunStateComponent::InitializeComponent()
{
	//This component should be on PlayerState, but it's possible for it to end up on Pawn.
	if (UArcPlayerStateExtensionComponent* PSExt = UArcPlayerStateExtensionComponent::Get(GetOwner()))
	{
		PSExt->AddOnPlayerStateReady(FSimpleMulticastDelegate::FDelegate::CreateUObject(
			this, &ThisClass::HandleComponentReady
		));
	}
	else if (UArcPawnExtensionComponent* PawnExtComp = UArcPawnExtensionComponent::FindPawnExtensionComponent(GetOwner()))
	{
		PawnExtComp->OnPawnReadyToInitialize_RegisterAndCall(FSimpleMulticastDelegate::FDelegate::CreateUObject(
			this, &ThisClass::HandleComponentReady));
	}
	
	HandleComponentReady();
	
	Super::InitializeComponent();

	if (Arcx::Utils::IsPlayableWorld(GetWorld()))
	{
		UArcItemsSubsystem* ItemsSubsystem = UArcItemsSubsystem::Get(this);

		if (ItemsSubsystem)
		{
			ItemsSubsystem->AddActorOnRemovedFromSlot(GetOwner(), FArcGenericItemSlotDelegate::FDelegate::CreateUObject(
				this, &UArcGunStateComponent::HandleOnItemRemovedFromSlot
				));
		
			ItemsSubsystem->AddActorOnAddedToSocket(GetOwner(), FArcGenericItemSocketSlotDelegate::FDelegate::CreateUObject(
				this, &UArcGunStateComponent::HandleOnItemAddedToSocket));

			ItemsSubsystem->AddActorOnAddedToSlot(GetOwner(), FArcGenericItemSlotDelegate::FDelegate::CreateUObject(
				this, &UArcGunStateComponent::HandleOnItemAddedToSlot));
		}
	}
}

void UArcGunStateComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	if (!CharacterOwner)
	{
		return;
	}
	
	if (ArcPC && !CameraManager)
	{
		CameraManager = Cast<APlayerCameraManager>(ArcPC->PlayerCameraManager);
		if (CameraManager)
		{
			CameraManager->PrimaryActorTick.AddPrerequisite(this, PrimaryComponentTick);
		}
	}
	
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	
	UTargetingSubsystem* Targeting = UTargetingSubsystem::Get(GetWorld());

	FArcTargetingSourceContext Context;
	Context.InstigatorActor = GetOwner();
	Context.SourceActor = CharacterOwner;
	Context.SourceObject = GetOwner();
	
	if (Targeting && CameraAimTargetingPreset)
	{
		FTargetingRequestDelegate CompletionDelegate = FTargetingRequestDelegate::CreateUObject(this, &UArcGunStateComponent::HandleCameraAimTargetingCompleted);
		CameraAimTargetingHandle = Arcx::MakeTargetRequestHandle(CameraAimTargetingPreset, Context);
		Targeting->ExecuteTargetingRequestWithHandle(CameraAimTargetingHandle, CompletionDelegate);		
	}
	
	if (GunState.TargetingObject.Get() && SelectedGun.WeaponItemDef)
	{
		const ArcGunStats* WeaponStats = SelectedGun.WeaponItemDef->GetScalableFloatFragment<ArcGunStats>();
		
		const float FixedUpdateRate = WeaponStats->RecoilUpdateRate;

		// Add the current frame's delta time to our accumulator
		TimeAccumulator += DeltaTime;

		// Process as many fixed time steps as we've accumulated
		//while (TimeAccumulator >= FixedUpdateRate)
		{
			FArcGunRecoilInstance* RecoilInstance = SelectedGunRecoil.GetMutablePtr<FArcGunRecoilInstance>();
			if (RecoilInstance != nullptr)
			{
				RecoilInstance->UpdateRecoil(FixedUpdateRate, this);
			}
		
			TimeAccumulator -= FixedUpdateRate;
			TimeAccumulator += 0.001;
		}

		if (Targeting)
		{
			FTargetingRequestDelegate CompletionDelegate = FTargetingRequestDelegate::CreateUObject(this, &UArcGunStateComponent::HandleTargetingCompleted);
			TargetingHandle = Arcx::MakeTargetRequestHandle(GunState.TargetingObject->TargetingPreset, Context);
			Targeting->ExecuteTargetingRequestWithHandle(TargetingHandle, CompletionDelegate);		
		}
	}

	FVector StartLocation = GetGunAimPoint();
	FVector EndLocation = CameraAimHitResult.bBlockingHit ? CameraAimHitResult.ImpactPoint : CameraAimHitResult.TraceEnd;
	FVector NewDirection = (EndLocation - StartLocation).GetSafeNormal();

	if (AimInterpolationSpeed > 0)
	{
		AimDirection = FMath::VInterpConstantTo(AimDirection, NewDirection, DeltaTime, AimInterpolationSpeed);
	}
	else
	{
		AimDirection = NewDirection;
	}
	
	FArcGunFireMode* FireMode = SelectedGunFireMode.GetMutablePtr<FArcGunFireMode>();
	if (FireMode != nullptr && GunState.bIsFiring == true)
	{
		const bool bPreFired = FireMode->FireUpdate(GunState.RequestingAbility, this, DeltaTime);
		if (bPreFired)
		{
			PreFire();
		}
	}
}

void UArcGunStateComponent::DrawDebug()
{
	UWorld* World = GetWorld();
	const FVector AimStart = GetGunAimPoint();
	
	DrawDebugPoint(World, AimStart, 25.f, FColor::Red);

	FVector EndLocation = CameraAimHitResult.bBlockingHit ? CameraAimHitResult.ImpactPoint : CameraAimHitResult.TraceEnd;
	FVector NewDirection = (EndLocation - AimStart).GetSafeNormal();
	FVector DirectAimEnd = AimStart + NewDirection * 600.f;
	
	DrawDebugDirectionalArrow(World, AimStart, DirectAimEnd, 10.f, FColor::Red);

	FVector InterpolatedEnd = AimStart + (AimDirection * 600.f);
	DrawDebugDirectionalArrow(World, AimStart, InterpolatedEnd, 10.f, FColor::Green);
}

void UArcGunStateComponent::HandleCameraAimTargetingCompleted(FTargetingRequestHandle TargetingRequestHandle)
{
	UTargetingSubsystem* Targeting = UTargetingSubsystem::Get(GetWorld());
	if (Targeting == nullptr)
	{
		return;
	}

	FTargetingDefaultResultsSet& TargetingResults = FTargetingDefaultResultsSet::FindOrAdd(TargetingRequestHandle);

	if (TargetingResults.TargetResults.Num() == 1)
	{
		CameraAimHitResult = TargetingResults.TargetResults[0].HitResult;
	}
}

void UArcGunStateComponent::HandleTargetingCompleted(FTargetingRequestHandle TargetingRequestHandle)
{
	UTargetingSubsystem* Targeting = UTargetingSubsystem::Get(GetWorld());
	if (Targeting == nullptr)
	{
		return;
	}

	FTargetingDefaultResultsSet& TargetingResults = FTargetingDefaultResultsSet::FindOrAdd(TargetingRequestHandle);

	if (TargetingResults.TargetResults.Num() == 1)
	{
		CurrentHitResult = TargetingResults.TargetResults[0].HitResult;
		OnTargetingResultDelegate.Broadcast(CurrentHitResult);
	}
}
void UArcGunStateComponent::PreFire()
{
	FArcGunShotInfo ShotInfo;
	ShotInfo.GunStateComponent = this;
	ShotInfo.Ability = GunState.SourceAbility.Get();
	ShotInfo.CurrentGun = SelectedGun;
	ShotInfo.Targets = {CurrentHitResult};
	ShotInfo.TargetingObject = GunState.TargetingObject;
	ShotInfo.TargetRequestHandle = GunState.RequestingAbility;
	ShotInfo.bEnded = false;
	ShotInfo.TimeHeld = 0;
	
	BroadcastOnPreShoot(ShotInfo);
}
void UArcGunStateComponent::HandleComponentReady()
{
	ENetMode NetMode = GetNetMode();
	if (NetMode != ENetMode::NM_DedicatedServer)
	{
		if (UArcPlayerStateExtensionComponent* PSExt = UArcPlayerStateExtensionComponent::Get(GetOwner()))
		{
			CharacterOwner = PSExt->GetPawn<ACharacter>();
			if (CharacterOwner != nullptr)
			{
				ArcPC = CharacterOwner->GetController<AArcCorePlayerController>();
			}
			
			if (ArcASC == nullptr)
			{
				ArcASC = PSExt->GetOwner()->FindComponentByClass<UArcCoreAbilitySystemComponent>();
			}
			
		}
		else if (UArcPawnExtensionComponent* PawnExtComp = UArcPawnExtensionComponent::FindPawnExtensionComponent(GetOwner()))
		{
			CharacterOwner = PawnExtComp->GetPawn<ACharacter>();
			if (CharacterOwner != nullptr)
			{
				ArcPC = CharacterOwner->GetController<AArcCorePlayerController>();
			}
		}
		
		if(ArcPC != nullptr)
		{
			Camera = CharacterOwner->FindComponentByClass<UCameraComponent>();
			
			PrimaryComponentTick.bCanEverTick = true;
			//PrimaryComponentTick.TickInterval = 0.033f;
			
			//ArcPC->PrimaryActorTick.AddPrerequisite(this, PrimaryComponentTick);
			//PrimaryComponentTick.AddPrerequisite(this, ArcPC->PrimaryActorTick);
			PrimaryComponentTick.SetTickFunctionEnable(true);
			
			SetComponentTickEnabled(true);
		}
		else
		{
			PrimaryComponentTick.bCanEverTick = true;
			//PrimaryComponentTick.TickInterval = 0.033f;
			PrimaryComponentTick.SetTickFunctionEnable(true);
			
			SetComponentTickEnabled(true);
		}


		
		if(CharacterOwner)
		{
			UArcPawnExtensionComponent* PawnExtComp = UArcPawnExtensionComponent::FindPawnExtensionComponent(CharacterOwner);
			ArcASC = PawnExtComp->GetArcAbilitySystemComponent();
			
			//EndPhysicsTickFunction.AddPrerequisite(this, );
		}
	}

	UGameFrameworkComponentManager::SendGameFrameworkComponentExtensionEvent(GetOwner(), UArcGunStateComponent::NAME_GunComponentReady);
	OnWeaponComponentReady();
}

void UArcGunStateComponent::HandleQuickSlotSelected(FGameplayTag BarId, FGameplayTag QuickSlotId)
{
	UArcQuickBarComponent* QuickBar = UArcQuickBarComponent::GetQuickBar(GetOwner());

	FArcItemId ItemId = QuickBar->GetItemId(BarId, QuickSlotId);
	const FArcItemData* ItemData = QuickBar->FindQuickSlotItem(BarId, QuickSlotId);	

	FArcGunFireMode* CurrentFireMode = SelectedGunFireMode.GetMutablePtr<FArcGunFireMode>();

	if (CurrentFireMode != nullptr && GunState.bIsFiring == true)
	{
		StopFire();
	}
	
	// No matter if we go further reset everything.
	SelectedGun.Reset();
	SelectedGunFireMode.Reset();
	SelectedGunInstance.Reset();
	if (SelectedGunRecoil.IsValid())
	{
		SelectedGunRecoil.GetMutablePtr<FArcGunRecoilInstance>()->Deinitialize();
	}
	SelectedGunRecoil.Reset();
	
	if(ItemId.IsValid() == false)
	{
		return;
	}

	if (ItemData == nullptr)
	{
		return;
	}

	const FArcItemFragment_GunFireMode* FireMode = ArcItems::FindFragment<FArcItemFragment_GunFireMode>(ItemData);
	const FArcItemFragment_GunInstancePreset* GunInstance = ArcItems::FindFragment<FArcItemFragment_GunInstancePreset>(ItemData);
	const FArcItemFragment_GunRecoilPreset* GunRecoil = ArcItems::FindFragment<FArcItemFragment_GunRecoilPreset>(ItemData);

	if (FireMode == nullptr || GunInstance == nullptr || GunRecoil == nullptr)
	{
		MARK_PROPERTY_DIRTY_FROM_NAME(UArcGunStateComponent, SelectedGun, this);
		return;
	}
	
	SelectedGunFireMode = FireMode->FireModeAsset->FireMode;
	
	SelectedGunInstance = GunInstance->InstanceAsset->GunInstance;
	SelectedGunInstance.GetMutablePtr<FArcGunInstanceBase>()->Initialize(this);

	SelectedGunRecoil = GunRecoil->InstancePreset->RecoilInstance;

	SelectedGun.QuickBarId = BarId;
	SelectedGun.QuickSlotId = QuickSlotId;
	
	SelectedGun.GunItemPtr = ItemData;
	SelectedGun.WeaponItemHandle = ItemId;
	SelectedGun.SetWeaponItemPtr(ItemData);
	SelectedGun.Slot = ItemData->GetSlotId();
	SelectedGun.WeaponItemDef = ItemData->GetItemDefinition();
	
	UArcItemAttachmentComponent* IAC = GetOwner()->FindComponentByClass<UArcItemAttachmentComponent>();
	
	SelectedGun.GunActor = Cast<AArcGunActor>(IAC->GetAttachedActor(ItemId));
	if (SelectedGun.GunActor.IsValid())
	{
		SelectedGun.GunActor->Initialize(SelectedGun.WeaponItemHandle);
	}
	
	OnGunSelectedDelegate.Broadcast(SelectedGun);
	
	MARK_PROPERTY_DIRTY_FROM_NAME(UArcGunStateComponent, SelectedGun, this);
}

void UArcGunStateComponent::HandleQuickSlotDeselected(FGameplayTag BarId
	, FGameplayTag QuickSlotId)
{
	UArcQuickBarComponent* QuickBar = UArcQuickBarComponent::GetQuickBar(GetOwner());
	FArcItemId OldItemId = QuickBar->GetItemId(BarId, QuickSlotId);
	const FArcItemData* OldItemData = QuickBar->FindQuickSlotItem(BarId, QuickSlotId);	

	if(OldItemId.IsValid() == false)
	{
		return;
	}

	if (OldItemData != nullptr)
	{
		OldSelectedGun.WeaponItemHandle = OldItemData->GetItemId();
		OldSelectedGun.SetWeaponItemPtr(OldItemData);
		OldSelectedGun.Slot = OldItemData->GetSlotId();
		OldSelectedGun.WeaponItemDef = OldItemData->GetItemDefinition();
	}

	// If somehow we already set new gun, don't reset.
	if (SelectedGun.WeaponItemHandle == OldItemId)
	{
		SelectedGun.Reset();	
	}
}

void UArcGunStateComponent::HandleOnItemRemovedFromSlot(UArcItemsStoreComponent* ItemSlotComponent
	, const FGameplayTag& ItemSlot
	, const FArcItemData* ItemData)
{
	if (ItemSlotComponent->GetClass()->IsChildOf(ItemSlotComponentClass) == false)
	{
		return;
	}

	FArcGunFireMode* CurrentFireMode = SelectedGunFireMode.GetMutablePtr<FArcGunFireMode>();

	if (CurrentFireMode != nullptr && GunState.bIsFiring == true)
	{
		StopFire();
	}
	
	if (SelectedGun.WeaponItemHandle == ItemData->GetItemId())
	{
		OldSelectedGun = SelectedGun;
		
		SelectedGun.Reset();
		SelectedGun.ForceUpdate++;
		
		MARK_PROPERTY_DIRTY_FROM_NAME(UArcGunStateComponent, SelectedGun, this);
		
		SelectedGunInstance.Reset();
		SelectedGunFireMode.Reset();
		SelectedGunRecoil.Reset();
	}
}

void UArcGunStateComponent::HandleOnItemAddedToSlot(UArcItemsStoreComponent* ItemSlotComponent
	, const FGameplayTag& ItemSlot
	, const FArcItemData* ItemData)
{
	if (OldSelectedGun.IsValid() == false)
	{
		return;
	}

	if (OldSelectedGun.Slot != ItemSlot)
	{
		return;
	}

	FArcGunFireMode* CurrentFireMode = SelectedGunFireMode.GetMutablePtr<FArcGunFireMode>();

	if (CurrentFireMode != nullptr && GunState.bIsFiring == true)
	{
		StopFire();
	}
	
	OldSelectedGun.WeaponItemHandle = SelectedGun.WeaponItemHandle;
	OldSelectedGun.SetWeaponItemPtr(SelectedGun.GunItemPtr);
	OldSelectedGun.Slot = SelectedGun.Slot;
	
	// No matter if we go further reset everything.
	SelectedGun.Reset();
	SelectedGunFireMode.Reset();
	SelectedGunInstance.Reset();
	if (SelectedGunRecoil.IsValid())
	{
		SelectedGunRecoil.GetMutablePtr<FArcGunRecoilInstance>()->Deinitialize();
	}
	SelectedGunRecoil.Reset();
	
	if(ItemData->GetItemId().IsValid() == false)
	{
		return;
	}

	if (ItemData == nullptr)
	{
		return;
	}

	const FArcItemFragment_GunFireMode* FireMode = ArcItems::FindFragment<FArcItemFragment_GunFireMode>(ItemData);
	const FArcItemFragment_GunInstancePreset* GunInstance = ArcItems::FindFragment<FArcItemFragment_GunInstancePreset>(ItemData);
	const FArcItemFragment_GunRecoilPreset* GunRecoil = ArcItems::FindFragment<FArcItemFragment_GunRecoilPreset>(ItemData);

	if (FireMode == nullptr || GunInstance == nullptr || GunRecoil == nullptr)
	{
		MARK_PROPERTY_DIRTY_FROM_NAME(UArcGunStateComponent, SelectedGun, this);
		return;
	}
	
	SelectedGunFireMode = FireMode->FireModeAsset->FireMode;
	
	SelectedGunInstance = GunInstance->InstanceAsset->GunInstance;
	SelectedGunInstance.GetMutablePtr<FArcGunInstanceBase>()->Initialize(this);

	SelectedGunRecoil = GunRecoil->InstancePreset->RecoilInstance;
	
	SelectedGun.GunItemPtr = ItemData;
	SelectedGun.WeaponItemHandle = ItemData->GetItemId();
	SelectedGun.SetWeaponItemPtr(ItemData);
	SelectedGun.Slot = ItemData->GetSlotId();

	UArcItemAttachmentComponent* IAC = GetOwner()->FindComponentByClass<UArcItemAttachmentComponent>();
	
	SelectedGun.GunActor = Cast<AArcGunActor>(IAC->GetAttachedActor(ItemData->GetItemId()));
	if (SelectedGun.GunActor.IsValid())
	{
		SelectedGun.GunActor->Initialize(SelectedGun.WeaponItemHandle);
	}

	const FArcItemFragment_GunUnholster* WeaponAttachment = ItemData->GetItemDefinition()->FindFragment<FArcItemFragment_GunUnholster>();
	if(WeaponAttachment == nullptr)
	{
		//UE_LOG(LogArcWeaponComponent, Log, TEXT("AttachUnholster - Weapon attachment is invalid"));
		return;
	}

	IAC->AttachItemToSocket(ItemData->GetItemId(), WeaponAttachment->UnholsterSocket, NAME_None, WeaponAttachment->Transform);
	IAC->LinkAnimLayer(ItemData->GetItemId());
	
	OnGunSelectedDelegate.Broadcast(SelectedGun);

	UArcQuickBarComponent* QuickBar = UArcQuickBarComponent::GetQuickBar(GetOwner());
	//QuickBar->SetBarSlotDeselected(SelectedGun.QuickBarId, SelectedGun.QuickSlotId);
	
	MARK_PROPERTY_DIRTY_FROM_NAME(UArcGunStateComponent, SelectedGun, this);
	
}

void UArcGunStateComponent::HandleOnItemAddedToSocket(UArcItemsStoreComponent* ItemSlotComponent
													  , const FGameplayTag& ItemSlot
													  , const FArcItemData* OwnerItemData
													  , const FGameplayTag& ItemSocketSlot
													  , const FArcItemData* SocketItemData)
{
	if (ItemSlotComponent->GetOwner() != GetOwner())
	{
		return;
	}
	
	if (SelectedGun.IsValid() == false)
	{
		return;
	}

	if(OwnerItemData->GetItemId() != SelectedGun.WeaponItemHandle)
	{
		return;
	}

	// Try to reinstance everything new.
	const FArcItemFragment_GunFireMode* FireMode = ArcItems::FindFragment<FArcItemFragment_GunFireMode>(SocketItemData);
	if (FireMode != nullptr)
	{
		if (SelectedGunFireMode.GetScriptStruct() != FireMode->FireModeAsset->FireMode.GetScriptStruct())
		{
			SelectedGunFireMode = FireMode->FireModeAsset->FireMode;
		}
	}

	const FArcItemFragment_GunInstancePreset* GunInstance = ArcItems::FindFragment<FArcItemFragment_GunInstancePreset>(SocketItemData);
	if (GunInstance != nullptr)
	{
		if (SelectedGunInstance.GetScriptStruct() != GunInstance->InstanceAsset->GunInstance.GetScriptStruct())
		{
			SelectedGunInstance = GunInstance->InstanceAsset->GunInstance;
		}
	}
	
	const FArcItemFragment_GunRecoilPreset* GunRecoil = ArcItems::FindFragment<FArcItemFragment_GunRecoilPreset>(SocketItemData);
	if (GunRecoil != nullptr)
	{
		if (SelectedGunRecoil.GetScriptStruct() != GunRecoil->InstancePreset->RecoilInstance.GetScriptStruct())
		{
			SelectedGunRecoil = GunRecoil->InstancePreset->RecoilInstance;
		}
	}
}

void UArcGunStateComponent::StartFire(
	UGameplayAbility* SourceAbility
	, UArcTargetingObject* InTargetingObject
	, const FGameplayAbilitySpecHandle& TargetRequest)
{
	if (SelectedGunInstance.IsValid() == false)
	{
		return;
	}

	UArcCoreGameplayAbility* ArcAbility = Cast<UArcCoreGameplayAbility>(SourceAbility);
	
	ShootCount = 0;
	OnStartFireDelegate.Broadcast(SelectedGun);
	GunState.bIsFiring = true;
	GunState.TargetingObject = InTargetingObject;
	GunState.RequestingAbility = TargetRequest;
	GunState.StateRep++;
	GunState.SourceAbility = ArcAbility;
	
	MARK_PROPERTY_DIRTY_FROM_NAME(UArcGunStateComponent, GunState, this);
	
	if(GetOwnerRole() < ROLE_Authority)
	{
		ServerFireStart(InTargetingObject, TargetRequest);
	}
	SelectedGunInstance.GetMutablePtr<FArcGunInstanceBase>()->Initialize(this);
	
	if (GetNetMode() != ENetMode::NM_DedicatedServer)
	{
		FArcGunRecoilInstance* RecoilInstance = SelectedGunRecoil.GetMutablePtr<FArcGunRecoilInstance>();
		if (RecoilInstance != nullptr)
		{
			RecoilInstance->StartRecoil(this);
		}
	}
	
	if (FArcGunFireMode* FireMode = SelectedGunFireMode.GetMutablePtr<FArcGunFireMode>())
	{
		const bool bPreFired = FireMode->FireStart(GunState.RequestingAbility, this);
		if (bPreFired)
		{
			PreFire();
		}
	}
	
	GetOwner()->ForceNetUpdate();
}

void UArcGunStateComponent::StopFire()
{
	OnStopFireDelegate.Broadcast(SelectedGun);
	GunState.bIsFiring = false;
	GunState.StateRep++;
	MARK_PROPERTY_DIRTY_FROM_NAME(UArcGunStateComponent, GunState, this);
	if(GetOwnerRole() < ROLE_Authority)
	{
		ServerFireStop();
	}
	
	if (FArcGunFireMode* FireMode = SelectedGunFireMode.GetMutablePtr<FArcGunFireMode>())
	{
		FireMode->FireStop(GunState.RequestingAbility, this);
	}
	
	if (GetNetMode() != ENetMode::NM_DedicatedServer)
	{
		FArcGunRecoilInstance* RecoilInstance = SelectedGunRecoil.GetMutablePtr<FArcGunRecoilInstance>();
		if (RecoilInstance != nullptr)
		{
			RecoilInstance->StopRecoil(this);
		}
	}

	GetOwner()->ForceNetUpdate();
}

bool UArcGunStateComponent::CommitAmmo()
{
	if (CharacterOwner && CharacterOwner->IsLocallyControlled() == false)
	{
		return true;
	}
	
	if(GetNetMode() == ENetMode::NM_DedicatedServer)
	{
		return false;
	}
	if (!GetOwner()->HasAuthority())
	{
		ServerCommitAmmo();
	}
	
	const bool bCommited =  SelectedGunInstance.GetMutablePtr<FArcGunInstanceBase>()->CommitAmmo(SelectedGun.GunItemPtr, this);
	if(bCommited == false)
	{
		OnCostCheckFailedDelegate.Broadcast(SelectedGun);
	}
	
	return bCommited;
}

void UArcGunStateComponent::ShootGun(
	class UGameplayAbility* InAbility
	, const FGameplayAbilityTargetDataHandle& InHits
	, class UArcTargetingObject* InTrace)
{
	LastShootTime = GetWorld()->GetTimeSeconds();
	ShootCount++;
	
	OnGunShootDelegate.Broadcast(InAbility, SelectedGun, InHits, InTrace, InAbility->GetCurrentAbilitySpecHandle());
	
	if (GetNetMode() != ENetMode::NM_DedicatedServer)
	{
		FArcGunRecoilInstance* RecoilInstance = SelectedGunRecoil.GetMutablePtr<FArcGunRecoilInstance>();
		if (RecoilInstance != nullptr)
		{
			RecoilInstance->OnShootFired(this);
		}
	}
}

void UArcGunStateComponent::BroadcastOnPreShoot(const FArcGunShotInfo& ShotInfo)
{
	OnPreShootDelegate.Broadcast(ShotInfo);
}

void UArcGunStateComponent::ServerFireStart_Implementation(UArcTargetingObject* InTargetingObject, const FGameplayAbilitySpecHandle& TargetRequest)
{
	ShootCount = 0;
	GunState.bIsFiring = true;
	GunState.TargetingObject = InTargetingObject;
	GunState.RequestingAbility = TargetRequest;
	GunState.StateRep++;
	MARK_PROPERTY_DIRTY_FROM_NAME(UArcGunStateComponent, GunState, this);

	GetOwner()->ForceNetUpdate();
}

void UArcGunStateComponent::ServerFireStop_Implementation()
{
	GunState.bIsFiring = false;
	GunState.StateRep++;
	MARK_PROPERTY_DIRTY_FROM_NAME(UArcGunStateComponent, GunState, this);

	GetOwner()->ForceNetUpdate();
}

void UArcGunStateComponent::ClientStopFire_Implementation(const FArcClientStopFireData& InData)
{
	// TODO: Replace with new instances
	OnWeaponAmmoChangedDelegate.Broadcast(InData.AmmoLeft);
}

void UArcGunStateComponent::ClientReloadCompleted_Implementation(const FArcClientStopFireData& InData)
{
	// TODO: Replace with new instances
	OnWeaponAmmoChangedDelegate.Broadcast(InData.AmmoLeft);
}

void UArcGunStateComponent::ServerCommitAmmo_Implementation()
{
	SelectedGunInstance.GetMutablePtr<FArcGunInstanceBase>()->CommitAmmo(SelectedGun.GunItemPtr, this);
}

TArray<const FArcEffectSpecItem*> UArcGunStateComponent::GetAmmoGameplayEffectSpecs(const FGameplayTag& InTag) const
{
	const FArcItemInstance_EffectToApply* Instance = ArcItems::FindInstance<FArcItemInstance_EffectToApply>(SelectedGun.GunItemPtr);
	return Instance->GetEffectSpecHandles(InTag);
}

FVector UArcGunStateComponent::GetWeaponSocketLocation(FName SocketName) const
{
	if(SelectedGun.GunActor == nullptr)
	{
		FVector SocketLocation = CharacterOwner->GetMesh()->GetSocketLocation(BaseSkeletonSocketName);
		FVector EyeLoc;
		FRotator EyeRot;
		CharacterOwner->GetActorEyesViewPoint(EyeLoc, EyeRot);

		return SocketLocation + EyeRot.Vector() * ForwardOffset;
	}
	
	USkeletalMeshComponent* Mesh = SelectedGun.GunActor->FindComponentByClass<USkeletalMeshComponent>();
	if(Mesh)
	{
		return Mesh->GetSocketLocation(SocketName);
	}
	
	FVector SocketLocation = CharacterOwner->GetMesh()->GetSocketLocation(BaseSkeletonSocketName);
	FVector EyeLoc;
	FRotator EyeRot;
	CharacterOwner->GetActorEyesViewPoint(EyeLoc, EyeRot);

	return SocketLocation + EyeRot.Vector() * ForwardOffset;
}

FVector UArcGunStateComponent::GetRecoilDirection(const FVector& InitialDirection) const
{
	if (const FArcGunRecoilInstance* Instance = GetGunRecoil<FArcGunRecoilInstance>())
	{
		return Instance->GetScreenORecoilffset(InitialDirection);
	}

	return InitialDirection;
}

AArcGunActor* UArcGunStateComponent::GetGunActor() const
{
	return SelectedGun.GunActor.Get();
}

FVector UArcGunStateComponent::GetGunAimPoint() const
{
	FVector AimPoint = FVector::ZeroVector;

	FVector EyeLoc;
	FRotator EyeRot;
	CharacterOwner->GetActorEyesViewPoint(EyeLoc, EyeRot);
	const FVector SocketLocation = CharacterOwner->GetMesh()->GetSocketLocation(BaseSkeletonSocketName);
	AimPoint = SocketLocation + (EyeRot.Vector() * ForwardOffset);
	
	AArcGunActor* GunActor = GetGunActor();
	if (GunActor)
	{
		FVector MuzzleSocketLocation = GunActor->GetMuzzleSocketLocation();
		AimPoint = MuzzleSocketLocation;
	}
	
	return AimPoint;
}

AArcCorePlayerController* UArcGunStateComponent::GetPlayerController() const
{
	if (ArcPC == nullptr)
	{
		if (APlayerState* PS = GetOwner<APlayerState>())
		{
			ArcPC = Cast<AArcCorePlayerController>(PS->GetOwningController());
		}
	}

	if (ArcPC == nullptr)
	{
		if (APawn* PS = GetOwner<APawn>())
		{
			ArcPC = Cast<AArcCorePlayerController>(PS->GetController());
		}
	}
	
	if (ArcPC == nullptr)
	{
		ArcPC = GetOwner<AArcCorePlayerController>();
	}
	
	return ArcPC;
}

UArcTargetingObject* UArcGunStateComponent::GetTargetingObject() const
{
	return GunState.TargetingObject;
}

UArcCoreAbilitySystemComponent* UArcGunStateComponent::GetArcASC() const
{
	return ArcASC;
}

float UArcGunStateComponent::GetCurrentSpread() const
{
	return SelectedGunRecoil.GetPtr<FArcGunRecoilInstance>()->GetCurrentSpread();
}

void UArcGunStateComponent::AccumulateScreenPos(FWeaponSystemComponentDebugInfo& Info)
{
	const float ColumnWidth = Info.Canvas ? Info.Canvas->ClipX * 0.4f : 0.f;

	float NewY = Info.YPos + Info.YL;
	if (NewY > Info.MaxY)
	{
		// Need new column, reset Y to original height
		NewY = Info.NewColumnYPadding;
		Info.XPos += ColumnWidth;
	}
	Info.YPos = NewY;
}

void UArcGunStateComponent::DebugLine(FWeaponSystemComponentDebugInfo& Info, FString Str, float XOffset, float YOffset, int32 MinTextRowsToAdvance)
{
	if (Info.Canvas)
	{
		FFontRenderInfo RenderInfo = FFontRenderInfo();
		RenderInfo.bEnableShadow = true;
		if (const UFont * Font = GEngine->GetSmallFont())
		{
			float ScaleY = 1.1f;
			Info.YL = Info.Canvas->DrawText(Font, Str, Info.XPos + XOffset, Info.YPos, 1.1f, ScaleY, RenderInfo);
			if (Info.YL < MinTextRowsToAdvance * (Font->GetMaxCharHeight() * ScaleY))
			{
				Info.YL = MinTextRowsToAdvance * (Font->GetMaxCharHeight() * ScaleY);
			}
			AccumulateScreenPos(Info);
		}
	}
}

void UArcGunStateComponent::DisplayDebug(AHUD* HUD, class UCanvas* Canvas, const class FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos)
{
	if (DebugDisplay.IsDisplayOn(FName(TEXT("ArcGun"))) == false)
	{
		return;
	}
	
	UArcGunStateComponent* GunComponent = nullptr;
	if (APawn* P = Cast<APawn>(HUD->GetCurrentDebugTargetActor()))
	{
		GunComponent = P->FindComponentByClass<UArcGunStateComponent>();
		if(GunComponent == nullptr)
		{
			GunComponent = P->GetPlayerState()->FindComponentByClass<UArcGunStateComponent>();
		}
	}

	if(GunComponent == nullptr)
	{
		return;
	}

	if (const FArcGunRecoilInstance* GunRecoil = GunComponent->GetGunRecoil<FArcGunRecoilInstance>())
	{
		GunRecoil->DrawDebugHUD(HUD, Canvas, DebugDisplay, YL, YPos);
	}
}

USkeletalMeshComponent* UArcGunStateComponent::GetAimAttachComponent() const
{
	UE_LOG(LogArcGunStateComponent, Warning, TEXT("Using Default GetAimAttachComponent"));

	APlayerState* PS = Cast<APlayerState>(GetOwner());
	if(PS)
	{
		ACharacter* C = PS->GetPawn<ACharacter>();
		return C->GetMesh();
	}

	ACharacter* C = Cast<ACharacter>(GetOwner());
	return C->GetMesh();
}

USceneComponent* UArcGunStateComponent::GetAttachComponent(const FGameplayTag& InSlot) const
{
	UE_LOG(LogArcGunStateComponent, Warning, TEXT("Using Default GetAttachComponent"));
	APlayerState* PS = Cast<APlayerState>(GetOwner());
	if (PS)
	{
		ACharacter* C = PS->GetPawn<ACharacter>();
		return C->GetMesh();
	}

	ACharacter* C = Cast<ACharacter>(GetOwner());
	return C->GetMesh();
}

AArcPlayerCameraManager* UArcGunStateComponent::GetCameraManager()
{
	unimplemented();
	return nullptr;
	//APlayerState* PS = Cast<APlayerState>(GetOwner());
	//if (PS)
	//{
	//	APlayerController* PC = PS->GetPlayerController();
	//	AArcPlayerCameraManager* ArcPCM = Cast<AArcPlayerCameraManager>(PC->PlayerCameraManager.Get());
	//	return ArcPCM;
	//}
	//ACharacter* C = Cast<ACharacter>(GetOwner());
	//
	//APlayerController* PC = C->GetController<APlayerController>();
	//AArcPlayerCameraManager* ArcPCM = Cast<AArcPlayerCameraManager>(PC->PlayerCameraManager.Get());
	//return ArcPCM;
}


