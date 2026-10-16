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

#include "ArcGunActor.h"


#include "ArcGunFireMode.h"
#include "ArcGunInstanceBase.h"
#include "ArcGunItemInstance.h"
#include "ArcGunRecoilInstance.h"
#include "DisplayDebugHelpers.h"
#include "Camera/CameraComponent.h"
#include "Components/GameFrameworkComponentManager.h"
#include "Engine/Canvas.h"
#include "Fragments/ArcItemFragment_GunStats.h"
#include "Player/ArcMassPlayerController.h"
#include "Player/ArcMassPlayerStateExtensionComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/HUD.h"
#include "Pawn/ArcMassPawnExtensionComponent.h"

#include "Items/ArcItemDefinitionData.h"
#include "Items/ArcItemData.h"

#include "Net/UnrealNetwork.h"

#include "Items/ArcItemsHelpers.h"


#include "ArcCoreUtils.h"
#include "AbilitySystem/Targeting/ArcTargetingObject.h"
#include "Equipment/ArcSkeletalMeshOwnerInterface.h"
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

	const FArcItemDefinitionData* WeaponItemDef = FArcItemDefinitionData::Find(SelectedGun.WeaponItemDef);
	if (WeaponItemDef == nullptr)
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

	const FArcItemFragment_GunFireMode* FireMode = WeaponItemDef->FindFragment<FArcItemFragment_GunFireMode>();
	if (FireMode != nullptr)
	{
		// TODO Move it to SelectedGun.
		SelectedGunFireMode = FireMode->FireModeAsset->FireMode;
	}

	if (SelectedGunInstance.IsValid() == true)
	{
		SelectedGunInstance.Reset();
	}

	const FArcItemFragment_GunInstancePreset* GunInstance = WeaponItemDef->FindFragment<FArcItemFragment_GunInstancePreset>();
	if (GunInstance != nullptr)
	{
		SelectedGunInstance = GunInstance->InstanceAsset->GunInstance;
	}
}

