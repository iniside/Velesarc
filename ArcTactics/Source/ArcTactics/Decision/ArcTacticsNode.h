// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Mass/EntityHandle.h"
#include "StructUtils/InstancedStruct.h"
#include "StructUtils/StructView.h"

#include "ArcTacticsNode.generated.h"

struct FArcTacticsActionContext;
struct FArcTacticsActionView;
struct FArcTacticsAsyncRequestToken;
struct FArcTacticsStepContext;
struct FArcTQSQueryInstance;
struct FArcTQSQueryReference;
class FProperty;
class IPropertyBindingBindingCollectionOwner;
enum class EArcTacticsActionCancelMode : uint8;
enum class EArcTacticsActionStatus : uint8;
enum class EArcTacticsActionTickLane : uint8;

enum class EArcTacticsExecutionRequirement : uint32
{
	None = 0,
	Transform = 1 << 0,
	OwnedTags = 1 << 1,
	Team = 1 << 2,
	Sight = 1 << 3,
	AbilityCollection = 1 << 4,
	PathFollow = 1 << 5,
	KnowledgeSubsystem = 1 << 6,
	TQSSubsystem = 1 << 7,
	AbilitySubsystem = 1 << 8,
	Focus = 1 << 9
};
ENUM_CLASS_FLAGS(EArcTacticsExecutionRequirement);

USTRUCT(BlueprintType, meta = (ToolTip = "One successful tagged action recorded in the entity's action history."))
struct ARCTACTICS_API FArcTacticsActionHistoryEntry
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = "History", meta = (ToolTip = "History tag of the action that succeeded."))
	FGameplayTag Tag;

	UPROPERTY(VisibleAnywhere, Category = "History", meta = (ToolTip = "World time in seconds when the action succeeded."))
	double Time = 0.0;
};

USTRUCT(BlueprintType, meta = (ToolTip = "Most recent successful tagged actions of the entity, newest last, capped at the trait settings HistorySize."))
struct ARCTACTICS_API FArcTacticsActionHistory
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = "History", meta = (ToolTip = "Recorded actions, oldest first and newest last."))
	TArray<FArcTacticsActionHistoryEntry> Entries;
};

USTRUCT(BlueprintType)
struct ARCTACTICS_API FArcTacticsContextData
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = "Context", meta = (ArcTacticsOutput))
	FMassEntityHandle Self;

	UPROPERTY(VisibleAnywhere, Category = "Context", meta = (ArcTacticsOutput, ToolTip = "Successful tagged actions of this entity, for the action history considerations."))
	FArcTacticsActionHistory History;
};

USTRUCT()
struct ARCTACTICS_API FArcTacticsEmptyInstanceData
{
	GENERATED_BODY()
};

UENUM()
enum class EArcTacticsNodeRole : uint8
{
	Invalid,
	GlobalTask,
	Action,
	Consideration,
	Selector,
	Step,
	ActiveStep
};

UENUM()
enum class EArcTacticsPropertyRole : uint8
{
	Invalid,
	Input,
	Output,
	Parameter
};

#if WITH_EDITOR
namespace ArcTacticsNode
{
	ARCTACTICS_API EArcTacticsPropertyRole ResolvePropertyRole(const FProperty& Property);
	ARCTACTICS_API bool IsRequiredInput(const FProperty& Property);
	ARCTACTICS_API void ForEachQueryReference(
		const UStruct& Struct,
		void* Memory,
		TFunctionRef<void(FArcTQSQueryReference& Reference)> Function);
}
#endif

USTRUCT(BlueprintType, meta = (Hidden, ShowTooltip))
struct ARCTACTICS_API FArcTacticsNode
{
	GENERATED_BODY()

	virtual ~FArcTacticsNode() = default;
	virtual const UScriptStruct* GetInstanceDataType() const;
	virtual FText GetDisplayName() const;
	virtual EArcTacticsNodeRole GetRole() const;

#if WITH_EDITOR
	virtual FName GetIconName() const;
	virtual FColor GetIconColor() const;
	virtual FText GetDescription(
		const FGuid& NodeID,
		FConstStructView InstanceData,
		const IPropertyBindingBindingCollectionOwner& BindingOwner,
		bool bRichText) const;
#endif
};

