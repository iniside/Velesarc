// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTacticsAuthoring.h"

#include "Considerations/ArcTacticsConsiderations.h"
#include "Decision/ArcTacticsConfig.h"
#include "Decision/ArcTacticsNode.h"
#include "Decision/ArcTacticsSelectors.h"
#include "GlobalTasks/ArcTacticsGlobalTask.h"

#define LOCTEXT_NAMESPACE "ArcTacticsAuthoring"

namespace ArcTacticsAuthoringPrivate
{
	void SetError(FText* OutError, const FText& Error)
	{
		if (OutError)
		{
			*OutError = Error;
		}
	}
}

bool ArcTacticsAuthoring::IsRoleBaseNodeType(const UScriptStruct* NodeType)
{
	return NodeType == FArcTacticsGlobalTask::StaticStruct()
		|| NodeType == FArcTacticsActionNode::StaticStruct()
		|| NodeType == FArcTacticsConsiderationNode::StaticStruct()
		|| NodeType == FArcTacticsSelectorNode::StaticStruct()
		|| NodeType == FArcTacticsStepNode::StaticStruct()
		|| NodeType == FArcTacticsActiveStepNode::StaticStruct();
}

bool ArcTacticsAuthoring::IsNodeTypeHidden(const UScriptStruct* NodeType)
{
	return NodeType && NodeType->HasMetaData(TEXT("Hidden"));
}

bool ArcTacticsAuthoring::PrepareNode(
	const UArcTacticsConfig& Config,
	FArcTacticsNodeDefinition& OutNode,
	const UScriptStruct* NodeType,
	const EArcTacticsNodeRole ExpectedRole,
	FText* OutError)
{
	if (!NodeType || !NodeType->IsChildOf(FArcTacticsNode::StaticStruct()))
	{
		ArcTacticsAuthoringPrivate::SetError(OutError, LOCTEXT("NotANodeType", "The type is not an Arc Tactics node."));
		return false;
	}
	if (IsRoleBaseNodeType(NodeType))
	{
		ArcTacticsAuthoringPrivate::SetError(OutError, LOCTEXT("RoleBaseNodeType", "The type is a role base struct, not a concrete node."));
		return false;
	}
	OutNode.Node.InitializeAs(NodeType);
	OutNode.InstanceData.Reset();
	if (!Config.SynchronizeNode(OutNode, ExpectedRole))
	{
		ArcTacticsAuthoringPrivate::SetError(OutError, LOCTEXT("InvalidNodeRole", "The type is not valid for this node role."));
		return false;
	}
	return true;
}

#undef LOCTEXT_NAMESPACE
