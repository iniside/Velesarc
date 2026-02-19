// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcMassStateTreeRunEnvQueryTask.h"
#include "MassProcessor.h"
#include "ArcMass/ArcMassGameplayTagContainerFragment.h"
#include "ArcMassCooldownTask.generated.h"

struct FArcMassCooldownTaskFragment;
class UMassStateTreeSubsystem;

USTRUCT()
struct FArcMassCooldownTaskInstanceData
{
	GENERATED_BODY()

	/** Delay before the task ends. Default (0 or any negative) will run indefinitely, so it requires a transition in the state tree to stop it. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FGameplayTag CooldownTag;

	UPROPERTY(EditAnywhere, Category = Parameter)
	float Duration;
};

USTRUCT(meta = (DisplayName = "Arc Mass Cooldown Task", Category = "Arc|Common"))
struct FArcMassCooldownTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()
	
	using FInstanceDataType = FArcMassCooldownTaskInstanceData;

public:
	FArcMassCooldownTask();
	
protected:
	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual void GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const override;
	
	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	
	TStateTreeExternalDataHandle<FArcMassCooldownTaskFragment> MassCooldownHandle;
	TStateTreeExternalDataHandle<FArcMassGameplayTagContainerFragment> MassGameplayTagHandle;
};

USTRUCT()
struct FArcMassIsOnCooldownConditionInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Parameter)
	FGameplayTag CooldownTag;
};

USTRUCT(DisplayName="Arc Mass Is On Cooldown Condition", meta = (Category = "Arc|Common"))
struct FArcMassIsOnCooldownCondition : public FMassStateTreeConditionBase
{
	GENERATED_BODY()
	
public:
	using FInstanceDataType = FArcMassIsOnCooldownConditionInstanceData;

	FArcMassIsOnCooldownCondition() = default;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	
	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual void GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const override;
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
	
	TStateTreeExternalDataHandle<FArcMassGameplayTagContainerFragment> MassGameplayTagsHandle;
};

USTRUCT()
struct FArcCooldownTaskItem
{
	GENERATED_BODY()
	
	UPROPERTY()
	FGameplayTag CooldownTag;
	
	UPROPERTY()
	float CooldownDuration = 0.0f;
	
	UPROPERTY()
	float CurrentDuration = 0.0f;
};

USTRUCT()
struct ARCAI_API FArcMassCooldownTaskFragment : public FMassFragment
{
	GENERATED_BODY()
	
public:
	UPROPERTY()
	TArray<FArcCooldownTaskItem> Cooldowns;
};

template<>
struct TMassFragmentTraits<FArcMassCooldownTaskFragment> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};

USTRUCT()
struct ARCAI_API FArcCooldownTaskTag : public FMassTag
{
	GENERATED_BODY()
};

/**
 * 
 */
UCLASS()
class ARCAI_API UArcMassCooldownTaskProcessor : public UMassProcessor
{
	GENERATED_BODY()
	
	UArcMassCooldownTaskProcessor();

	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& ) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery EntityQuery_Conditional;
	
	UPROPERTY(Transient)
	TObjectPtr<UMassStateTreeSubsystem> MassStateTreeSubsystem = nullptr;
};
