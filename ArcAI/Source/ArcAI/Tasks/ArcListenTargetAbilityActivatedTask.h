#pragma once
#include "MassStateTreeTypes.h"
#include "StateTreeDelegate.h"
#include "UObject/ObjectPtr.h"

#include "ArcListenTargetAbilityActivatedTask.generated.h"

class AActor;

USTRUCT()
struct FArcListenTargetAbilityActivatedTaskInstanceData
{
	GENERATED_BODY()

	/** The target actor whose AbilitySystemComponent will be monitored for ability activations. Must implement IAbilitySystemInterface. */
	UPROPERTY(EditAnywhere, Category = Input)
	TObjectPtr<AActor> InputTarget;

	/** Delegate dispatcher that fires whenever any gameplay ability is activated on the target's ASC. Use this to trigger state tree transitions. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FStateTreeDelegateDispatcher OnAbilityActivated;

	/** Internal handle used to unsubscribe from the ability activation callback on exit. */
	FDelegateHandle OnAbilityActivatedHandle;
};

/**
 * Latent task that listens for any gameplay ability activation on a target actor's AbilitySystemComponent.
 *
 * On EnterState, subscribes to AbilityActivatedCallbacks on the target's ASC and returns Running.
 * When any ability activates, the OnAbilityActivated delegate dispatcher fires and the entity is signaled.
 * On ExitState, the callback is unregistered.
 *
 * This is a latent event listener — it should be the primary or only task in its state,
 * and state transitions should be driven by the OnAbilityActivated delegate.
 */
USTRUCT(DisplayName="Arc Listen Target Ability Activated", meta = (Category = "Arc|Events", ToolTip = "Latent task that listens for gameplay ability activations on a target actor's ASC. Fires OnAbilityActivated delegate when any ability is activated. Should be the primary task in its state." ))
struct FArcListenTargetAbilityActivatedTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcListenTargetAbilityActivatedTaskInstanceData;

	FArcListenTargetAbilityActivatedTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	
	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual void GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const override;
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif
};

/** Generic state tree event payload carrying gameplay tags. Can be used to pass tag-based event data through state tree transitions. */
USTRUCT()
struct FArcMassStateTreeEvent
{
	GENERATED_BODY()

public:
	/** Gameplay tags associated with this event, used for filtering or identifying the event type. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FGameplayTagContainer Tags;
};