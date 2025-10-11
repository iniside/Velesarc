// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcPotentialEntity.h"
#include "ArcSmartObjectPlanContainer.h"
#include "ArcSmartObjectPlanRequest.h"
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
