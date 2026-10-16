// Copyright Lukasz Baran. All Rights Reserved.

#include "Compile/ArcRmlDocumentCompiler.h"

#if WITH_EDITOR
#include "ArcRmlUILog.h"
#include "Components/ArcRmlComponentExpander.h"
#include "HAL/FileManager.h"
#include "Interfaces/ArcRmlFileInterface.h"
#include "Interfaces/ArcRmlPaths.h"
#include "Interfaces/ArcRmlStrings.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#include "RmlUi/Core/CompiledDocument.h"
#include "RmlUi/Core/FileInterface.h"
#include "RmlUi/Core/StreamMemory.h"
#include "RmlUi/Core/StyleSheetBinary.h"
#include "RmlUi/Core/StyleSheetContainer.h"
#include "RmlUi/Core/URL.h"

namespace ArcRmlDocumentCompilerPrivate
{
	class FSourceTrackingFileInterface final : public Rml::FileInterface
	{
	public:
		explicit FSourceTrackingFileInterface(Rml::FileInterface& InInner)
			: Inner(InInner)
		{
		}

		virtual Rml::FileHandle Open(const Rml::String& Path) override
		{
			Paths.AddUnique(ArcRmlStrings::ToFString(Path));
			return Inner.Open(Path);
		}

		virtual void Close(Rml::FileHandle File) override
		{
			Inner.Close(File);
		}

		virtual size_t Read(void* Buffer, size_t Size, Rml::FileHandle File) override
		{
			return Inner.Read(Buffer, Size, File);
		}

		virtual bool Seek(Rml::FileHandle File, long Offset, int Origin) override
		{
			return Inner.Seek(File, Offset, Origin);
		}

		virtual size_t Tell(Rml::FileHandle File) override
		{
			return Inner.Tell(File);
		}

		virtual size_t Length(Rml::FileHandle File) override
		{
			return Inner.Length(File);
		}

		TArray<FString> Paths;

	private:
		Rml::FileInterface& Inner;
	};

	TConstArrayView<uint8> ToView(const Rml::Vector<Rml::byte>& Bytes)
	{
		return TConstArrayView<uint8>(Bytes.data(), static_cast<int32>(Bytes.size()));
	}

	bool IsCompiledExtension(const FString& Extension)
	{
		return Extension.Equals(TEXT("rmlc"), ESearchCase::IgnoreCase) || Extension.Equals(TEXT("rcssc"), ESearchCase::IgnoreCase);
	}

	bool IsUnderAny(const FString& PackagePath, TConstArrayView<FString> Prefixes)
	{
		for (const FString& Prefix : Prefixes)
		{
			if (PackagePath.StartsWith(Prefix))
			{
				return true;
			}
		}
		return false;
	}
}

FArcRmlDocumentCompiler::FArcRmlDocumentCompiler(FArcRmlFileInterface& InFileInterface, const FArcRmlContentRoots& InRoots)
	: FileInterface(InFileInterface)
	, Roots(InRoots)
{
}

bool FArcRmlDocumentCompiler::ResolveSource(const FString& Path, FArcRmlCompiledSource& OutSource, FString& OutFullPath) const
{
	OutSource = FArcRmlCompiledSource();
	OutFullPath.Reset();

	const FString PackagePath = ArcRmlPaths::NormalizePackagePath(Path);
	FString FullPath;
	if (!ArcRmlPaths::ToFilename(Roots, PackagePath, FullPath))
	{
		return false;
	}

	OutSource.Path = PackagePath;
	OutSource.bPresent = IFileManager::Get().FileExists(*FullPath);
	OutFullPath = MoveTemp(FullPath);
	return true;
}

