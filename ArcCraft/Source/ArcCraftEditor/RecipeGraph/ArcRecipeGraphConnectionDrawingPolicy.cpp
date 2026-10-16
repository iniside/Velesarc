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

#include "RecipeGraph/ArcRecipeGraphConnectionDrawingPolicy.h"
#include "RecipeGraph/ArcRecipeGraph.h"
#include "RecipeGraph/ArcRecipeGraphNode.h"
#include "EdGraph/EdGraphPin.h"

FArcRecipeGraphConnectionDrawingPolicy::FArcRecipeGraphConnectionDrawingPolicy(
	int32 InBackLayerID,
	int32 InFrontLayerID,
	float InZoomFactor,
	const FSlateRect& InClippingRect,
	FSlateWindowElementList& InDrawElements,
	UEdGraph* InGraphObj)
	: FConnectionDrawingPolicy(InBackLayerID, InFrontLayerID, InZoomFactor, InClippingRect, InDrawElements)
{
	RecipeGraph = Cast<UArcRecipeGraph>(InGraphObj);
}

void FArcRecipeGraphConnectionDrawingPolicy::DetermineWiringStyle(
	UEdGraphPin* OutputPin,
	UEdGraphPin* InputPin,
	FConnectionParams& Params)
{
	FConnectionDrawingPolicy::DetermineWiringStyle(OutputPin, InputPin, Params);

	UArcRecipeGraph* G = RecipeGraph.Get();
	if (G == nullptr)
	{
		return;
	}

	UArcRecipeGraphNode* OutNode = OutputPin
		? Cast<UArcRecipeGraphNode>(OutputPin->GetOwningNodeUnchecked())
		: nullptr;
	UArcRecipeGraphNode* InNode = InputPin
		? Cast<UArcRecipeGraphNode>(InputPin->GetOwningNodeUnchecked())
		: nullptr;

	// Guard against in-progress drag preview where one pin is null.
	if (OutNode == nullptr || InNode == nullptr)
	{
		return;
	}

	if (G->OptionalEdges.Contains(MakeTuple(OutNode->AssetId, InNode->AssetId)))
	{
		Params.WireColor = FLinearColor(0.35f, 0.75f, 1.0f);
	}

	if (!G->bChainHighlightActive)
	{
		return;
	}

	if (OutNode->HighlightState == EArcRecipeNodeHighlight::Normal
		&& InNode->HighlightState == EArcRecipeNodeHighlight::Normal)
	{
		Params.WireColor = FLinearColor(1.0f, 0.85f, 0.4f);
		Params.WireThickness = 4.0f;
		Params.bDrawBubbles = true;
	}
	else
	{
		Params.WireColor = FLinearColor(0.5f, 0.5f, 0.5f, 0.15f);
		Params.WireThickness = 1.0f;
	}
}
