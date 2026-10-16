// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Mass/EntityHandle.h"
#include "MVVMViewModelBase.h"

#include "ArcQuickBarSlotViewModel.generated.h"

class UArcItemViewModel;
class UArcAbilityViewModel;
class UObject;

UCLASS(BlueprintType)
class ARCUIMODELS_API UArcQuickBarSlotViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	FGameplayTag BarId;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	FGameplayTag QuickSlotId;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter = "GetbHasItem")
	bool bHasItem = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter = "GetbIsActive")
	bool bIsActive = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	TObjectPtr<UArcItemViewModel> Item = nullptr;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	TObjectPtr<UArcAbilityViewModel> Ability = nullptr;

	const FGameplayTag& GetBarId() const { return BarId; }
	const FGameplayTag& GetQuickSlotId() const { return QuickSlotId; }
	bool GetbHasItem() const { return bHasItem; }
	bool GetbIsActive() const { return bIsActive; }
	UArcItemViewModel* GetItem() const { return Item; }
	UArcAbilityViewModel* GetAbility() const { return Ability; }

	void SetBarId(FGameplayTag InBarId);
	void SetQuickSlotId(FGameplayTag InQuickSlotId);
	void SetItem(UArcItemViewModel* InItem);
	void SetAbility(UArcAbilityViewModel* InAbility);
	void SetHasItem(bool bInHasItem);
	void SetActive(bool bInActive);

	// Sets the target entity handle. Called by the struct-model during initialization so
	// that RequestAddItem / RequestRemoveItem can build commands with a valid entity.
	void SetEntity(FMassEntityHandle InEntity);

	// Sends FArcMassAddItemToQuickSlotCommand. WorldContextObject is required to resolve
	// the local PlayerController. Returns false if any guard (entity, slot ids, item) fails.
	UFUNCTION(BlueprintCallable, Category = "Arc UI", meta = (WorldContext = "WorldContextObject"))
	bool RequestAddItem(UObject* WorldContextObject, UArcItemViewModel* InItem);

	// Sends FArcMassRemoveItemFromQuickSlotCommand for the currently slotted item.
	// Returns false if the entity or slot ids are invalid.
	UFUNCTION(BlueprintCallable, Category = "Arc UI", meta = (WorldContext = "WorldContextObject"))
	bool RequestRemoveItem(UObject* WorldContextObject);

private:
	// Set by the struct-model during Initialize. Not replicated; lives on client only.
	FMassEntityHandle TargetEntity;
};
