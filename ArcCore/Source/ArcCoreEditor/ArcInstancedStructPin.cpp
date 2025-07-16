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

#include "ArcInstancedStructPin.h"
#include "ContentBrowserModule.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/Blueprint.h"
#include "IContentBrowserSingleton.h"
#include "K2Node_Variable.h"
#include "PropertyCustomizationHelpers.h"
#include "UObject/PropertyAccessUtil.h"
#include "Widgets/SBoxPanel.h"

void SArcInstancedStructPin::Construct(const FArguments& InArgs
									   , UEdGraphPin* InGraphPinObj)
{
	SGraphPin::Construct(SGraphPin::FArguments()
		, InGraphPinObj);
}

TSharedRef<SWidget> SArcInstancedStructPin::GetDefaultValueWidget()
{
	FName PinName = GetPinObj()->GetFName();
	UEdGraphNode* Node = GraphPinObj->GetOwningNode();

	FString Value = Node->GetPinMetaData(PinName
		, FName(TEXT("BaseStruct")));

	if (GraphPinObj->LinkedTo.Num() > 0)
	{
		UEdGraphPin* Link = GraphPinObj->LinkedTo[0];
		if (Link)
		{
			UEdGraphNode* LinkNode = Link->GetOwningNode();
			if (UK2Node_Variable* VarNode = Cast<UK2Node_Variable>(LinkNode))
			{
				FProperty* Prop = VarNode->GetPropertyForVariable();
				UClass* C = VarNode->GetVariableSourceClass();

				UClass* C2 = Cast<UBlueprint>(C->ClassGeneratedBy)->GeneratedClass;
				FProperty* Prop3 = PropertyAccessUtil::FindPropertyByName(Prop->GetFName()
					, C2);

				if (Prop3)
				{
					Prop3->SetMetaData(TEXT("BaseStruct")
						, MoveTemp(Value));
				}
			}
		}
	}

	TSharedPtr<SWidget> PinWidget = SGraphPinObject::GetDefaultValueWidget();
	return SNullWidget::NullWidget; // PinWidget.ToSharedRef();
}
