// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if WITH_GAMEPLAY_DEBUGGER

#include "GameplayDebuggerCategory.h"
#include "ArcSettlementTypes.h"

class UArcSettlementSubsystem;

/**
 * Gameplay Debugger category for the Arc Settlement / Knowledge system.
 *
 * Visualizes:
 * - All settlements as cyan circles with labels (name, handle, knowledge count)
 * - All regions as yellow circles
 * - Knowledge entries within a selected settlement as colored points
 * - Action entries highlighted separately (claimed vs unclaimed)
 * - Text overlay with system stats (total entries, settlements, regions)
 *
 * Supports cycling settlements via Shift+S.
 */
class FGameplayDebuggerCategory_ArcSettlement : public FGameplayDebuggerCategory
{
	using Super = FGameplayDebuggerCategory;

public:
	FGameplayDebuggerCategory_ArcSettlement();

	static TSharedRef<FGameplayDebuggerCategory> MakeInstance();

	virtual void CollectData(APlayerController* OwnerPC, AActor* DebugActor) override;
	virtual void DrawData(APlayerController* OwnerPC, FGameplayDebuggerCanvasContext& CanvasContext) override;

private:
	void OnCycleSettlement();

	FArcSettlementHandle SelectedSettlement;
	int32 SelectedSettlementIndex = 0;
	bool bCycleSettlement = false;
};

#endif // WITH_GAMEPLAY_DEBUGGER
