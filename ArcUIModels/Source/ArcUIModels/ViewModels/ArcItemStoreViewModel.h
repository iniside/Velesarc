// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcRmlStructList.h"
#include "GameplayTagContainer.h"
#include "Items/ArcItemViewFilter.h"
#include "MVVMViewModelBase.h"

#include "ArcItemStoreViewModel.generated.h"

UCLASS(BlueprintType)
class ARCUIMODELS_API UArcItemStoreViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	UArcItemStoreViewModel();

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	FArcRmlStructList Items;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	FArcItemViewFilter Filter;

	const FArcRmlStructList& GetItems() const { return Items; }
	const FArcItemViewFilter& GetFilter() const { return Filter; }

	UFUNCTION(BlueprintCallable, Category = "Arc UI|Filter")
	void SetFilter(const FArcItemViewFilter& InFilter);

	UFUNCTION(BlueprintCallable, Category = "Arc UI|Filter")
	void ClearFilter();

	UFUNCTION(BlueprintCallable, Category = "Arc UI|Filter")
	void SetFilterRequiredTags(const FGameplayTagContainer& InTags);

	UFUNCTION(BlueprintCallable, Category = "Arc UI|Filter")
	void SetFilterIgnoreTags(const FGameplayTagContainer& InTags);

	UFUNCTION(BlueprintCallable, Category = "Arc UI|Filter")
	void SetFilterSlotPolicy(EArcItemViewSlotPolicy InPolicy, FGameplayTag InMatchSlot);

	UFUNCTION(BlueprintCallable, Category = "Arc UI|Filter")
	void SetFilterLevelRange(int32 InMin, int32 InMax);

	FSimpleMulticastDelegate OnFilterChanged;

	FArcRmlStructList& GetMutableItems() { return Items; }
	void BroadcastItemsChanged();

	void Reset();

private:
	void ApplyFilter(const FArcItemViewFilter& NewFilter);
};
