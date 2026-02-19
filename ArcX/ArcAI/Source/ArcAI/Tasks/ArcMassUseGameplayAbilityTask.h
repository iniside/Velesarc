#pragma once
#include "GameplayAbilitySpecHandle.h"
#include "MassActorSubsystem.h"
#include "MassStateTreeTypes.h"

#include "ArcMassUseGameplayAbilityTask.generated.h"

class UGameplayEffect;
class UGameplayAbility;

USTRUCT()
struct FArcMassUseGameplayAbilityTaskInstanceData
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Parameter")
	TSubclassOf<UGameplayAbility> AbilityClassToActivate;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	bool bUseEvent = false;
	
	UPROPERTY(EditAnywhere, Category = "Parameter")
	FGameplayTag EventTag;
	
	UPROPERTY(EditAnywhere, Category = "Parameter")
	TObjectPtr<AActor> TargetActor;
	
	UPROPERTY(EditAnywhere, Category = "Parameter")
	TObjectPtr<AActor> Instigator;
	
	UPROPERTY(EditAnywhere, Category = "Parameter")
	FVector Location;
	
	UPROPERTY(EditAnywhere, Category = "Parameter")
	FGameplayTagContainer ActivateAbilityWithTags;

	FDelegateHandle OnAbilityEndedHandle;
	FGameplayAbilitySpecHandle AbilityHandle;
};

USTRUCT(meta = (DisplayName = "Arc Mass Actor Use Gameplay Ability", Category = "Arc|Action"))
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
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

	TStateTreeExternalDataHandle<FMassActorFragment> MassActorHandle;
};

USTRUCT()
struct FArcMassApplyGameplayEffectToOwnerTaskInstanceData
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Parameter")
	TSubclassOf<UGameplayEffect> GameplayEffectClass;
};

USTRUCT(meta = (DisplayName = "Arc Mass Apply Gameplay Effect To Owner Task", Category = "AI|Action"))
struct FArcMassApplyGameplayEffectToOwnerTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()
public:
	using FInstanceDataType = FArcMassApplyGameplayEffectToOwnerTaskInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	FArcMassApplyGameplayEffectToOwnerTask();
	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual void GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const override;
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	
	TStateTreeExternalDataHandle<FMassActorFragment> MassActorHandle;
};
