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


#include "ArcNamedPrimaryAssetId.h"
#include "GameplayTagContainer.h"
#include "Engine/DataAsset.h"
#include "StructUtils/InstancedStruct.h"
#include "Items/ArcItemsComponent.h"
#include "Input/InputMappingContextAndPriority.h"

#include "ArcPawnData.generated.h"

class USkeletalMesh;

USTRUCT()
struct ARCCORE_API FArcDefaultItem
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, Category = GameMode, meta = (AllowedClasses = "/Script/ArcCore.ArcItemDefinition", DisplayThumbnail = false))
	FArcNamedPrimaryAssetId ItemDefinitionId;

	UPROPERTY(EditAnywhere, Category = "Data")
	int32 Amount;

	FArcDefaultItem()
		: ItemDefinitionId()
		, Amount(0)
	{
	}
};

USTRUCT()
struct ARCCORE_API FArcDefaultSlotItem
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, Category = GameMode, meta = (AllowedClasses = "/Script/ArcCore.ArcItemDefinition", DisplayThumbnail = false))
	FArcNamedPrimaryAssetId ItemDefinitionId;

	UPROPERTY(EditAnywhere, Category = "Data", meta = (Categories = "SlotId"))
	FGameplayTag Slot;

	FArcDefaultSlotItem()
		: ItemDefinitionId()
	{
	}
};

USTRUCT()
struct ARCCORE_API FArcDefaultQuickSlotItem
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, Category = GameMode, meta = (AllowedClasses = "/Script/ArcCore.ArcItemDefinition", DisplayThumbnail = false))
	FArcNamedPrimaryAssetId ItemDefinitionId;

	UPROPERTY(EditAnywhere, Category = "Data", meta = (Categories = "SlotId"))
	FGameplayTag ItemSlot;

	UPROPERTY(EditAnywhere, Category = "Data", meta = (Categories = "BarId"))
	FGameplayTag BarId;

	UPROPERTY(EditAnywhere, Category = "Data", meta = (Categories = "QuickSlot"))
	FGameplayTag QuickSlot;
	
	FArcDefaultQuickSlotItem()
		: ItemDefinitionId()
	{
	}
};

USTRUCT(BlueprintType)
struct ARCCORE_API FArcPawnDataFragment
{
	GENERATED_BODY()

public:
	virtual void GiveFragment(class APawn* InCharacter
							  , class AArcCorePlayerState* InPlayerState) const
	{
	};

	virtual ~FArcPawnDataFragment()
	{
	}
};

/*
 * TODO: These should have guarantee to only run once.
 */
USTRUCT(BlueprintType)
struct ARCCORE_API FArcPawnInitializationFragment
{
	GENERATED_BODY()

public:
	virtual void Initialize(class APawn* InCharacter
							, class AArcCorePlayerState* InPlayerState
							, class AArcCorePlayerController* InPlayerController) const
	{
	};

	virtual ~FArcPawnInitializationFragment()
	{
	}
};

USTRUCT(BlueprintType)
struct ARCCORE_API FArcPawnInitializationFragment_SetSkeletalMesh : public FArcPawnInitializationFragment
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditAnywhere)
	TSoftObjectPtr<USkeletalMesh> SkeletalMesh;

public:
	virtual void Initialize(class APawn* InCharacter
							, class AArcCorePlayerState* InPlayerState
							, class AArcCorePlayerController* InPlayerController) const override;;

	virtual ~FArcPawnInitializationFragment_SetSkeletalMesh() override
	{
	}
};

class UAnimInstance;

USTRUCT(BlueprintType)
struct ARCCORE_API FArcPawnInitializationFragment_SetAnimBlueprint : public FArcPawnInitializationFragment
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditAnywhere)
	TSoftClassPtr<UAnimInstance> AnimBlueprint;

	UPROPERTY(EditAnywhere)
	TArray<TSoftClassPtr<UAnimInstance>> AnimLayer;
	
public:
	virtual void Initialize(class APawn* InCharacter
							, class AArcCorePlayerState* InPlayerState
							, class AArcCorePlayerController* InPlayerController) const override;;

	virtual ~FArcPawnInitializationFragment_SetAnimBlueprint() override = default;

	TArray<TSoftClassPtr<UAnimInstance>> GetAnimLayers() const
	{
		return AnimLayer;
	}
};

