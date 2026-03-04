/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2026 Lukasz Baran
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

#include "CoreMinimal.h"

namespace UE::ArcPersistence
{
    enum class EKeyCategory : uint8
    {
        World,    // world/{WorldId}/{SubKey}
        Player,   // players/{PlayerId}/{Domain}
        Unknown
    };

    struct FParsedKey
    {
        EKeyCategory Category = EKeyCategory::Unknown;
        FString OwnerId;   // WorldId or PlayerId
        FString SubKey;     // remainder after owner segment
        bool bValid = false;
    };

    ARCPERSISTENCE_API FParsedKey ParseKey(const FString& Key);
    ARCPERSISTENCE_API bool ValidateKey(const FString& Key);
    ARCPERSISTENCE_API FString MakeWorldKey(const FString& WorldId, const FString& SubKey);
    ARCPERSISTENCE_API FString MakePlayerKey(const FString& PlayerId, const FString& Domain);
}
