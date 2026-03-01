// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "MassStateTreeTypes.h"
#include "ArcAreaTypes.h"
#include "GameplayTagContainer.h"
#include "SmartObjectTypes.h"
#include "ArcAreaQueryTasks.generated.h"

// ------------------------------------------------------------------
// Get Area Info
// ------------------------------------------------------------------

USTRUCT()
struct FArcAreaGetAreaInfoTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Output)
	FArcAreaHandle AreaHandle;

	UPROPERTY(EditAnywhere, Category = Output)
	FGameplayTagContainer AreaTags;

	UPROPERTY(EditAnywhere, Category = Output)
	FVector Location = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category = Output)
	FSmartObjectHandle SmartObjectHandle;
};

/**
 * Reads the current Area entity's info from UArcAreaSubsystem.
 * Requires FArcAreaFragment on the entity.
 */
USTRUCT(meta = (DisplayName = "Arc Get Area Info", Category = "Arc|Area|Self"))
struct ARCAREA_API FArcAreaGetAreaInfoTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcAreaGetAreaInfoTaskInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FName GetIconName() const override { return FName("StateTreeEditorStyle|Node.Task"); }
	virtual FColor GetIconColor() const override { return FColor(60, 160, 220); }
#endif
};

// ------------------------------------------------------------------
// Get Slot Counts
// ------------------------------------------------------------------

USTRUCT()
struct FArcAreaGetSlotCountsTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Output)
	int32 TotalSlots = 0;

	UPROPERTY(EditAnywhere, Category = Output)
	int32 VacantCount = 0;

	UPROPERTY(EditAnywhere, Category = Output)
	int32 AssignedCount = 0;

	UPROPERTY(EditAnywhere, Category = Output)
	int32 ActiveCount = 0;

	UPROPERTY(EditAnywhere, Category = Output)
	int32 DisabledCount = 0;
};

/**
 * Reads the current Area entity's slot occupancy counts.
 * Requires FArcAreaFragment on the entity.
 */
USTRUCT(meta = (DisplayName = "Arc Get Area Slot Counts", Category = "Arc|Area|Self"))
struct ARCAREA_API FArcAreaGetSlotCountsTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcAreaGetSlotCountsTaskInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FName GetIconName() const override { return FName("StateTreeEditorStyle|Node.Task"); }
	virtual FColor GetIconColor() const override { return FColor(60, 160, 220); }
#endif
};

// ------------------------------------------------------------------
// Get Slot State
// ------------------------------------------------------------------

USTRUCT()
struct FArcAreaGetSlotStateTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Input)
	int32 SlotIndex = 0;

	UPROPERTY(EditAnywhere, Category = Output)
	EArcAreaSlotState SlotState = EArcAreaSlotState::Vacant;

	UPROPERTY(EditAnywhere, Category = Output)
	FMassEntityHandle AssignedEntity;
};

/**
 * Gets the state and assigned entity for a specific slot by index.
 * Requires FArcAreaFragment on the entity.
 */
USTRUCT(meta = (DisplayName = "Arc Get Area Slot State", Category = "Arc|Area|Self"))
struct ARCAREA_API FArcAreaGetSlotStateTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcAreaGetSlotStateTaskInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FName GetIconName() const override { return FName("StateTreeEditorStyle|Node.Task"); }
	virtual FColor GetIconColor() const override { return FColor(60, 160, 220); }
#endif
};

// ------------------------------------------------------------------
// Get Slot Definition
// ------------------------------------------------------------------

USTRUCT()
struct FArcAreaGetSlotDefinitionTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Input)
	int32 SlotIndex = 0;

	UPROPERTY(EditAnywhere, Category = Output)
	FGameplayTag RoleTag;

	UPROPERTY(EditAnywhere, Category = Output)
	bool bAutoPostVacancy = false;

	UPROPERTY(EditAnywhere, Category = Output)
	float VacancyRelevance = 0.0f;

	UPROPERTY(EditAnywhere, Category = Output)
	int32 SmartObjectSlotIndex = 0;
};

/**
 * Reads the design-time slot definition for a specific slot by index.
 * Requires FArcAreaFragment on the entity.
 */
USTRUCT(meta = (DisplayName = "Arc Get Area Slot Definition", Category = "Arc|Area|Self"))
struct ARCAREA_API FArcAreaGetSlotDefinitionTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcAreaGetSlotDefinitionTaskInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FName GetIconName() const override { return FName("StateTreeEditorStyle|Node.Task"); }
	virtual FColor GetIconColor() const override { return FColor(60, 160, 220); }
#endif
};

// ------------------------------------------------------------------
// Find Vacant Slot
// ------------------------------------------------------------------

USTRUCT()
struct FArcAreaFindVacantSlotTaskInstanceData
{
	GENERATED_BODY()

	/** Optional: only consider slots with this role tag. */
	UPROPERTY(EditAnywhere, Category = Input)
	FGameplayTag RoleTagFilter;

	/** Index of the first vacant slot found, or INDEX_NONE. */
	UPROPERTY(EditAnywhere, Category = Output)
	int32 SlotIndex = INDEX_NONE;

	UPROPERTY(EditAnywhere, Category = Output)
	bool bFound = false;
};

/**
 * Finds the first vacant slot in the current Area entity.
 * Optionally filtered by role tag. Returns Failed if no vacancy found.
 * Requires FArcAreaFragment on the entity.
 */
USTRUCT(meta = (DisplayName = "Arc Find Vacant Slot", Category = "Arc|Area|Self"))
struct ARCAREA_API FArcAreaFindVacantSlotTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcAreaFindVacantSlotTaskInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FName GetIconName() const override { return FName("StateTreeEditorStyle|Node.Task"); }
	virtual FColor GetIconColor() const override { return FColor(60, 160, 220); }
#endif
};
