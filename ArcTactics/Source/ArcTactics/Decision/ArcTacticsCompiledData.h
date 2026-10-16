// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Decision/ArcTacticsBindings.h"
#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"
#include "StructUtils/InstancedStructContainer.h"
#include "StructUtils/PropertyBag.h"

#include "ArcTacticsCompiledData.generated.h"

UENUM(BlueprintType)
enum class EArcTacticsSelectionMode : uint8
{
	HighestScore,
	RandomFromTopPercent,
	WeightedRandom
};

USTRUCT(BlueprintType)
struct ARCTACTICS_API FArcTacticsSelectionSettings
{
	GENERATED_BODY()

	bool IsValid() const;

	UPROPERTY(EditAnywhere, Category = "Selection")
	EArcTacticsSelectionMode Mode = EArcTacticsSelectionMode::HighestScore;

	UPROPERTY(EditAnywhere, Category = "Selection", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float TopPercent = 0.25f;
};

/** Per-entity decision settings authored on the ArcTactics trait. */
USTRUCT(BlueprintType)
struct ARCTACTICS_API FArcTacticsDecisionSettings
{
	GENERATED_BODY()

	bool IsValid() const;

	UPROPERTY(EditAnywhere, Category = "Timing", meta = (ClampMin = "0.0", ToolTip = "Minimum seconds between two decisions. Decisions run only when requested; requests arriving within this time coalesce into one decision at its end."))
	float DecisionInterval = 0.2f;

	UPROPERTY(EditAnywhere, Category = "Selection", meta = (ToolTip = "How one action is picked from the eligible candidates of the highest priority bucket."))
	FArcTacticsSelectionSettings Selection;

	UPROPERTY(EditAnywhere, Category = "History", meta = (ClampMin = "1", ToolTip = "Number of most recent successful tagged actions kept in the entity's action history."))
	int32 HistorySize = 8;
};

USTRUCT(BlueprintType)
struct ARCTACTICS_API FArcTacticsDecisionSettingsParameters
{
	GENERATED_BODY()

	bool IsValid() const;

	UPROPERTY(EditAnywhere, Category = "Scoring", meta = (ArcTacticsParameter, ClampMin = "0.0"))
	float MinScore = 0.1f;
};

UENUM(BlueprintType)
enum class EArcTacticsCooldownStart : uint8
{
	OnEnd UMETA(ToolTip = "The cooldown starts when the action ends: success, failure or replacement by a higher priority action."),
	OnEnter UMETA(ToolTip = "The cooldown starts when the action starts (Enter returns Running or Succeeded); a failed start sets no cooldown and its end does not restart it.")
};

USTRUCT(BlueprintType)
struct ARCTACTICS_API FArcTacticsBranchParameters
{
	GENERATED_BODY()

	bool IsValid() const;

	UPROPERTY(EditAnywhere, Category = "Scoring", meta = (ArcTacticsParameter, ClampMin = "0.0"))
	float Weight = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Scoring", meta = (ArcTacticsParameter, ToolTip = "Priority bucket; higher is more important. A decision selects only among eligible candidates of the highest priority present, and while an action runs only candidates of a strictly higher priority are scored and can replace it."))
	int32 Priority = 0;

	UPROPERTY(EditAnywhere, Category = "Cooldown", meta = (ArcTacticsParameter, ClampMin = "0.0", ToolTip = "Seconds, counted from the cooldown start chosen by Cooldown Start, before this action can be selected again. 0 disables the cooldown. A selector shares one cooldown across all its items."))
	float Cooldown = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Cooldown", meta = (ArcTacticsParameter, ToolTip = "When the cooldown starts: on action end (success, failure or replacement) or on action enter."))
	EArcTacticsCooldownStart CooldownStart = EArcTacticsCooldownStart::OnEnd;

	UPROPERTY(EditAnywhere, Category = "History", meta = (ArcTacticsParameter, ToolTip = "Tag recorded into the entity's action history when this action succeeds. None records nothing."))
	FGameplayTag HistoryTag;
};

UENUM()
enum class EArcTacticsCompiledNodeKind : uint8
{
	GlobalTask,
	Action,
	Consideration,
	Selector,
	Step,
	ActiveStep
};

USTRUCT()
struct ARCTACTICS_API FArcTacticsCompiledNode
{
	GENERATED_BODY()

	UPROPERTY()
	FName Name;

	UPROPERTY()
	FInstancedStruct Node;

	UPROPERTY()
	FArcTacticsDataHandle InstanceData;

