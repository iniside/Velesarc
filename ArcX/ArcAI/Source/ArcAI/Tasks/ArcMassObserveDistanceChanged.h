#pragma once
#include "ArcMassStateTreeRunEnvQueryTask.h"
#include "MassEntityFragments.h"
#include "MassEQSBlueprintLibrary.h"
#include "Containers/Ticker.h"

#include "ArcMassObserveDistanceChanged.generated.h"

USTRUCT()
struct FArcMassObserveDistanceChangedTaskInstanceData
{
	GENERATED_BODY()

	/** Target actor to measure distance to. Takes priority over TargetEntity if set. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	TObjectPtr<AActor> TargetActor;

	/** Target Mass entity to measure distance to. Used when TargetActor is not set. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FMassEnvQueryEntityInfoBlueprintWrapper TargetEntity;

	/** If true, uses CompareLocation instead of the entity's own transform as the source position. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	bool bUseCompareLocation = false;

	/** Fixed world location to measure distance from when bUseCompareLocation is true. Overrides the entity's own position. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FVector CompareLocation;

	/** Distance threshold in world units. The OnDistanceChanged delegate fires when the distance exceeds this value. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	float DistanceThreshold = 100.f;

	/** Delegate dispatcher that fires when the distance to the target exceeds DistanceThreshold. Use this to trigger state tree transitions. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FStateTreeDelegateDispatcher OnDistanceChanged;

	/** Internal tick delegate handle used to poll distance each frame. Cleaned up on ExitState. */
	FTSTicker::FDelegateHandle TickDelegate;
};

/**
 * Latent task that monitors the distance between the entity and a target (actor, entity, or fixed location).
 *
 * On EnterState, registers a tick delegate via FTSTicker that polls the distance each frame. Returns Running.
 * When the distance exceeds DistanceThreshold, fires OnDistanceChanged and signals the entity.
 * On ExitState, the tick delegate is removed.
 *
 * Supports three distance sources: TargetActor (AActor), TargetEntity (Mass entity), or CompareLocation (fixed vector).
 *
 * This is a latent event listener — it should be the primary or only task in its state,
 * and state transitions should be driven by the OnDistanceChanged delegate.
 */
USTRUCT(meta = (DisplayName = "Arc Mass Observe Distance Changed", ToolTip = "Latent task that monitors distance to a target. Fires OnDistanceChanged when the distance exceeds DistanceThreshold. Supports actor, entity, or fixed location targets. Should be the primary task in its state."))
struct FArcMassObserveDistanceChangedTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()
	
	using FInstanceDataType = FArcMassObserveDistanceChangedTaskInstanceData;

public:
	FArcMassObserveDistanceChangedTask();
	
protected:
	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual void GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const override;
	
	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif

	TStateTreeExternalDataHandle<FTransformFragment> TransformFragment;
};