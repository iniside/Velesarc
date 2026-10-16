// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UArcTacticsConfig;
class UScriptStruct;
struct FArcTacticsNodeDefinition;
enum class EArcTacticsNodeRole : uint8;

namespace ArcTacticsAuthoring
{
	ARCTACTICSEDITOR_API bool IsRoleBaseNodeType(const UScriptStruct* NodeType);

	ARCTACTICSEDITOR_API bool IsNodeTypeHidden(const UScriptStruct* NodeType);

	ARCTACTICSEDITOR_API bool PrepareNode(
		const UArcTacticsConfig& Config,
		FArcTacticsNodeDefinition& OutNode,
		const UScriptStruct* NodeType,
		EArcTacticsNodeRole ExpectedRole,
		FText* OutError = nullptr);
}
