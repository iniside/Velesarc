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

#include "ArcCore/GameFeatures/ArcGFA_AddItem.h"
#include "Components/GameFrameworkComponentManager.h"
#include "Engine/AssetManager.h"
#include "Engine/World.h"
#include "GameFeaturesSubsystem.h"
#include "GameFeaturesSubsystemSettings.h"

#include "ArcCore/Items/ArcItemsComponent.h"
#include "Engine/GameInstance.h"
#include "Items/ArcItemSpec.h"
#include "Items/ArcItemsStoreComponent.h"

//////////////////////////////////////////////////////////////////////
// UArcGFA_AddItem

void UArcGFA_AddItem::OnGameFeatureActivating(FGameFeatureActivatingContext& Context)
{
	Super::OnGameFeatureActivating();
	for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
	{
		UWorld* World = WorldContext.World();
		UGameInstance* GameInstance = WorldContext.OwningGameInstance;

		if ((GameInstance != nullptr) && (World != nullptr) && World->IsGameWorld())
		{
			if (UGameFrameworkComponentManager* ComponentMan = UGameInstance::GetSubsystem<
				UGameFrameworkComponentManager>(GameInstance))
			{
				UGameFrameworkComponentManager::FExtensionHandlerDelegate AddAbilitiesDelegate =
						UGameFrameworkComponentManager::FExtensionHandlerDelegate::CreateUObject(this
							, &UArcGFA_AddItem::HandleActorExtension);
				
				TSharedPtr<FComponentRequestHandle> ExtensionRequestHandle = ComponentMan->AddExtensionHandler(ActorClass, AddAbilitiesDelegate);
				ComponentRequests.Add(ExtensionRequestHandle);
			}
		}
	}
}

void UArcGFA_AddItem::OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context)
{
	Super::OnGameFeatureDeactivating(Context);
	ComponentRequests.Empty();
}

#if WITH_EDITOR
EDataValidationResult UArcGFA_AddItem::IsDataValid(class FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(Context), EDataValidationResult::Valid);

	return EDataValidationResult::NotValidated;
}
#endif

void UArcGFA_AddItem::HandleActorExtension(AActor* Actor
										   , FName EventName)
{
	if (!Actor->HasAuthority())
	{
		return;
	}
	if (ComponentClass == nullptr)
	{
		return;
	}
	if (EventName == ComponentClass->GetFName())
	{
		UArcItemsStoreComponent* OIC = Cast<UArcItemsStoreComponent>(Actor->GetComponentByClass(ComponentClass));
		FArcItemId Id;
		if (OIC)
		{
			FPrimaryAssetId DefinitionId;
			DefinitionId = ItemDefinitionId;
			
			FArcItemSpec Spec;
			Spec.SetItemId(FArcItemId::Generate()).SetItemDefinition(DefinitionId).SetAmount(ItemNum);

			OIC->AddItem(Spec, FArcItemId());
		}
	}
}
