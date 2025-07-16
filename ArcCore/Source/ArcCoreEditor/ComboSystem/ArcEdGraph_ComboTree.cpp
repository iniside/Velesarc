/**
 * This file is part of ArcX.
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or –
 * as soon as they will be approved by the European Commission – later versions
 * of the EUPL (the "License");
 *
 * You may not use this work except in compliance with the License.
 * You may get a copy of the License at:
 *
 * https://eupl.eu/
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 */

#include "ComboSystem/ArcEdGraph_ComboTree.h"
#include "AbilitySystem/Combo/ArcComboComponent.h"
#include "ArcComboTreeGraphSchema.h"
#include "EdGraph/EdGraphNode.h"
#include "GraphEditAction.h"

#define SNAP_GRID (16)

#define LOCTEXT_NAMESPACE "MkEdGraph_AbilityTree"

const FName FMkAbilityTreeTypes::PinType_Entry = "Entry";
const FName FMkAbilityTreeTypes::PinType_Input = "Input";

UArcEdGraph_ComboTree::UArcEdGraph_ComboTree(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Schema = UArcComboTreeGraphSchema::StaticClass();
}

void UArcEdGraph_ComboTree::InitializeGraph(UArcComboGraph* DataAsset)
{
	AbilityTreePtr = DataAsset;
}

void UArcEdGraph_ComboTree::RefreshNodeSelection(UEdGraphNode* Node)
{
	TSet<const UEdGraphNode*> NodesToFocus;

	FEdGraphEditAction SelectionAction;
	SelectionAction.Action = GRAPHACTION_SelectNode;
	SelectionAction.Graph = this;
	NotifyGraphChanged(SelectionAction);
	SelectionAction.Nodes = NodesToFocus;
	NotifyGraphChanged(SelectionAction);
}

void UArcEdGraph_ComboTree::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
}

UArcComboGraph* UArcEdGraph_ComboTree::GetAbilityTree() const
{
	return AbilityTreePtr;
}

#undef LOCTEXT_NAMESPACE
