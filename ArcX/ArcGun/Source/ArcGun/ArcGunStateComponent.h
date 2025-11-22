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


#include "ArcGunInstanceBase.h"
#include "ArcMacroDefines.h"
#include "Components/ActorComponent.h"
#include "ArcGunTypes.h"
#include "Abilities/GameplayAbility.h"
#include "ArcGunStateComponent.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogArcGunStateComponent, Log, All);

class UArcTargetingFilterPreset;
class UArcItemsStoreComponent;
class UArcItemsStoreComponent;
class UArcTargetingObject;
struct FArcEffectSpecItem;

struct FArcItemFragment_GunStats;

class UArcItemAttachmentComponent;
class AArcCorePlayerController;
class ACharacter;
class UArcCoreAbilitySystemComponent;
class APlayerCameraManager;
class UCameraComponent;
class UInputAction;

struct FArcItemAttachment;

class UArcGunStateComponent;

USTRUCT()
struct FArcGunState
{
	GENERATED_BODY()
	
public:
	UPROPERTY()
	bool bIsFiring = false;

	UPROPERTY()
	uint8 StateRep;

	// Targeting presets we use for actual target aquisition and shooting mechanic.
	UPROPERTY()
	TObjectPtr<UArcTargetingObject> TargetingObject = nullptr;

	
	UPROPERTY()
	TObjectPtr<UArcCoreGameplayAbility> SourceAbility;
	
	// This is based of ability. Abilties are granted on server. So handl >should< be unique on all connected clients.
	UPROPERTY()
	FGameplayAbilitySpecHandle RequestingAbility;
	
	FArcGunState()
		: bIsFiring(false)
		, StateRep(0)
	{};
};


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FArcGunTargetingResultDynamicDelegate, const FHitResult&, HitResult);

/**
 * Attempt at separating gun weapon works from where it is equipped, since we may want support somewhat more
 * complex cases for equipment.
 */
UCLASS(ClassGroup=(Arc), Blueprintable, meta=(BlueprintSpawnableComponent))
class ARCGUN_API UArcGunStateComponent : public UActorComponent
{
	GENERATED_BODY()

	friend class UArcAT_WaitSwapWeapon;
	friend class UArcAnimNotify_Holster;
	friend class UArcAnimNotify_Unholster;
	friend struct FArcWeaponItemInstance;
	friend class UArcGunBFL;
	
protected:
	//Currently Equipped weapon from which we can use.
	// TODO Might want to make it replicated. Need to check.
	UPROPERTY(EditAnywhere, ReplicatedUsing=OnRep_SelectedGun)
	FArcSelectedGun SelectedGun;

	FInstancedStruct SelectedGunFireMode;
	FInstancedStruct SelectedGunInstance;

public:
	FInstancedStruct SelectedGunRecoil;
	template<typename T>
	const T* GetGunRecoil() const
	{
		return SelectedGunRecoil.GetPtr<T>();
	}
	
	int32 ShootCount = 0;
	double LastShootTime = 0;
	double TimeAccumulator = 0;
	
	FHitResult CurrentHitResult;
	FHitResult CameraAimHitResult;

	FTargetingRequestHandle CameraAimTargetingHandle;
	FTargetingRequestHandle TargetingHandle;

	FVector AimDirection;
	
	UPROPERTY(BlueprintAssignable)
	FArcGunTargetingResultDynamicDelegate OnTargetingResultDelegate;

	void DrawDebug();
	
	void HandleCameraAimTargetingCompleted(FTargetingRequestHandle TargetingRequestHandle);
	void HandleTargetingCompleted(FTargetingRequestHandle TargetingRequestHandle);
	
	void PreFire();

	UFUNCTION(BlueprintCallable)
	const FHitResult& GetCameraAimHitResult() const
	{
		return CameraAimHitResult;
	}
	
	UFUNCTION(BlueprintCallable)
	const FHitResult& GetCurrentHitResult() const
	{
		return CurrentHitResult;
	}

	UFUNCTION(BlueprintCallable)
	const FVector& GetAimDirection() const
	{
		return AimDirection;
	}
	
public:
	const double GetLastShootTime() const
	{
		return LastShootTime;
	}
	
public:
	template<typename T>
	T* GetGunInstance()
	{
		return SelectedGunInstance.GetMutablePtr<T>();
	}

