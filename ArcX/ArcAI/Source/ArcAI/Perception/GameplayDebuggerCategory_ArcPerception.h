// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if WITH_GAMEPLAY_DEBUGGER

#include "GameplayDebuggerCategory.h"
#include "MassEntityHandle.h"
#include "MassEntityManager.h"

class FGameplayDebuggerCategory_ArcPerception : public FGameplayDebuggerCategory
{
	using Super = FGameplayDebuggerCategory;

public:
	FGameplayDebuggerCategory_ArcPerception();
	virtual ~FGameplayDebuggerCategory_ArcPerception() override;

	static TSharedRef<FGameplayDebuggerCategory> MakeInstance();

	virtual void CollectData(APlayerController* OwnerPC, AActor* DebugActor) override;
	virtual void DrawData(APlayerController* OwnerPC, FGameplayDebuggerCanvasContext& CanvasContext) override;

private:
	void OnPickEntity();
	void OnToggleSight();
	void OnToggleHearing();

	void PickEntity(const FVector& ViewLocation, const FVector& ViewDirection,
		const UWorld& World, FMassEntityManager& EntityManager, bool bLimitAngle = true);
	void SetCachedEntity(FMassEntityHandle Entity, const FMassEntityManager& EntityManager);

	void OnEntitySelected(const FMassEntityManager& EntityManager, FMassEntityHandle EntityHandle);

	void CollectSightData(FMassEntityManager& EntityManager);
	void CollectHearingData(FMassEntityManager& EntityManager);

	void DrawSenseRange(UWorld* World, FMassEntityManager& EntityManager);
	void DrawPerceivedEntities(UWorld* World, FMassEntityManager& EntityManager);

	FDelegateHandle OnEntitySelectedHandle;
	FMassEntityHandle CachedEntity;
	TWeakObjectPtr<AActor> CachedDebugActor;
	bool bPickEntity = false;
	bool bShowSight = true;
	bool bShowHearing = true;
};

#endif // WITH_GAMEPLAY_DEBUGGER
