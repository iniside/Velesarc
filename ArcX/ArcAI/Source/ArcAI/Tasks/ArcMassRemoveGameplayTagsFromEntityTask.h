#pragma once
#include "ArcMassStateTreeRunEnvQueryTask.h"
#include "GameplayTagContainer.h"
#include "ArcMass/ArcMassGameplayTagContainerFragment.h"

#include "ArcMassRemoveGameplayTagsFromEntityTask.generated.h"

/** Instance data for FArcMassRemoveGameplayTagsFromEntityTask. Holds the tags to remove from the entity. */
USTRUCT()
struct FArcMassRemoveGameplayTagsFromEntityTaskInstanceData
{
	GENERATED_BODY()

	/** The gameplay tags to remove from the entity's Mass gameplay tag fragment (not the GAS AbilitySystemComponent). */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FGameplayTagContainer Tags;
};

/**
 * Instant task that removes gameplay tags from the entity's Mass gameplay tag fragment
 * (FArcMassGameplayTagContainerFragment). This operates on Mass-level tags, not the GAS AbilitySystemComponent.
 */
USTRUCT(meta = (DisplayName = "Arc Mass Remove Gameplay Tags From Entity", Category = "Arc|Common", ToolTip = "Instant task. Removes gameplay tags from the entity's Mass tag fragment."))
struct FArcMassRemoveGameplayTagsFromEntityTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcMassRemoveGameplayTagsFromEntityTaskInstanceData;

public:
	FArcMassRemoveGameplayTagsFromEntityTask();
	
protected:
	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual void GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const override;
	
	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

	/** Handle to the entity's Mass gameplay tag container fragment. */
	TStateTreeExternalDataHandle<FArcMassGameplayTagContainerFragment> MassGameplayTagsHandle;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif
};
