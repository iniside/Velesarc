// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "ArcTQSQueryReference.h"
#include "ArcTQSTypes.h"
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GlobalTasks/ArcTacticsGlobalTask.h"
#include "Mass/EntityHandle.h"
#include "Memory/ArcMassMemory.h"
#include "Teams/ArcTeamAttitude.h"

#include "ArcTacticsDataTasks.generated.h"

USTRUCT(BlueprintType, meta = (ToolTip = "A target entity together with its last known location."))
struct ARCTACTICS_API FArcTacticsTarget
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = "Target", meta = (ToolTip = "The target entity."))
	FMassEntityHandle Entity;

	UPROPERTY(VisibleAnywhere, Category = "Target", meta = (ToolTip = "Freshest known world location of the target in cm."))
	FVector LastLocation = FVector::ZeroVector;
};

USTRUCT(BlueprintType)
struct ARCTACTICS_API FArcTacticsCollectOwnDataTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ArcTacticsParameter, ToolTip = "Reads the entity transform into Transform, Location and Forward; the task fails if the transform is missing."))
	bool bCollectTransform = true;

	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ArcTacticsParameter, ToolTip = "Reads the entity owned tags into OwnedTags; the task fails if they are missing."))
	bool bCollectOwnedTags = true;

	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ArcTacticsParameter, ToolTip = "Reads the entity team into TeamId; the task fails if it is missing."))
	bool bCollectTeam = true;

	UPROPERTY(VisibleAnywhere, Category = "Output", meta = (ArcTacticsOutput, ToolTip = "The entity the decision runs for."))
	FMassEntityHandle Entity;

	UPROPERTY(VisibleAnywhere, Category = "Output", meta = (ArcTacticsOutput, ToolTip = "World transform of the entity when Collect Transform is on."))
	FTransform Transform;

	UPROPERTY(VisibleAnywhere, Category = "Output", meta = (ArcTacticsOutput, ToolTip = "World location of the entity in cm when Collect Transform is on."))
	FVector Location = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, Category = "Output", meta = (ArcTacticsOutput, ToolTip = "Forward direction of the entity when Collect Transform is on."))
	FVector Forward = FVector::ForwardVector;

	UPROPERTY(VisibleAnywhere, Category = "Output", meta = (ArcTacticsOutput, ToolTip = "Owned gameplay tags of the entity when Collect Owned Tags is on."))
	FGameplayTagContainer OwnedTags;

	UPROPERTY(VisibleAnywhere, Category = "Output", meta = (ArcTacticsOutput, ToolTip = "Team id of the entity when Collect Team is on."))
	int32 TeamId = INDEX_NONE;
};

USTRUCT(BlueprintType, meta = (DisplayName = "Collect Own Data", ToolTip = "Publishes the entity handle and, when enabled, its transform, owned tags and team id."))
struct ARCTACTICS_API FArcTacticsCollectOwnDataTask : public FArcTacticsGlobalTask
{
	GENERATED_BODY()

	virtual const UScriptStruct* GetInstanceDataType() const override;
	virtual FText GetDisplayName() const override;
	virtual EArcTacticsTaskStatus Enter(FArcTacticsTaskContext& Context, FStructView InstanceData) const override;
	virtual EArcTacticsExecutionRequirement GetRequirements(FConstStructView InstanceData) const override;

#if WITH_EDITOR
	virtual FName GetIconName() const override;
	virtual FColor GetIconColor() const override;
	virtual FText GetDescription(const FGuid& NodeID, FConstStructView InstanceData, const IPropertyBindingBindingCollectionOwner& BindingOwner, bool bRichText) const override;
#endif
};

