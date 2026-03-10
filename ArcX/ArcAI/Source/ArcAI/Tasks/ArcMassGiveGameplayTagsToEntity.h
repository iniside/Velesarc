#pragma once
#include "MassStateTreeTypes.h"
#include "ArcMass/ArcMassGameplayTagContainerFragment.h"
#include "ArcMassGiveGameplayTagsToEntity.generated.h"

/** Instance data for FArcMassGiveGameplayTagsToEntityTask. Holds the tags to add to the entity. */
USTRUCT()
struct FArcMassGiveGameplayTagsToEntityInstanceData
{
	GENERATED_BODY()

	/** The gameplay tags to add to the entity's Mass gameplay tag fragment (not the GAS AbilitySystemComponent). */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FGameplayTagContainer Tags;
};

/**
 * Instant task that adds gameplay tags to the entity's Mass gameplay tag fragment
 * (FArcMassGameplayTagContainerFragment), not the GAS AbilitySystemComponent.
 * When bRemoveOnExit is true, the tags are automatically removed when the state is exited.
 */
USTRUCT(meta = (DisplayName = "Arc Mass Give Gameplay Tags To Entity", Category = "Arc|Common", ToolTip = "Instant task. Adds gameplay tags to the entity's Mass tag fragment. Optionally removes them on state exit."))
struct FArcMassGiveGameplayTagsToEntityTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcMassGiveGameplayTagsToEntityInstanceData;

public:
	FArcMassGiveGameplayTagsToEntityTask();

	/** If true, the added tags are automatically removed from the entity when this state is exited. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	bool bRemoveOnExit = false;
	
protected:
	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual void GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const override;
	
	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	
	/** Handle to the entity's Mass gameplay tag container fragment. */
	TStateTreeExternalDataHandle<FArcMassGameplayTagContainerFragment> MassGameplayTagsHandle;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif
};