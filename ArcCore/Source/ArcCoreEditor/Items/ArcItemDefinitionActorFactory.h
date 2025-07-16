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

#pragma once

#include "CoreMinimal.h"
#include "ArcEditorItemFragment_FactoryBase.h"
#include "ActorFactories/ActorFactory.h"
#include "ArcItemDefinitionActorFactory.generated.h"


/**
 * 
 */
UCLASS()
class ARCCOREEDITOR_API UArcItemDefinitionActorFactory : public UActorFactory
{
	GENERATED_BODY()

public:
	UArcItemDefinitionActorFactory(const FObjectInitializer& ObjectInitializer);
	//~ Begin UActorFactory Interface

	/** Initialize NewActorClass if necessary, and return that class. */
	virtual auto GetDefaultActorClass(const FAssetData& AssetData) -> UClass* override;
	
	virtual void PostSpawnActor( UObject* Asset, AActor* NewActor ) override;
	virtual bool CanCreateActorFrom( const FAssetData& AssetData, FText& OutErrorMsg ) override;
	//~ End UActorFactory Interface
};

USTRUCT()
struct ARCCOREEDITOR_API FArcEditorItemFragment_TestFactory : public FArcEditorItemFragment_FactoryBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere)
	TSoftClassPtr<AActor> ActorToSpawn;
	
	//~ Begin UActorFactory Interface
	virtual UClass* GetDefaultActorClass(const FAssetData& AssetData) const override;
	virtual void PostSpawnActor( UArcItemDefinition* Asset, AActor* NewActor ) const override;
	virtual bool CanCreateActorFrom( const FAssetData& AssetData, FText& OutErrorMsg ) const override;
	//~ End UActorFactory Interface
};