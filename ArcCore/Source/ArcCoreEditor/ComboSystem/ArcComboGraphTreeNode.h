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


#include "EdGraph/EdGraphNode.h"
#include "Styling/SlateColor.h"
#include "UObject/Object.h"
#include "ArcComboGraphTreeNode.generated.h"

UCLASS()
class UArcComboGraphTreeNode : public UEdGraphNode
{
	GENERATED_BODY()

public:
	UArcComboGraphTreeNode();

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;

	virtual FLinearColor GetNodeTitleColor() const override;

	virtual void NodeConnectionListChanged() override;

	virtual void DestroyNode() override;

	virtual void PinConnectionListChanged(UEdGraphPin* Pin) override;

	virtual void AutowireNewNode(UEdGraphPin* FromPin) override;

	virtual void AllocateDefaultPins() override;

	class UArcComboGraph* GetAbilityTree() const;

	class UArcEdGraph_ComboTree* GetAbilityTreeGraph() const;

	UEdGraphPin* GetOutputPin() const;

	UEdGraphPin* GetInputPin() const;

	UPROPERTY()
	FGuid NodeId;
};
