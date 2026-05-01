// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcPotentialEntity.h"
#include "ArcSmartObjectPlanContainer.h"
#include "ArcSmartObjectPlanRequest.h"
#include "ArcSmartObjectPlanSensor.h"
#include "ArcSmartObjectPlanDebugData.h"
#include "GameplayDebuggerCategory.h"
#include "Subsystems/WorldSubsystem.h"
struct FArcSmartObjectPlanEvaluationContext;

#include "ArcSmartObjectPlannerSubsystem.generated.h"

/**
 * World subsystem that processes SmartObject planning requests with time-sliced ticking.
 * Replaces the previous Mass processor approach — the planner is inherently serial/recursive
 * and gains nothing from Mass's chunk-based iteration.
 */
UCLASS()
class ARCAI_API UArcSmartObjectPlannerSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	FArcSmartObjectPlanRequestHandle AddRequest(FArcSmartObjectPlanRequest& Request)
	{
		Request.Handle = FArcSmartObjectPlanRequestHandle::Make();
		RequestQueue.Add(Request);
		return Request.Handle;
	}

	TMap<FMassEntityHandle, FArcSmartObjectPlanContainer> DebugPlans;

	void SetDebugPlan(FMassEntityHandle EntityHandle, const FArcSmartObjectPlanContainer& Plan)
	{
		DebugPlans.Add(EntityHandle, Plan);
	}

	const FArcSmartObjectPlanContainer* GetDebugPlan(FMassEntityHandle EntityHandle) const
	{
		return DebugPlans.Find(EntityHandle);
	}

#if !UE_BUILD_SHIPPING
	TMap<FMassEntityHandle, FArcSmartObjectPlanDebugData> DebugDiagnostics;

	void SetDebugDiagnostics(FMassEntityHandle EntityHandle, FArcSmartObjectPlanDebugData&& Data)
	{
		DebugDiagnostics.Add(EntityHandle, MoveTemp(Data));
	}

	const FArcSmartObjectPlanDebugData* GetDebugDiagnostics(FMassEntityHandle EntityHandle) const
	{
		return DebugDiagnostics.Find(EntityHandle);
	}
#endif

	// UTickableWorldSubsystem
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return !RequestQueue.IsEmpty(); }
	virtual bool IsTickableInEditor() const override { return false; }

	/** Time budget per tick in milliseconds. Default 1ms. */
	float TimeBudgetMs = 1.0f;

	void BuildAllPlans(const FArcSmartObjectPlanRequest& Request);

	static void RunSensors(
		const FArcSmartObjectPlanRequest& Request,
		const FArcSmartObjectPlanEvaluationContext& Context,
		TArray<FArcPotentialEntity>& OutCandidates);

	static void DeduplicateCandidates(TArray<FArcPotentialEntity>& Candidates);

	static bool BuildPlanRecursive(
		TArray<FArcPotentialEntity>& AvailableEntities,
		const FGameplayTagContainer& NeededTags,
		FGameplayTagContainer& CurrentTags,
		FGameplayTagContainer& AlreadyProvided,
		TArray<FArcSmartObjectPlanStep>& CurrentPlan,
		TArray<FArcSmartObjectPlanContainer>& OutPlans,
		TArray<bool>& UsedEntities,
		int32 MaxPlans,
		const FArcSmartObjectPlanEvaluationContext* Context = nullptr
#if !UE_BUILD_SHIPPING
		, FArcSmartObjectPlanDebugData* DebugData = nullptr
#endif
	);

	static bool EvaluateCustomConditions(const FArcPotentialEntity& Entity,
								const FArcSmartObjectPlanEvaluationContext& Context
#if !UE_BUILD_SHIPPING
								, FString* OutFailedConditionName = nullptr
#endif
	);

private:
	TArray<FArcSmartObjectPlanRequest> RequestQueue;
};

class FGameplayDebuggerCategory_SmartObjectPlanner : public FGameplayDebuggerCategory
{
	using Super = FGameplayDebuggerCategory;

public:
	FGameplayDebuggerCategory_SmartObjectPlanner();
	virtual ~FGameplayDebuggerCategory_SmartObjectPlanner() override;

	void OnEntitySelected(const FMassEntityManager& EntityManager, FMassEntityHandle EntityHandle);

	static TSharedRef<FGameplayDebuggerCategory> MakeInstance();

	void SetCachedEntity(const FMassEntityHandle Entity, const FMassEntityManager& EntityManager);

	void PickEntity(const FVector& ViewLocation, const FVector& ViewDirection, const UWorld& World, FMassEntityManager& EntityManager
					, bool bLimitAngle = true);

	virtual void CollectData(APlayerController* OwnerPC, AActor* DebugActor) override;
	virtual void DrawData(APlayerController* OwnerPC, FGameplayDebuggerCanvasContext& CanvasContext) override;

	void OnPickEntity()
	{
		bPickEntity = true;
	}

	FDelegateHandle OnEntitySelectedHandle;
	bool bPickEntity = false;
	TWeakObjectPtr<AActor> CachedDebugActor;
	FMassEntityHandle CachedEntity;
};
