// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if WITH_GAMEPLAY_DEBUGGER

#include "GameplayDebuggerCategory.h"
#include "MassEntityHandle.h"
#include "MassEntityManager.h"

struct FArcConditionState;
struct FArcConditionConfig;

class FGameplayDebuggerCategory_ArcConditions : public FGameplayDebuggerCategory
{
	using Super = FGameplayDebuggerCategory;

public:
	FGameplayDebuggerCategory_ArcConditions();
	virtual ~FGameplayDebuggerCategory_ArcConditions() override;

	static TSharedRef<FGameplayDebuggerCategory> MakeInstance();

	virtual void CollectData(APlayerController* OwnerPC, AActor* DebugActor) override;
	virtual void DrawData(APlayerController* OwnerPC, FGameplayDebuggerCanvasContext& CanvasContext) override;

private:
	void OnEntitySelected(const FMassEntityManager& EntityManager, FMassEntityHandle EntityHandle);

	/** Formats and adds a text line for a single condition. */
	void CollectConditionLine(FMassEntityManager& EntityManager, const TCHAR* Name,
		const FArcConditionState& State, const FArcConditionConfig* Config);

	FDelegateHandle OnEntitySelectedHandle;
	FMassEntityHandle CachedEntity;
	TWeakObjectPtr<AActor> CachedDebugActor;
};

#endif // WITH_GAMEPLAY_DEBUGGER
