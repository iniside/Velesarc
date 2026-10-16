// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR
#include "Compile/ArcRmlCompiledFile.h"

class FArcRmlFileInterface;
struct FArcRmlContentRoots;

struct FArcRmlCompileReport
{
	int32 Documents = 0;
	int32 StyleSheets = 0;
	uint64 RegistryHash = 0;
	TArray<FString> Compiled;
	TArray<FString> Failed;
	TArray<FString> Removed;
};

struct FArcRmlFoundBinary
{
	FString FullPath;
	FString PackagePath;
};

class ARCRMLUI_API FArcRmlDocumentCompiler
{
public:
	FArcRmlDocumentCompiler(FArcRmlFileInterface& InFileInterface, const FArcRmlContentRoots& InRoots);

	bool CompileStyleSheet(const FString& Path, FArcRmlCompileReport& Report);
	bool CompileDocument(const FString& Path, FArcRmlCompileReport& Report);
	void CompileAll(TConstArrayView<FString> ExcludedPrefixes, FArcRmlCompileReport& Report);
	void CompileChanged(TConstArrayView<FString> ChangedPackagePaths, TConstArrayView<FString> OpenDocuments, FArcRmlCompileReport& Report);

	bool IsFresh(const FArcRmlCompiledHeader& Header) const;
	bool ResolveSource(const FString& Path, FArcRmlCompiledSource& OutSource, FString& OutFullPath) const;

private:
	void FindFiles(TArray<FString>& OutDocuments, TArray<FString>& OutStyleSheets, TArray<FArcRmlFoundBinary>& OutBinaries) const;
	void RemoveOrphanBinaries(TConstArrayView<FArcRmlFoundBinary> Binaries, FArcRmlCompileReport& Report) const;
	void RemoveBinary(const FString& BinaryFullPath, const TCHAR* Reason, FArcRmlCompileReport& Report) const;
	void RecordCompiled(const FArcRmlCompiledSource& Source, FArcRmlCompileReport& Report) const;
	bool DependsOnAny(const FString& Document, const TSet<FString>& Paths) const;
	bool HashSources(TConstArrayView<FArcRmlCompiledSource> Sources, FBlake3Hash& OutHash) const;
	bool Write(const FString& SourceFullPath, EArcRmlCompiledKind Kind, TArray<FArcRmlCompiledSource>&& Sources, TConstArrayView<uint8> Body);
	void Fail(const FString& Path, const FString& Reason, FArcRmlCompileReport& Report) const;

	FArcRmlFileInterface& FileInterface;
	const FArcRmlContentRoots& Roots;
};
#endif
