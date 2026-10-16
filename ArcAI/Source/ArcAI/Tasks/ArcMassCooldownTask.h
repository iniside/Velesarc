// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassStateTreeTypes.h"
#include "Fragments/ArcOwnedTagsFragment.h"
#include "ArcMass/ArcMassDecayingTags.h"
#include "ArcMassCooldownTask.generated.h"

/** Instance data for FArcMassCooldownTask. Holds the cooldown tag identifier and duration. */
USTRUCT()
struct FArcMassCooldownTaskInstanceData
{
	GENERATED_BODY()

	/** Gameplay tag that uniquely identifies this cooldown. Used to check/query cooldown state via FArcMassIsOnCooldownCondition. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FGameplayTag CooldownTag;

	/** Duration of the cooldown in seconds. The entity will be considered "on cooldown" for this tag until the duration elapses. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	float Duration = 0.f;
};

/**
 * Instant task that starts a cooldown for the entity, identified by a gameplay tag.
 * The cooldown duration is tracked by UArcMassDecayingTagSubsystem and ticked down by UArcMassDecayingTagDecayProcessor.
 * Use FArcMassIsOnCooldownCondition to check whether the entity is still on cooldown for a given tag.
 */
USTRUCT(meta = (DisplayName = "Arc Mass Cooldown Task", Category = "Arc|Common", ToolTip = "Instant task. Starts a tag-based cooldown on the entity for the specified duration."))
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

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif

	/** Handle to the entity's owned tag fragment, used to add/remove cooldown indicator tags. */
	TStateTreeExternalDataHandle<FArcOwnedTagsFragment> MassGameplayTagHandle;
};

/** Instance data for FArcMassIsOnCooldownCondition. Holds the cooldown tag to check. */
USTRUCT()
struct FArcMassIsOnCooldownConditionInstanceData
{
	GENERATED_BODY()

	/** The cooldown tag to check. Returns true if the entity currently has an active cooldown with this tag. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FGameplayTag CooldownTag;
};

/**
 * StateTree condition that checks whether the entity is currently on cooldown for a given gameplay tag.
 * Returns true if the entity has an active cooldown matching the specified tag.
 */
USTRUCT(DisplayName="Arc Mass Is On Cooldown Condition", meta = (Category = "Arc|Common", ToolTip = "Condition. Returns true if the entity has an active cooldown for the specified gameplay tag."))
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
	
	/** Handle to the entity's owned tag fragment, used to check for cooldown indicator tags. */
	TStateTreeExternalDataHandle<FArcOwnedTagsFragment> MassGameplayTagsHandle;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif
};

