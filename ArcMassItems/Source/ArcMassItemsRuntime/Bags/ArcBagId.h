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

#include "Misc/Guid.h"
#include "ArcBagId.generated.h"

/**
 * Represents a unique identifier for a bag instance.
 * Relies on default FGuid member serialization — no custom net serializer.
 */
USTRUCT(BlueprintType)
struct ARCMASSITEMSRUNTIME_API FArcBagId
{
	GENERATED_BODY()

protected:
	UPROPERTY(SaveGame)
	FGuid Id;

public:
	static const FGuid InvalidGuid;
	static const FArcBagId InvalidId;

	FArcBagId()
		: Id(InvalidGuid)
	{
	}

	FArcBagId(FGuid InHandle)
		: Id(InHandle)
	{
	}

public:
	static FArcBagId Generate();

	void Reset()
	{
		Id = FGuid(0, 0, 0, 0);
	}

	bool IsValid() const
	{
		return Id.IsValid();
	}

	bool IsNone() const
	{
		return Id.IsValid() == false;
	}

	FString ToString() const
	{
		return Id.ToString(EGuidFormats::DigitsLower);
	}

	void FromString(const FString& InString)
	{
		Id = FGuid(InString);
	}

	FArcBagId& operator=(const FArcBagId& Other)
	{
		Id = Other.Id;
		return *this;
	}

	FArcBagId& operator=(const FGuid& Other)
	{
		Id = Other;
		return *this;
	}

	bool operator==(const FArcBagId& Other) const
	{
		return Id == Other.Id;
	}

	bool operator!=(const FArcBagId& Other) const
	{
		return Id != Other.Id;
	}

	friend inline uint64 GetTypeHash(const FArcBagId& Key)
	{
		return uint64(CityHash64((char*)&Key.Id, sizeof(FGuid)));
	}

	const FGuid& GetGuid() const
	{
		return Id;
	}

	friend FArchive& operator<<(FArchive& Ar, FArcBagId& InId)
	{
		Ar << InId.Id;
		return Ar;
	}
};
