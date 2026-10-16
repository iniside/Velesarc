// Copyright Lukasz Baran. All Rights Reserved.

#include "Compile/ArcRmlCompiledFile.h"

#include "ArcRmlUILog.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"

namespace ArcRmlCompiledFilePrivate
{
	constexpr uint32 MakeMagic(char A, char B, char C, char D)
	{
		return uint32(A) | (uint32(B) << 8) | (uint32(C) << 16) | (uint32(D) << 24);
	}

	constexpr uint32 DocumentMagic = MakeMagic('R', 'M', 'L', 'C');
	constexpr uint32 StyleSheetMagic = MakeMagic('R', 'C', 'S', 'C');
	constexpr int64 MinSourceRecordSize = sizeof(uint32) + sizeof(uint8);

	uint32 GetMagic(EArcRmlCompiledKind Kind)
	{
		return Kind == EArcRmlCompiledKind::Document ? DocumentMagic : StyleSheetMagic;
	}

	FString MakeTempPath(const FString& FilePath)
	{
		return FString::Printf(TEXT("%s.%s.tmp"), *FilePath, *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	}

	class FWriter
	{
	public:
		explicit FWriter(TArray<uint8>& InBytes)
			: Bytes(InBytes)
		{
		}

		void Raw(const void* Data, int32 Size)
		{
			Bytes.Append(static_cast<const uint8*>(Data), Size);
		}

		void U8(uint8 Value)
		{
			Bytes.Add(Value);
		}

		void U32(uint32 Value)
		{
			Raw(&Value, sizeof(Value));
		}

		void U64(uint64 Value)
		{
			Raw(&Value, sizeof(Value));
		}

		void Str(const FString& Value)
		{
			const FTCHARToUTF8 Utf8(*Value);
			U32(static_cast<uint32>(Utf8.Length()));
			Raw(Utf8.Get(), Utf8.Length());
		}

	private:
		TArray<uint8>& Bytes;
	};

	class FReader
	{
	public:
		explicit FReader(TConstArrayView<uint8> InBytes)
			: Bytes(InBytes)
		{
		}

		bool Raw(void* Destination, int64 Size)
		{
			if (bFailed || Size < 0 || Size > Bytes.Num() - Position)
			{
				bFailed = true;
				return false;
			}
			FMemory::Memcpy(Destination, Bytes.GetData() + Position, Size);
			Position += Size;
			return true;
		}

		uint8 U8()
		{
			uint8 Value = 0;
			Raw(&Value, sizeof(Value));
			return Value;
		}

		uint32 U32()
		{
			uint32 Value = 0;
			Raw(&Value, sizeof(Value));
			return Value;
		}

		uint64 U64()
		{
			uint64 Value = 0;
			Raw(&Value, sizeof(Value));
			return Value;
		}

		FString Str()
		{
			const uint32 Size = U32();
			if (bFailed || static_cast<int64>(Size) > Bytes.Num() - Position)
			{
				bFailed = true;
				return FString();
			}
			FString Value = FString::ConstructFromPtrSize(reinterpret_cast<const UTF8CHAR*>(Bytes.GetData() + Position), static_cast<int32>(Size));
			Position += Size;
			return Value;
		}

		int64 Remaining() const
		{
			return Bytes.Num() - Position;
		}

		int64 GetPosition() const
		{
			return Position;
		}

		bool Failed() const
		{
			return bFailed;
		}

	private:
		TConstArrayView<uint8> Bytes;
		int64 Position = 0;
		bool bFailed = false;
	};
}

const TCHAR* ArcRmlCompiledFile::GetExtension(EArcRmlCompiledKind Kind)
{
	return Kind == EArcRmlCompiledKind::Document ? TEXT(".rmlc") : TEXT(".rcssc");
}

bool ArcRmlCompiledFile::SaveAtomic(const FString& FilePath, EArcRmlCompiledKind Kind, const FArcRmlCompiledHeader& Header, TConstArrayView<uint8> Body)
{
	using namespace ArcRmlCompiledFilePrivate;

	TArray<uint8> Bytes;
	Bytes.Reserve(Body.Num() + 256);
	FWriter Out(Bytes);
	Out.U32(GetMagic(Kind));
	Out.U32(ArcRmlCompiledFormatVersion);
	Out.U64(Header.RegistryHash);
	Out.U32(static_cast<uint32>(Header.Sources.Num()));
	for (const FArcRmlCompiledSource& Source : Header.Sources)
	{
		Out.Str(Source.Path);
		Out.U8(Source.bPresent ? 1 : 0);
	}
	Out.Raw(Header.SourceHash.GetBytes(), sizeof(FBlake3Hash::ByteArray));
	Out.U32(static_cast<uint32>(Body.Num()));
	Out.Raw(Body.GetData(), Body.Num());

	IFileManager& FileManager = IFileManager::Get();
	// A reader never sees a half-written binary: the bytes land under a temporary name and are moved into place.
	const FString TempPath = MakeTempPath(FilePath);
	if (!FFileHelper::SaveArrayToFile(Bytes, *TempPath, &FileManager))
	{
		UE_LOGF(LogArcRmlUI, Error, "ArcRmlUI: cannot write '%ls'", *TempPath);
		return false;
	}
	if (!FileManager.Move(*FilePath, *TempPath, true, true))
	{
		UE_LOGF(LogArcRmlUI, Error, "ArcRmlUI: cannot move '%ls' into place", *TempPath);
		FileManager.Delete(*TempPath, false, true, true);
		return false;
	}
	return true;
}

EArcRmlCompiledLoad ArcRmlCompiledFile::Load(const FString& FilePath, EArcRmlCompiledKind ExpectedKind, uint64 ExpectedRegistryHash, FArcRmlCompiledFileData& OutData)
{
	using namespace ArcRmlCompiledFilePrivate;

	TArray<uint8> Bytes;
	if (FilePath.IsEmpty() || !FFileHelper::LoadFileToArray(Bytes, *FilePath, FILEREAD_Silent))
	{
		return !FilePath.IsEmpty() && IFileManager::Get().FileExists(*FilePath) ? EArcRmlCompiledLoad::Unusable : EArcRmlCompiledLoad::Missing;
	}

	FReader In(Bytes);
	if (In.U32() != GetMagic(ExpectedKind) || In.U32() != ArcRmlCompiledFormatVersion)
	{
		return EArcRmlCompiledLoad::Unusable;
	}

	FArcRmlCompiledHeader Header;
	Header.RegistryHash = In.U64();
	if (Header.RegistryHash != ExpectedRegistryHash)
	{
		return EArcRmlCompiledLoad::Unusable;
	}

	const uint32 NumSources = In.U32();
	if (In.Failed() || NumSources > In.Remaining() / MinSourceRecordSize)
	{
		return EArcRmlCompiledLoad::Unusable;
	}
	Header.Sources.Reserve(NumSources);
	for (uint32 Index = 0; Index < NumSources && !In.Failed(); ++Index)
	{
		FArcRmlCompiledSource& Source = Header.Sources.AddDefaulted_GetRef();
		Source.Path = In.Str();
		Source.bPresent = In.U8() != 0;
	}
	In.Raw(Header.SourceHash.GetBytes(), sizeof(FBlake3Hash::ByteArray));

	const uint32 BodySize = In.U32();
	if (In.Failed() || static_cast<int64>(BodySize) != In.Remaining())
	{
		return EArcRmlCompiledLoad::Unusable;
	}

	OutData.Header = MoveTemp(Header);
	OutData.BodyOffset = static_cast<int32>(In.GetPosition());
	OutData.Bytes = MoveTemp(Bytes);
	return EArcRmlCompiledLoad::Loaded;
}