	template<typename T>
	const T* GetFireMode() const
	{
		return SelectedGunFireMode.GetPtr<T>();
	}
	
protected:
	UFUNCTION()
	void OnRep_SelectedGun(const FArcSelectedGun& Old);
	
	/*
	 * Keep it around on clients. We don't need to on server, but we need it to know how to detach old weapon.
	 * We might store slot, but what if player changes weapon between slots ?
	 */
	FArcSelectedGun OldSelectedGun;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	TSubclassOf<UArcItemsStoreComponent> ItemSlotComponentClass;

	UPROPERTY(EditAnywhere, Category = "Weapon")
	TObjectPtr<UInputAction> AxisInputAction;

	
public:
	UInputAction* GetAxisInputAction() const
	{
		return AxisInputAction;
	}
	
protected:
	UPROPERTY()
	mutable TObjectPtr<UArcItemsStoreComponent> ItemsComponent;
	/**
	 * Not fully fledged weapon state, just indicates if weapon is firing or not.
	 */
	UPROPERTY(ReplicatedUsing = OnRep_GunState)
	FArcGunState GunState;

	UFUNCTION()
	void OnRep_GunState(const FArcGunState& OldWeaponState);

	DEFINE_ARC_DELEGATE(FArcGunDelegate, OnGunSelected);
	
protected:
	mutable FArcOnShootFired OnPreShootDelegate;

public:
	FDelegateHandle AddOnPreShoot(typename FArcOnShootFired::FDelegate InDelegate) const { return OnPreShootDelegate.Add(InDelegate); }
	void RemoveOnPreShoot(FDelegateHandle InHandle) const { OnPreShootDelegate.Remove(InHandle); }
	void RemoveAllOnPreShoot(const void* InUserObject) const { OnPreShootDelegate.RemoveAll(InUserObject); }
	
	DEFINE_ARC_DELEGATE(FArcGunAmmoChanged, OnWeaponAmmoChanged);
	DEFINE_ARC_DELEGATE(FRecoilSpreadChanged, OnSpreadChanged);
	DEFINE_ARC_DELEGATE(FRecoilSwayChanged, OnSwayChanged);
	DEFINE_ARC_DELEGATE(FArcGunDelegate, OnStartFire);
	DEFINE_ARC_DELEGATE(FArcGunDelegate, OnStopFire);
	DEFINE_ARC_DELEGATE(FArcGunDelegate, OnCostCheckFailed);
	
	DEFINE_ARC_DELEGATE(FArcGunFireHitDelegate, OnGunShoot);
	
	DEFINE_ARC_DELEGATE(FArcGunReloadDelegate, OnReloadStart);
	DEFINE_ARC_DELEGATE(FArcGunReloadDelegate, OnReloadEnd);
	DEFINE_ARC_DELEGATE(FArcGunReloadDelegate, OnReloadAborted);

public:
	static const FName NAME_GunComponentReady;
	// Sets default values for this component's properties
	UArcGunStateComponent();


	
	UFUNCTION(BlueprintPure, Category = "Arc Gun|Weapon")
	static UArcGunStateComponent* FindGunStateComponent(class AActor* InActor)
	{
		return InActor->FindComponentByClass<UArcGunStateComponent>();
	}

	UArcItemsStoreComponent* GetItemsComponent() const;

	const UArcItemDefinition* GetWeaponItemDef() const
	{
		return SelectedGun.WeaponItemDef;
	}
	const FArcItemId& GetEquippedWeaponItemId() const
	{
		return SelectedGun.WeaponItemHandle;
	}
	
	const FArcItemData* GetEquippedWeaponPtr() const
	{
		if(SelectedGun.GunItemPtr != nullptr)
		{
			return SelectedGun.GunItemPtr;
		}
		return nullptr;
	}