USTRUCT(BlueprintType, meta = (DisplayName = "Ability Sets"))
struct ARCCORE_API FArcPawnDataFragment_AbilitySets : public FArcPawnDataFragment
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Abilities")
	TArray<TSoftObjectPtr<class UArcAbilitySet>> AbilitySets;

public:
	virtual void GiveFragment(class APawn* InCharacter
							  , class AArcCorePlayerState* InPlayerState) const override;

	virtual ~FArcPawnDataFragment_AbilitySets() override
	{
	}
};

USTRUCT(BlueprintType, meta = (DisplayName = "Default Items"))
struct ARCCORE_API FArcPawnDataFragment_DefaultItems : public FArcPawnDataFragment
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Item")
	TSubclassOf<class UArcItemsStoreComponent> Component;

	UPROPERTY(EditDefaultsOnly, Category = "Item")
	bool bUseItemDefinitionId = false;
	
	UPROPERTY(EditDefaultsOnly, Category = "Item", meta = (TitleProperty = "{ItemDefinitionId}"))
	TArray<FArcDefaultItem> ItemsToAdd;

public:
	virtual void GiveFragment(class APawn* InCharacter
							  , class AArcCorePlayerState* InPlayerState) const override;

	virtual ~FArcPawnDataFragment_DefaultItems() override
	{
	}
};

USTRUCT(BlueprintType, meta = (DisplayName = "Default Slot Items"))
struct ARCCORE_API FArcPawnDataFragment_DefaultItemsOnSlot : public FArcPawnDataFragment
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Item")
	TSubclassOf<class UArcItemsStoreComponent> ItemComponent;
	
	UPROPERTY(EditDefaultsOnly, Category = "Item", meta = (TitleProperty = "{ItemDefinitionId} -> {Slot}"))
	TArray<FArcDefaultSlotItem> ItemsToAdd;

public:
	virtual void GiveFragment(class APawn* InCharacter
							  , class AArcCorePlayerState* InPlayerState) const override;

	virtual ~FArcPawnDataFragment_DefaultItemsOnSlot() override
	{
	}
};

USTRUCT(BlueprintType, meta = (DisplayName = "Add Items To Quickbar"))
struct ARCCORE_API FArcPawnDataFragment_AddItemsToQuickbar : public FArcPawnDataFragment
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Item")
	TSubclassOf<class UArcItemsStoreComponent> ItemComponent;

	UPROPERTY(EditDefaultsOnly, Category = "Item")
	TSubclassOf<class UArcQuickBarComponent> QuickBarComponent;
	
	UPROPERTY(EditDefaultsOnly, Category = "Item", meta = (TitleProperty = "{QuickSlot} - {ItemDefinitionId} -> {Slot}"))
	TArray<FArcDefaultQuickSlotItem> ItemsToAdd;

public:
	virtual void GiveFragment(class APawn* InCharacter
							  , class AArcCorePlayerState* InPlayerState) const override;

	virtual ~FArcPawnDataFragment_AddItemsToQuickbar() override
	{
	}
};

USTRUCT(BlueprintType
	, meta = (DisplayName = "Add Pawn Component"))
struct ARCCORE_API FArcAddPawnComponent
{
	GENERATED_BODY()
public:
	// can be subclassed and if some componenets some special initializaion, do it here.
	virtual void GiveComponent(class APawn* InPawn) const {};

	virtual ~FArcAddPawnComponent() = default;
};

USTRUCT(BlueprintType
	, meta = (DisplayName = "Add Pawn Component Add Single"))
struct ARCCORE_API FArcAddPawnComponent_AddSingle : public FArcAddPawnComponent
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditDefaultsOnly
	, Category = "Data")
	bool bReplicate = true;

	UPROPERTY(EditDefaultsOnly
		, Category = "Data")
	bool bSpawnOnServer = true;

	UPROPERTY(EditDefaultsOnly
			, Category = "Data", meta = (EditCondition="bReplicate==false"))
	bool bSpawnOnClient = false;

	UPROPERTY(EditDefaultsOnly
		, Category = "Data")
	TSubclassOf<class UActorComponent> ComponentClass;