void UArcGunStateComponent::OnRep_GunState(const FArcGunState& OldWeaponState)
{
	if(GunState.bIsFiring)
	{
		OnStartFireDelegate.Broadcast(SelectedGun);
		if (FArcGunFireMode* FireMode = SelectedGunFireMode.GetMutablePtr<FArcGunFireMode>())
		{
			const bool bPreFired = FireMode->FireStart(this);
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
			FireMode->FireStop(this);
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

// Called when the game starts
void UArcGunStateComponent::BeginPlay()
{
	Super::BeginPlay();
	
	SelectedGun.WCOwner = this;
}

void UArcGunStateComponent::InitializeComponent()
{
	//This component should be on PlayerState, but it's possible for it to end up on Pawn.
	if (UArcMassPlayerStateExtensionComponent* PSExt = UArcMassPlayerStateExtensionComponent::Get(GetOwner()))
	{
		PSExt->OnPlayerStateReady().AddUObject(this, &ThisClass::HandleComponentReady);
	}
	else if (UArcMassPawnExtensionComponent* PawnExtComp = UArcMassPawnExtensionComponent::FindPawnExtensionComponent(GetOwner()))
	{
		PawnExtComp->OnPawnReadyToInitialize_RegisterAndCall(
			UArcMassPawnExtensionComponent::FArcMassOnPawnReadyToInitialize::FDelegate::CreateWeakLambda(
				this, [this](UArcMassPawnExtensionComponent*) { HandleComponentReady(); }));
	}
	
	HandleComponentReady();

	Super::InitializeComponent();

	// Item-store / slot lifecycle hooks retired with UArcItemsStoreComponent (plan step 8b-gun).
	// Mass-side equivalent will rewire via FArcMassItemStoreFragment events when reintroduced.
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
	
	const FArcItemDefinitionData* WeaponItemDef = GunState.TargetingObject.Get() ? FArcItemDefinitionData::Find(SelectedGun.WeaponItemDef) : nullptr;
	if (WeaponItemDef != nullptr)
	{
		const ArcGunStats* WeaponStats = WeaponItemDef->GetScalableFloatFragment<ArcGunStats>();
		
		//const float FixedUpdateRate = WeaponStats->RecoilUpdateRate;

		// Add the current frame's delta time to our accumulator
		TimeAccumulator += DeltaTime;

		// Process as many fixed time steps as we've accumulated
		//while (TimeAccumulator >= FixedUpdateRate)
		{
			FArcGunRecoilInstance* RecoilInstance = SelectedGunRecoil.GetMutablePtr<FArcGunRecoilInstance>();
			if (RecoilInstance != nullptr)
			{
				RecoilInstance->UpdateRecoil(DeltaTime, this);
			}
		
			//TimeAccumulator -= FixedUpdateRate;
			//TimeAccumulator += 0.001;
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
		const bool bPreFired = FireMode->FireUpdate(this, DeltaTime);
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
	ShotInfo.CurrentGun = SelectedGun;
	ShotInfo.Targets = {CurrentHitResult};
	ShotInfo.TargetingObject = GunState.TargetingObject;
	ShotInfo.bEnded = false;
	ShotInfo.TimeHeld = 0;
	
	BroadcastOnPreShoot(ShotInfo);
}
void UArcGunStateComponent::HandleComponentReady()
{
	ENetMode NetMode = GetNetMode();
	if (NetMode != ENetMode::NM_DedicatedServer)
	{
		if (UArcMassPlayerStateExtensionComponent* PSExt = UArcMassPlayerStateExtensionComponent::Get(GetOwner()))
		{
			if (APlayerState* PS = Cast<APlayerState>(PSExt->GetOwner()))
			{
				CharacterOwner = PS->GetPawn();
				if (CharacterOwner != nullptr)
				{
					ArcPC = CharacterOwner->GetController<AArcMassPlayerController>();
				}
			}
			// GAS-side ArcASC wiring removed (Bucket B); weapon ASC integration retires in Bucket D.
		}
		else if (UArcMassPawnExtensionComponent* PawnExtComp = UArcMassPawnExtensionComponent::FindPawnExtensionComponent(GetOwner()))
		{
			CharacterOwner = PawnExtComp->GetPawn<APawn>();
			if (CharacterOwner != nullptr)
			{
				ArcPC = CharacterOwner->GetController<AArcMassPlayerController>();
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


		
		// GAS-side ArcASC fetch removed (Bucket B); weapon ASC integration retires in Bucket D.
	}

	UGameFrameworkComponentManager::SendGameFrameworkComponentExtensionEvent(GetOwner(), UArcGunStateComponent::NAME_GunComponentReady);
	OnWeaponComponentReady();
}


void UArcGunStateComponent::StartFire(UArcTargetingObject* InTargetingObject)
{
	if (SelectedGunInstance.IsValid() == false)
	{
		return;
	}

	ShootCount = 0;
	OnStartFireDelegate.Broadcast(SelectedGun);
	GunState.bIsFiring = true;
	GunState.TargetingObject = InTargetingObject;
	GunState.StateRep++;

	MARK_PROPERTY_DIRTY_FROM_NAME(UArcGunStateComponent, GunState, this);

	if(GetOwnerRole() < ROLE_Authority)
	{
		ServerFireStart(InTargetingObject);
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
		const bool bPreFired = FireMode->FireStart(this);
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
		FireMode->FireStop(this);
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

void UArcGunStateComponent::ShootGun(class UArcTargetingObject* InTrace)
{
	LastShootTime = GetWorld()->GetTimeSeconds();
	ShootCount++;

	OnGunShootDelegate.Broadcast(SelectedGun, InTrace);

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

void UArcGunStateComponent::ServerFireStart_Implementation(UArcTargetingObject* InTargetingObject)
{
	ShootCount = 0;
	GunState.bIsFiring = true;
	GunState.TargetingObject = InTargetingObject;
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

FVector UArcGunStateComponent::GetWeaponSocketLocation(FName SocketName) const
{
	if(SelectedGun.GunActor == nullptr)
	{
		IArcSkeletalMeshOwnerInterface* SMOwner = Cast<IArcSkeletalMeshOwnerInterface>(CharacterOwner);
		FVector SocketLocation = SMOwner->GetSkeletalMesh()->GetSocketLocation(BaseSkeletonSocketName);
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

	IArcSkeletalMeshOwnerInterface* SMOwner = Cast<IArcSkeletalMeshOwnerInterface>(CharacterOwner);
	FVector SocketLocation = SMOwner->GetSkeletalMesh()->GetSocketLocation(BaseSkeletonSocketName);
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
	IArcSkeletalMeshOwnerInterface* SMOwner = Cast<IArcSkeletalMeshOwnerInterface>(CharacterOwner);
	
	FVector AimPoint = FVector::ZeroVector;

	FVector EyeLoc;
	FRotator EyeRot;
	CharacterOwner->GetActorEyesViewPoint(EyeLoc, EyeRot);
	const FVector SocketLocation = SMOwner->GetSkeletalMesh()->GetSocketLocation(BaseSkeletonSocketName);
	AimPoint = SocketLocation + (EyeRot.Vector() * ForwardOffset);
	
	AArcGunActor* GunActor = GetGunActor();
	if (GunActor)
	{
		FVector MuzzleSocketLocation = GunActor->GetMuzzleSocketLocation();
		AimPoint = MuzzleSocketLocation;
	}
	
	return AimPoint;
}

AArcMassPlayerController* UArcGunStateComponent::GetPlayerController() const
{
	if (ArcPC == nullptr)
	{
		if (APlayerState* PS = GetOwner<APlayerState>())
		{
			ArcPC = Cast<AArcMassPlayerController>(PS->GetOwningController());
		}
	}

	if (ArcPC == nullptr)
	{
		if (APawn* PS = GetOwner<APawn>())
		{
			ArcPC = Cast<AArcMassPlayerController>(PS->GetController());
		}
	}
	
	if (ArcPC == nullptr)
	{
		ArcPC = GetOwner<AArcMassPlayerController>();
	}
	
	return ArcPC;
}

UArcTargetingObject* UArcGunStateComponent::GetTargetingObject() const
{
	return GunState.TargetingObject;
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



