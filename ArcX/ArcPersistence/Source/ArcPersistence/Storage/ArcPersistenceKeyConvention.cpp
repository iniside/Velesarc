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

#include "Storage/ArcPersistenceKeyConvention.h"

namespace UE::ArcPersistence
{

static const FString WorldPrefix = TEXT("world/");
static const FString PlayerPrefix = TEXT("players/");

FParsedKey ParseKey(const FString& Key)
{
    FParsedKey Result;

    if (Key.IsEmpty())
    {
        return Result;
    }

    EKeyCategory Category = EKeyCategory::Unknown;
    int32 PrefixLen = 0;

    if (Key.StartsWith(WorldPrefix))
    {
        Category = EKeyCategory::World;
        PrefixLen = WorldPrefix.Len();
    }
    else if (Key.StartsWith(PlayerPrefix))
    {
        Category = EKeyCategory::Player;
        PrefixLen = PlayerPrefix.Len();
    }
    else
    {
        return Result;
    }

    const FString Remainder = Key.Mid(PrefixLen);
    int32 SlashIndex = INDEX_NONE;
    if (!Remainder.FindChar(TEXT('/'), SlashIndex) || SlashIndex <= 0)
    {
        return Result;
    }

    Result.Category = Category;
    Result.OwnerId = Remainder.Left(SlashIndex);
    Result.SubKey = Remainder.Mid(SlashIndex + 1);
    Result.bValid = !Result.OwnerId.IsEmpty() && !Result.SubKey.IsEmpty();

    return Result;
}

bool ValidateKey(const FString& Key)
{
    const FParsedKey Parsed = ParseKey(Key);
    if (!Parsed.bValid)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("ArcPersistence: Key '%s' does not match convention (world/{id}/{sub} or players/{id}/{domain})"),
            *Key);
    }
    return Parsed.bValid;
}

FString MakeWorldKey(const FString& WorldId, const FString& SubKey)
{
    return FString::Printf(TEXT("world/%s/%s"), *WorldId, *SubKey);
}

FString MakePlayerKey(const FString& PlayerId, const FString& Domain)
{
    return FString::Printf(TEXT("players/%s/%s"), *PlayerId, *Domain);
}

} // namespace UE::ArcPersistence
