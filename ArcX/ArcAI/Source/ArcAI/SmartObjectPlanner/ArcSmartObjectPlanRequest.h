#pragma once
#include "ArcSmartObjectPlanRequestHandle.h"
#include "GameplayTagContainer.h"
#include "MassEntityHandle.h"
#include "StructUtils/StructView.h"

#include "ArcSmartObjectPlanRequest.generated.h"

struct FArcSmartObjectPlanResponse;
DECLARE_DELEGATE_OneParam(FArcSmartObjectPlanResponseDelegate, const FArcSmartObjectPlanResponse&)

USTRUCT()
struct ARCAI_API FArcSmartObjectPlanRequest
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere)
	FGameplayTagContainer Requires;

	UPROPERTY(EditAnywhere)
	int32 MaxPlans = 10;

	FMassEntityHandle RequestingEntity;
	
	TArray<FConstStructView> CustomConditionsArray;
	
	FGameplayTagContainer InitialTags;
	
	UPROPERTY()
	float SearchRadius = 1000.f;

	UPROPERTY()
	FVector SearchOrigin = FVector::ZeroVector;

	FArcSmartObjectPlanResponseDelegate FinishedDelegate;

	bool IsValid() const
	{
		return FinishedDelegate.IsBound();
	}

	FArcSmartObjectPlanRequestHandle Handle;
};