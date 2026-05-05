// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"
#include "ArcMassQuickBarTypes.generated.h"

UENUM()
enum class EArcMassQuickSlotsMode : uint8
{
	Cyclable,
	AutoActivateOnly,
	ManualActivationOnly
};

USTRUCT()
struct ARCMASSITEMSRUNTIME_API FArcMassQuickBarSlot
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, Category = "Data")
	bool bAutoSelect = true;

	UPROPERTY(EditDefaultsOnly, Category = "Data", meta = (Categories = "QuickSlot"))
	FGameplayTag QuickBarSlotId;

	UPROPERTY(EditDefaultsOnly, Category = "Data", meta = (Categories = "SlotId"))
	FGameplayTag DefaultItemSlotId;

	UPROPERTY(EditDefaultsOnly, Category = "Data", meta = (Categories = "SlotId"))
	FGameplayTag ItemSlotId;

	UPROPERTY(EditDefaultsOnly, Category = "Data")
	FGameplayTagContainer ItemRequiredTags;

	UPROPERTY(EditDefaultsOnly, Category = "Data", meta = (BaseStruct = "/Script/ArcMassItemsRuntime.ArcMassQuickBarInputBinding"))
	FInstancedStruct InputBind;

	UPROPERTY(EditDefaultsOnly, Category = "Data", meta = (BaseStruct = "/Script/ArcMassItemsRuntime.ArcMassQuickBarSlotValidator", ShowTreeView, ExcludeBaseStruct))
	TArray<FInstancedStruct> Validators;

	UPROPERTY(EditDefaultsOnly, Category = "Data", meta = (BaseStruct = "/Script/ArcMassItemsRuntime.ArcMassQuickSlotHandler", ShowTreeView, ExcludeBaseStruct))
	TArray<FInstancedStruct> SelectedHandlers;
};

USTRUCT()
struct ARCMASSITEMSRUNTIME_API FArcMassQuickBarEntry
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, Category = "Data", meta = (Categories = "QuickBar,BarId"))
	FGameplayTag BarId;

	UPROPERTY(EditDefaultsOnly, Category = "Data")
	EArcMassQuickSlotsMode QuickSlotsMode = EArcMassQuickSlotsMode::Cyclable;

	UPROPERTY(EditDefaultsOnly, Category = "Data", meta = (Categories = "QuickSlot"))
	FGameplayTag ParentQuickSlot;

	UPROPERTY(EditDefaultsOnly, Category = "Data")
	FGameplayTagContainer ItemRequiredTags;

	UPROPERTY(EditDefaultsOnly, Category = "Data", meta = (BaseStruct = "/Script/ArcMassItemsRuntime.ArcMassQuickBarSelectedAction", ShowTreeView, ExcludeBaseStruct))
	TArray<FInstancedStruct> QuickBarSelectedActions;

	UPROPERTY(EditDefaultsOnly, Category = "Data")
	TArray<FArcMassQuickBarSlot> Slots;
};
