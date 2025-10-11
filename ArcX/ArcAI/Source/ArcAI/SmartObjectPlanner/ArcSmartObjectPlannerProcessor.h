#pragma once
#include "ArcGameplayInteractionContext.h"
#include "ArcMassGoalPlanInfoSharedFragment.h"
#include "GameplayInteractionContext.h"
#include "GameplayTagContainer.h"
#include "MassCommonFragments.h"
#include "MassEntityTraitBase.h"
#include "MassMovementFragments.h"
#include "MassNavigationFragments.h"
#include "MassNavMeshNavigationFragments.h"
#include "MassProcessor.h"
#include "MassSmartObjectFragments.h"
#include "MassSmartObjectRequest.h"
#include "MassSubsystemBase.h"
#include "SmartObjectRuntime.h"
#include "SmartObjectTypes.h"
#include "StateTreePropertyRef.h"
#include "ArcMass/ArcMassEntityHandleWrapper.h"
#include "Tasks/ArcMassStateTreeRunEnvQueryTask.h"

#include "ArcSmartObjectPlannerProcessor.generated.h"

class UGameplayBehavior;



template<>
struct TMassFragmentTraits<FArcMassGoalPlanInfoSharedFragment> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};

UCLASS(MinimalAPI)
class UArcMassGoalPlanInfoTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere)
	FArcMassGoalPlanInfoSharedFragment GoalPlanInfo;
	
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};



// Signal Processor ?
UCLASS()
class UArcSmartObjectPlannerProcessor : public UMassProcessor
{
	GENERATED_BODY()

private:
	FMassEntityQuery Query;

public:
	UArcSmartObjectPlannerProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;
};


USTRUCT()
struct FArcMassUseSmartObjectTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Input)
	FArcMassEntityHandleWrapper SmartObjectEntityHandle;
	
	UPROPERTY(EditAnywhere, Category = Input)
	FSmartObjectHandle SmartObjectHandle;

	UPROPERTY(EditAnywhere, Category = Input)
	FSmartObjectClaimHandle SmartObjectClaimHandle;

	UPROPERTY()
	TObjectPtr<UGameplayBehavior> GameplayBehavior;

	UPROPERTY()
	FArcGameplayInteractionContext GameplayInteractionContext;
};

/**
* Task that runs an async environment query and outputs the result to an outside parameter. Supports Actor and vector types EQS.
* The task is usually run in a sibling state to the result user will be with the data being stored in the parent state's parameters.
* - Parent (Has an EQS result parameter)
*	- Run Env Query (If success go to Use Query Result)
*	- Use Query Result
*/
USTRUCT(meta = (DisplayName = "Arc Mass Use Smart Object", Category = "Common"))
struct FArcMassUseSmartObjectTask: public FMassStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcMassUseSmartObjectTaskInstanceData;

	FArcMassUseSmartObjectTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	
	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual void GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const override;
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transitio) const override;

	UPROPERTY(EditAnywhere)
	bool bAlwaysRunMassBehavior = true;

	TStateTreeExternalDataHandle<FMassSmartObjectUserFragment> SmartObjectUserHandle;
	TStateTreeExternalDataHandle<FMassMoveTargetFragment> MoveTargetHandle;
	
	TStateTreeExternalDataHandle<USmartObjectSubsystem> SmartObjectSubsystemHandle;
	TStateTreeExternalDataHandle<UMassSignalSubsystem> MassSignalSubsystemHandle;
};

