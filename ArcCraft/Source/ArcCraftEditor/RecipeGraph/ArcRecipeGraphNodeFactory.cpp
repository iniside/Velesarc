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

#include "RecipeGraph/ArcRecipeGraphNodeFactory.h"

#include "RecipeGraph/ArcRecipeGraphNode.h"
#include "RecipeGraph/SArcRecipeGraphNode.h"

// ---------------------------------------------------------------------------
// File-scoped namespace (unique per CLAUDE.md rule)
// ---------------------------------------------------------------------------

namespace ArcRecipeGraphNodeFactoryInternal
{
	// No helpers needed; namespace present for rule compliance.
} // namespace ArcRecipeGraphNodeFactoryInternal

// ---------------------------------------------------------------------------
// FArcRecipeGraphNodeFactory
// ---------------------------------------------------------------------------

TSharedPtr<SGraphNode> FArcRecipeGraphNodeFactory::CreateNode(UEdGraphNode* Node) const
{
	UArcRecipeGraphNode* ArcNode = Cast<UArcRecipeGraphNode>(Node);
	if (ArcNode != nullptr)
	{
		return SNew(SArcRecipeGraphNode, ArcNode);
	}

	return nullptr;
}