	const FArcItemData* GetOldEquippedWeaponPtr() const
	{
		if(OldSelectedGun.GunItemPtr != nullptr)
		{
			return OldSelectedGun.GunItemPtr;
		}
		return nullptr;
	}
	
protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	virtual void InitializeComponent() override;
	
public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	
protected:
	void HandleComponentReady();
	virtual void OnWeaponComponentReady() {}
	
public:
	/**
	 * @brief Handles the selection of a quick slot.
	 *
	 * This method is triggered when a quick slot is selected in the gameplay.
	 * It retrieves the item ID and data from the corresponding quick bar and
	 * performs further actions based on the item's properties. It resets the
	 * selected gun, fire mode, gun instance, and gun recoil. If the item ID
	 * is invalid or the item data is null, the method returns without further
	 * processing. Otherwise, it retrieves the gun's fire mode, gun instance,
	 * and gun recoil from the item data. If any of these properties are null,
	 * it marks the SelectedGun property as dirty and returns. Otherwise, it
	 * initializes the selected gun instance and stores the item ID, item data,
	 * and slot ID in the SelectedGun property. It also retrieves the gun actor
	 * associated with the item ID from the item attachment component and triggers
	 * the OnGunSelectedDelegate event with the selected gun.
	 *
	 * @param BarId      The gameplay tag of the quick bar.
	 * @param QuickSlotId The gameplay tag of the selected quick slot.
	 *
	 * @see Called from FArcQuickSlotHandler_GunChanged
	 */
	void HandleQuickSlotSelected(FGameplayTag BarId
								 , FGameplayTag QuickSlotId);

	void HandleQuickSlotDeselected(FGameplayTag BarId
		, FGameplayTag QuickSlotId);

	void HandleOnItemRemovedFromSlot(UArcItemsStoreComponent* ItemSlotComponent
								 , const FGameplayTag& ItemSlot
								 , const FArcItemData* ItemData);
	
	void HandleOnItemAddedToSlot(UArcItemsStoreComponent* ItemSlotComponent
								 , const FGameplayTag& ItemSlot
								 , const FArcItemData* ItemData);
	
	void HandleOnItemAddedToSocket(UArcItemsStoreComponent* ItemSlotComponent
								   , const FGameplayTag& ItemSlot
								   , const FArcItemData*OwnerItemData
								   , const FGameplayTag&ItemSocketSlot
								   , const FArcItemData*SocketItemData);
public:
	/**
	 * @brief Starts the firing sequence of the gun.
	 *
	 * This method is used to initiate the firing sequence of the gun. It broadcasts the start of the firing event
	 * and sets the state of the gun to firing. If the gun is not valid, the method returns immediately.
	 * The gun state is then incremented and marked as dirty. If the owner role is not authority, the method
	 * calls the ServerFireStart method. The selected gun instance is then initialized. If the net mode is not
	 * dedicated server, it starts the recoil instance and triggers the fire start method of the selected gun fire
	 * mode.
	 *
	 * @param InTargetingObject The targeting object associated with the gun.
	 * @param InTargetData The custom trace data handle associated with the gun.
	 */
	UFUNCTION(BlueprintCallable, Category = "Arc Gun")
	void StartFire(
		UGameplayAbility* SourceAbility
		, UArcTargetingObject* InTargetingObject
		, const FGameplayAbilitySpecHandle& TargetRequest);
	
	UFUNCTION(BlueprintCallable, Category = "Arc Gun")
	void StopFire();

	bool CommitAmmo();

	/**
	 * Shoots the gun and performs the necessary actions.
	 * This is only called on server and locally. Never on Sim proxies.
	 * 
	 * @param InAbility The gameplay ability used to shoot the gun.
	 * @param InHits The target data handle representing the hit result.
	 * @param InTrace The arc targeting object used for tracing.
	 * @param RequestHandle The request handle representing the target request.
	 */
	void ShootGun(
		UGameplayAbility* InAbility
		, const FGameplayAbilityTargetDataHandle& InHits
		, class UArcTargetingObject* InTrace);

	// This is local only and called from FireMode manually.
	void BroadcastOnPreShoot(const FArcGunShotInfo& ShotInfo);

	UFUNCTION(Server, Reliable)
	void ServerFireStart(UArcTargetingObject* InTargetingObject, const FGameplayAbilitySpecHandle& TargetRequest);
	
	UFUNCTION(Server, Reliable)
	void ServerFireStop();
	
private:
	UFUNCTION(Client, Reliable)
	void ClientStopFire(const FArcClientStopFireData& InData);

	UFUNCTION(Client, Reliable)
	void ClientReloadCompleted(const FArcClientStopFireData& InData);

	UFUNCTION(Server, Reliable)
	void ServerCommitAmmo();
	
protected:
	/*
	 * Get effects to apply when weapon hits target. Can be overriden if you want to provide custom source
	 * for Gameplay Effects. For example from different item than equipped weapon.
	 * It allows for example, to implement different damage types by switching "ammo" item.
	 */
	virtual TArray<const FArcEffectSpecItem*> GetAmmoGameplayEffectSpecs(const FGameplayTag& InTag) const;

public:
	
