// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "MassActorSubsystem.h"
#include "MassStateTreeTypes.h"
#include "StateTreeDelegate.h"
#include "UObject/Object.h"
#include "ArcMassActorListenGameplayEffectAppliedTask.generated.h"

USTRUCT()
struct FArcMassActorListenGameplayEffectAppliedTaskInstanceData
{
	GENERATED_BODY()

	/** Tags that the applied gameplay effect must have for the delegate to fire. Leave empty to match any effect. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FGameplayTagContainer RequiredTags;

	/** Tags that, if present on the applied gameplay effect, will prevent the delegate from firing. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FGameplayTagContainer DenyTags;

	/** Delegate dispatcher that fires when a gameplay effect matching the tag filters is applied to the entity's actor. Use this to trigger state tree transitions. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FStateTreeDelegateDispatcher OnEffectAppliedToSelf;;

	/** The duration of the applied gameplay effect. Available after OnEffectAppliedToSelf fires. 0 for instant effects. */
	UPROPERTY(EditAnywhere, Category = Output)
	float Duration = 0.f;

	/** Internal handle used to unsubscribe from the effect applied delegate on exit. */
	FDelegateHandle OnAppliedToSelfDelegateHandle;
};

/**
 * Latent task that listens for gameplay effects applied to the entity's actor.
 *
 * On EnterState, retrieves the entity's actor via FMassActorFragment, gets its AbilitySystemComponent,
 * and registers OnGameplayEffectAppliedDelegateToSelf. Returns Running.
 * When an effect is applied that passes RequiredTags/DenyTags filtering, outputs the Duration
 * and fires OnEffectAppliedToSelf, signaling the entity.
 *
 * This is a latent event listener — it should be the primary or only task in its state,
 * and state transitions should be driven by the OnEffectAppliedToSelf delegate.
 */
USTRUCT(meta = (DisplayName = "Arc Mass Actor Listen Gameplay Effect Applied Task", Category = "Arc|Events", ToolTip = "Latent task that listens for gameplay effects applied to the entity's actor. Filters by RequiredTags/DenyTags. Fires OnEffectAppliedToSelf with Duration. Should be the primary task in its state."))
struct FArcMassActorListenGameplayEffectAppliedTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()
	
	using FInstanceDataType = FArcMassActorListenGameplayEffectAppliedTaskInstanceData;

public:
	FArcMassActorListenGameplayEffectAppliedTask();
	
protected:
	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual void GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const override;
	
	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif

	TStateTreeExternalDataHandle<FMassActorFragment> MassActorFragment;
};
