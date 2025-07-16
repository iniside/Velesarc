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

#include "ArcItemDefinitionFactory.h"
#include "ArcCore/Items/ArcItemDefinition.h"

#include "ClassViewerModule.h"
#include "Kismet2/SClassPickerDialog.h"
#include "ArcClassFilter.h"
#include "ArcStaticItemHelpers.h"
#include "IContentBrowserSingleton.h"
#include "Modules/ModuleManager.h"

#include "Engine/BlueprintGeneratedClass.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"

UArcItemDefinitionFactory::UArcItemDefinitionFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UArcItemDefinition::StaticClass();
}

bool UArcItemDefinitionFactory::ConfigureProperties()
{
	UArcItemDefinitionTemplate* SelectedTemplate = nullptr;
	const bool bPressedOk = FArcStaticItemHelpers::PickItemSourceTemplate(SelectedTemplate);

	if (bPressedOk == false)
	{
		return false;
	}
	
	SourceItemTemplate = SelectedTemplate;
	return true;
}

UObject* UArcItemDefinitionFactory::FactoryCreateNew(UClass* Class
											   , UObject* InParent
											   , FName Name
											   , EObjectFlags Flags
											   , UObject* Context
											   , FFeedbackContext* Warn
											   , FName CallingContext)
{
	UArcItemDefinition* NewItemDefinition = NewObject<UArcItemDefinition>(InParent, Class, Name, Flags);

	if (SourceItemTemplate)
	{
		SourceItemTemplate->SetNewOrReplaceItemTemplate(NewItemDefinition);
	}
	
	return NewItemDefinition;
}