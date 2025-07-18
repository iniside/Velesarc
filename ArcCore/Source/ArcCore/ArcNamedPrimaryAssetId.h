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

#include "UObject/PrimaryAssetId.h"
#include "ArcNamedPrimaryAssetId.generated.h"

class UObject;

/**
 * Wraps FPrimaryAssetId to have display name as separate property. Makes easier handling in editor for
 * primary assets which consist from GUID as their asset name.
 *
 * If you use guid for generating primary asset id, you should use in places where you want to have asset picker for PrimaryAssetId.
 */
USTRUCT(BlueprintType)
struct ARCCORE_API FArcNamedPrimaryAssetId
{
	GENERATED_BODY()

public:
#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere)
	FString DisplayName;
#endif

	UPROPERTY(EditAnywhere)
	FPrimaryAssetId AssetId;

	operator const FPrimaryAssetId&() const
	{
		return AssetId;
	}

	FArcNamedPrimaryAssetId()
#if WITH_EDITORONLY_DATA
		: DisplayName()
		, AssetId()
#else
		: AssetId()
#endif
	{}

	FArcNamedPrimaryAssetId(const FArcNamedPrimaryAssetId& Other)
	#if WITH_EDITORONLY_DATA
		: DisplayName(Other.DisplayName)
		, AssetId(Other.AssetId)
	#else
		: AssetId(Other.AssetId)
	#endif
	{}

	FArcNamedPrimaryAssetId(FArcNamedPrimaryAssetId&& Other)
	#if WITH_EDITORONLY_DATA
		: DisplayName(MoveTemp(Other.DisplayName))
		, AssetId(MoveTemp(Other.AssetId))
	#else
		: AssetId(MoveTemp(Other.AssetId))
	#endif
	{}

	FArcNamedPrimaryAssetId(const FPrimaryAssetId& OtherAssetId)
	#if WITH_EDITORONLY_DATA
		: DisplayName()
		, AssetId(OtherAssetId)
	#else
		: AssetId(OtherAssetId)
	#endif
	{}

	FArcNamedPrimaryAssetId(FPrimaryAssetId&& OtherAssetId)
	#if WITH_EDITORONLY_DATA
		: DisplayName()
		, AssetId(MoveTemp(OtherAssetId))
	#else
		: AssetId(MoveTemp(OtherAssetId))
	#endif
	{}
	
	FArcNamedPrimaryAssetId& operator=(const FArcNamedPrimaryAssetId& Other)
	{
#if WITH_EDITORONLY_DATA
		DisplayName = Other.DisplayName;
#endif
		AssetId = Other.AssetId;

		return *this;
	}

	FArcNamedPrimaryAssetId& operator=(FArcNamedPrimaryAssetId&& Other)
	{
#if WITH_EDITORONLY_DATA
		DisplayName = MoveTemp(Other.DisplayName);
#endif
		AssetId = MoveTemp(Other.AssetId);

		return *this;
	}
	
	FArcNamedPrimaryAssetId& operator=(const FPrimaryAssetId& OtherAssetId)
	{
		AssetId = OtherAssetId;
		return *this;
	}

	FArcNamedPrimaryAssetId& operator=(FPrimaryAssetId&& OtherAssetId)
	{
		AssetId = MoveTemp(OtherAssetId);
		return *this;
	}

	bool operator==(const FPrimaryAssetId& OtherAssetId) const
	{
		return AssetId == OtherAssetId;
	}

	bool operator==(const FArcNamedPrimaryAssetId& OtherAssetId) const
	{
		return AssetId == OtherAssetId;
	}

	const bool IsValid() const
	{
		return AssetId.IsValid();
	}

	FString ToString() const
	{
		return AssetId.ToString();
	}

	void FromString(const FString& InString)
	{
		AssetId = FPrimaryAssetId::FromString(InString);
	}
	bool ExportTextItem(FString& ValueStr, FArcNamedPrimaryAssetId const& DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope) const;
	bool ImportTextItem(const TCHAR*& Buffer, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText);
	friend inline uint32 GetTypeHash(const FArcNamedPrimaryAssetId& Key)
	{
		uint32 Hash = 0;

		Hash = HashCombine(Hash, GetTypeHash(Key.AssetId.PrimaryAssetType));
		Hash = HashCombine(Hash, GetTypeHash(Key.AssetId.PrimaryAssetName));
		return Hash;
	}
};

template <>
struct TStructOpsTypeTraits<FArcNamedPrimaryAssetId> : public TStructOpsTypeTraitsBase2<FArcNamedPrimaryAssetId>
{
	enum
	{
		WithExportTextItem = true
		, WithImportTextItem = true // We have a custom compare operator
	};
};