USTRUCT(BlueprintType)
struct ARCTACTICS_API FArcTacticsCollectTargetsTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Targets", meta = (ArcTacticsParameter, ToolTip = "Only targets with this attitude toward the entity are collected."))
	EArcTeamAttitude TargetAttitude = EArcTeamAttitude::Hostile;

	UPROPERTY(EditAnywhere, Category = "Targets", meta = (ArcTacticsParameter, ToolTip = "Targets must own all of these tags."))
	FGameplayTagContainer RequiredTargetTags;

	UPROPERTY(EditAnywhere, Category = "Targets", meta = (ArcTacticsParameter, ToolTip = "Targets owning any of these tags are excluded."))
	FGameplayTagContainer BlockedTargetTags;

	UPROPERTY(EditAnywhere, Category = "Targets", meta = (ArcTacticsParameter, ToolTip = "Also collects targets reported by teammates through shared team knowledge; ignored for Friendly targets."))
	bool bIncludeSharedTargets = true;

	UPROPERTY(EditAnywhere, Category = "Targets", meta = (ArcTacticsParameter, ClampMin = "0.0", Units = "s", ToolTip = "Seconds a shared target report stays eligible."))
	float SharedTargetWindow = 8.0f;

	UPROPERTY(EditAnywhere, Category = "Targets", meta = (ArcTacticsParameter, ClampMin = "0.0", Units = "cm", ToolTip = "Radius in cm around the entity in which shared target reports are gathered."))
	float SharedTargetGatherRadius = 5000.0f;

	UPROPERTY(VisibleAnywhere, Category = "Output", meta = (ArcTacticsOutput, ToolTip = "Entity handles of the eligible targets."))
	TArray<FMassEntityHandle> Targets;

	UPROPERTY(VisibleAnywhere, Category = "Output", meta = (ArcTacticsOutput, ToolTip = "Eligible targets with their freshest last known location."))
	TArray<FArcTacticsTarget> TargetInfos;

	UPROPERTY(VisibleAnywhere, Category = "Output", meta = (ArcTacticsOutput, ToolTip = "True when at least one eligible target was found."))
	bool bHasEligibleTarget = false;
};

USTRUCT(BlueprintType, meta = (DisplayName = "Collect Perception Targets", ToolTip = "Collects sight-perceived and team-shared targets of the chosen attitude that pass the tag filters."))
struct ARCTACTICS_API FArcTacticsCollectTargetsTask : public FArcTacticsGlobalTask
{
	GENERATED_BODY()

	virtual const UScriptStruct* GetInstanceDataType() const override;
	virtual FText GetDisplayName() const override;
	virtual EArcTacticsTaskStatus Enter(FArcTacticsTaskContext& Context, FStructView InstanceData) const override;
	virtual EArcTacticsExecutionRequirement GetRequirements(FConstStructView InstanceData) const override;

#if WITH_EDITOR
	virtual FName GetIconName() const override;
	virtual FColor GetIconColor() const override;
	virtual FText GetDescription(const FGuid& NodeID, FConstStructView InstanceData, const IPropertyBindingBindingCollectionOwner& BindingOwner, bool bRichText) const override;
#endif
};

struct ARCTACTICS_API FArcTacticsTaskQueryState
{
	int32 QueryID = INDEX_NONE;
	uint32 RequestGeneration = 0;
	bool bCompleted = false;
	bool bSucceeded = false;
};

USTRUCT(BlueprintType)
struct ARCTACTICS_API FArcTacticsRunTQSTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Query", meta = (ArcTacticsParameter, ToolTip = "Query to run; the task fails without a definition."))
	FArcTQSQueryReference TargetQuery;

	UPROPERTY(EditAnywhere, Category = "Input", meta = (ArcTacticsInput, ArcTacticsRequiredBinding, ToolTip = "World location in cm the query is evaluated from."))
	FVector QuerierLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "Input", meta = (ArcTacticsInput, ToolTip = "Forward direction the query is evaluated with."))
	FVector QuerierForward = FVector::ForwardVector;

	UPROPERTY(EditAnywhere, Category = "Query", meta = (ArcTacticsParameter, ToolTip = "Minimum score an item needs to pass the query."))
	float MinPassingScore = 0.0f;

	UPROPERTY(VisibleAnywhere, Category = "Output", meta = (ArcTacticsOutput, ToolTip = "Locations of all passing query results."))
	TArray<FVector> Positions;

	FArcTacticsTaskQueryState Query;
};

