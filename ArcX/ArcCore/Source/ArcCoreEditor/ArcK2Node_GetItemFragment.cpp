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

#include "ArcK2Node_GetItemFragment.h"

#include "ArcStaticsBFL.h"
#include "ArcCore/Items/ArcItemsBPF.h"
#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintNodeSpawner.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "KismetCompiler.h"
#include "AbilitySystem/ArcCoreGameplayAbility.h"
#include "Items/ArcItemDefinition.h"

UArcK2Node_GetItemFragmentFromItemDefinition::UArcK2Node_GetItemFragmentFromItemDefinition()
{
	FunctionReference.SetExternalMember(GET_FUNCTION_NAME_CHECKED(UArcItemDefinition
			, BP_FindItemFragment)
		, UArcItemDefinition::StaticClass());
}

void UArcK2Node_GetItemFragmentFromItemDefinition::PostReconstructNode()
{
	Super::PostReconstructNode();
	UEdGraphPin* PayloadPin = FindPin(FName("OutFragment"));
	UEdGraphPin* PayloadTypePin = FindPinChecked(FName("InFragmentType"));

	if (PayloadTypePin->DefaultObject != PayloadPin->PinType.PinSubCategoryObject)
	{
		PayloadPin->PinType.PinSubCategoryObject = PayloadTypePin->DefaultObject;
		PayloadPin->PinType.PinCategory = (PayloadTypePin->DefaultObject == nullptr)
										  ? UEdGraphSchema_K2::PC_Wildcard
										  : UEdGraphSchema_K2::PC_Struct;
	}
}

void UArcK2Node_GetItemFragmentFromItemDefinition::PinDefaultValueChanged(UEdGraphPin* ChangedPin)
{
	Super::PinDefaultValueChanged(ChangedPin);
	UEdGraphPin* Pin = FindPin(FName("InFragmentType"));
	if (ChangedPin == Pin)
	{
		if (ChangedPin->LinkedTo.Num() == 0)
		{
			UEdGraphPin* PayloadPin = FindPin(FName("OutFragment"));
			UEdGraphPin* PayloadTypePin = FindPinChecked(FName("InFragmentType"));

			if (PayloadTypePin->DefaultObject != PayloadPin->PinType.PinSubCategoryObject)
			{
				if (PayloadPin->SubPins.Num() > 0)
				{
					GetSchema()->RecombinePin(PayloadPin);
				}

				PayloadPin->PinType.PinSubCategoryObject = PayloadTypePin->DefaultObject;
				PayloadPin->PinType.PinCategory = (PayloadTypePin->DefaultObject == nullptr)
												  ? UEdGraphSchema_K2::PC_Wildcard
												  : UEdGraphSchema_K2::PC_Struct;
			}
		}
	}
}

FText UArcK2Node_GetItemFragmentFromItemDefinition::GetTooltipText() const
{
	return FText::FromString("Get Item Fragment from Item Definition");
}

bool UArcK2Node_GetItemFragmentFromItemDefinition::IsNodePure() const
{
	return false;
}

void UArcK2Node_GetItemFragmentFromItemDefinition::GetMenuActions(FBlueprintActionDatabaseRegistrar& InActionRegistrar) const
{
	const UClass* ActionKey = GetClass();
	if (InActionRegistrar.IsOpenForRegistration(ActionKey))
	{
		UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
		check(NodeSpawner != nullptr);

		InActionRegistrar.AddBlueprintAction(ActionKey
			, NodeSpawner);
	}
}

////////////////////////////////////////////////////////////////////////////////////////
UArcK2Node_GetItemFragment::UArcK2Node_GetItemFragment()
{
	FunctionReference.SetExternalMember(GET_FUNCTION_NAME_CHECKED(UArcItemsBPF
			, GetItemFragment)
		, UArcItemsBPF::StaticClass());
}

void UArcK2Node_GetItemFragment::PostReconstructNode()
{
	Super::PostReconstructNode();
	UEdGraphPin* PayloadPin = FindPin(FName("OutFragment"));
	UEdGraphPin* PayloadTypePin = FindPinChecked(FName("InFragmentType"));

	if (PayloadTypePin->DefaultObject != PayloadPin->PinType.PinSubCategoryObject)
	{
		PayloadPin->PinType.PinSubCategoryObject = PayloadTypePin->DefaultObject;
		PayloadPin->PinType.PinCategory = (PayloadTypePin->DefaultObject == nullptr)
										  ? UEdGraphSchema_K2::PC_Wildcard
										  : UEdGraphSchema_K2::PC_Struct;
	}
}