bool FArcRmlDocumentCompiler::HashSources(TConstArrayView<FArcRmlCompiledSource> Sources, FBlake3Hash& OutHash) const
{
	FBlake3 Hasher;
	const uint32 RootCount = static_cast<uint32>(Roots.Roots.Num());
	Hasher.Update(&RootCount, sizeof(RootCount));
	for (const FArcRmlContentRoot& Root : Roots.Roots)
	{
		const FTCHARToUTF8 MountName(*Root.MountName.ToLower());
		const uint32 MountNameLength = static_cast<uint32>(MountName.Length());
		Hasher.Update(&MountNameLength, sizeof(MountNameLength));
		Hasher.Update(MountName.Get(), MountNameLength);
	}

	TArray<uint8> Bytes;
	for (const FArcRmlCompiledSource& Source : Sources)
	{
		const FTCHARToUTF8 Utf8(*Source.Path);
		const uint32 PathLength = static_cast<uint32>(Utf8.Length());
		const uint8 bPresent = Source.bPresent ? 1 : 0;
		Hasher.Update(&PathLength, sizeof(PathLength));
		Hasher.Update(Utf8.Get(), PathLength);
		Hasher.Update(&bPresent, sizeof(bPresent));
		if (!Source.bPresent)
		{
			continue;
		}

		FString FullPath;
		if (!ArcRmlPaths::ToFilename(Roots, Source.Path, FullPath) || !FFileHelper::LoadFileToArray(Bytes, *FullPath, FILEREAD_Silent))
		{
			return false;
		}
		const uint64 Size = static_cast<uint64>(Bytes.Num());
		Hasher.Update(&Size, sizeof(Size));
		Hasher.Update(Bytes.GetData(), Size);
	}
	OutHash = Hasher.Finalize();
	return true;
}

bool FArcRmlDocumentCompiler::Write(const FString& SourceFullPath, EArcRmlCompiledKind Kind, TArray<FArcRmlCompiledSource>&& Sources, TConstArrayView<uint8> Body)
{
	FArcRmlCompiledHeader Header;
	Header.RegistryHash = Rml::GetCompiledRegistryHash();
	Header.Sources = MoveTemp(Sources);
	if (!HashSources(Header.Sources, Header.SourceHash))
	{
		return false;
	}
	return ArcRmlCompiledFile::SaveAtomic(SourceFullPath + TEXT("c"), Kind, Header, Body);
}

void FArcRmlDocumentCompiler::RemoveBinary(const FString& BinaryFullPath, const TCHAR* Reason, FArcRmlCompileReport& Report) const
{
	const FString AbsolutePath = FPaths::ConvertRelativePathToFull(BinaryFullPath);
	if (!IFileManager::Get().Delete(*BinaryFullPath, false, false, true))
	{
		UE_LOGF(LogArcRmlUI, Error, "ArcRmlUI: cannot remove compiled file '%ls'", *AbsolutePath);
		return;
	}
	UE_LOGF(LogArcRmlUI, Log, "ArcRmlUI: removed compiled file '%ls': %ls", *AbsolutePath, Reason);
	Report.Removed.Add(AbsolutePath);
}

void FArcRmlDocumentCompiler::RemoveOrphanBinaries(TConstArrayView<FArcRmlFoundBinary> Binaries, FArcRmlCompileReport& Report) const
{
	for (const FArcRmlFoundBinary& Binary : Binaries)
	{
		if (!IFileManager::Get().FileExists(*Binary.FullPath.LeftChop(1)))
		{
			RemoveBinary(Binary.FullPath, TEXT("its source is absent"), Report);
		}
	}
}

void FArcRmlDocumentCompiler::RecordCompiled(const FArcRmlCompiledSource& Source, FArcRmlCompileReport& Report) const
{
	UE_LOGF(LogArcRmlUI, Log, "ArcRmlUI: compiled '%ls'", *Source.Path);
	Report.Compiled.Add(Source.Path);
}

void FArcRmlDocumentCompiler::Fail(const FString& Path, const FString& Reason, FArcRmlCompileReport& Report) const
{
	UE_LOGF(LogArcRmlUI, Error, "ArcRmlUI: cannot compile '%ls': %ls", *Path, *Reason);
	Report.Failed.Add(Path);
}

