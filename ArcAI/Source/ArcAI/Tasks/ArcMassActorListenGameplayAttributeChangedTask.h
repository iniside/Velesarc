// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "MassActorSubsystem.h"
#include "StateTreeDelegate.h"
#include "AttributeSet.h"
#include "MassStateTreeTypes.h"

#include "ArcMassActorListenGameplayAttributeChangedTask.generated.h"

/** Condition that controls when the OnAttributeChanged delegate should fire.
 * Allows filtering attribute change events based on comparing the new value, old value, or difference against ConditionValue. */
UENUM()
enum class EArcAttributeBroadcastCondition : uint8
{
	/** Always broadcast on any attribute change. */
	Always,
	/** Only broadcast when the new attribute value is greater than ConditionValue. */
	NewValueGreater,
	/** Only broadcast when the new attribute value is less than ConditionValue. */
	NewValueLess,
	/** Only broadcast when the old attribute value was greater than ConditionValue. */
	OldValueGreater,
	/** Only broadcast when the old attribute value was less than ConditionValue. */
	OldValueLess,
	/** Only broadcast when the difference (NewValue - OldValue) is greater than ConditionValue. */
	DifferenceValueGreater,
	/** Only broadcast when the difference (NewValue - OldValue) is less than ConditionValue. */
	DifferenceValueLess,
};

USTRUCT()
struct FArcMassActorListenGameplayAttributeChangedTaskInstanceData
{
	GENERATED_BODY()

	/** The gameplay attribute to monitor for value changes (e.g., Health, Mana). */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FGameplayAttribute Attribute;

	/** Delegate dispatcher that fires when the attribute changes and the BroadcastCondition is met. Use this to trigger state tree transitions. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FStateTreeDelegateDispatcher OnAttributeChanged;

	/** Condition that must be satisfied for the delegate to fire. Controls filtering of attribute change events. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	EArcAttributeBroadcastCondition BroadcastCondition = EArcAttributeBroadcastCondition::Always;

	/** Threshold value used by the BroadcastCondition for comparisons (e.g., only fire when NewValue > ConditionValue). */
	UPROPERTY(EditAnywhere, Category = Parameter)
	float ConditionValue = 0.f;

	/** The new value of the attribute after the change. Available after OnAttributeChanged fires. */
	UPROPERTY(EditAnywhere, Category = Output)
	float NewValue = 0.f;

	/** The previous value of the attribute before the change. Available after OnAttributeChanged fires. */
	UPROPERTY(EditAnywhere, Category = Output)
	float OldValue = 0.f;

	/** The difference between NewValue and OldValue (NewValue - OldValue). Available after OnAttributeChanged fires. */
	UPROPERTY(EditAnywhere, Category = Output)
	float Difference = 0.f;

	/** Internal handle used to unsubscribe from the attribute change delegate on exit. */
	FDelegateHandle OnAttributeChangedDelegateHandle;
};

/**
 * Latent task that monitors a gameplay attribute on the entity's actor for value changes.
 *
 * On EnterState, retrieves the entity's actor via FMassActorFragment, gets its AbilitySystemComponent,
 * and registers a delegate on the specified Attribute. Returns Running.
 * When the attribute changes and the BroadcastCondition is met, outputs NewValue/OldValue/Difference
 * and fires the OnAttributeChanged delegate dispatcher, signaling the entity.
 *
 * This is a latent event listener — it should be the primary or only task in its state,
 * and state transitions should be driven by the OnAttributeChanged delegate.
 */
USTRUCT(meta = (DisplayName = "Arc Mass Actor Listen Gameplay Attribute Changed Task", Category = "Arc|Events", ToolTip = "Latent task that monitors a gameplay attribute for value changes. Fires OnAttributeChanged with NewValue/OldValue/Difference when the BroadcastCondition is met. Should be the primary task in its state."))
struct FArcMassActorListenGameplayAttributeChangedTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()
	
	using FInstanceDataType = FArcMassActorListenGameplayAttributeChangedTaskInstanceData;

public:
	FArcMassActorListenGameplayAttributeChangedTask();
	
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
