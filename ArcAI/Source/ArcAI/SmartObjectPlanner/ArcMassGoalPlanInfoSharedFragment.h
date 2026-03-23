// Copyright Lukasz Baran. All Rights Reserved.

#pragma once
#include "ArcSmartObjectPlanConditionEvaluator.h"
#include "GameplayTagContainer.h"
#include "MassEntityElementTypes.h"
#include "MassEntityTraitBase.h"
#include "StructUtils/InstancedStruct.h"

#include "ArcMassGoalPlanInfoSharedFragment.generated.h"

USTRUCT()
struct FArcMassGoalPlanInfoSharedFragment : public FMassConstSharedFragment
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere)
	FGameplayTagContainer Provides;

	UPROPERTY(EditAnywhere)
	FGameplayTagContainer Requires;

	UPROPERTY(EditAnywhere)
	TArray<TInstancedStruct<FArcSmartObjectPlanConditionEvaluator>> CustomConditions;
};

//template<>
//struct TMassFragmentTraits<FArcMassGoalPlanInfoSharedFragment> final
//{
//	enum
//	{
//		AuthorAcceptsItsNotTriviallyCopyable = true
//	};
//};

/** Trait that adds goal plan info to a Mass entity for SmartObject planning.
 *  Configures what the entity provides/requires and custom plan conditions for goal selection. */
UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc Goal Plan Info", Category = "AI"))
class UArcMassGoalPlanInfoTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere)
	FArcMassGoalPlanInfoSharedFragment GoalPlanInfo;

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};