/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or -
 * as soon as they will be approved by the European Commission - later versions
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

#include "Factories/Factory.h"
#include "EditorReimportHandler.h"

#include "ArcQualityTierTableJsonImportFactory.generated.h"

class UArcQualityTierTable;

/**
 * Factory that imports JSON files into UArcQualityTierTable assets.
 * Also serves as a reimport handler.
 *
 * Supported JSON format:
 *   { "$type": "ArcQualityTierTable", "tiers": [ ... ] }
 */
UCLASS()
class ARCCRAFTEDITOR_API UArcQualityTierTableJsonImportFactory : public UFactory, public FReimportHandler
{
	GENERATED_BODY()

public:
	UArcQualityTierTableJsonImportFactory(const FObjectInitializer& ObjectInitializer);

	// ---- UFactory interface ----
	virtual bool FactoryCanImport(const FString& Filename) override;
	virtual UObject* FactoryCreateFile(UClass* InClass, UObject* InParent,
		FName InName, EObjectFlags Flags, const FString& Filename,
		const TCHAR* Parms, FFeedbackContext* Warn,
		bool& bOutOperationCanceled) override;

	// ---- FReimportHandler interface ----
	virtual bool CanReimport(UObject* Obj, TArray<FString>& OutFilenames) override;
	virtual void SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths) override;
	virtual EReimportResult::Type Reimport(UObject* Obj) override;
	virtual int32 GetPriority() const override;

private:
	static void ResetToDefaults(UArcQualityTierTable* Table);
};
