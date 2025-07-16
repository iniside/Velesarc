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



#include "ArcItemDefinitionTemplateFactory.h"

#include "ArcAssetDefinition_ArcItemDefinitionTemplate.h"

UArcItemDefinitionTemplateFactory::UArcItemDefinitionTemplateFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UArcItemDefinitionTemplate::StaticClass();
}

bool UArcItemDefinitionTemplateFactory::ConfigureProperties()
{
	return true;
}

UObject* UArcItemDefinitionTemplateFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context,
														FFeedbackContext* Warn, FName CallingContext)
{
	// Not calling super is intentional. It doesn't do anything useful. 
	// Like it shouldn't happen.
	// Just use passed in class. No need to allow selection.
	check(Class->IsChildOf(UArcItemDefinitionTemplate::StaticClass()));
	UArcItemDefinitionTemplate* NewItemDefinition = NewObject<UArcItemDefinitionTemplate>(InParent, Class, Name, Flags);

	return NewItemDefinition;
}