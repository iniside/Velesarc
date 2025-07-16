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

#pragma once

#include "CoreMinimal.h"
#include "EdGraphSchema_K2.h"
#include "EdGraphUtilities.h"
#include "SGraphPin.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"

#include "StructUtils/InstancedStruct.h"
#include "KismetPins/SGraphPinObject.h"
/**
 *
 */
class ARCCOREEDITOR_API SArcInstancedStructPin : public SGraphPinObject
{
public:
	SLATE_BEGIN_ARGS(SArcInstancedStructPin)
		{
		}

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs
				   , UEdGraphPin* InGraphPinObj);

	//~ Begin SGraphPin Interface
	virtual TSharedRef<SWidget> GetDefaultValueWidget() override;

	//~ End SGraphPin Interface
};

class FArcInstancedStructPinGraphPinFactory : public FGraphPanelPinFactory
{
	virtual TSharedPtr<class SGraphPin> CreatePin(class UEdGraphPin* InPin) const override
	{
		if (InPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Struct && InPin->PinType.PinSubCategoryObject ==
			FInstancedStruct::StaticStruct())
		{
			return SNew(SArcInstancedStructPin
				, InPin);
		}
		return NULL;
	}
};
