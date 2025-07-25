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


#include "ArcCueTranslatorTypes.generated.h"

USTRUCT()
struct FArcTranslationUnit
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere
		, Config
		, Category = "Config")
	FName From;

	UPROPERTY(EditAnywhere
		, Config
		, Category = "Config")
	TArray<FName> To;
};

USTRUCT()
struct FArcTranslatorUnit
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere
		, Config
		, Category = "Config")
	TSoftClassPtr<class UGameplayCueTranslator> TranslatorClass;
	UPROPERTY(EditAnywhere
		, Config
		, Category = "Config")
	TArray<FString> Units;

	FName GetCombinedUnit() const
	{
		FString Combined;
		for (int32 Idx = 0; Idx < Units.Num(); Idx++)
		{
			bool bUseSeprator = Idx + 1 < Units.Num();
			FString Seprator = bUseSeprator ? "-" : "";
			Combined += (Units[Idx] + Seprator);
		}
		return *Combined;
	}
};

USTRUCT()
struct FArcCueName
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere
		, Category = "Config")
	FName CueName;
};