USTRUCT(BlueprintType, meta = (DisplayName = "Run Tactical Query", ToolTip = "Runs a tactical query from the bound querier location and outputs all passing points. Other inputs reach the query through its parameters."))
struct ARCTACTICS_API FArcTacticsRunTQSTask : public FArcTacticsGlobalTask
{
	GENERATED_BODY()

	virtual const UScriptStruct* GetInstanceDataType() const override;
	virtual FText GetDisplayName() const override;
	virtual EArcTacticsTaskStatus Enter(FArcTacticsTaskContext& Context, FStructView InstanceData) const override;
	virtual EArcTacticsTaskStatus Tick(FArcTacticsTaskContext& Context, FStructView InstanceData) const override;
	virtual void Cancel(FArcTacticsTaskContext& Context, FStructView InstanceData) const override;
	virtual void OnAsyncQueryCompleted(
		FArcTacticsTaskContext& Context,
		FStructView InstanceData,
		const FArcTacticsAsyncRequestToken& Token,
		FArcTQSQueryInstance& CompletedQuery) const override;
	virtual EArcTacticsExecutionRequirement GetRequirements(FConstStructView InstanceData) const override;

#if WITH_EDITOR
	virtual FName GetIconName() const override;
	virtual FColor GetIconColor() const override;
	virtual FText GetDescription(const FGuid& NodeID, FConstStructView InstanceData, const IPropertyBindingBindingCollectionOwner& BindingOwner, bool bRichText) const override;
#endif
};

USTRUCT(BlueprintType)
struct ARCTACTICS_API FArcTacticsPatrolPointsTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Query", meta = (ArcTacticsParameter, ToolTip = "Query that generates patrol points; the task fails without a definition."))
	FArcTQSQueryReference TargetQuery;

	UPROPERTY(EditAnywhere, Category = "Query", meta = (ArcTacticsParameter, ClampMin = "0.0", Units = "cm",
		ToolTip = "3D distance at which the held patrol point counts as reached. Must be at least the patrol MoveTo arrival range plus any transform-to-navmesh vertical offset, otherwise MoveTo finishes outside it and the point is held forever."))
	float ArrivalRange = 250.0f;

	UPROPERTY(EditAnywhere, Category = "Memory", meta = (ArcTacticsParameter, Categories = "AI.Memory", ArcMemoryAccess = "Read|Write|Forget", ArcMemoryType = "/Script/CoreUObject.Vector",
		ToolTip = "NPC memory key that holds the patrol point between decisions."))
	FGameplayTag MemoryKey = TAG_AI_Memory_PatrolPoint;

	UPROPERTY(VisibleAnywhere, Category = "Output", meta = (ArcTacticsOutput, ToolTip = "At most one held patrol point."))
	TArray<FVector> Points;

	FArcTacticsTaskQueryState Query;
};

USTRUCT(BlueprintType, meta = (DisplayName = "Generate Patrol Points", ToolTip = "Queries around the entity and holds one patrol point until the entity arrives at it."))
struct ARCTACTICS_API FArcTacticsPatrolPointsTask : public FArcTacticsGlobalTask
{
	GENERATED_BODY()

	virtual const UScriptStruct* GetInstanceDataType() const override;
	virtual FText GetDisplayName() const override;
	virtual EArcTacticsTaskStatus Enter(FArcTacticsTaskContext& Context, FStructView InstanceData) const override;
	virtual EArcTacticsTaskStatus Tick(FArcTacticsTaskContext& Context, FStructView InstanceData) const override;
	virtual void Cancel(FArcTacticsTaskContext& Context, FStructView InstanceData) const override;
	virtual void OnAsyncQueryCompleted(
		FArcTacticsTaskContext& Context,
		FStructView InstanceData,
		const FArcTacticsAsyncRequestToken& Token,
		FArcTQSQueryInstance& CompletedQuery) const override;
	virtual EArcTacticsExecutionRequirement GetRequirements(FConstStructView InstanceData) const override;

#if WITH_EDITOR
	virtual FName GetIconName() const override;
	virtual FColor GetIconColor() const override;
	virtual FText GetDescription(const FGuid& NodeID, FConstStructView InstanceData, const IPropertyBindingBindingCollectionOwner& BindingOwner, bool bRichText) const override;
#endif
};
