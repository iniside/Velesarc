// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if WITH_GAMEPLAY_DEBUGGER

#include "GameplayDebuggerCategory.h"
#include "MassEntityHandle.h"
#include "MassEntityManager.h"

class FGameplayDebuggerCategory_ArcNeeds : public FGameplayDebuggerCategory
{
	using Super = FGameplayDebuggerCategory;

public:
	FGameplayDebuggerCategory_ArcNeeds();
	virtual ~FGameplayDebuggerCategory_ArcNeeds() override;

	static TSharedRef<FGameplayDebuggerCategory> MakeInstance();

	virtual void CollectData(APlayerController* OwnerPC, AActor* DebugActor) override;
	virtual void DrawData(APlayerController* OwnerPC, FGameplayDebuggerCanvasContext& CanvasContext) override;

private:
	void OnPickEntity();
	void PickEntity(const FVector& ViewLocation, const FVector& ViewDirection,
		const UWorld& World, FMassEntityManager& EntityManager, bool bLimitAngle = true);
	void SetCachedEntity(FMassEntityHandle Entity, const FMassEntityManager& EntityManager);

	void OnEntitySelected(const FMassEntityManager& EntityManager, FMassEntityHandle EntityHandle);

	FDelegateHandle OnEntitySelectedHandle;
	FMassEntityHandle CachedEntity;
	TWeakObjectPtr<AActor> CachedDebugActor;
	bool bPickEntity = false;
};

#endif // WITH_GAMEPLAY_DEBUGGER
