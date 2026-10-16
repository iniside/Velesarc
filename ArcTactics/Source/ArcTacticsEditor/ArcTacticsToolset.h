// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "ToolsetRegistry/UToolsetRegistry.h"

#include "ArcTacticsToolset.generated.h"

UCLASS()
class UArcTacticsToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	/** List concrete ArcTactics node types usable in ApplyTacticsConfig. Role: GlobalTask, Action, Step, ActiveStep, Consideration, Selector or empty for all. Filter: optional substring of the type or display name. Each type lists nodeFields (fields of the node struct) and fields (fields of its instance data); each field has name, C++ type, role (Input/Output/Parameter/None) and required flag, plus when present baseStruct, default (the default value in the JSON form ApplyTacticsConfig accepts), tooltip, clampMin and clampMax (numbers from the ClampMin/ClampMax meta), units (Units meta) and enumValues (the accepted enum value names, for enum fields). Each type also lists requirements: the names of the execution requirements its default instance data declares (any of Transform, OwnedTags, Team, Sight, AbilityCollection, PathFollow, KnowledgeSubsystem, TQSSubsystem, AbilitySubsystem, Focus). */
	UFUNCTION(meta = (AICallable, AIAccessMode = "Read"))
	static FString ListTacticsNodeTypes(const FString& Role, const FString& Filter);

	/** Export the UArcTacticsConfig at Path (e.g. /Game/AI/TacticsConfig_DPS) as version-2 JSON: settings, global parameters, global tasks, actions, selectors (nodes by unique name and short type, non-default editable values; a Sequence action and a selector action object list their sequential steps under "steps" and their while-active steps (role ActiveStep, entered with the action and exited when it ends) under "activeSteps") and bindings as "to <- from" strings. The result can be edited and passed to ApplyTacticsConfig. */
	UFUNCTION(meta = (AICallable, AIAccessMode = "Read"))
	static FString ExportTacticsConfig(const FString& Path);

	/** Rebuild the whole UArcTacticsConfig at Path from ConfigJson (the ExportTacticsConfig format), validating types, names, values and every binding like the editor, then compile. Nothing is changed (and no asset is created) unless the whole JSON is valid. ConfigJson.options: {"dryRun": true} validates and compiles a scratch copy only; {"save": true} saves the package. Returns success, compiled, revision, errors, warnings, compiler diagnostics and lint (also for a dry run). lint is a list of static findings {severity: warning|info, code, node, message} computed from the authoring data only when the config compiled; lint never fails the call and skips checks whose values are bound. Codes: Unreachable (warning, the branch best score with every consideration at its curve maximum is below MinScore); AlwaysEligible (warning, a standalone action with only Constant considerations, no cooldown and a worst score at or above MinScore blocks every lower-priority branch); FlatCurve (warning, a non-Constant, non-gate consideration curve output varies by less than 0.01 over its input set); UnusedOutput (info, a Global Task output no binding reads); UnreadParameter (info, a global parameter no binding reads); HistoryTagUnwritten (warning, a history consideration Tag matches no branch HistoryTag); AbilityUnchecked (info, a Use Ability or Use Ability Focus step has no Can Afford Cost or Cooldown Ready consideration for its ability in the branch). */
	UFUNCTION(meta = (AICallable, AIAccessMode = "Write"))
	static FString ApplyTacticsConfig(const FString& Path, const FString& ConfigJson);

	/** Compile the UArcTacticsConfig at Path exactly like the editor Compile command and return success, compiled, revision, compiler diagnostics and lint. lint is a list of static findings {severity: warning|info, code, node, message} computed from the authoring data only when the config compiled; lint never fails the call and skips checks whose values are bound. Codes: Unreachable (warning, the branch best score with every consideration at its curve maximum is below MinScore); AlwaysEligible (warning, a standalone action with only Constant considerations, no cooldown and a worst score at or above MinScore blocks every lower-priority branch); FlatCurve (warning, a non-Constant, non-gate consideration curve output varies by less than 0.01 over its input set); UnusedOutput (info, a Global Task output no binding reads); UnreadParameter (info, a global parameter no binding reads); HistoryTagUnwritten (warning, a history consideration Tag matches no branch HistoryTag); AbilityUnchecked (info, a Use Ability or Use Ability Focus step has no Can Afford Cost or Cooldown Ready consideration for its ability in the branch). */
	UFUNCTION(meta = (AICallable, AIAccessMode = "Write"))
	static FString CompileTacticsConfig(const FString& Path);
};
