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

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "EditorReimportHandler.h"
#include "ArcSvgAssetFactory.generated.h"

/**
 * Imports .svg source files into UArcSvgAsset, storing the raw UTF-8 bytes directly on
 * the asset. Reimport reloads the source bytes and busts the asset's extracted-file cache
 * (ClearExtractedCache + re-extract) so the new content-hashed path defeats Slate's
 * {path, size} vector graphics cache.
 */
UCLASS()
class UArcSvgAssetFactory : public UFactory, public FReimportHandler
{
	GENERATED_BODY()

public:
	UArcSvgAssetFactory(const FObjectInitializer& ObjectInitializer);

	//~ Begin UFactory interface
	virtual UObject* FactoryCreateBinary(UClass* InClass
										 , UObject* InParent
										 , FName InName
										 , EObjectFlags Flags
										 , UObject* Context
										 , const TCHAR* Type
										 , const uint8*& Buffer
										 , const uint8* BufferEnd
										 , FFeedbackContext* Warn) override;
	//~ End UFactory interface

	//~ Begin FReimportHandler interface
	virtual bool CanReimport(UObject* Obj, TArray<FString>& OutFilenames) override;
	virtual void SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths) override;
	virtual EReimportResult::Type Reimport(UObject* Obj) override;
	//~ End FReimportHandler interface
};
