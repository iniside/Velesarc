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

#include "GameFramework/SaveGame.h"
#include "StructUtils/PropertyBag.h"

#include "ArcSettingsStorage.generated.h"

namespace ArcSettingsStorage
{
	ARCGAMESETTINGS_API bool SaveLocalStore(FInstancedPropertyBag& InBag, const FString& InSlotName);
	ARCGAMESETTINGS_API bool LoadLocalStore(FInstancedPropertyBag& OutBag, const FString& InSlotName);
}

UCLASS()
class ARCGAMESETTINGS_API UArcSettingsSharedSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TArray<uint8> PropertyBagData;

	void StorePropertyBag(FInstancedPropertyBag& InBag);
	bool RestorePropertyBag(FInstancedPropertyBag& OutBag) const;
};
