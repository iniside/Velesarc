// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcRmlStructList.h"
#include "Mass/EntityHandle.h"
#include "MVVMViewModelBase.h"

#include "ArcBagContainerViewModel.generated.h"

class UArcBagDefinition;
class UObject;

UCLASS(BlueprintType)
class ARCUIMODELS_API UArcBagContainerViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	UArcBagContainerViewModel();

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	FArcRmlStructList Bags;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	FArcRmlStructList Slots;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	int32 ActiveBagIndex = 0;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	FString Capacity;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	bool bHoverShown = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	FText HoverName;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	FText HoverDescription;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	TSoftObjectPtr<UObject> HoverIcon;

	const FArcRmlStructList& GetBags() const { return Bags; }
	const FArcRmlStructList& GetSlots() const { return Slots; }
	int32 GetActiveBagIndex() const { return ActiveBagIndex; }
	const FString& GetCapacity() const { return Capacity; }
	bool GetbHoverShown() const { return bHoverShown; }
	const FText& GetHoverName() const { return HoverName; }
	const FText& GetHoverDescription() const { return HoverDescription; }
	const TSoftObjectPtr<UObject>& GetHoverIcon() const { return HoverIcon; }

	UFUNCTION(BlueprintCallable, Category = "Arc UI")
	void SelectBag(int32 Index);

	UFUNCTION(BlueprintCallable, Category = "Arc UI")
	void SelectSlot(int32 Index);

	UFUNCTION(BlueprintCallable, Category = "Arc UI")
	void HoverSlot(int32 Index);

	UFUNCTION(BlueprintCallable, Category = "Arc UI")
	void UnhoverSlot();

	UFUNCTION(BlueprintCallable, Category = "Arc UI", meta = (WorldContext = "WorldContextObject"))
	bool RequestAddBag(UObject* WorldContextObject, UArcBagDefinition* Def);

	void SetEntity(FMassEntityHandle InEntity);

	FArcRmlStructList& GetMutableBags() { return Bags; }
	FArcRmlStructList& GetMutableSlots() { return Slots; }
	void BroadcastBagsChanged();
	void BroadcastSlotsChanged();
	void SetActiveBagIndex(int32 InIndex);
	void SetCapacity(const FString& InCapacity);

	int32 GetSelectedSlotIndex() const { return SelectedSlotIndex; }
	void ClearSelectedSlot();

	FSimpleMulticastDelegate OnActiveBagChanged;

private:
	void RefreshHover();

	FMassEntityHandle TargetEntity;

	int32 SelectedSlotIndex = INDEX_NONE;
	int32 HoveredSlotIndex = INDEX_NONE;
};
