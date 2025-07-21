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

#include "ArcTargetDataId.generated.h"

namespace Arcx::Net
{
	struct FArcTargetDataIdNetSerializer;
}

USTRUCT(BlueprintType)
struct ARCCORE_API FArcTargetDataId
{
	GENERATED_BODY()

	friend struct Arcx::Net::FArcTargetDataIdNetSerializer;

private:
	// might not be an issue (it is generated on client)
	// but it is combined with Ability spec which is unique on server.
	UPROPERTY()
	uint16 Handle = 0;

	FArcTargetDataId(uint16 NewHandle)
		: Handle(NewHandle)
	{
	}

public:
	FArcTargetDataId()
		: Handle(0)
	{
	}

	bool IsValid() const
	{
		return Handle > 0;
	}

	bool operator==(const FArcTargetDataId& Other) const
	{
		return Handle == Other.Handle;
	}

	static FArcTargetDataId Generate()
	{
		static uint16 NewHandle = 0;
		NewHandle++;
		return FArcTargetDataId(NewHandle);
	}

	friend uint32 GetTypeHash(const FArcTargetDataId& Item)
	{
		return Item.Handle;
	}

	friend FArchive& operator<<(FArchive& Ar
								, FArcTargetDataId& Item)
	{
		Ar << Item.Handle;
		return Ar;
	}

	FString ToString() const
	{
		return FString::FormatAsNumber(Handle);
	}
};