void UArcK2Node_GetItemFragment::PinDefaultValueChanged(UEdGraphPin* ChangedPin)
{
	Super::PinDefaultValueChanged(ChangedPin);
	UEdGraphPin* Pin = FindPin(FName("InFragmentType"));
	if (ChangedPin == Pin)
	{
		if (ChangedPin->LinkedTo.Num() == 0)
		{
			UEdGraphPin* PayloadPin = FindPin(FName("OutFragment"));
			UEdGraphPin* PayloadTypePin = FindPinChecked(FName("InFragmentType"));

			if (PayloadTypePin->DefaultObject != PayloadPin->PinType.PinSubCategoryObject)
			{
				if (PayloadPin->SubPins.Num() > 0)
				{
					GetSchema()->RecombinePin(PayloadPin);
				}

				PayloadPin->PinType.PinSubCategoryObject = PayloadTypePin->DefaultObject;
				PayloadPin->PinType.PinCategory = (PayloadTypePin->DefaultObject == nullptr)
												  ? UEdGraphSchema_K2::PC_Wildcard
												  : UEdGraphSchema_K2::PC_Struct;
			}
		}
	}
}

FText UArcK2Node_GetItemFragment::GetTooltipText() const
{
	return FText::FromString("Get Item Fragment from provided Item Entry");
}

bool UArcK2Node_GetItemFragment::IsNodePure() const
{
	return false;
}

void UArcK2Node_GetItemFragment::GetMenuActions(FBlueprintActionDatabaseRegistrar& InActionRegistrar) const
{
	const UClass* ActionKey = GetClass();
	if (InActionRegistrar.IsOpenForRegistration(ActionKey))
	{
		UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
		check(NodeSpawner != nullptr);

		InActionRegistrar.AddBlueprintAction(ActionKey
			, NodeSpawner);
	}
}



UArcK2Node_FindItemFragment::UArcK2Node_FindItemFragment()
{
	FunctionReference.SetExternalMember(GET_FUNCTION_NAME_CHECKED(UArcCoreGameplayAbility
			, FindItemFragment)
		, UArcCoreGameplayAbility::StaticClass());
}

void UArcK2Node_FindItemFragment::PostReconstructNode()
{
	Super::PostReconstructNode();
	UEdGraphPin* PayloadPin = FindPin(FName("OutFragment"));
	UEdGraphPin* PayloadTypePin = FindPinChecked(FName("InFragmentType"));

	if (PayloadTypePin->DefaultObject != PayloadPin->PinType.PinSubCategoryObject)
	{
		PayloadPin->PinType.PinSubCategoryObject = PayloadTypePin->DefaultObject;
		PayloadPin->PinType.PinCategory = (PayloadTypePin->DefaultObject == nullptr)
										  ? UEdGraphSchema_K2::PC_Wildcard
										  : UEdGraphSchema_K2::PC_Struct;
	}
}

void UArcK2Node_FindItemFragment::PinDefaultValueChanged(UEdGraphPin* ChangedPin)
{
	Super::PinDefaultValueChanged(ChangedPin);
	
	UEdGraphPin* Pin = FindPin(FName("InFragmentType"));
	if (ChangedPin == Pin)
	{
		if (ChangedPin->LinkedTo.Num() == 0)
		{
			UEdGraphPin* PayloadPin = FindPin(FName("OutFragment"));
			UEdGraphPin* PayloadTypePin = FindPinChecked(FName("InFragmentType"));

			if (PayloadTypePin->DefaultObject != PayloadPin->PinType.PinSubCategoryObject)
			{
				if (PayloadPin->SubPins.Num() > 0)
				{
					GetSchema()->RecombinePin(PayloadPin);
				}

				PayloadPin->PinType.PinSubCategoryObject = PayloadTypePin->DefaultObject;
				PayloadPin->PinType.PinCategory = (PayloadTypePin->DefaultObject == nullptr)
												  ? UEdGraphSchema_K2::PC_Wildcard
												  : UEdGraphSchema_K2::PC_Struct;
			}
		}
	}
}