USTRUCT(BlueprintType, meta = (Hidden))
struct ARCTACTICS_API FArcTacticsActionNode : public FArcTacticsNode
{
	GENERATED_BODY()

	virtual EArcTacticsNodeRole GetRole() const override;
	virtual EArcTacticsActionStatus Enter(FArcTacticsActionContext& Context, FStructView InstanceData) const;
	virtual EArcTacticsActionStatus Tick(FArcTacticsActionContext& Context, FStructView InstanceData) const;
	virtual void Cancel(
		FArcTacticsActionContext& Context,
		FStructView InstanceData,
		EArcTacticsActionCancelMode CancelMode) const;
	virtual void OnAsyncQueryCompleted(
		FArcTacticsActionContext& Context,
		FStructView InstanceData,
		const FArcTacticsAsyncRequestToken& Token,
		FArcTQSQueryInstance& CompletedQuery) const;
	virtual EArcTacticsActionTickLane GetTickLane(const FArcTacticsActionView& Action) const;
	virtual EArcTacticsExecutionRequirement GetRequirements(FConstStructView InstanceData) const;
	virtual bool BlocksDecision(const FArcTacticsActionView& Action) const;
	virtual double GetNextWakeTime(const FArcTacticsActionView& Action) const;
	virtual int32 GetCurrentStepIndex(const FArcTacticsActionView& Action) const;
};

USTRUCT(BlueprintType, meta = (Hidden))
struct ARCTACTICS_API FArcTacticsStepNode : public FArcTacticsNode
{
	GENERATED_BODY()

	virtual EArcTacticsNodeRole GetRole() const override;
	virtual bool Enter(FArcTacticsStepContext& Context, FStructView InstanceData) const;
	virtual EArcTacticsActionStatus Tick(FArcTacticsStepContext& Context, FStructView InstanceData) const;
	virtual void Cancel(
		FArcTacticsStepContext& Context,
		FStructView InstanceData,
		EArcTacticsActionCancelMode CancelMode) const;
	virtual void OnAsyncQueryCompleted(
		FArcTacticsStepContext& Context,
		FStructView InstanceData,
		const FArcTacticsAsyncRequestToken& Token,
		FArcTQSQueryInstance& CompletedQuery) const;
	virtual EArcTacticsActionTickLane GetTickLane(FConstStructView InstanceData) const;
	virtual EArcTacticsExecutionRequirement GetRequirements(FConstStructView InstanceData) const;
	virtual bool BlocksDecision(FConstStructView InstanceData) const;
	virtual double GetNextWakeTime(FConstStructView InstanceData) const;
};

USTRUCT(BlueprintType, meta = (Hidden))
struct ARCTACTICS_API FArcTacticsActiveStepNode : public FArcTacticsNode
{
	GENERATED_BODY()

	virtual EArcTacticsNodeRole GetRole() const override;
	virtual bool Enter(FArcTacticsActionContext& Context, FStructView InstanceData) const;
	virtual void Exit(FArcTacticsActionContext& Context, FStructView InstanceData) const;
	virtual EArcTacticsExecutionRequirement GetRequirements(FConstStructView InstanceData) const;
};

USTRUCT(BlueprintType)
struct ARCTACTICS_API FArcTacticsNodeDefinition
{
	GENERATED_BODY()

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	FGuid ID;
#endif

	UPROPERTY(EditAnywhere, Category = "Node")
	FName Name;

	UPROPERTY(EditAnywhere, Category = "Node", meta = (BaseStruct = "/Script/ArcTactics.ArcTacticsNode", ExcludeBaseStruct))
	FInstancedStruct Node;

	UPROPERTY(EditAnywhere, Category = "Node")
	FInstancedStruct InstanceData;
};
