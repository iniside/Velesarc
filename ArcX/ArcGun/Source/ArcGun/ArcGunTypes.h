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


#include "GameplayAbilitySpec.h"
#include "ArcCore/Items/ArcItemTypes.h"
#include "Tasks/TargetingTask.h"

#include "ArcGunTypes.generated.h"

class UArcItemDefinition;
class UArcGunStateComponent;
class UArcGunFireModePreset;
class AArcGunActor;
class UArcCoreGameplayAbility;
class UArcTargetingObject;

struct FGameplayAbilityTargetDataHandle;

USTRUCT(BlueprintType)
struct ARCGUN_API FArcSelectedGun
{
	GENERATED_BODY()
	
public:
	FGameplayTag QuickBarId;
	FGameplayTag QuickSlotId;
	
	UPROPERTY(BlueprintReadOnly, Category = "Data")
	FGameplayTag Slot;
	
	UPROPERTY(BlueprintReadOnly, Category = "Data")
	FArcItemId WeaponItemHandle;
	
	UPROPERTY()
	TObjectPtr<const UArcItemDefinition> WeaponItemDef = nullptr;
	
	UPROPERTY()
	TObjectPtr<UArcGunFireModePreset> FireMode = nullptr;

	UPROPERTY()
	uint8 ForceUpdate = 0;

	UPROPERTY(NotReplicated)
	TWeakObjectPtr<AArcGunActor> GunActor = nullptr;
	 
	const FArcItemData* GunItemPtr = nullptr;

	UPROPERTY(NotReplicated)
	TWeakObjectPtr<UArcGunStateComponent> WCOwner = nullptr;

	FArcSelectedGun()
	{}

	FArcSelectedGun(const FArcItemData* InWeaponItemPtr);

	void SetWeaponItemPtr(const FArcItemData* InWeaponItemPtr);

	bool IsValid() const
	{
		return Slot.IsValid() && WeaponItemHandle.IsValid() && GunItemPtr != nullptr;
	}

	void Reset()
	{
		Slot = FGameplayTag();
		WeaponItemHandle.Reset();
		GunActor = nullptr;
		GunItemPtr = nullptr;
		ForceUpdate = 0;
		FireMode = nullptr;
		WeaponItemDef = nullptr;
	}
};

USTRUCT()
struct FArcGunShotInfo
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UArcGunStateComponent> GunStateComponent;
	
	UPROPERTY()
	TObjectPtr<UGameplayAbility> Ability;
	
	UPROPERTY()
	FArcSelectedGun CurrentGun;

	UPROPERTY()
	TArray<FHitResult> Targets;
	
	UPROPERTY()
	TObjectPtr<UArcTargetingObject> TargetingObject;
	
	UPROPERTY()
	FGameplayAbilitySpecHandle TargetRequestHandle;

	UPROPERTY()
	bool bEnded = false;
	
	UPROPERTY()
	float TimeHeld = 0;
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FRecoilSpreadChanged, float /* CurrentSpread */, float /* MaxSpread */);
DECLARE_MULTICAST_DELEGATE_FourParams(FRecoilSwayChanged, float /* HorizontalSway */, float /* MaxHorizontalSway */,  float /* VerticalSway */, float /* MaxVerticalSway */);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FArcOnShootFiered, float, TimeHeld, bool, bEnded, const TArray<FHitResult>&, Hits);
DECLARE_MULTICAST_DELEGATE_OneParam(FArcOnShootFired, const FArcGunShotInfo&);

DECLARE_MULTICAST_DELEGATE_OneParam(FArcGunAmmoChanged, int32)
DECLARE_MULTICAST_DELEGATE_TwoParams(FArcGunReloadDelegate, const FArcSelectedGun&, float /*ReloadTime*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FArcGunDelegate, const FArcSelectedGun&);
DECLARE_MULTICAST_DELEGATE_TwoParams(FArcGunWeaponAmmoDelegate, const FArcSelectedGun&, int32);
DECLARE_MULTICAST_DELEGATE_FiveParams(FArcGunFireHitDelegate, class UGameplayAbility*, const FArcSelectedGun&, const FGameplayAbilityTargetDataHandle&, class UArcTargetingObject*, const FGameplayAbilitySpecHandle&);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FArcMagazineAmmoChanged, const FArcSelectedGun&, float/* CurrentAmmo*/, float/*MaxAmmo*/);

USTRUCT(BlueprintType)
struct FArcHolsterSocket
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category = "Data")
		FName SocketName;
	UPROPERTY(EditAnywhere, Category = "Data")
		bool bAvailable;

	FArcHolsterSocket()
		: SocketName(NAME_None)
		, bAvailable(false)
	{}
};

USTRUCT(BlueprintType)
struct ARCGUN_API FArcWeaponAttachmentSpawned
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category = "Data")
	TObjectPtr<class AArcGunActor> WeaponActor;

	UPROPERTY(EditAnywhere, Category = "Data")
	FName UnholsterSocket;

	//possible places where this weapon can be attached when holstered.
	UPROPERTY(EditAnywhere, Category = "Data")
	TArray<FName> HolsterSockets;

	FArcWeaponAttachmentSpawned()
		: WeaponActor(nullptr)
	{}
};

USTRUCT()
struct FArcClientStopFireData
{
	GENERATED_BODY()
public:
	UPROPERTY()
	int32 AmmoLeft;

	FArcClientStopFireData()
		: AmmoLeft(0)
	{};
};

UCLASS()
class ARCGUN_API UArcTargetingTask_GunTrace : public UTargetingTask
{
	GENERATED_BODY()

	/** The trace channel to use */
	UPROPERTY(EditAnywhere, Category = "Target Trace Selection | Collision Data")
	TEnumAsByte<ETraceTypeQuery> TraceChannel;

	/** The trace channel to use */
	UPROPERTY(EditAnywhere, Category = "Target Trace Selection | Collision Data")
	FName SocketName;
	
public:
	UArcTargetingTask_GunTrace(const FObjectInitializer& ObjectInitializer);

	/** Lifecycle function called when the task first begins */
	virtual void Init(const FTargetingRequestHandle& TargetingHandle) const override;

	/** Evaluation function called by derived classes to process the targeting request */
	virtual void Execute(const FTargetingRequestHandle& TargetingHandle) const override;

	/** Lifecycle function called when the task was cancelled while in the Executing Async state */
	virtual void CancelAsync() const override;

	/** Debug Helper Methods */
#if ENABLE_DRAW_DEBUG
public:
	virtual void DrawDebug(UTargetingSubsystem* TargetingSubsystem, FTargetingDebugInfo& Info, const FTargetingRequestHandle& TargetingHandle, float XOffset, float YOffset, int32 MinTextRowsToAdvance = 0) const override;
#endif // ENABLE_DRAW_DEBUG
	/** ~Debug Helper Methods */
};
