// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassStateTreeTypes.h"
#include "StateTreeDelegate.h"
#include "ArcAreaTypes.h"
#include "ArcKnowledgeTypes.h"
#include "MassSmartObjectRequest.h"
#include "ArcMass/ArcMassEntityHandleWrapper.h"
#include "ArcMassListenAreaAssignmentChangedTask.generated.h"

USTRUCT()
struct FArcMassListenAreaAssignmentChangedTaskInstanceData
{
	GENERATED_BODY()

	/** Fires when this entity is assigned to an area slot. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FStateTreeDelegateDispatcher OnAreaAssigned;

	/** Fires when this entity is unassigned from an area slot. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FStateTreeDelegateDispatcher OnAreaUnassigned;

	/** The slot handle involved in the most recent assignment change. */
	UPROPERTY(EditAnywhere, Category = Output)
	FArcAreaSlotHandle SlotHandle;

	/** The Mass entity handle of the area involved in the assignment change. */
	UPROPERTY(EditAnywhere, Category = Output)
	FArcMassEntityHandleWrapper AreaEntity;

	/** The vacancy advertisement handle for the slot (valid on assignment, looked up from auto-vacancy listener). */
	UPROPERTY(EditAnywhere, Category = Output)
	FArcKnowledgeHandle AdvertisementHandle;

	UPROPERTY(EditAnywhere, Category = Output)
	FMassSmartObjectCandidateSlots CandidateSlots;
	
	/** Internal delegate handles for cleanup. */
	FDelegateHandle AssignedHandle;
	FDelegateHandle UnassignedHandle;
};

/**
 * Latent StateTree task that listens for area assignment changes on the current entity.
 *
 * Binds to the per-entity delegates on UArcAreaSubsystem. Fires OnAreaAssigned / OnAreaUnassigned
 * dispatchers when the entity's area assignment changes. Returns Running and stays alive as a listener.
 *
 * Follows the established WeakExecutionContext + BroadcastDelegate + SignalEntities pattern.
 *
 * Typical StateTree composition for area-driven Smart Object usage:
 *
 * Root (outputs: SlotHandle, AreaEntity, AdvertisementHandle, CandidateSlots)
 * |
 * |-- State: Listen [ListenAreaAssignmentChangedTask]
 * |     Latent — stays Running, fires OnAreaAssigned / OnAreaUnassigned delegates
 * |     Populates SlotHandle, AreaEntity, AdvertisementHandle, CandidateSlots on assignment
 * |     -> OnAreaAssigned: go to Bound Task
 * |
 * '-- State: Bound Task (can be empty or do pre-work)
 *       -> Succeeded: go to Use Slot
 *       |
 *       '-- State: Use Slot
 *             Claim the smart object, then use it
 *             Task: [ClaimSmartObjectTask] bind CandidateSlots from listener output
 *             Task: [ArcMassUseSmartObjectTask] bind ClaimHandle from claim output,
 *                    SmartObjectEntityHandle from AreaEntity output
 *             -> Completed: return to Listen
 */
USTRUCT(meta = (DisplayName = "Listen Area Assignment Changed", Category = "Arc|Area|NPC"))
struct ARCAREA_API FArcMassListenAreaAssignmentChangedTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcMassListenAreaAssignmentChangedTaskInstanceData;

	FArcMassListenAreaAssignmentChangedTask();

protected:
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
	virtual FName GetIconName() const override { return FName("StateTreeEditorStyle|Node.Task"); }
	virtual FColor GetIconColor() const override { return FColor(220, 160, 60); }
#endif
};
