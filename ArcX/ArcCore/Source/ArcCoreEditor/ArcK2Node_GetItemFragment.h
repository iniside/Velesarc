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


#include "K2Node_CallFunction.h"
#include "ArcK2Node_GetItemFragment.generated.h"

UCLASS()
class ARCCOREEDITOR_API UArcK2Node_GetItemFragmentFromItemDefinition : public UK2Node_CallFunction
{
	GENERATED_BODY()

public:
	UArcK2Node_GetItemFragmentFromItemDefinition();

	//~ Begin UEdGraphNode Interface.
	virtual void PostReconstructNode() override;

	virtual void PinDefaultValueChanged(UEdGraphPin* ChangedPin) override;

	virtual FText GetTooltipText() const override;

	//~ End UEdGraphNode Interface.

	//~ Begin K2Node Interface
	virtual bool IsNodePure() const override;

	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& InActionRegistrar) const override;

	//~ End K2Node Interface
};

/**
 *
 */
UCLASS()
class ARCCOREEDITOR_API UArcK2Node_GetItemFragment : public UK2Node_CallFunction
{
	GENERATED_BODY()

public:
	UArcK2Node_GetItemFragment();

	//~ Begin UEdGraphNode Interface.
	virtual void PostReconstructNode() override;

	virtual void PinDefaultValueChanged(UEdGraphPin* ChangedPin) override;

	virtual FText GetTooltipText() const override;

	//~ End UEdGraphNode Interface.

	//~ Begin K2Node Interface
	virtual bool IsNodePure() const override;

	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& InActionRegistrar) const override;

	//~ End K2Node Interface
};


UCLASS()
class ARCCOREEDITOR_API UArcK2Node_FindItemFragment : public UK2Node_CallFunction
{
	GENERATED_BODY()

public:
	UArcK2Node_FindItemFragment();

	//~ Begin UEdGraphNode Interface.
	virtual void PostReconstructNode() override;

	virtual void PinDefaultValueChanged(UEdGraphPin* ChangedPin) override;

	virtual FText GetTooltipText() const override;

	//~ End UEdGraphNode Interface.

	//~ Begin K2Node Interface
	virtual bool IsNodePure() const override;

	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& InActionRegistrar) const override;

	//~ End K2Node Interface
};

UCLASS()
class ARCCOREEDITOR_API UArcK2Node_FindItemFragmentAbilityActor : public UK2Node_CallFunction
{
	GENERATED_BODY()

public:
	UArcK2Node_FindItemFragmentAbilityActor();

	//~ Begin UEdGraphNode Interface.
	virtual void PostReconstructNode() override;

	virtual void PinDefaultValueChanged(UEdGraphPin* ChangedPin) override;

	virtual FText GetTooltipText() const override;

	//~ End UEdGraphNode Interface.

	//~ Begin K2Node Interface
	virtual bool IsNodePure() const override;

	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& InActionRegistrar) const override;

	//~ End K2Node Interface
};

UCLASS()
class ARCCOREEDITOR_API UArcK2Node_FindItemFragmentPure : public UK2Node_CallFunction
{
	GENERATED_BODY()

public:
	UArcK2Node_FindItemFragmentPure();

	//~ Begin UEdGraphNode Interface.
	virtual void PostReconstructNode() override;

	virtual void PinDefaultValueChanged(UEdGraphPin* ChangedPin) override;

	virtual FText GetTooltipText() const override;

	//~ End UEdGraphNode Interface.

	//~ Begin K2Node Interface
	virtual bool IsNodePure() const override;

	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& InActionRegistrar) const override;

	//~ End K2Node Interface
};

UCLASS()
class ARCCOREEDITOR_API UArcK2Node_FindScalableItemFragment : public UK2Node_CallFunction
{
	GENERATED_BODY()

public:
	UArcK2Node_FindScalableItemFragment();

	//~ Begin UEdGraphNode Interface.
	virtual void PostReconstructNode() override;

	virtual void PinDefaultValueChanged(UEdGraphPin* ChangedPin) override;

	virtual FText GetTooltipText() const override;

	//~ End UEdGraphNode Interface.

	//~ Begin K2Node Interface
	virtual bool IsNodePure() const override;

	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& InActionRegistrar) const override;

	//~ End K2Node Interface
};


UCLASS()
class ARCCOREEDITOR_API UArcK2Node_SendCommandToServer : public UK2Node_CallFunction
{
	GENERATED_BODY()

public:
	UArcK2Node_SendCommandToServer();

	//~ Begin UEdGraphNode Interface.
	virtual void PostReconstructNode() override;

	virtual void PinDefaultValueChanged(UEdGraphPin* ChangedPin) override;

	virtual FText GetTooltipText() const override;

	//~ End UEdGraphNode Interface.

	//~ Begin K2Node Interface
	virtual bool IsNodePure() const override;

	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& InActionRegistrar) const override;

	//~ End K2Node Interface
};