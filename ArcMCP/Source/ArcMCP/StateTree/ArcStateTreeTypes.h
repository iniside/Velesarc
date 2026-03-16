// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StateTreeTypes.h"

// ── Enum string mappings ────────────────────────────────────────────

namespace UE::ArcMCP::StateTree
{

inline EStateTreeStateType ParseStateType(const FString& Str)
{
	if (Str == TEXT("state"))        return EStateTreeStateType::State;
	if (Str == TEXT("group"))        return EStateTreeStateType::Group;
	if (Str == TEXT("subtree"))      return EStateTreeStateType::Subtree;
	if (Str == TEXT("linked"))       return EStateTreeStateType::Linked;
	if (Str == TEXT("linked_asset")) return EStateTreeStateType::LinkedAsset;
	return EStateTreeStateType::State; // default
}

inline EStateTreeStateSelectionBehavior ParseSelectionBehavior(const FString& Str)
{
	if (Str == TEXT("sequential"))                return EStateTreeStateSelectionBehavior::TrySelectChildrenInOrder;
	if (Str == TEXT("random"))                    return EStateTreeStateSelectionBehavior::TrySelectChildrenAtRandom;
	if (Str == TEXT("utility"))                   return EStateTreeStateSelectionBehavior::TrySelectChildrenWithHighestUtility;
	if (Str == TEXT("weighted_random_utility"))    return EStateTreeStateSelectionBehavior::TrySelectChildrenAtRandomWeightedByUtility;
	if (Str == TEXT("follow_transitions"))         return EStateTreeStateSelectionBehavior::TryFollowTransitions;
	return EStateTreeStateSelectionBehavior::TrySelectChildrenInOrder; // default
}

inline EStateTreeTransitionTrigger ParseTransitionTrigger(const FString& Str)
{
	if (Str == TEXT("on_tick"))              return EStateTreeTransitionTrigger::OnTick;
	if (Str == TEXT("on_state_completed"))   return EStateTreeTransitionTrigger::OnStateCompleted;
	if (Str == TEXT("on_state_succeeded"))   return EStateTreeTransitionTrigger::OnStateSucceeded;
	if (Str == TEXT("on_state_failed"))      return EStateTreeTransitionTrigger::OnStateFailed;
	if (Str == TEXT("on_event"))             return EStateTreeTransitionTrigger::OnEvent;
	return EStateTreeTransitionTrigger::OnStateCompleted; // default
}

inline EStateTreeTransitionType ParseTransitionType(const FString& Str)
{
	if (Str == TEXT("goto_state"))              return EStateTreeTransitionType::GotoState;
	if (Str == TEXT("succeeded"))               return EStateTreeTransitionType::Succeeded;
	if (Str == TEXT("failed"))                  return EStateTreeTransitionType::Failed;
	if (Str == TEXT("next_state"))              return EStateTreeTransitionType::NextState;
	if (Str == TEXT("none"))                    return EStateTreeTransitionType::None;
	if (Str == TEXT("next_selectable_state"))   return EStateTreeTransitionType::NextSelectableState;
	if (Str == TEXT("next_parent"))             return EStateTreeTransitionType::NextParent;
	if (Str == TEXT("next_selectable_parent"))  return EStateTreeTransitionType::NextSelectableParent;
	return EStateTreeTransitionType::GotoState; // default
}

inline EStateTreeTransitionPriority ParseTransitionPriority(const FString& Str)
{
	if (Str == TEXT("normal"))   return EStateTreeTransitionPriority::Normal;
	if (Str == TEXT("medium"))   return EStateTreeTransitionPriority::Medium;
	if (Str == TEXT("high"))     return EStateTreeTransitionPriority::High;
	if (Str == TEXT("critical")) return EStateTreeTransitionPriority::Critical;
	return EStateTreeTransitionPriority::Normal; // default
}

inline EStateTreeExpressionOperand ParseOperand(const FString& Str)
{
	if (Str == TEXT("or")) return EStateTreeExpressionOperand::Or;
	return EStateTreeExpressionOperand::And; // default
}

} // namespace UE::ArcMCP::StateTree

// ── Parsed intermediate structs ────────────────────────────────────

struct FArcSTNodeDesc
{
	FString Id;
	FString Type; // alias or full struct name
	TSharedPtr<FJsonObject> Properties;
	TSharedPtr<FJsonObject> InstanceData;
	FString Operand; // "and"/"or", conditions/considerations only
};

struct FArcSTTransitionDesc
{
	FString Trigger;
	FString Type;
	FString Target; // state id
	FString Priority;
	float Delay = 0.f;
	FString Event; // gameplay tag for on_event
	TArray<FArcSTNodeDesc> Conditions;
};

struct FArcSTStateDesc
{
	FString Id;
	FString Name;
	FString Type; // state/group/subtree/linked/linked_asset
	FString Parent; // parent state id, empty for roots
	TArray<FString> Children; // ordered child state ids
	FString Selection; // sequential/random/utility/etc.
	FString Tag; // gameplay tag
	FString LinkedAsset; // asset path for linked_asset type
	TArray<FArcSTNodeDesc> EnterConditions;
	TArray<FArcSTNodeDesc> Tasks;
	TArray<FArcSTTransitionDesc> Transitions;
	TArray<FArcSTNodeDesc> Considerations;
};

struct FArcSTBindingDesc
{
	FString Source; // "node_id.PropertyPath" or "parameters.X" or "context.X"
	FString Target; // "node_id.PropertyPath"
};

struct FArcSTDescription
{
	FString Name;
	FString PackagePath;
	FString Schema;
	TSharedPtr<FJsonObject> Parameters; // name -> {type, default}
	TArray<FArcSTNodeDesc> Evaluators;
	TArray<FArcSTNodeDesc> GlobalTasks;
	TArray<FArcSTStateDesc> States;
	TArray<FArcSTBindingDesc> Bindings;
};
