// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "MassActorSubsystem.h"
#include "MassStateTreeTypes.h"
#include "Components/SceneComponent.h"
#include "UObject/Object.h"
#include "ArcMassSelectApproachSlotTask.generated.h"

/** Describes a single dynamic approach slot: its tag, world-space transform, and current owner. */
USTRUCT(BlueprintType)
struct FArcDynamicSlotInfo
{
	GENERATED_BODY()

	/** Gameplay tag identifying the slot type (e.g., melee attack position, flanking position). Filtered by "AI, DynamicSlots" categories. */
	UPROPERTY(EditAnywhere, meta = (Categories = "AI, DynamicSlots"))
	FGameplayTag SlotTag;

	/** The local-space transform of this slot relative to the owning component. Editable as a widget in the viewport. */
	UPROPERTY(EditAnywhere, meta = (MakeEditWidget = true))
	FTransform SlotLocation;

	/** The object that currently owns (has claimed) this slot, or nullptr if the slot is free. */
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UObject> Owner;

	/** Mass entity handle of the entity that currently owns this slot. */
	UPROPERTY()
	FMassEntityHandle OwnerHandle;
};

/**
 * Scene component that provides dynamic approach slots around an actor.
 * AI entities claim slots via FArcMassSelectApproachSlotTask to avoid overlapping approach positions.
 */
UCLASS(BlueprintType, editinlinenew, meta = (BlueprintSpawnableComponent))
class UArcDynamicSlotComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	/** Array of configurable approach slots with per-slot tags and transforms. Editable in the viewport. */
	UPROPERTY(EditAnywhere, Category = "Slots", meta = (MakeEditWidget = true))
	TArray<FArcDynamicSlotInfo> Slots;

	/** Legacy slot transforms (deprecated in favor of Slots array with FArcDynamicSlotInfo). */
	UPROPERTY(EditAnywhere, meta = (MakeEditWidget = true))
	TArray<FTransform> SlotLocation;

	/** Indices of currently claimed slots. */
	TArray<int32> TakenSlots;

	/** Claims the first available slot matching SlotTag. Outputs the world location and slot index. */
	void ClaimSlot(const FGameplayTag& SlotTag, UObject* Owner, FVector& OutLocation, int32 &OutSlotIndex);

	/** Releases a previously claimed slot by index, making it available again. */
	void FreeSlot(int32 SlotIndex);
};

/** Instance data for FArcMassSelectApproachSlotTask. Holds the target actor, slot filter tag, and outputs. */
USTRUCT()
struct FArcMassSelectApproachSlotTaskInstanceData
{
	GENERATED_BODY()

	/** The target actor whose UArcDynamicSlotComponent will be queried for available slots. */
	UPROPERTY(EditAnywhere, Category = Input)
	TObjectPtr<AActor> Target;

	/** Gameplay tag filter to match against slot tags. Only slots with this tag will be considered. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FGameplayTag SlotTag;

	/** World-space location of the claimed slot. Valid only when bSuccess is true. */
	UPROPERTY(EditAnywhere, Category = Output)
	FVector WorldLocation;

	/** True if a slot was successfully claimed, false if no matching slot was available. */
	UPROPERTY(EditAnywhere, Category = Output)
	bool bSuccess = false;

	/** Internal index of the claimed slot. Used to free the slot on ExitState. Not intended for external binding. */
	UPROPERTY(VisibleAnywhere)
	int32 SlotIndex = INDEX_NONE;
};

/**
 * Instant task that claims an approach slot on the target actor's UArcDynamicSlotComponent.
 * Outputs the world-space location of the claimed slot. The slot is automatically freed when the state is exited.
 * Succeeds immediately if a matching slot is available, fails otherwise.
 */
USTRUCT(DisplayName="Arc Mass Select Approach Slot Task", meta = (Category = "Arc|Action", ToolTip = "Instant task. Claims an approach slot on the target's DynamicSlotComponent. Frees slot on state exit."))
struct FArcMassSelectApproachSlotTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcMassSelectApproachSlotTaskInstanceData;

	FArcMassSelectApproachSlotTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	
	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual void GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const override;
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif

	/** Handle to the entity's actor fragment, used to identify the claiming entity. */
	TStateTreeExternalDataHandle<FMassActorFragment> MassActorHandle;
};