FText UArcK2Node_FindItemFragment::GetTooltipText() const
{
	return FText::FromString("Get Item Fragment for this ability source item");
}

bool UArcK2Node_FindItemFragment::IsNodePure() const
{
	return false;
}

void UArcK2Node_FindItemFragment::GetMenuActions(FBlueprintActionDatabaseRegistrar& InActionRegistrar) const
{
	const UClass* ActionKey = GetClass();
	if (InActionRegistrar.IsOpenForRegistration(ActionKey))
	{
		UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
		check(NodeSpawner != nullptr);

		InActionRegistrar.AddBlueprintAction(ActionKey
			, NodeSpawner);
	}
}
//==========================================

UArcK2Node_FindItemFragmentAbilityActor::UArcK2Node_FindItemFragmentAbilityActor()
{
	FunctionReference.SetExternalMember(GET_FUNCTION_NAME_CHECKED(UArcAbilityActorComponent
			, FindItemFragment)
		, UArcAbilityActorComponent::StaticClass());
}

void UArcK2Node_FindItemFragmentAbilityActor::PostReconstructNode()
{
	Super::PostReconstructNode();
	UEdGraphPin* PayloadPin = FindPin(FName("OutFragment"));
	UEdGraphPin* PayloadTypePin = FindPinChecked(FName("InFragmentType"));

	if (PayloadTypePin->DefaultObject != PayloadPin->PinType.PinSubCategoryObject)
	{
		PayloadPin->PinType.PinSubCategoryObject = PayloadTypePin->DefaultObject;
		PayloadPin->PinType.PinCategory = (PayloadTypePin->DefaultObject == nullptr)
										  ? UEdGraphSchema_K2::PC_Wildcard
										  : UEdGraphSchema_K2::PC_Struct;
	}
}

void UArcK2Node_FindItemFragmentAbilityActor::PinDefaultValueChanged(UEdGraphPin* ChangedPin)
{
	Super::PinDefaultValueChanged(ChangedPin);
	
	UEdGraphPin* Pin = FindPin(FName("InFragmentType"));
	if (ChangedPin == Pin)
	{
		if (ChangedPin->LinkedTo.Num() == 0)
		{
			UEdGraphPin* PayloadPin = FindPin(FName("OutFragment"));
			UEdGraphPin* PayloadTypePin = FindPinChecked(FName("InFragmentType"));

			if (PayloadTypePin->DefaultObject != PayloadPin->PinType.PinSubCategoryObject)
			{
				if (PayloadPin->SubPins.Num() > 0)
				{
					GetSchema()->RecombinePin(PayloadPin);
				}

				PayloadPin->PinType.PinSubCategoryObject = PayloadTypePin->DefaultObject;
				PayloadPin->PinType.PinCategory = (PayloadTypePin->DefaultObject == nullptr)
												  ? UEdGraphSchema_K2::PC_Wildcard
												  : UEdGraphSchema_K2::PC_Struct;
			}
		}
	}
}

FText UArcK2Node_FindItemFragmentAbilityActor::GetTooltipText() const
{
	return FText::FromString("Get Item Fragment for this ability source item");
}

bool UArcK2Node_FindItemFragmentAbilityActor::IsNodePure() const
{
	return false;
}

void UArcK2Node_FindItemFragmentAbilityActor::GetMenuActions(FBlueprintActionDatabaseRegistrar& InActionRegistrar) const
{
	const UClass* ActionKey = GetClass();
	if (InActionRegistrar.IsOpenForRegistration(ActionKey))
	{
		UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
		check(NodeSpawner != nullptr);

		InActionRegistrar.AddBlueprintAction(ActionKey
			, NodeSpawner);
	}
}

//==========================================

UArcK2Node_FindItemFragmentPure::UArcK2Node_FindItemFragmentPure()
{
	FunctionReference.SetExternalMember(GET_FUNCTION_NAME_CHECKED(UArcCoreGameplayAbility
		, FindItemFragmentPure)
	, UArcCoreGameplayAbility::StaticClass());
}

