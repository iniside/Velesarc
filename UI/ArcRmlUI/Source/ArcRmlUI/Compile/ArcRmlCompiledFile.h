// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Hash/Blake3.h"

inline constexpr uint32 ArcRmlCompiledFormatVersion = 3;

enum class EArcRmlCompiledKind : uint8
{
	Document,
	StyleSheet,
};

enum class EArcRmlCompiledLoad : uint8
{
	Loaded,
	Missing,
	Unusable,
};

struct FArcRmlCompiledSource
{
	FString Path;
	bool bPresent = false;
};

struct FArcRmlCompiledHeader
{
	uint64 RegistryHash = 0;
	TArray<FArcRmlCompiledSource> Sources;
	FBlake3Hash SourceHash;
};

struct FArcRmlCompiledFileData
{
	FArcRmlCompiledHeader Header;
	TArray<uint8> Bytes;
	int32 BodyOffset = 0;

	TConstArrayView<uint8> GetBody() const
	{
		return TConstArrayView<uint8>(Bytes.GetData() + BodyOffset, Bytes.Num() - BodyOffset);
	}
};

namespace ArcRmlCompiledFile
{
	ARCRMLUI_API const TCHAR* GetExtension(EArcRmlCompiledKind Kind);

	ARCRMLUI_API bool SaveAtomic(const FString& FilePath, EArcRmlCompiledKind Kind, const FArcRmlCompiledHeader& Header, TConstArrayView<uint8> Body);

	ARCRMLUI_API EArcRmlCompiledLoad Load(const FString& FilePath, EArcRmlCompiledKind ExpectedKind, uint64 ExpectedRegistryHash, FArcRmlCompiledFileData& OutData);
}
