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



#include "ArcItemDefinitionActorFactory.h"

#include "Items/ArcItemDefinition.h"
#include "Misc/DataValidation.h"

UArcItemDefinitionActorFactory::UArcItemDefinitionActorFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NewActorClass = AActor::StaticClass();
}

UClass* UArcItemDefinitionActorFactory::GetDefaultActorClass(const FAssetData& AssetData)
{
	UArcItemDefinition* ItemDef = Cast<UArcItemDefinition>(AssetData.GetAsset());

	if (ItemDef == nullptr)
	{
		return nullptr;
	}
	
	const FArcEditorItemFragment_FactoryBase* FactoryFragment = ItemDef->GetChildEditorFragment<FArcEditorItemFragment_FactoryBase>();
	if (FactoryFragment == nullptr)
	{
		return nullptr;
	}

	UClass* ActorClass = FactoryFragment->GetDefaultActorClass(AssetData);
	NewActorClass = ActorClass;
	return ActorClass;
}

void UArcItemDefinitionActorFactory::PostSpawnActor(UObject* Asset
													, AActor* NewActor)
{
	Super::PostSpawnActor(Asset, NewActor);
	
	UArcItemDefinition* ItemDef = Cast<UArcItemDefinition>(Asset);
	const FArcEditorItemFragment_FactoryBase* FactoryFragment = ItemDef->GetChildEditorFragment<FArcEditorItemFragment_FactoryBase>();
	if (FactoryFragment == nullptr)
	{
		return;
	}

	FactoryFragment->PostSpawnActor(ItemDef, NewActor);
}

bool UArcItemDefinitionActorFactory::CanCreateActorFrom(const FAssetData& AssetData
	, FText& OutErrorMsg)
{
	if (!AssetData.IsValid() || !AssetData.GetClass()->IsChildOf(UArcItemDefinition::StaticClass()))
	{
		OutErrorMsg = NSLOCTEXT("CanCreateActor", "NoFieldSystemSpecified", "No FieldSystem mesh was specified.");
		return false;
	}

	UArcItemDefinition* ItemDef = Cast<UArcItemDefinition>(AssetData.GetAsset());

	const FArcEditorItemFragment_FactoryBase* FactoryFragment = ItemDef->GetChildEditorFragment<FArcEditorItemFragment_FactoryBase>();
	if (FactoryFragment == nullptr)
	{
		return false;
	}
	
	return FactoryFragment->CanCreateActorFrom(AssetData, OutErrorMsg);
}

EDataValidationResult FArcEditorItemFragment_FactoryBase::IsDataValid(const UArcItemDefinition* ItemDefinition
	, FDataValidationContext& Context) const
{
	TArray<const FArcEditorItemFragment_FactoryBase*> FactoryFragments = ItemDefinition->GetAllChildEditorFragment<FArcEditorItemFragment_FactoryBase>();

	if (FactoryFragments.Num() > 1)
	{
		Context.AddError(FText::FromString("Only one Item Factory Fragment can be present"));
		return EDataValidationResult::Invalid;
	}

	return EDataValidationResult::Valid;
}

UClass* FArcEditorItemFragment_TestFactory::GetDefaultActorClass(const FAssetData& AssetData) const
{
	UClass* ActorClass = ActorToSpawn.LoadSynchronous();

	return ActorClass;
}

void FArcEditorItemFragment_TestFactory::PostSpawnActor(UArcItemDefinition* Asset
	, AActor* NewActor) const
{
	
}

bool FArcEditorItemFragment_TestFactory::CanCreateActorFrom(const FAssetData& AssetData
	, FText& OutErrorMsg) const
{
	return true;
}