void UArcK2Node_FindItemFragmentPure::PostReconstructNode()
{
	Super::PostReconstructNode();
	
	UEdGraphPin* PayloadPin = FindPin(FName("OutFragment"));
	UEdGraphPin* PayloadTypePin = FindPinChecked(FName("InFragmentType"));

	if (PayloadTypePin->DefaultObject != PayloadPin->PinType.PinSubCategoryObject)
	{
		PayloadPin->PinType.PinSubCategoryObject = PayloadTypePin->DefaultObject;
		PayloadPin->PinType.PinCategory = (PayloadTypePin->DefaultObject == nullptr)
										  ? UEdGraphSchema_K2::PC_Wildcard
										  : UEdGraphSchema_K2::PC_Struct;
	}
}

void UArcK2Node_FindItemFragmentPure::PinDefaultValueChanged(UEdGraphPin* ChangedPin)
{
	Super::PinDefaultValueChanged(ChangedPin);

	UEdGraphPin* Pin = FindPin(FName("InFragmentType"));
	if (ChangedPin == Pin)
	{
		if (ChangedPin->LinkedTo.Num() == 0)
		{
			UEdGraphPin* PayloadPin = FindPin(FName("OutFragment"));
			UEdGraphPin* PayloadTypePin = FindPinChecked(FName("InFragmentType"));

			if (PayloadTypePin->DefaultObject != PayloadPin->PinType.PinSubCategoryObject)
			{
				if (PayloadPin->SubPins.Num() > 0)
				{
					GetSchema()->RecombinePin(PayloadPin);
				}

				PayloadPin->PinType.PinSubCategoryObject = PayloadTypePin->DefaultObject;
				PayloadPin->PinType.PinCategory = (PayloadTypePin->DefaultObject == nullptr)
												  ? UEdGraphSchema_K2::PC_Wildcard
												  : UEdGraphSchema_K2::PC_Struct;
			}
		}
	}
}

FText UArcK2Node_FindItemFragmentPure::GetTooltipText() const
{
	return FText::FromString("Get Item Fragment for this ability source item");
}

bool UArcK2Node_FindItemFragmentPure::IsNodePure() const
{
	return true;
}

void UArcK2Node_FindItemFragmentPure::GetMenuActions(FBlueprintActionDatabaseRegistrar& InActionRegistrar) const
{
	const UClass* ActionKey = GetClass();
	if (InActionRegistrar.IsOpenForRegistration(ActionKey))
	{
		UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
		check(NodeSpawner != nullptr);

		InActionRegistrar.AddBlueprintAction(ActionKey
			, NodeSpawner);
	}
}

UArcK2Node_FindScalableItemFragment::UArcK2Node_FindScalableItemFragment()
{
	FunctionReference.SetExternalMember(GET_FUNCTION_NAME_CHECKED(UArcCoreGameplayAbility
		, FindScalableItemFragment)
	, UArcCoreGameplayAbility::StaticClass());
}

void UArcK2Node_FindScalableItemFragment::PostReconstructNode()
{
	Super::PostReconstructNode();
	
	UEdGraphPin* PayloadPin = FindPin(FName("OutFragment"));
	UEdGraphPin* PayloadTypePin = FindPinChecked(FName("InFragmentType"));

	if (PayloadTypePin->DefaultObject != PayloadPin->PinType.PinSubCategoryObject)
	{
		PayloadPin->PinType.PinSubCategoryObject = PayloadTypePin->DefaultObject;
		PayloadPin->PinType.PinCategory = (PayloadTypePin->DefaultObject == nullptr)
										  ? UEdGraphSchema_K2::PC_Wildcard
										  : UEdGraphSchema_K2::PC_Struct;
	}
}

void UArcK2Node_FindScalableItemFragment::PinDefaultValueChanged(UEdGraphPin* ChangedPin)
{
	Super::PinDefaultValueChanged(ChangedPin);

	UEdGraphPin* Pin = FindPin(FName("InFragmentType"));
	if (ChangedPin == Pin)
	{
		if (ChangedPin->LinkedTo.Num() == 0)
		{
			UEdGraphPin* PayloadPin = FindPin(FName("OutFragment"));
			UEdGraphPin* PayloadTypePin = FindPinChecked(FName("InFragmentType"));

			if (PayloadTypePin->DefaultObject != PayloadPin->PinType.PinSubCategoryObject)
			{
				if (PayloadPin->SubPins.Num() > 0)
				{
					GetSchema()->RecombinePin(PayloadPin);
				}

				PayloadPin->PinType.PinSubCategoryObject = PayloadTypePin->DefaultObject;
				PayloadPin->PinType.PinCategory = (PayloadTypePin->DefaultObject == nullptr)
												  ? UEdGraphSchema_K2::PC_Wildcard
												  : UEdGraphSchema_K2::PC_Struct;
			}
		}
	}
}