bool FArcRmlDocumentCompiler::CompileStyleSheet(const FString& Path, FArcRmlCompileReport& Report)
{
	FArcRmlCompiledSource Source;
	FString FullPath;
	if (!ResolveSource(Path, Source, FullPath) || !Source.bPresent)
	{
		Fail(Path, TEXT("the style sheet is not under any content root"), Report);
		return false;
	}

	TArray<uint8> Text;
	if (!FFileHelper::LoadFileToArray(Text, *FullPath))
	{
		Fail(Source.Path, TEXT("the style sheet cannot be read"), Report);
		return false;
	}

	Rml::StreamMemory Stream(Text.GetData(), static_cast<size_t>(Text.Num()));
	Stream.SetSourceURL(Rml::URL(ArcRmlStrings::ToRmlString(Source.Path)));
	Rml::StyleSheetContainer Container;
	if (!Container.LoadStyleSheetContainer(&Stream, 1, Rml::StyleSheetParseMode::Compile))
	{
		Fail(Source.Path, TEXT("the style sheet does not parse"), Report);
		return false;
	}

	Rml::Vector<Rml::byte> Body;
	Rml::String Error;
	if (!Rml::WriteStyleSheetBinary(Container, Body, Error))
	{
		Fail(Source.Path, ArcRmlStrings::ToFString(Error), Report);
		return false;
	}

	TArray<FArcRmlCompiledSource> Sources;
	Sources.Add(Source);
	if (!Write(FullPath, EArcRmlCompiledKind::StyleSheet, MoveTemp(Sources), ArcRmlDocumentCompilerPrivate::ToView(Body)))
	{
		Fail(Source.Path, TEXT("the compiled style sheet cannot be written"), Report);
		return false;
	}
	RecordCompiled(Source, Report);
	return true;
}

bool FArcRmlDocumentCompiler::CompileDocument(const FString& Path, FArcRmlCompileReport& Report)
{
	using namespace ArcRmlDocumentCompilerPrivate;

	FArcRmlCompiledSource Source;
	FString FullPath;
	if (!ResolveSource(Path, Source, FullPath) || !Source.bPresent)
	{
		Fail(Path, TEXT("the document is not under any content root"), Report);
		return false;
	}

	FSourceTrackingFileInterface TrackingInterface(FileInterface);
	FString Expanded;
	{
		FArcRmlComponentExpander Expander(TrackingInterface, Roots);
		if (!Expander.ExpandDocument(Source.Path, Expanded))
		{
			Fail(Source.Path, TEXT("component expansion failed"), Report);
			return false;
		}
	}

	Rml::Vector<Rml::byte> Body;
	Rml::String Error;
	if (!Rml::CompileDocument(ArcRmlStrings::ToRmlString(Expanded), ArcRmlStrings::ToRmlString(Source.Path), Body, Error))
	{
		Fail(Source.Path, ArcRmlStrings::ToFString(Error), Report);
		return false;
	}

	TArray<FArcRmlCompiledSource> Sources;
	for (const FString& TrackedPath : TrackingInterface.Paths)
	{
		FArcRmlCompiledSource Tracked;
		FString TrackedFullPath;
		if (!ResolveSource(TrackedPath, Tracked, TrackedFullPath))
		{
			Fail(Source.Path, FString::Printf(TEXT("'%s' is not a package path under a content root"), *TrackedPath), Report);
			return false;
		}
		Sources.Add(MoveTemp(Tracked));
	}

	if (!Write(FullPath, EArcRmlCompiledKind::Document, MoveTemp(Sources), ToView(Body)))
	{
		Fail(Source.Path, TEXT("the compiled document cannot be written"), Report);
		return false;
	}
	RecordCompiled(Source, Report);
	return true;
}

