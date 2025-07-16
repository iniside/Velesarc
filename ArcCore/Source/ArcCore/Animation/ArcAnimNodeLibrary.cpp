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



#include "ArcAnimNodeLibrary.h"

#include "Chooser.h"
#include "BlendStack/BlendStackAnimNodeLibrary.h"

FChooserPlayerAnimNodeReference UArcAnimNodeLibrary::ConvertToChoosePlayerNode(const FAnimNodeReference& Node
																			 , EAnimNodeReferenceConversionResult& Result)
{
	return FAnimNodeReference::ConvertToType<FChooserPlayerAnimNodeReference>(Node, Result);
}

void UArcAnimNodeLibrary::SetChooser(const FAnimNodeReference& Node
	, UChooserTable* ChooserTable)
{
	if (FAnimNode_ChooserPlayer* BlendStackInput = Node.GetAnimNodePtr<FAnimNode_ChooserPlayer>())
	{
		if (FEvaluateChooser* Existing = BlendStackInput->Chooser.GetMutablePtr<FEvaluateChooser>())
		{
			Existing->Chooser = ChooserTable;
		}
		else
		{
			BlendStackInput->Chooser = FInstancedStruct::Make<FEvaluateChooser>(ChooserTable);	
		}
		
	}
}
