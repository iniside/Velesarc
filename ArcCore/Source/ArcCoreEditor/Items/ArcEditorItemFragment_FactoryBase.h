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
#include "ArcEditorItemFragment.h"

#include "ArcEditorItemFragment_FactoryBase.generated.h"

class UArcItemDefinition;

/**
 * 
 */
USTRUCT(meta = (Hidden))
struct ARCCOREEDITOR_API FArcEditorItemFragment_FactoryBase : public FArcEditorItemFragment
{
	GENERATED_BODY()

public:
	virtual EDataValidationResult IsDataValid(const UArcItemDefinition* ItemDefinition, class FDataValidationContext& Context) const override;
	//~ Begin UActorFactory Interface
	virtual UClass* GetDefaultActorClass(const FAssetData& AssetData) const { return nullptr; }
	virtual void PostSpawnActor( UArcItemDefinition* Asset, AActor* NewActor ) const {};
	virtual bool CanCreateActorFrom( const FAssetData& AssetData, FText& OutErrorMsg ) const { return true; };
	//~ End UActorFactory Interface
};