void FArcRmlDocumentCompiler::FindFiles(TArray<FString>& OutDocuments, TArray<FString>& OutStyleSheets, TArray<FArcRmlFoundBinary>& OutBinaries) const
{
	for (const FArcRmlContentRoot& Root : Roots.Roots)
	{
		const FString Prefix = Root.ContentDir + TEXT("/");
		IFileManager::Get().IterateDirectoryRecursively(*Root.ContentDir, [&Root, &Prefix, &OutDocuments, &OutStyleSheets, &OutBinaries](const TCHAR* FilenameOrDirectory, bool bIsDirectory) -> bool
		{
			if (bIsDirectory)
			{
				return true;
			}
			const FString Extension = FPaths::GetExtension(FilenameOrDirectory);
			const bool bStyleSheet = Extension.Equals(TEXT("rcss"), ESearchCase::IgnoreCase);
			const bool bDocument = Extension.Equals(TEXT("rml"), ESearchCase::IgnoreCase);
			const bool bBinary = ArcRmlDocumentCompilerPrivate::IsCompiledExtension(Extension);
			if (!bStyleSheet && !bDocument && !bBinary)
			{
				return true;
			}

			FString File = FPaths::ConvertRelativePathToFull(FilenameOrDirectory);
			FPaths::NormalizeFilename(File);
			if (!File.StartsWith(Prefix))
			{
				return true;
			}
			FString PackagePath = ArcRmlPaths::MakePackagePath(Root, File.RightChop(Prefix.Len()));
			if (bStyleSheet)
			{
				OutStyleSheets.Add(MoveTemp(PackagePath));
			}
			else if (bDocument)
			{
				if (!ArcRmlPaths::IsComponentPath(PackagePath))
				{
					OutDocuments.Add(MoveTemp(PackagePath));
				}
			}
			else
			{
				OutBinaries.Add({ MoveTemp(File), MoveTemp(PackagePath) });
			}
			return true;
		});
	}
	OutDocuments.Sort();
	OutStyleSheets.Sort();
}

bool FArcRmlDocumentCompiler::IsFresh(const FArcRmlCompiledHeader& Header) const
{
	TArray<FArcRmlCompiledSource> Current;
	Current.Reserve(Header.Sources.Num());
	for (const FArcRmlCompiledSource& Recorded : Header.Sources)
	{
		FArcRmlCompiledSource& Resolved = Current.AddDefaulted_GetRef();
		FString ResolvedFullPath;
		if (!ResolveSource(Recorded.Path, Resolved, ResolvedFullPath))
		{
			return false;
		}
	}

	FBlake3Hash Hash;
	return HashSources(Current, Hash) && Hash == Header.SourceHash;
}

bool FArcRmlDocumentCompiler::DependsOnAny(const FString& Document, const TSet<FString>& Paths) const
{
	FArcRmlCompiledSource Source;
	FString FullPath;
	if (!ResolveSource(Document, Source, FullPath) || !Source.bPresent)
	{
		return false;
	}

	FArcRmlCompiledFileData Data;
	if (ArcRmlCompiledFile::Load(FullPath + TEXT("c"), EArcRmlCompiledKind::Document, Rml::GetCompiledRegistryHash(), Data) != EArcRmlCompiledLoad::Loaded)
	{
		return false;
	}

	for (const FArcRmlCompiledSource& Recorded : Data.Header.Sources)
	{
		if (Paths.Contains(Recorded.Path))
		{
			return true;
		}
	}
	return false;
}