FText UArcK2Node_FindScalableItemFragment::GetTooltipText() const
{
	return FText::FromString("Get Item Fragment for this ability source item");
}

bool UArcK2Node_FindScalableItemFragment::IsNodePure() const
{
	return true;
}

void UArcK2Node_FindScalableItemFragment::GetMenuActions(FBlueprintActionDatabaseRegistrar& InActionRegistrar) const
{
	const UClass* ActionKey = GetClass();
	if (InActionRegistrar.IsOpenForRegistration(ActionKey))
	{
		UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
		check(NodeSpawner != nullptr);

		InActionRegistrar.AddBlueprintAction(ActionKey
			, NodeSpawner);
	}
}
////////////////////////////////////////////////////////////////////////////
////////////////////////////////
////////////////////////////////////////////////////////////////
///
UArcK2Node_SendCommandToServer::UArcK2Node_SendCommandToServer()
{
	FunctionReference.SetExternalMember(GET_FUNCTION_NAME_CHECKED(UArcStaticsBFL
			, SendCommandToServer)
		, UArcStaticsBFL::StaticClass());
}

void UArcK2Node_SendCommandToServer::PostReconstructNode()
{
	Super::PostReconstructNode();
	UEdGraphPin* PayloadPin = FindPin(FName("InCommand"));
	UEdGraphPin* PayloadTypePin = FindPinChecked(FName("InCommandType"));

	if (PayloadTypePin->DefaultObject != PayloadPin->PinType.PinSubCategoryObject)
	{
		PayloadPin->PinType.PinSubCategoryObject = PayloadTypePin->DefaultObject;
		PayloadPin->PinType.PinCategory = (PayloadTypePin->DefaultObject == nullptr)
										  ? UEdGraphSchema_K2::PC_Wildcard
										  : UEdGraphSchema_K2::PC_Struct;
	}
}

void UArcK2Node_SendCommandToServer::PinDefaultValueChanged(UEdGraphPin* ChangedPin)
{
	Super::PinDefaultValueChanged(ChangedPin);
	UEdGraphPin* Pin = FindPin(FName("InCommandType"));
	if (ChangedPin == Pin)
	{
		if (ChangedPin->LinkedTo.Num() == 0)
		{
			UEdGraphPin* PayloadPin = FindPin(FName("InCommand"));
			UEdGraphPin* PayloadTypePin = FindPinChecked(FName("InCommandType"));

			if (PayloadTypePin->DefaultObject != PayloadPin->PinType.PinSubCategoryObject)
			{
				if (PayloadPin->SubPins.Num() > 0)
				{
					GetSchema()->RecombinePin(PayloadPin);
				}

				PayloadPin->PinType.PinSubCategoryObject = PayloadTypePin->DefaultObject;
				PayloadPin->PinType.PinCategory = (PayloadTypePin->DefaultObject == nullptr)
												  ? UEdGraphSchema_K2::PC_Wildcard
												  : UEdGraphSchema_K2::PC_Struct;
			}
		}
	}
}

FText UArcK2Node_SendCommandToServer::GetTooltipText() const
{
	return FText::FromString("Send replicated command to server.");
}

bool UArcK2Node_SendCommandToServer::IsNodePure() const
{
	return false;
}

void UArcK2Node_SendCommandToServer::GetMenuActions(FBlueprintActionDatabaseRegistrar& InActionRegistrar) const
{
	const UClass* ActionKey = GetClass();
	if (InActionRegistrar.IsOpenForRegistration(ActionKey))
	{
		UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
		check(NodeSpawner != nullptr);

		InActionRegistrar.AddBlueprintAction(ActionKey
			, NodeSpawner);
	}
}
