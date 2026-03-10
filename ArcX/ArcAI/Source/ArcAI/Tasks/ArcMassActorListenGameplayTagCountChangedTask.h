// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcMassStateTreeRunEnvQueryTask.h"
#include "GameplayEffectTypes.h"
#include "MassActorSubsystem.h"
#include "UObject/Object.h"
#include "ArcMassActorListenGameplayTagCountChangedTask.generated.h"

USTRUCT()
struct FArcMassActorListenGameplayTagCountChangedTaskInstanceData
{
	GENERATED_BODY()

	/** The gameplay tag to monitor for count changes on the entity's actor's ASC. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FGameplayTag Tag;

	/** Type of tag event to listen for: NewOrRemoved (tag added/removed entirely) or AnyCountChange (any count modification). */
	UPROPERTY(EditAnywhere, Category = Parameter)
	TEnumAsByte<EGameplayTagEventType::Type> EventType = EGameplayTagEventType::NewOrRemoved;

	/** Minimum tag count threshold. The delegate only fires when NewCount >= MinimumCount. Set to 0 to fire on any change. */
	UPROPERTY(EditAnywhere, Category = Output)
	int32 MinimumCount = 0;

	/** Delegate dispatcher that fires when the tag count changes and meets the MinimumCount threshold. Use this to trigger state tree transitions. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FStateTreeDelegateDispatcher OnGameplayTagCountChanged;;

	/** The new count of the monitored tag after the change. Available after OnGameplayTagCountChanged fires. */
	UPROPERTY(EditAnywhere, Category = Output)
	int32 NewCount = 0;

	/** Internal handle used to unsubscribe from the tag count change delegate on exit. */
	FDelegateHandle OnGameplayEffectCountChangedDelegateHandle;
};

/**
 * Latent task that monitors a gameplay tag count on the entity's actor's AbilitySystemComponent.
 *
 * On EnterState, retrieves the entity's actor via FMassActorFragment, gets its ASC,
 * and registers a tag event delegate via RegisterGameplayTagEvent. Returns Running.
 * When the tag count changes and meets the MinimumCount threshold, outputs NewCount
 * and fires OnGameplayTagCountChanged, signaling the entity.
 *
 * This is a latent event listener — it should be the primary or only task in its state,
 * and state transitions should be driven by the OnGameplayTagCountChanged delegate.
 */
USTRUCT(meta = (DisplayName = "Arc Mass Actor Listen Gameplay Tag Count Changed Task", Category = "Arc|Events", ToolTip = "Latent task that monitors a gameplay tag count on the entity's actor. Fires OnGameplayTagCountChanged when the tag count changes and meets MinimumCount threshold. Should be the primary task in its state."))
struct FArcMassActorListenGameplayTagCountChangedTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()
	
	using FInstanceDataType = FArcMassActorListenGameplayTagCountChangedTaskInstanceData;

public:
	FArcMassActorListenGameplayTagCountChangedTask();
	
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
