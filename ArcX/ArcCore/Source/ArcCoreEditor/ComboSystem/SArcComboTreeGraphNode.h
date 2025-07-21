/**
 * This file is part of Velesarc
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

#pragma once

#include "ComboSystem/ArcComboGraphTreeNode.h"
#include "SGraphNode.h"
#include "SGraphPin.h"

class SArcComboTreeGraphNode : public SGraphNode
{
public:
	SLATE_BEGIN_ARGS(SArcComboTreeGraphNode)
		{
		}

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs
				   , UArcComboGraphTreeNode* InNode);

	// End of SNodePanel::SNode interface

	// SGraphNode interface
	virtual void UpdateGraphNode() override;

	virtual void CreatePinWidgets() override;

	virtual TSharedPtr<SToolTip> GetComplexTooltip() override;

	// End of SGraphNode interface

	// SWidget interface
	virtual void OnMouseEnter(const FGeometry& MyGeometry
							  , const FPointerEvent& MouseEvent) override;

	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override;

	// End of SWidget interface

protected:
	FSlateColor GetBorderBackgroundColor() const;

	virtual FText GetPreviewCornerText() const;

	virtual const FSlateBrush* GetNameIcon() const;
};