void FArcRmlDocumentCompiler::CompileAll(TConstArrayView<FString> ExcludedPrefixes, FArcRmlCompileReport& Report)
{
	Report.RegistryHash = Rml::GetCompiledRegistryHash();
	TArray<FString> Documents;
	TArray<FString> StyleSheets;
	TArray<FArcRmlFoundBinary> Binaries;
	FindFiles(Documents, StyleSheets, Binaries);
	RemoveOrphanBinaries(Binaries, Report);

	for (const FArcRmlFoundBinary& Binary : Binaries)
	{
		if (ArcRmlDocumentCompilerPrivate::IsUnderAny(Binary.PackagePath, ExcludedPrefixes) && IFileManager::Get().FileExists(*Binary.FullPath))
		{
			RemoveBinary(Binary.FullPath, TEXT("its directory is excluded from this compile"), Report);
		}
	}

	Documents.RemoveAll([ExcludedPrefixes](const FString& PackagePath) -> bool { return ArcRmlDocumentCompilerPrivate::IsUnderAny(PackagePath, ExcludedPrefixes); });
	StyleSheets.RemoveAll([ExcludedPrefixes](const FString& PackagePath) -> bool { return ArcRmlDocumentCompilerPrivate::IsUnderAny(PackagePath, ExcludedPrefixes); });

	Report.Documents += Documents.Num();
	Report.StyleSheets += StyleSheets.Num();

	TArray<FString> FailedPaths;
	for (const FString& StyleSheet : StyleSheets)
	{
		if (!CompileStyleSheet(StyleSheet, Report))
		{
			FailedPaths.Add(StyleSheet);
		}
	}
	for (const FString& Document : Documents)
	{
		if (!CompileDocument(Document, Report))
		{
			FailedPaths.Add(Document);
		}
	}

	for (const FString& FailedPath : FailedPaths)
	{
		FArcRmlCompiledSource Source;
		FString FullPath;
		if (!ResolveSource(FailedPath, Source, FullPath) || !Source.bPresent)
		{
			continue;
		}
		const FString BinaryFullPath = FullPath + TEXT("c");
		if (IFileManager::Get().FileExists(*BinaryFullPath))
		{
			RemoveBinary(BinaryFullPath, TEXT("its source failed to compile"), Report);
		}
	}
}

void FArcRmlDocumentCompiler::CompileChanged(TConstArrayView<FString> ChangedPackagePaths, TConstArrayView<FString> OpenDocuments, FArcRmlCompileReport& Report)
{
	TSet<FString> Changed;
	for (const FString& PackagePath : ChangedPackagePaths)
	{
		Changed.Add(ArcRmlPaths::NormalizePackagePath(PackagePath));
	}
	if (Changed.IsEmpty())
	{
		return;
	}

	TArray<FString> ChangedPaths = Changed.Array();
	ChangedPaths.Sort();
	TArray<FString> Documents;
	for (const FString& Path : ChangedPaths)
	{
		FArcRmlCompiledSource Source;
		FString FullPath;
		if (!ResolveSource(Path, Source, FullPath))
		{
			continue;
		}
		if (!Source.bPresent)
		{
			const FString BinaryFullPath = FullPath + TEXT("c");
			if (IFileManager::Get().FileExists(*BinaryFullPath))
			{
				RemoveBinary(BinaryFullPath, TEXT("its source is absent"), Report);
			}
			continue;
		}

		const FString Extension = FPaths::GetExtension(Source.Path);
		if (Extension.Equals(TEXT("rcss"), ESearchCase::IgnoreCase))
		{
			++Report.StyleSheets;
			CompileStyleSheet(Source.Path, Report);
		}
		else if (Extension.Equals(TEXT("rml"), ESearchCase::IgnoreCase) && !ArcRmlPaths::IsComponentPath(Source.Path))
		{
			Documents.AddUnique(Source.Path);
		}
	}

	for (const FString& OpenDocument : OpenDocuments)
	{
		const FString Document = ArcRmlPaths::NormalizePackagePath(OpenDocument);
		if (!Documents.Contains(Document) && DependsOnAny(Document, Changed))
		{
			Documents.Add(Document);
		}
	}

	for (const FString& Document : Documents)
	{
		++Report.Documents;
		CompileDocument(Document, Report);
	}
}
#endif
