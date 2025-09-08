#pragma once
#include "GameplayAbilitySpecHandle.h"
#include "MassActorSubsystem.h"
#include "MassStateTreeTypes.h"

#include "ArcMassUseGameplayAbilityTask.generated.h"

class UGameplayAbility;

USTRUCT()
struct FArcMassUseGameplayAbilityTaskInstanceData
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Parameter")
	TSubclassOf<UGameplayAbility> AbilityClassToActivate;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	FGameplayTagContainer ActivateAbilityWithTags;

	FDelegateHandle OnAbilityEndedHandle;
	FGameplayAbilitySpecHandle AbilityHandle;
};

USTRUCT(meta = (DisplayName = "Arc Mass Actor Use Gameplay Ability", Category = "AI|Action"))
struct FArcMassUseGameplayAbilityTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()
public:
	using FInstanceDataType = FArcMassUseGameplayAbilityTaskInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	FArcMassUseGameplayAbilityTask();
	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual void GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const override;
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	//virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	//virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

	TStateTreeExternalDataHandle<FMassActorFragment> MassActorHandle;
};
