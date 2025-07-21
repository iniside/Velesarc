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
#include "ArcItemId.generated.h"

/**
 * Represents unique identifier for every item instance.
 */
USTRUCT(BlueprintType)
struct ARCCORE_API FArcItemId
{
	GENERATED_BODY()

protected:
	UPROPERTY(SaveGame)
	FGuid Id;

public:
	static const FGuid InvalidGuid;
	static const FArcItemId InvalidId;
	
	FArcItemId()
		: Id(InvalidGuid)
	{
	}
	
	FArcItemId(FGuid InHandle)
		: Id(InHandle)
	{
	};

public:
	static FArcItemId Generate();

	void Reset()
	{
		Id = FGuid(0
			, 0
			, 0
			, 0);
	}

	bool IsValid() const
	{
		return Id.IsValid(); // > 0;
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
	};
	
	FArcItemId& operator=(const FArcItemId& Other)
	{
		Id = Other.Id;
		return *this;
	}

	FArcItemId& operator=(const FGuid& Other)
	{
		Id = Other;
		return *this;
	}
	
	bool operator==(const FArcItemId& Other) const
	{
		return Id == Other.Id;
	}

	bool operator!=(const FArcItemId& Other) const
	{
		return Id != Other.Id;
	}

	friend inline uint64 GetTypeHash(const FArcItemId& Key)
	{
		return uint64(CityHash64((char*)&Key.Id
			, sizeof(FGuid)));
	}

	const FGuid& GetGuid() const
	{
		return Id;
	}

	void GetNetSerializer(uint32& GuidA
						  , uint32& GuidB
						  , uint32& GuidC
						  , uint32& GuidD)
	{
		GuidA = Id.A;
		GuidB = Id.B;
		GuidC = Id.C;
		GuidD = Id.D;
	}

	void SetNetSerializer(uint32 GuidA
						  , uint32 GuidB
						  , uint32 GuidC
						  , uint32 GuidD)
	{
		Id.A = GuidA;
		Id.B = GuidB;
		Id.C = GuidC;
		Id.D = GuidD;
	}

	bool NetSerialize(FArchive& Ar
					  , class UPackageMap* Map
					  , bool& bOutSuccess)
	{
		Ar << Id;
		bOutSuccess = true;
		return true;
	}

	friend FArchive& operator<<(FArchive& Ar, FArcItemId& InId)
	{
		Ar << InId.Id;
		return Ar;
	}
};

template <>
struct TStructOpsTypeTraits<FArcItemId> : public TStructOpsTypeTraitsBase2<FArcItemId>
{
	enum
	{
		WithNetSerializer = true
		,
	};
};

