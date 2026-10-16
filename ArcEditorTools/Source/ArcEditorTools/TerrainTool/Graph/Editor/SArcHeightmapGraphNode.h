/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or -
 * as soon as they will be approved by the European Commission - later versions
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

#pragma once

#include "CoreMinimal.h"
#include "SGraphNode.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class UArcHeightmapEdGraphNode;

/**
 * Slate widget for a single UArcHeightmapEdGraphNode in the heightmap editor graph.
 *
 * Subclasses SGraphNode directly so we control the Construct signature.
 * The inherited UpdateGraphNode() wires title color through
 * SGraphNode::GetNodeTitleColor() -> GraphNode->GetNodeTitleColor(), so
 * per-type coloring (Noise/Erosion/Math/Constant/Output) works for free
 * once GraphNode is set.
 */
class SArcHeightmapGraphNode : public SGraphNode
{
public:
	SLATE_BEGIN_ARGS(SArcHeightmapGraphNode) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UArcHeightmapEdGraphNode* InNode);
};
