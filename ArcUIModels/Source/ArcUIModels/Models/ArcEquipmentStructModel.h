// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcFrontendMVVMModelComponent.h"
#include "Items/ArcItemTypes.h"
#include "Mass/EntityHandle.h"

#include "ArcEquipmentStructModel.generated.h"

class UArcEquipmentSlotViewModel;
class UArcMassItemEventSubsystem;
class UArcGameEventSubsystem;
class UArcMassEquipmentSlotPreset;
class UArcTagDisplayRegistry;
struct FArcMassEquipmentFragment;
struct FArcMassItemStoreFragment;
struct FMassEntityManager;

// One authored slot binding: pairs a slot DEFINITION selector with the registration
// tag its created VM is published under.
USTRUCT(BlueprintType)
struct FArcEquipmentSlotBinding
{
	GENERATED_BODY()

	// Selects the equipment slot DEFINITION only: FindSlotDef(Preset, SlotId) and
	// EquippedItems.Find(SlotId). It is not the registration key — the created
	// VM is registered under ContextName + RegistrationTag (the tag the slot resolver reads).
	UPROPERTY(EditAnywhere, meta=(Categories="SlotId"))
	FGameplayTag SlotId;

	// The tag the widget slot resolver reads to find this slot's VM.
	UPROPERTY(EditAnywhere, meta=(Categories="ViewModel"))
	FGameplayTag RegistrationTag;
};

// Per-slot runtime state, one entry per successfully-built binding. Not reflected —
// holds live weak VM refs, the currently-equipped item id, and the slot-changed handle.
struct FArcEquipmentSlotRuntime
{
	FGameplayTag SlotId;
	FGameplayTag RegistrationTag;

	// Weak ref — the strong ref lives in the component's ViewModels UPROPERTY TMap.
	TWeakObjectPtr<UArcEquipmentSlotViewModel> SlotVM;

	FDelegateHandle SlotChangedHandle;
};

USTRUCT(BlueprintType)
struct ARCUIMODELS_API FArcEquipmentStructModel : public FArcFrontendMVVMStructModel
{
	GENERATED_BODY()

	FArcEquipmentStructModel() = default;

	// The set of equipment slots this single model drives. Each binding builds one
	// UArcEquipmentSlotViewModel registered under ContextName + binding.RegistrationTag.
	UPROPERTY(EditAnywhere)
	TArray<FArcEquipmentSlotBinding> SlotBindings;

	// Context name this slot's VM is registered under in the global VM collection.
	UPROPERTY(EditAnywhere)
	FName ContextName = TEXT("Equipment");

	// Which store fragment to listen on for slot-assigned/slot-cleared edges.
	// Must derive FArcMassItemStoreFragment. Default (nullptr) resolves to the base type.
	UPROPERTY(EditAnywhere, meta=(MetaStruct="/Script/ArcMassItemsRuntime.ArcMassItemStoreFragment"))
	TObjectPtr<const UScriptStruct> StoreFragmentType = nullptr;

	/** Tag-display registry used to resolve the item rarity label + color. */
	UPROPERTY(EditAnywhere, Category = "Display")
	TObjectPtr<UArcTagDisplayRegistry> TagDisplayRegistry;

	virtual void Initialize(UArcFrontendMVVMModelComponent& Component) override;
	virtual void Deinitialize(UArcFrontendMVVMModelComponent& Component) override;

	// Called from signal subscriptions. Public so the file-scoped namespace helper
	// that captures this pointer inside a lambda can reach it.
	void HandleSignal(FName SignalName, TConstArrayView<FMassEntityHandle> Entities, TWeakObjectPtr<UArcFrontendMVVMModelComponent> WeakComponent);

private:
	// Resolves the entity and builds+wires every slot binding's VM. Returns true once
	// bound, false on the early bail paths (entity/world/subsystem not ready yet).
	bool TryBindToEntity(UArcFrontendMVVMModelComponent& Component);

	// "All slots" wrappers — iterate SlotRuntimes and refresh each.
	void RefreshOccupants(UArcFrontendMVVMModelComponent& Component);
	void RefreshAvailability(UArcFrontendMVVMModelComponent& Component);

	// Per-runtime workers shared by the wrappers (frag/store/preset resolved by the caller).
	void RefreshOccupantsForRuntime(FArcEquipmentSlotRuntime& Runtime, const FArcMassEquipmentFragment* EquipFrag, FArcMassItemStoreFragment* StoreFrag);
	void RefreshAvailabilityForRuntime(FArcEquipmentSlotRuntime& Runtime, FMassEntityManager& Manager, const UArcMassEquipmentSlotPreset* Preset);

	void HandleStacksChanged(FMassEntityHandle InEntity, FArcItemId ItemId, TWeakObjectPtr<UArcFrontendMVVMModelComponent> WeakComponent);

	FMassEntityHandle BoundEntity;
	FDelegateHandle EntityAssociatedHandle;
	TWeakObjectPtr<UArcMassItemEventSubsystem> BoundEventSubsystem;
	TWeakObjectPtr<UArcGameEventSubsystem> BoundGameEventSubsystem;

	// One entry per successfully-built binding; holds the per-slot VM and handle.
	TArray<FArcEquipmentSlotRuntime> SlotRuntimes;

	FDelegateHandle StacksChangedHandle;
	FDelegateHandle TagAddedHandle;
	FDelegateHandle TagRemovedHandle;
};
