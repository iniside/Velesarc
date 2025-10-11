#pragma once
#include "MassActorSubsystem.h"
#include "MassStateTreeTypes.h"

#include "ArcMassActorPlayAnimMontage.generated.h"
class UAnimMontage;

USTRUCT()
struct FArcMassActorPlayAnimMontageInstanceData
{
	GENERATED_BODY()

	/** Delay before the task ends. Default (0 or any negative) will run indefinitely, so it requires a transition in the state tree to stop it. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	TObjectPtr<UAnimMontage> AnimMontage;
};

/**
 * Stop, and stand on current navmesh location
 */
USTRUCT(meta = (DisplayName = "Arc Mass Actor Play Anim Montage"))
struct FArcMassActorPlayAnimMontageTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcMassActorPlayAnimMontageInstanceData;

public:
	FArcMassActorPlayAnimMontageTask();
	
protected:
	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual void GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const override;
	
	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	//virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;

	TStateTreeExternalDataHandle<FMassActorFragment> MassActorFragment;
};