	// We will use this to trace where the gun should be aiming
	UPROPERTY(EditAnywhere)
	TObjectPtr<UTargetingPreset> CameraAimTargetingPreset = nullptr;

	UPROPERTY(EditAnywhere)
	float AimInterpolationSpeed = 10.f;
	
	/**
	 * If there is no weapon socket available we will use this offset to
	 * move simulate where muzzle would be.
	 */
	UPROPERTY(EditAnywhere)
	float ForwardOffset = 40.0f;

	/**
	 * Socket name we will use to get base location from if there is no weapon mesh available.
	 */
	UPROPERTY(EditAnywhere)
	FName BaseSkeletonSocketName = "head";
	
	UFUNCTION(BlueprintPure, Category = "Arc Game Core|Weapon")
	FVector GetWeaponSocketLocation(FName SocketName) const;
	
	UFUNCTION(BlueprintPure, Category = "Arc Game Core|Weapon")
	FVector GetRecoilDirection(const FVector& InitialDirection) const;
	
	UFUNCTION(BlueprintPure, Category = "Arc Game Core|Weapon")
	AArcGunActor* GetGunActor() const;

	UFUNCTION(BlueprintPure, Category = "Arc Game Core|Weapon")
	FVector GetGunAimPoint() const;
	
	const FArcSelectedGun& GetSelectedGun() const
	{
		return SelectedGun;
	}
	
	AArcCorePlayerController* GetPlayerController() const;

	UArcTargetingObject* GetTargetingObject() const;
	
protected:
	UPROPERTY()
	mutable TObjectPtr<AArcCorePlayerController> ArcPC;

	UPROPERTY()
	TObjectPtr<UArcCoreAbilitySystemComponent> ArcASC;

	UPROPERTY()
	TObjectPtr<APawn> CharacterOwner;

	UPROPERTY()
	TObjectPtr<APlayerCameraManager> CameraManager;

	UPROPERTY()
	TObjectPtr<UCameraComponent> Camera;
	
public:
	UArcCoreAbilitySystemComponent* GetArcASC() const;

	FDelegateHandle PreProcessRotationHandle;

public:
	int32 GetShootCount() const
	{
		return ShootCount;
	}
		
public:
	/**
	 * @brief Retrieves the current spread of the gun.
	 *
	 * This method returns the current spread value of the gun, which is used to determine
	 * the accuracy and dispersion of the shots fired. The spread value can be influenced
	 * by various factors such as the gun's recoil, firing mode, and other gameplay mechanics.
	 *
	 * @return The current spread value as a float.
	 */
	float GetCurrentSpread() const;
    
private:
	struct FWeaponSystemComponentDebugInfo
	{
		FWeaponSystemComponentDebugInfo()
		{
			FMemory::Memzero(*this);
		}

		class UCanvas* Canvas;

		bool bPrintToLog;

		bool bShowWeapon;

		float XPos;
		float YPos;
		float OriginalX;
		float OriginalY;
		float MaxY;
		float NewColumnYPadding;
		float YL;

		bool Accumulate;
		TArray<FString>	Strings;

		int32 GameFlags; // arbitrary flags for games to set/read in Debug_Internal
	};
	struct FNxWeaponDebugTargetInfo
	{
		FNxWeaponDebugTargetInfo()
		{
			DebugCategoryIndex = 0;
			DebugCategories.Add(TEXT("Gun"));
		}

		TArray<FName> DebugCategories;
		int32 DebugCategoryIndex;

		TWeakObjectPtr<UWorld>	TargetWorld;
		TWeakObjectPtr<UArcGunStateComponent>	LastDebugTarget;
	};
	static void AccumulateScreenPos(FWeaponSystemComponentDebugInfo& Info);
	static void DebugLine(FWeaponSystemComponentDebugInfo& Info, FString Str, float XOffset, float YOffset, int32 MinTextRowsToAdvance = 0);
	
public:
	void static DisplayDebug(AHUD* HUD, class UCanvas* Canvas, const class FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos);

public:
	virtual class USkeletalMeshComponent* GetAimAttachComponent() const;

	virtual class USceneComponent* GetAttachComponent(const FGameplayTag& InSlot) const;

	virtual class AArcPlayerCameraManager* GetCameraManager();
};
