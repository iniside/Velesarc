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
#include "ConnectionDrawingPolicy.h"

class UEdGraph;

/**
 * Connection drawing policy for the ArcEditorTools heightmap editor graph.
 *
 * Delegates all wire-style decisions to the base FConnectionDrawingPolicy;
 * override points are provided for future heightmap-specific highlighting.
 */
class FArcHeightmapGraphConnectionDrawingPolicy : public FConnectionDrawingPolicy
{
public:
	FArcHeightmapGraphConnectionDrawingPolicy(int32 InBackLayerID, int32 InFrontLayerID, float InZoomFactor,
		const FSlateRect& InClippingRect, FSlateWindowElementList& InDrawElements, UEdGraph* InGraphObj);

	virtual void DetermineWiringStyle(UEdGraphPin* OutputPin, UEdGraphPin* InputPin,
		/*inout*/ FConnectionParams& Params) override;
};
