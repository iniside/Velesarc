// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if WITH_GAMEPLAY_DEBUGGER

#include "GameplayDebuggerCategory.h"

class UArcKnowledgeSubsystem;

/**
 * Gameplay Debugger category for the Arc Knowledge system.
 *
 * Visualizes:
 * - Total knowledge entry count
 * - Total advertisement count (claimed vs unclaimed)
 * - Knowledge entry locations as colored points
 */
class FGameplayDebuggerCategory_ArcKnowledge : public FGameplayDebuggerCategory
{
	using Super = FGameplayDebuggerCategory;

public:
	FGameplayDebuggerCategory_ArcKnowledge();

	static TSharedRef<FGameplayDebuggerCategory> MakeInstance();

	virtual void CollectData(APlayerController* OwnerPC, AActor* DebugActor) override;
	virtual void DrawData(APlayerController* OwnerPC, FGameplayDebuggerCanvasContext& CanvasContext) override;
};

#endif // WITH_GAMEPLAY_DEBUGGER
