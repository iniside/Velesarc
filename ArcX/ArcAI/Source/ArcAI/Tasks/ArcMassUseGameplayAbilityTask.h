#pragma once
#include "GameplayAbilitySpecHandle.h"
#include "MassActorSubsystem.h"
#include "MassStateTreeTypes.h"

#include "ArcMassUseGameplayAbilityTask.generated.h"

class UGameplayEffect;
class UGameplayAbility;

/** Instance data for FArcMassUseGameplayAbilityTask. Holds ability class, activation parameters, and runtime handles. */
USTRUCT()
struct FArcMassUseGameplayAbilityTaskInstanceData
{
	GENERATED_BODY()

public:
	/** The gameplay ability class to activate on the entity's AbilitySystemComponent. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	TSubclassOf<UGameplayAbility> AbilityClassToActivate;

	/** If true, activates the ability via gameplay event (using EventTag) instead of direct activation. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	bool bUseEvent = false;

	/** Gameplay tag used as the event trigger when bUseEvent is true. Ignored for direct activation. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	FGameplayTag EventTag;

	/** Optional target actor passed in the ability's event data or target data. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	TObjectPtr<AActor> TargetActor;

	/** Optional instigator actor passed in the ability's event data. Typically the entity's own actor. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	TObjectPtr<AActor> Instigator;

	/** Optional world-space location passed in the ability's event data (e.g., for targeted abilities). */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	FVector Location;

	/** Additional gameplay tags required for ability activation. The ability must match these tags to activate. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	FGameplayTagContainer ActivateAbilityWithTags;

	/** Internal: delegate handle for the AbilityEnded callback, used to detect when the ability finishes. */
	FDelegateHandle OnAbilityEndedHandle;

	/** Internal: handle to the activated ability spec, used for cleanup on ExitState. */
	FGameplayAbilitySpecHandle AbilityHandle;
};

/**
 * Latent StateTree task that activates a gameplay ability on the entity's actor via its AbilitySystemComponent.
 * Supports both direct activation and event-based activation (via bUseEvent + EventTag).
 * EnterState activates the ability and returns Running. The task completes when the ability ends,
 * detected via AbilityEndedCallbacks. ExitState cancels the ability if still active.
 * This should be the primary task in its state since it is latent.
 */
USTRUCT(meta = (DisplayName = "Arc Mass Actor Use Gameplay Ability", Category = "Arc|Action", ToolTip = "Latent task that activates a gameplay ability and returns Running until the ability ends."))
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

	/** Handle to the entity's actor fragment, used to retrieve the AActor and its AbilitySystemComponent. */
	TStateTreeExternalDataHandle<FMassActorFragment> MassActorHandle;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif
};

/** Instance data for FArcMassApplyGameplayEffectToOwnerTask. Holds the GameplayEffect class to apply. */
USTRUCT()
struct FArcMassApplyGameplayEffectToOwnerTaskInstanceData
{
	GENERATED_BODY()

public:
	/** The GameplayEffect class to apply to the entity's actor. Applied at default level with the actor as both source and target. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	TSubclassOf<UGameplayEffect> GameplayEffectClass;
};

/**
 * Instant StateTree task that applies a GameplayEffect to the entity's actor.
 * EnterState applies the GE via the actor's AbilitySystemComponent and immediately returns Succeeded.
 * Can be combined with other tasks in the same state.
 */
USTRUCT(meta = (DisplayName = "Arc Mass Apply Gameplay Effect To Owner Task", Category = "AI|Action", ToolTip = "Instant task that applies a GameplayEffect to the entity's actor and immediately succeeds."))
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

	/** Handle to the entity's actor fragment, used to retrieve the AActor and its AbilitySystemComponent for GE application. */
	TStateTreeExternalDataHandle<FMassActorFragment> MassActorHandle;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif
};
