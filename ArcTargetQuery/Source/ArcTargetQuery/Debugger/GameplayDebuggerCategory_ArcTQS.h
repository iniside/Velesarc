// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if WITH_GAMEPLAY_DEBUGGER

#include "GameplayDebuggerCategory.h"
#include "Mass/EntityHandle.h"
#include "Mass/ArchetypeCompositionTypes.h"
#include "MassLogging.h"

class UArcTQSQuerySubsystem;
struct FArcTQSQueryResult;

/**
 * Gameplay Debugger category for the Arc Target Query System.
 *
 * Visualizes:
 * - All generated items as spheres (color-coded by score)
 * - Filtered-out items as grey X marks
 * - Selected results as highlighted spheres with score labels
 * - Querier location
 * - Text overlay with query stats (item counts, timing, selection mode)
 *
 * Supports Mass Entity picking via Shift+T to select which entity's query to inspect.
 */
class FGameplayDebuggerCategory_ArcTQS : public FGameplayDebuggerCategory
{
	using Super = FGameplayDebuggerCategory;

public:
	FGameplayDebuggerCategory_ArcTQS();
	virtual ~FGameplayDebuggerCategory_ArcTQS() override;

	static TSharedRef<FGameplayDebuggerCategory> MakeInstance();

	virtual void CollectData(APlayerController* OwnerPC, AActor* DebugActor) override;
	virtual void DrawData(APlayerController* OwnerPC, FGameplayDebuggerCanvasContext& CanvasContext) override;

private:
	void OnPickEntity();

	void PickEntity(const FVector& ViewLocation, const FVector& ViewDirection,
		const UWorld& World, FMassEntityManager& EntityManager, bool bLimitAngle = true);

	void SetCachedEntity(FMassEntityHandle Entity, const FMassEntityManager& EntityManager);

	void CollectDebugShapes(const FArcTQSQueryResult& Result);

	FDelegateHandle OnEntitySelectedHandle;
	void OnEntitySelected(const FMassEntityManager& EntityManager, FMassEntityHandle EntityHandle);

	FMassEntityHandle CachedEntity;
	TWeakObjectPtr<AActor> CachedDebugActor;
	bool bPickEntity = false;
};

#endif // WITH_GAMEPLAY_DEBUGGER
