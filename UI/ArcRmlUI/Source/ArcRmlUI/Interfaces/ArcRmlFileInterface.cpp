// Copyright Lukasz Baran. All Rights Reserved.

#include "Interfaces/ArcRmlFileInterface.h"

#include "GenericPlatform/GenericPlatformFile.h"
#include "Interfaces/ArcRmlPaths.h"
#include "Interfaces/ArcRmlStrings.h"

#include "ArcRmlUIPerf.h"

#include <atomic>
#include <cstdio>

namespace ArcRmlFileInterfacePrivate
{
	IFileHandle* ToHandle(Rml::FileHandle File)
	{
		return reinterpret_cast<IFileHandle*>(File);
	}

#if !UE_BUILD_SHIPPING
	std::atomic<int64> Opens{0};
	std::atomic<int64> Reads{0};
	std::atomic<int64> Bytes{0};
	std::atomic<uint64> Cycles{0};

	class FIoTimer
	{
	public:
		FIoTimer() : StartCycles(ArcRmlUIPerf::IsEnabled() ? FPlatformTime::Cycles64() : 0)
		{
		}

		~FIoTimer()
		{
			if (StartCycles != 0)
			{
				Cycles += FPlatformTime::Cycles64() - StartCycles;
			}
		}

	private:
		uint64 StartCycles;
	};
#endif
}

#if !UE_BUILD_SHIPPING
FArcRmlFileIoStats FArcRmlFileInterface::TakeIoStats()
{
	FArcRmlFileIoStats Stats;
	Stats.Opens = ArcRmlFileInterfacePrivate::Opens.exchange(0);
	Stats.Reads = ArcRmlFileInterfacePrivate::Reads.exchange(0);
	Stats.Bytes = ArcRmlFileInterfacePrivate::Bytes.exchange(0);
	const uint64 Cycles = ArcRmlFileInterfacePrivate::Cycles.exchange(0);
	Stats.Ms = ArcRmlUIPerf::CyclesToMs(0, Cycles);
	return Stats;
}
#endif

FArcRmlFileInterface::FArcRmlFileInterface(IPlatformFile& InPlatformFile, const FArcRmlContentRoots& InRoots)
	: PlatformFile(InPlatformFile)
	, Roots(InRoots)
{
}

FString FArcRmlFileInterface::ResolvePath(const FString& Path) const
{
	FString Filename;
	ArcRmlPaths::ToFilename(Roots, ArcRmlPaths::NormalizePackagePath(Path), Filename);
	return Filename;
}

bool FArcRmlFileInterface::ReadFileBytes(const Rml::String& Path, Rml::String& OutData) const
{
	const FString Resolved = ResolvePath(ArcRmlStrings::ToFString(Path));
	if (Resolved.IsEmpty())
	{
		return false;
	}

	const TUniquePtr<IFileHandle> Handle(PlatformFile.OpenRead(*Resolved));
	if (!Handle)
	{
		return false;
	}

	const int64 Size = Handle->Size();
	if (Size < 0)
	{
		return false;
	}

	OutData.resize(static_cast<size_t>(Size));
	return Size == 0 || Handle->Read(reinterpret_cast<uint8*>(OutData.data()), Size);
}

Rml::FileHandle FArcRmlFileInterface::Open(const Rml::String& Path)
{
	ARCRMLUI_PERF(const ArcRmlFileInterfacePrivate::FIoTimer IoTimer);
	ARCRMLUI_PERF(++ArcRmlFileInterfacePrivate::Opens);
	const FString Resolved = ResolvePath(ArcRmlStrings::ToFString(Path));
	if (Resolved.IsEmpty())
	{
		return 0;
	}

	IFileHandle* Handle = PlatformFile.OpenRead(*Resolved);
	return reinterpret_cast<Rml::FileHandle>(Handle);
}

void FArcRmlFileInterface::Close(Rml::FileHandle File)
{
	ARCRMLUI_PERF(const ArcRmlFileInterfacePrivate::FIoTimer IoTimer);
	delete ArcRmlFileInterfacePrivate::ToHandle(File);
}

size_t FArcRmlFileInterface::Read(void* Buffer, size_t Size, Rml::FileHandle File)
{
	ARCRMLUI_PERF(const ArcRmlFileInterfacePrivate::FIoTimer IoTimer);
	ARCRMLUI_PERF(++ArcRmlFileInterfacePrivate::Reads);
	IFileHandle* Handle = ArcRmlFileInterfacePrivate::ToHandle(File);
	if (!Handle || Size == 0)
	{
		return 0;
	}

	const int64 Remaining = Handle->Size() - Handle->Tell();
	const int64 ToRead = FMath::Min(static_cast<int64>(Size), Remaining);
	if (ToRead <= 0 || !Handle->Read(static_cast<uint8*>(Buffer), ToRead))
	{
		return 0;
	}
	ARCRMLUI_PERF(ArcRmlFileInterfacePrivate::Bytes += ToRead);
	return static_cast<size_t>(ToRead);
}

bool FArcRmlFileInterface::Seek(Rml::FileHandle File, long Offset, int Origin)
{
	IFileHandle* Handle = ArcRmlFileInterfacePrivate::ToHandle(File);
	if (!Handle)
	{
		return false;
	}

	switch (Origin)
	{
	case SEEK_SET:
		return Handle->Seek(Offset);
	case SEEK_CUR:
		return Handle->Seek(Handle->Tell() + Offset);
	case SEEK_END:
		return Handle->SeekFromEnd(Offset);
	default:
		return false;
	}
}

size_t FArcRmlFileInterface::Tell(Rml::FileHandle File)
{
	IFileHandle* Handle = ArcRmlFileInterfacePrivate::ToHandle(File);
	return Handle ? static_cast<size_t>(Handle->Tell()) : 0;
}

size_t FArcRmlFileInterface::Length(Rml::FileHandle File)
{
	IFileHandle* Handle = ArcRmlFileInterfacePrivate::ToHandle(File);
	return Handle ? static_cast<size_t>(Handle->Size()) : 0;
}
