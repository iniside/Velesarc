// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "MVVMViewModelBase.h"

#include "ArcQuickBarViewModel.generated.h"

class UArcQuickBarSlotViewModel;

UCLASS(BlueprintType)
class ARCUIMODELS_API UArcQuickBarViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	FGameplayTag BarId;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	TArray<TObjectPtr<UArcQuickBarSlotViewModel>> Slots;

	const FGameplayTag& GetBarId() const { return BarId; }
	const TArray<TObjectPtr<UArcQuickBarSlotViewModel>>& GetSlots() const { return Slots; }

	void SetBarId(FGameplayTag InBarId);

	// Returns the first slot whose QuickSlotId matches InQuickSlotId, or nullptr if not found.
	UFUNCTION(BlueprintCallable, Category = "Arc UI")
	UArcQuickBarSlotViewModel* FindSlot(FGameplayTag InQuickSlotId) const;
};
