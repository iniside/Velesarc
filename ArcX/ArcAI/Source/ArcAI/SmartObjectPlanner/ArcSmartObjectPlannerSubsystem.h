// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcPotentialEntity.h"
#include "ArcSmartObjectPlanContainer.h"
#include "ArcSmartObjectPlanRequest.h"
#include "GameplayDebuggerCategory.h"
#include "MassSubsystemBase.h"
#include "ArcSmartObjectPlannerSubsystem.generated.h"

/**
 * 
 */
UCLASS()
class ARCAI_API UArcSmartObjectPlannerSubsystem : public UMassSubsystemBase
{
	GENERATED_BODY()

public:
	TArray<FArcSmartObjectPlanRequest> Requests;

	FArcSmartObjectPlanRequestHandle AddRequest(FArcSmartObjectPlanRequest& Request)
	{
		Request.Handle = FArcSmartObjectPlanRequestHandle::Make();
		Requests.Add(Request);
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
	
	
	void BuildAllPlans(
		FArcSmartObjectPlanRequest Request,
		TArray<FArcPotentialEntity>& AvailableEntities);

	bool BuildPlanRecursive(
		FMassEntityManager& EntityManager,
		TArray<FArcPotentialEntity>& AvailableEntities,
		const FGameplayTagContainer& NeededTags,
		FGameplayTagContainer CurrentTags,
		TArray<FArcSmartObjectPlanStep> CurrentPlan,
		TArray<FArcSmartObjectPlanContainer>& OutPlans,
		TArray<bool>& UsedEntities,
		int32 MaxPlans);

	bool ArePlansIdentical(
	const FArcSmartObjectPlanContainer& PlanA, 
	const FArcSmartObjectPlanContainer& PlanB);

	bool HasCircularDependency(
		const TArray<FArcPotentialEntity>& AvailableEntities,
		const FArcPotentialEntity& StartEntity,
		const TArray<bool>& UsedEntities,
		int32 MaxDepth = 10);
		
	bool HasCircularDependencyRecursive(
		const TArray<FArcPotentialEntity>& AvailableEntities,
		const FArcPotentialEntity& CurrentEntity,
		TSet<FMassEntityHandle>& VisitedEntities,
		TSet<FMassEntityHandle>& CurrentPath,
		const TArray<bool>& UsedEntities,
		int32 CurrentDepth,
		int32 MaxDepth);

	bool EvaluateCustomConditions(const FArcPotentialEntity& Entity,
								FMassEntityManager& EntityManager) const;
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