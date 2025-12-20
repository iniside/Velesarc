#pragma once
#include "ArcMassStateTreeRunEnvQueryTask.h"
#include "MassEntityFragments.h"
#include "MassEQSBlueprintLibrary.h"
#include "Containers/Ticker.h"

#include "ArcMassObserveDistanceChanged.generated.h"

USTRUCT()
struct FArcMassObserveDistanceChangedTaskInstanceData
{
	GENERATED_BODY()

	/** Delay before the task ends. Default (0 or any negative) will run indefinitely, so it requires a transition in the state tree to stop it. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	TObjectPtr<AActor> TargetActor;

	UPROPERTY(EditAnywhere, Category = Parameter)
	FMassEnvQueryEntityInfoBlueprintWrapper TargetEntity;
	
	UPROPERTY(EditAnywhere, Category = Parameter)
	bool bUseCompareLocation = false;
	
	UPROPERTY(EditAnywhere, Category = Parameter)
	FVector CompareLocation;
	
	UPROPERTY(EditAnywhere, Category = Parameter)
	float DistanceThreshold = 100.f;
	
	UPROPERTY(EditAnywhere, Category = Parameter)
	FStateTreeDelegateDispatcher OnDistanceChanged;
	
	FTSTicker::FDelegateHandle TickDelegate;
};

USTRUCT(meta = (DisplayName = "Arc Mass Observe Distance Changed"))
struct FArcMassObserveDistanceChangedTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()
	
	using FInstanceDataType = FArcMassObserveDistanceChangedTaskInstanceData;

public:
	FArcMassObserveDistanceChangedTask();
	
protected:
	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual void GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const override;
	
	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	
	TStateTreeExternalDataHandle<FTransformFragment> TransformFragment;
};