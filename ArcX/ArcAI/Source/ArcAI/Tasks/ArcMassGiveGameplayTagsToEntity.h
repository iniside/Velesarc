#pragma once
#include "MassStateTreeTypes.h"
#include "ArcMass/ArcMassGameplayTagContainerFragment.h"
#include "ArcMassGiveGameplayTagsToEntity.generated.h"

USTRUCT()
struct FArcMassGiveGameplayTagsToEntityInstanceData
{
	GENERATED_BODY()

	/** Delay before the task ends. Default (0 or any negative) will run indefinitely, so it requires a transition in the state tree to stop it. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FGameplayTagContainer Tags;
};

/**
 * Stop, and stand on current navmesh location
 */
USTRUCT(meta = (DisplayName = "Arc Mass Give Gameplay Tags To Entity"))
struct FArcMassGiveGameplayTagsToEntityTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcMassGiveGameplayTagsToEntityInstanceData;

public:
	FArcMassGiveGameplayTagsToEntityTask();
	
protected:
	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual void GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const override;
	
	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

	TStateTreeExternalDataHandle<FArcMassGameplayTagContainerFragment> MassGameplayTagsHandle;
};