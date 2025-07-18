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

#include "Components/GameFrameworkComponentManager.h"

#include "ArcNamedPrimaryAssetId.h"
#include "GameFeatureAction.h"
#include "ArcGFA_AddItem.generated.h"

class UArcItemDefinition;
class UArcItemsStoreComponent;
/**
 *
 */
UCLASS()
class ARCCORE_API UArcGFA_AddItem : public UGameFeatureAction
{
	GENERATED_BODY()
	TArray<TSharedPtr<FComponentRequestHandle>> ComponentRequests;

public:
	//~ Begin UGameFeatureAction interface
	virtual void OnGameFeatureActivating(FGameFeatureActivatingContext& Context) override;

	virtual void OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context) override;

	//~ End UGameFeatureAction interface

	//~ Begin UObject interface
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
	//~ End UObject interface

	/**  */

protected:
	UPROPERTY(EditAnywhere, Category = "Item")
	TSoftClassPtr<AActor> ActorClass;

	UPROPERTY(EditAnywhere, Category = "Item")
	TSubclassOf<UArcItemsStoreComponent> ComponentClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item", meta = (AllowedClasses = "/Script/ArcCore.ArcItemDefinition", DisplayThumbnail = false))
	FArcNamedPrimaryAssetId ItemDefinitionId;

	UPROPERTY(EditAnywhere, Category = "Item")
	int32 ItemNum;

private:
	void HandleActorExtension(AActor* Actor
							  , FName EventName); ;
};
