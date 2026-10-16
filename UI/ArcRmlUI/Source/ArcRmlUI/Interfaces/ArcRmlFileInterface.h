// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "RmlUi/Core/FileInterface.h"

class IPlatformFile;
struct FArcRmlContentRoots;

#if !UE_BUILD_SHIPPING
struct FArcRmlFileIoStats
{
	int64 Opens = 0;
	int64 Reads = 0;
	int64 Bytes = 0;
	double Ms = 0.0;
};
#endif

class FArcRmlFileInterface final : public Rml::FileInterface
{
public:
	FArcRmlFileInterface(IPlatformFile& InPlatformFile, const FArcRmlContentRoots& InRoots);

	FString ResolvePath(const FString& Path) const;
	bool ReadFileBytes(const Rml::String& Path, Rml::String& OutData) const;

	virtual Rml::FileHandle Open(const Rml::String& Path) override;
	virtual void Close(Rml::FileHandle File) override;
	virtual size_t Read(void* Buffer, size_t Size, Rml::FileHandle File) override;
	virtual bool Seek(Rml::FileHandle File, long Offset, int Origin) override;
	virtual size_t Tell(Rml::FileHandle File) override;
	virtual size_t Length(Rml::FileHandle File) override;

#if !UE_BUILD_SHIPPING
	static FArcRmlFileIoStats TakeIoStats();
#endif

private:
	IPlatformFile& PlatformFile;
	const FArcRmlContentRoots& Roots;
};
