// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcMassStateTreeRunEnvQueryTask.h"
#include "MassActorSubsystem.h"
#include "UObject/Object.h"
#include "ArcAddAbilityGameplayTagTask.generated.h"

/** Instance data for FArcAddAbilityGameplayTagTask. Holds the target actor and the GAS tags to add. */
USTRUCT()
struct FArcMassUseGameplayAbilityTagTaskInstanceData
{
	GENERATED_BODY()

public:
	/** The actor whose AbilitySystemComponent will receive the gameplay tags. Typically resolved from the entity's FMassActorFragment. */
	UPROPERTY(EditAnywhere, Category = "Prameter")
	TObjectPtr<AActor> Actor;

	/** The gameplay tags to add to the actor's AbilitySystemComponent (GAS level, not Mass tags). */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	FGameplayTagContainer TagsToAdd;
};
/**
 * Instant task that adds gameplay tags to the entity's actor at the GAS (AbilitySystemComponent) level.
 * Tags are automatically removed when the state is exited via ExitState.
 */
USTRUCT(meta = (DisplayName = "Arc Add AbilityGameplay Tag", Category = "Arc|Action", ToolTip = "Instant task. Adds gameplay tags to the actor's AbilitySystemComponent. Tags are removed on state exit."))
struct ARCAI_API FArcAddAbilityGameplayTagTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()
public:
	using FInstanceDataType = FArcMassUseGameplayAbilityTagTaskInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	FArcAddAbilityGameplayTagTask();
	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual void GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const override;
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	
	/** Handle to the entity's actor fragment, used to resolve the AActor for GAS access. */
	TStateTreeExternalDataHandle<FMassActorFragment> MassActorHandle;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif
};
