// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Mass/ArchetypeCompositionTypes.h"
#include "MassLogging.h"
#include "QuickBar/ArcMassQuickBarTypes.h"
#include "ArcMassQuickBarConfig.generated.h"

UCLASS(BlueprintType)
class ARCMASSITEMSRUNTIME_API UArcMassQuickBarConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	// Item store whose items this config's bars hold (FArcMassItemStoreFragment or a derived store). Every bar of
	// the config only sees items of this store; auto-select and item activation deferral only react to it.
	UPROPERTY(EditDefaultsOnly, Category = "Data", meta = (MetaStruct = "/Script/ArcMassItemsRuntime.ArcMassItemStoreFragment"))
	TObjectPtr<UScriptStruct> StoreType;

	// Ungrouped bars. Each one is active or inactive on its own (bActiveOnStart, ActivateBar, DeactivateBar)
	// and activating one never deactivates another. Use for bars that may be active together: a skill bar, a consumables bar,
	// a passive bar. Child bars must be listed here.
	UPROPERTY(EditDefaultsOnly, Category = "Data")
	TArray<FArcMassQuickBarEntry> QuickBars;

	// Groups of bars of which at most one is active at a time (weapon sets). Activating a grouped bar
	// deactivates the group's other active bar first; a cyclable group switches bars with CycleActiveBar.
	// Bar active state lives per bar in FArcMassQuickBarStateFragment; GetActiveBarInGroup derives the
	// group's active bar from it.
	UPROPERTY(EditDefaultsOnly, Category = "Data")
	TArray<FArcMassQuickBarGroup> Groups;
};
