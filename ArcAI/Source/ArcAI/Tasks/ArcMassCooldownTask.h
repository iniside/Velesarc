// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassStateTreeTypes.h"
#include "ArcMass/ArcMassGameplayTagContainerFragment.h"
#include "ArcMassCooldownTask.generated.h"

struct FArcMassCooldownTaskFragment;
class UMassStateTreeSubsystem;

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
	float Duration;
};

/**
 * Instant task that starts a cooldown for the entity, identified by a gameplay tag.
 * The cooldown duration is tracked in FArcMassCooldownTaskFragment and ticked down by UArcMassCooldownTaskProcessor.
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

	/** Handle to the entity's cooldown fragment, which stores all active cooldowns. */
	TStateTreeExternalDataHandle<FArcMassCooldownTaskFragment> MassCooldownHandle;
	/** Handle to the entity's Mass gameplay tag container, used to add/remove cooldown indicator tags. */
	TStateTreeExternalDataHandle<FArcMassGameplayTagContainerFragment> MassGameplayTagHandle;
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
	
	/** Handle to the entity's Mass gameplay tag container, used to check for cooldown indicator tags. */
	TStateTreeExternalDataHandle<FArcMassGameplayTagContainerFragment> MassGameplayTagsHandle;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif
};

/** Represents a single active cooldown entry on an entity. Tracked inside FArcMassCooldownTaskFragment. */
USTRUCT()
struct FArcCooldownTaskItem
{
	GENERATED_BODY()

	/** The gameplay tag that identifies this cooldown. */
	UPROPERTY()
	FGameplayTag CooldownTag;

	/** The total duration of the cooldown in seconds. */
	UPROPERTY()
	float CooldownDuration = 0.0f;

	/** The remaining time in seconds before this cooldown expires. Decremented each tick by UArcMassCooldownTaskProcessor. */
	UPROPERTY()
	float CurrentDuration = 0.0f;
};

/** Mass fragment that stores all active cooldowns for an entity. Processed by UArcMassCooldownTaskProcessor. */
USTRUCT()
struct ARCAI_API FArcMassCooldownTaskFragment : public FMassFragment
{
	GENERATED_BODY()

public:
	/** Array of currently active cooldowns on this entity. Entries are removed once their duration expires. */
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

/** Mass tag added to entities that have at least one active cooldown. Used by UArcMassCooldownTaskProcessor for query filtering. */
USTRUCT()
struct ARCAI_API FArcCooldownTaskTag : public FMassTag
{
	GENERATED_BODY()
};

/**
 * Mass processor that ticks down all active cooldowns on entities with FArcMassCooldownTaskFragment.
 * When a cooldown expires, its corresponding gameplay tag is removed and the entry is cleaned up.
 * Removes the FArcCooldownTaskTag from entities with no remaining active cooldowns.
 */
UCLASS()
class ARCAI_API UArcMassCooldownTaskProcessor : public UMassProcessor
{
	GENERATED_BODY()
	
	UArcMassCooldownTaskProcessor();

	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& ) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	/** Entity query for entities with active cooldowns. */
	FMassEntityQuery EntityQuery_Conditional;

	/** Cached reference to the MassStateTreeSubsystem for signaling StateTree re-evaluation. */
	UPROPERTY(Transient)
	TObjectPtr<UMassStateTreeSubsystem> MassStateTreeSubsystem = nullptr;
};