public:
	// can be subclassed and if some componenets some special initializaion, do it here.
	virtual void GiveComponent(class APawn* InPawn) const override;

	virtual ~FArcAddPawnComponent_AddSingle() override = default;
};

/**
 * Probabalyu should rename it, to something like ArcPlayerPawnData.
 */
UCLASS()
class ARCCORE_API UArcPawnData : public UPrimaryDataAsset
{
	GENERATED_BODY()

protected:
	/*
	 * Fragments containing data which will be "given" to either Pawn of Player state
	 * depending on which is valid. First Player State is checked for component and if not
	 * found - Pawn. Fragments will be given on client and server, If it is only valid to
	 * give on server you should make sure to check for it. Also fragments are given in
	 * order they are in editor. If there is particular order of data needed You should
	 * make sure they are in correct one.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Data", meta = (BaseStruct = "/Script/ArcCore.ArcPawnDataFragment", ExcludeBaseStruct))
	TArray<FInstancedStruct> Fragments;

	/*
	 * Function executed during Hero initialization
	 * @see UArcHeroComponentBase::HandleChangeInitState
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Data", meta = (BaseStruct = "/Script/ArcCore.ArcPawnInitializationFragment", ExcludeBaseStruct))
	TArray<FInstancedStruct> InitializationFragments;

	UPROPERTY(EditDefaultsOnly, Category = "Data")
	TSoftClassPtr<APawn> PawnClass;

	UPROPERTY(EditDefaultsOnly, Category = "Components", meta = (BaseStruct = "/Script/ArcCore.ArcAddPawnComponent", ExcludeBaseStruct))
	TArray<FInstancedStruct> Components;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TArray<TObjectPtr<class UArcInputActionConfig>> InputConfig;

	UPROPERTY(EditAnywhere, Category = "Input")
	TArray<FInputMappingContextAndPriority> AdditionalInputConfigs;

	UPROPERTY(EditDefaultsOnly, Category = UI)
	TSubclassOf<class AHUD> HUDClass;

public:
	template <typename T>
	const T* GetFragment() const
	{
		for (const FInstancedStruct& IS : Fragments)
		{
			if (IS.GetScriptStruct()->IsChildOf(T::StaticStruct()))
			{
				return IS.GetPtr<T>();
			}
		}
		return nullptr;
	}

	template <typename T>
	TArray<const T*> GetFragments() const
	{
		TArray<const T*> Out;
		for (const FInstancedStruct& IS : Fragments)
		{
			if (IS.GetScriptStruct()->IsChildOf(T::StaticStruct()))
			{
				Out.Add(IS.GetPtr<T>());
			}
		}
		return Out;
	}

	template<typename T>
	const T* FindInitializationFragment() const
	{
		for (const FInstancedStruct& IS : InitializationFragments)
		{
			if (IS.GetScriptStruct()->IsChildOf(T::StaticStruct()))
			{
				return IS.GetPtr<T>();
			}
		}
		return nullptr;
	}
	
	void GiveDataFragments(class APawn* InCharacter
						   , class AArcCorePlayerState* InPlayerState) const;

	/** 
	 * Components added from @link UArcHeroComponentBase::HandleChangeInitState.
	 * It's pretty late in initialization phase, which takes away ability to set
	 * components as NetAddressable, but in return we can override experience in game
	 * mode from command line.
	 *
	 * Unfortunetly experiences are handled server side and things like PawnData need to be replicated back to clients.
	 */
	void GiveComponents(class APawn* InCharacter) const;

	void RunInitializationFragments(class APawn* InCharacter
									, class AArcCorePlayerState* InPlayerState
									, class AArcCorePlayerController* InPlayerController) const;

	const TSoftClassPtr<APawn>& GetPawnClass() const
	{
		return PawnClass;
	}

	TArray<class UArcInputActionConfig*> GetInputConfig() const
	{
		return InputConfig;
	}

	const TArray<FInputMappingContextAndPriority>& GetAdditionalInputConfigs() const
	{
		return AdditionalInputConfigs;
	}

	const TSubclassOf<class AHUD>& GetHUDClass() const
	{
		return HUDClass;
	}
};