	UPROPERTY()
	FPropertyBindingIndex16 BindingsBatch;

	UPROPERTY()
	FPropertyBindingIndex16 OutputBindingsBatch;

	UPROPERTY()
	EArcTacticsCompiledNodeKind Kind = EArcTacticsCompiledNodeKind::GlobalTask;

	UPROPERTY()
	int32 OwnerIndex = INDEX_NONE;

	UPROPERTY()
	int32 ExecutionIndex = INDEX_NONE;
};

USTRUCT()
struct ARCTACTICS_API FArcTacticsCompiledAction
{
	GENERATED_BODY()

	UPROPERTY()
	FName Name;

	UPROPERTY()
	FArcTacticsNodeHandle Action;

	UPROPERTY()
	TArray<FArcTacticsNodeHandle> Steps;

	UPROPERTY()
	TArray<FArcTacticsNodeHandle> ActiveSteps;

	UPROPERTY()
	TArray<FArcTacticsNodeHandle> Considerations;

	UPROPERTY()
	FArcTacticsDataHandle Parameters;

	UPROPERTY()
	FArcTacticsDataHandle ParameterBag;

	UPROPERTY()
	FArcTacticsBranchParameters DefaultParameters;

	UPROPERTY()
	FInstancedPropertyBag DefaultParameterBag;

	UPROPERTY()
	FPropertyBindingIndex16 ParameterBagInitializationBindingsBatch;

	UPROPERTY()
	FPropertyBindingIndex16 ParametersInitializationBindingsBatch;
};

USTRUCT()
struct ARCTACTICS_API FArcTacticsCompiledSelector
{
	GENERATED_BODY()

	UPROPERTY()
	FName Name;

	UPROPERTY()
	FArcTacticsNodeHandle Selector;

	UPROPERTY()
	FArcTacticsNodeHandle Action;

	UPROPERTY()
	TArray<FArcTacticsNodeHandle> Steps;

	UPROPERTY()
	TArray<FArcTacticsNodeHandle> ActiveSteps;

	UPROPERTY()
	TArray<FArcTacticsNodeHandle> Considerations;

	UPROPERTY()
	FArcTacticsDataHandle Parameters;

	UPROPERTY()
	FArcTacticsDataHandle ParameterBag;

	UPROPERTY()
	FArcTacticsBranchParameters DefaultParameters;

	UPROPERTY()
	FInstancedPropertyBag DefaultParameterBag;

	UPROPERTY()
	FPropertyBindingIndex16 ParameterBagInitializationBindingsBatch;

	UPROPERTY()
	FPropertyBindingIndex16 ParametersInitializationBindingsBatch;
};

USTRUCT()
struct ARCTACTICS_API FArcTacticsCompiledFunction
{
	GENERATED_BODY()

	UPROPERTY(meta = (BaseStruct = "/Script/ArcTargetQuery.ArcTQSPropertyFunction"))
	FInstancedStruct Function;

	UPROPERTY()
	FPropertyBindingIndex16 InputBatch;
};

USTRUCT()
struct ARCTACTICS_API FArcTacticsCompiledData
{
	GENERATED_BODY()

	static constexpr uint32 CurrentVersion = 15;

	void Reset();
	bool ResolvePaths();
	bool IsExecutable() const;

	UPROPERTY()
	uint32 Version = 0;

	UPROPERTY()
	uint32 Revision = 0;

	UPROPERTY()
	bool bIsValid = false;

	UPROPERTY()
	FArcTacticsDecisionSettingsParameters DefaultDecisionSettings;

	UPROPERTY()
	FInstancedPropertyBag DefaultGlobalParameters;

	UPROPERTY()
	FPropertyBindingIndex16 GlobalInitializationBindingsBatch;

	UPROPERTY()
	FPropertyBindingIndex16 DecisionSettingsBindingsBatch;

	UPROPERTY()
	FInstancedStructContainer DefaultInstanceData;

	UPROPERTY()
	TArray<FArcTacticsCompiledNode> Nodes;

	UPROPERTY()
	TArray<FArcTacticsNodeHandle> GlobalTasks;

	UPROPERTY()
	TArray<FArcTacticsCompiledAction> Actions;

	UPROPERTY()
	TArray<FArcTacticsCompiledSelector> Selectors;

	UPROPERTY()
	TArray<FArcTacticsCompiledFunction> Functions;

	UPROPERTY()
	FArcTacticsBindingCollection Bindings;
};
