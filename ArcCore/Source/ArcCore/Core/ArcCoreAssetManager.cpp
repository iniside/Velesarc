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

#include "ArcCoreAssetManager.h"
#include "ArcCoreGameplayTags.h"
#include "Core/ArcMassGameFrameworkTags.h"
#include "ArcEventTags.h"
#include "AssetRegistry/AssetBundleData.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryState.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "CoreGlobals.h"
#include "GameplayTagsManager.h"
#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformTime.h"
#include "JsonAssets/ArcJsonAssetFile.h"
#include "JsonAssets/ArcJsonAssetPaths.h"
#include "JsonAssets/ArcJsonAssetType.h"
#include "JsonAssets/ArcJsonAssetsLog.h"
#include "Misc/App.h"
#include "Misc/Char.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Misc/ScopedSlowTask.h"
#include "Stats/StatsMisc.h"
#include "StructUtils/InstancedStruct.h"
#include "UObject/TextProperty.h"
#include "UObject/UnrealType.h"

const FName FArcBundles::Equipped("Equipped");

#define STARTUP_JOB_WEIGHTED(JobFunc, JobWeight)          \
	StartupJobs.Add(FArcAssetManagerStartupJob(           \
		#JobFunc,                                         \
		[this](                                           \
			const FArcAssetManagerStartupJob& StartupJob, \
			TSharedPtr<FStreamableHandle>& LoadHandle) {  \
			JobFunc;                                      \
		},                                                \
		JobWeight))
#define STARTUP_JOB(JobFunc) STARTUP_JOB_WEIGHTED(JobFunc, 1.f)

static FAutoConsoleCommand CVarDumpLoadedAssets(TEXT("Arc.DumpLoadedAssets")
	, TEXT("Shows all assets that were loaded via the asset manager and are currently in memory.")
	, FConsoleCommandDelegate::CreateStatic(UArcCoreAssetManager::DumpLoadedAssets));

DEFINE_LOG_CATEGORY(LogArcAssetManager)

namespace ArcCoreAssetManagerJsonPrivate
{
	const TCHAR* const IndexRoot = TEXT("/ArcJsonIndex");
	const FName MarkerBundleName(TEXT("ArcJsonAsset"));
	const FName SourcePathTag(TEXT("SourcePath"));
	const FName DisplayNameTag(TEXT("DisplayName"));

	struct FTypeStats
	{
		int32 Loaded = 0;
		int32 Rejected = 0;
		double Seconds = 0.0;
	};

	struct FParsedFile
	{
		FPrimaryAssetId Id;
		const FArcJsonAssetTypeInfo* Type = nullptr;
		FString Filename;
		FInstancedStruct Record;
	};

	const FArcJsonAssetTypeInfo* FindRegisteredDefaultType(FPrimaryAssetType PrimaryAssetType)
	{
		for (const FArcJsonAssetTypeInfo& Type : ArcJsonAssets::GetTypes())
		{
			if (Type.PrimaryAssetType == PrimaryAssetType)
			{
				return &Type;
			}
		}
		return nullptr;
	}

	template <typename KeyType>
	void RemoveWaitingFilename(TMap<KeyType, TArray<FString>>& Waiting, const FString& Filename)
	{
		TArray<KeyType> Emptied;
		for (TPair<KeyType, TArray<FString>>& Pair : Waiting)
		{
			Pair.Value.Remove(Filename);
			if (Pair.Value.IsEmpty())
			{
				Emptied.Add(Pair.Key);
			}
		}
		for (const KeyType& Key : Emptied)
		{
			Waiting.Remove(Key);
		}
	}

	template <typename KeyType>
	void RemoveWaitingUnderRoot(TMap<KeyType, TArray<FString>>& Waiting, const FArcJsonAssetContentRoot& Root)
	{
		for (TPair<KeyType, TArray<FString>>& Pair : Waiting)
		{
			Pair.Value.RemoveAll([&Root](const FString& Filename) -> bool
			{
				return ArcJsonAssetPaths::IsUnderRoot(Root, Filename);
			});
		}
	}

	// AddDynamicAsset registers an id only with a non-empty bundle list (AssetManager.cpp:1876); the marker has no asset paths, so nothing loads or cooks.
	const FAssetBundleData& GetMarkerBundle()
	{
		static const FAssetBundleData Marker = []() -> FAssetBundleData
		{
			FAssetBundleData Result;
			Result.Bundles.Add(FAssetBundleEntry(MarkerBundleName));
			return Result;
		}();
		return Marker;
	}

	FString MakeIndexPackageName(const FPrimaryAssetId& Id)
	{
		return FString(IndexRoot) + TEXT("/") + Id.PrimaryAssetType.ToString() + TEXT("/") + Id.PrimaryAssetName.ToString();
	}

	FSoftObjectPath MakeIndexObjectPath(const FPrimaryAssetId& Id)
	{
		return FSoftObjectPath(FTopLevelAssetPath(FName(MakeIndexPackageName(Id)), Id.PrimaryAssetName));
	}

	FString ExportSearchableValue(const FProperty& Property, const void* Container)
	{
		FString Result;
		if (const FTextProperty* TextProperty = CastField<FTextProperty>(&Property))
		{
			FTextStringHelper::WriteToBuffer(Result, TextProperty->GetPropertyValue_InContainer(Container), false, true);
			return Result;
		}
		const void* ValuePtr = Property.ContainerPtrToValuePtr<void>(Container);
		Property.ExportTextItem_Direct(Result, ValuePtr, ValuePtr, nullptr, PPF_None);
		return Result;
	}

	void CollectFiles(const FArcJsonAssetContentRoot& Root, TArray<FString>& OutFilenames)
	{
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		PlatformFile.IterateDirectoryRecursively(*Root.ContentDir, [&OutFilenames](const TCHAR* Path, bool bIsDirectory) -> bool
		{
			if (!bIsDirectory && ArcJsonAssets::FindTypeByFilename(Path))
			{
				OutFilenames.Add(ArcJsonAssetPaths::NormalizeFilename(Path));
			}
			return true;
		});
	}

	bool ParseFile(const FString& Filename, FParsedFile& OutParsed)
	{
		const FArcJsonAssetTypeInfo* Type = ArcJsonAssets::FindTypeByFilename(Filename);
		if (!Type)
		{
			UE_LOGF(LogArcJsonAssets, Error, "ArcJsonAssets: %ls has no registered JSON asset type", *Filename);
			return false;
		}
		FString PackagePath;
		if (!ArcJsonAssetPaths::ToPackagePath(ArcJsonAssetPaths::GetContentRoots(), Filename, PackagePath))
		{
			UE_LOGF(LogArcJsonAssets, Error, "ArcJsonAssets: %ls is not under a mounted content root", *Filename);
			return false;
		}

		FInstancedStruct Record;
		if (!ArcJsonAssetFile::ReadRecordFile(*Type, Filename, Record))
		{
			return false;
		}

		FString IdError;
		const FPrimaryAssetId Id = ArcJsonAssets::MakeRecordId(*Type, Record, &IdError);
		if (!Id.IsValid())
		{
			UE_LOGF(LogArcJsonAssets, Error, "ArcJsonAssets: %ls: %ls", *Filename, *IdError);
			return false;
		}

		OutParsed.Id = Id;
		OutParsed.Type = Type;
		OutParsed.Filename = Filename;
		OutParsed.Record = MoveTemp(Record);
		return true;
	}

	void IndexParsed(UAssetManager& AssetManager, TConstArrayView<FParsedFile> ParsedFiles)
	{
		if (ParsedFiles.IsEmpty())
		{
			return;
		}

		const FArcJsonAssetContentRoots& Roots = ArcJsonAssetPaths::GetContentRoots();
		FAssetRegistryState State;
		for (const FParsedFile& Parsed : ParsedFiles)
		{
			const FPrimaryAssetId& Id = Parsed.Id;
			const UScriptStruct* Struct = Parsed.Record.GetScriptStruct();
			FString SourcePath;
			ArcJsonAssetPaths::ToPackagePath(Roots, Parsed.Filename, SourcePath);

			const FString TypeName = Id.PrimaryAssetType.ToString();
			const FString AssetName = Id.PrimaryAssetName.ToString();
			FAssetDataTagMap Tags;
			Tags.Add(FPrimaryAssetId::PrimaryAssetTypeTag, TypeName);
			Tags.Add(FPrimaryAssetId::PrimaryAssetNameTag, AssetName);
			Tags.Add(SourcePathTag, SourcePath);
			Tags.Add(DisplayNameTag, FPaths::GetBaseFilename(Parsed.Filename));
			for (TFieldIterator<FProperty> It(Struct); It; ++It)
			{
				if (It->HasAnyPropertyFlags(CPF_AssetRegistrySearchable))
				{
					Tags.Add(It->GetFName(), ExportSearchableValue(**It, Parsed.Record.GetMemory()));
				}
			}
			if (Parsed.Type->AddAssetRegistryTags)
			{
				Parsed.Type->AddAssetRegistryTags(Parsed.Record, Tags);
			}

			State.AddAssetData(new FAssetData(
				FName(MakeIndexPackageName(Id)),
				FName(FString(IndexRoot) + TEXT("/") + TypeName),
				Id.PrimaryAssetName,
				FTopLevelAssetPath(Struct),
				MoveTemp(Tags)));
		}

		IAssetRegistry::GetChecked().AppendState(State, UE::AssetRegistry::EAppendMode::Append);

		for (const FParsedFile& Parsed : ParsedFiles)
		{
			AssetManager.AddDynamicAsset(Parsed.Id, MakeIndexObjectPath(Parsed.Id), GetMarkerBundle());
		}
	}

	void DumpCommand(const TArray<FString>& Args)
	{
		const UArcCoreAssetManager* AssetManager = Cast<UArcCoreAssetManager>(UAssetManager::GetIfInitialized());
		if (!AssetManager)
		{
			UE_LOGF(LogArcJsonAssets, Warning, "ArcJsonAssets.Dump: the Arc asset manager is not initialized");
			return;
		}
		AssetManager->DumpJsonRecords(Args.Num() > 0 ? FPrimaryAssetType(FName(*Args[0])) : FPrimaryAssetType());
	}

	FAutoConsoleCommand DumpConsoleCommand(
		TEXT("ArcJsonAssets.Dump"),
		TEXT("ArcJsonAssets.Dump [PrimaryAssetType]: logs every JSON asset record with its file, export, primary asset path and index tags."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&DumpCommand));
}

TSharedPtr<FStreamableHandle> FArcAssetManagerStartupJob::DoJob() const
{
	const double JobStartTime = FPlatformTime::Seconds();

	TSharedPtr<FStreamableHandle> Handle;
	UE_LOGF(LogArcAssetManager, Display,"Startup job \"%ls\" starting", *JobName);
	JobFunc(*this, Handle);

	if (Handle.IsValid())
	{
		Handle->BindUpdateDelegate(FStreamableUpdateDelegate::CreateRaw(this
			, &FArcAssetManagerStartupJob::UpdateSubstepProgressFromStreamable));
		Handle->WaitUntilComplete(0.0f
			, false);
		Handle->BindUpdateDelegate(FStreamableUpdateDelegate());
	}

	UE_LOGF(LogArcAssetManager
		, Display
		,"Startup job \"%ls\" took %.2f seconds to complete", *JobName
		, FPlatformTime::Seconds() - JobStartTime);

	return Handle;
}

UArcCoreAssetManager::UArcCoreAssetManager()
{
}

UArcCoreAssetManager& UArcCoreAssetManager::Get()
{
	check(GEngine);

	if (UArcCoreAssetManager* Singleton = Cast<UArcCoreAssetManager>(GEngine->AssetManager))
	{
		return *Singleton;
	}

	UE_LOGF(LogArcAssetManager
		, Fatal
		, "Invalid AssetManagerClassName in DefaultEngine.ini.  It must be set to ArcAssetManager!");

	// Fatal error above prevents this from being called.
	return *NewObject<UArcCoreAssetManager>();
}

TSharedPtr<FStreamableHandle> UArcCoreAssetManager::LoadAsset(FSoftObjectPath& Asset
															  , FStreamableDelegate DelegateToCall
															  , TAsyncLoadPriority Priority
															  , const FString& DebugName)
{
	TArray<FSoftObjectPath> List;
	List.Add(Asset);
	return LoadAssetList(List
		, DelegateToCall
		, Priority);
}

UObject* UArcCoreAssetManager::SynchronousLoadAsset(const FSoftObjectPath& AssetPath)
{
	if (AssetPath.IsValid())
	{
		TUniquePtr<FScopeLogTime> LogTimePtr;

		if (ShouldLogAssetLoads())
		{
			LogTimePtr = MakeUnique<FScopeLogTime>(*FString::Printf(TEXT("Synchronously loaded asset [%s]")
					, *AssetPath.ToString())
				, nullptr
				, FScopeLogTime::ScopeLog_Seconds);
		}

		if (UAssetManager::IsInitialized())
		{
			return UAssetManager::GetStreamableManager().LoadSynchronous(AssetPath
				, false);
		}

		// Use LoadObject if asset manager isn't ready yet.
		return AssetPath.TryLoad();
	}

	return nullptr;
}

bool UArcCoreAssetManager::ShouldLogAssetLoads()
{
	static bool bLogAssetLoads = FParse::Param(FCommandLine::Get()
		, TEXT("LogAssetLoads"));
	return bLogAssetLoads;
}

void UArcCoreAssetManager::AddLoadedAsset(const UObject* Asset)
{
	if (ensureAlways(Asset))
	{
		FScopeLock LoadedAssetsLock(&LoadedAssetsCritical);
		LoadedAssets.Add(Asset);
	}
}

void UArcCoreAssetManager::RemoveLoadedAsset(const UObject* Asset)
{
	LoadedAssets.Remove(Asset);
}

void UArcCoreAssetManager::DumpLoadedAssets()
{
	UE_LOGF(LogArcAssetManager
		, Log
		,"========== Start Dumping Loaded Assets ==========");

	for (const UObject* LoadedAsset : Get().LoadedAssets)
	{
		UE_LOGF(LogArcAssetManager
			, Log
			,"  %ls", *GetNameSafe(LoadedAsset));
	}

	UE_LOGF(LogArcAssetManager
		, Log
		,"... %d assets in loaded pool", Get().LoadedAssets.Num());
	UE_LOGF(LogArcAssetManager
		, Log
		,"========== Finish Dumping Loaded Assets ==========");
}

void UArcCoreAssetManager::StartInitialLoading()
{
	Super::StartInitialLoading();

	STARTUP_JOB(InitializeNativeGameplayTags());
	STARTUP_JOB(InitializeGameplayCueManager());

	DoAllStartupJobs();

	if (!IsRunningCookCommandlet())
	{
		JsonContentRootChangedHandle = ArcJsonAssetPaths::OnContentRootChanged().AddUObject(
			this, &UArcCoreAssetManager::HandleJsonContentRootChanged);
		CallOrRegister_OnCompletedInitialScan(
			FSimpleMulticastDelegate::FDelegate::CreateUObject(this, &UArcCoreAssetManager::HandleJsonInitialScanCompleted));
	}
}

void UArcCoreAssetManager::BeginDestroy()
{
	ArcJsonAssetPaths::OnContentRootChanged().Remove(JsonContentRootChangedHandle);
	JsonContentRootChangedHandle.Reset();
	Super::BeginDestroy();
}

const void* UArcCoreAssetManager::FindJsonRecord(const FPrimaryAssetId& Id, const UScriptStruct& Struct) const
{
	check(IsInGameThread());
	const FInstancedStruct* Record = JsonRecords.Find(Id);
	if (!Record)
	{
		return nullptr;
	}
	const FArcJsonAssetTypeInfo* Type = FindJsonRecordType(Id.PrimaryAssetType);
	if (!Type || Type->Struct != &Struct)
	{
		UE_LOGF(LogArcJsonAssets, Error, "ArcJsonAssets: FindJsonRecord: %ls is a %ls record, requested as %ls",
			*Id.ToString(), Type && Type->Struct ? *Type->Struct->GetName() : TEXT("<unregistered>"), *Struct.GetName());
		return nullptr;
	}
	return Record->GetMemory();
}

TArray<FPrimaryAssetType> UArcCoreAssetManager::GetJsonRecordTypes() const
{
	check(IsInGameThread());
	TArray<FPrimaryAssetType> Result;
	for (const FArcJsonAssetTypeInfo& Type : ArcJsonAssets::GetTypes())
	{
		Result.Add(Type.PrimaryAssetType);
	}
	for (const TPair<FPrimaryAssetType, FJsonTypeUse>& Pair : JsonCustomTypes)
	{
		Result.Add(Pair.Key);
	}
	Result.Sort([](const FPrimaryAssetType& A, const FPrimaryAssetType& B) -> bool
	{
		return A.LexicalLess(B);
	});
	return Result;
}

const FArcJsonAssetTypeInfo* UArcCoreAssetManager::FindJsonRecordType(FPrimaryAssetType PrimaryAssetType) const
{
	check(IsInGameThread());
	if (const FJsonTypeUse* Use = JsonCustomTypes.Find(PrimaryAssetType))
	{
		return ArcJsonAssets::FindTypeByExtension(Use->Extension);
	}
	return ArcCoreAssetManagerJsonPrivate::FindRegisteredDefaultType(PrimaryAssetType);
}

FString UArcCoreAssetManager::GetJsonRecordFilename(const FPrimaryAssetId& Id) const
{
	if (!FindJsonRecordType(Id.PrimaryAssetType))
	{
		return FString();
	}
	FAssetData AssetData;
	FString SourcePath;
	FString Filename;
	if (!GetPrimaryAssetData(Id, AssetData)
		|| !AssetData.GetTagValue(ArcCoreAssetManagerJsonPrivate::SourcePathTag, SourcePath)
		|| !ArcJsonAssetPaths::ToFilename(ArcJsonAssetPaths::GetContentRoots(), SourcePath, Filename))
	{
		return FString();
	}
	return Filename;
}

TArray<FArcJsonRecordEntry> UArcCoreAssetManager::GetJsonRecordEntries(FPrimaryAssetType PrimaryAssetType) const
{
	TArray<FArcJsonRecordEntry> Result;
	if (!FindJsonRecordType(PrimaryAssetType))
	{
		return Result;
	}
	TArray<FPrimaryAssetId> Ids;
	GetPrimaryAssetIdList(PrimaryAssetType, Ids);
	for (const FPrimaryAssetId& Id : Ids)
	{
		FAssetData AssetData;
		FString DisplayName;
		if (GetPrimaryAssetData(Id, AssetData) && AssetData.GetTagValue(ArcCoreAssetManagerJsonPrivate::DisplayNameTag, DisplayName))
		{
			Result.Add({ Id, DisplayName });
		}
	}
	Result.Sort([](const FArcJsonRecordEntry& A, const FArcJsonRecordEntry& B) -> bool
	{
		return A.DisplayName == B.DisplayName ? A.Id.ToString() < B.Id.ToString() : A.DisplayName < B.DisplayName;
	});
	return Result;
}

bool UArcCoreAssetManager::ReindexJsonFile(const FString& Filename)
{
	check(IsInGameThread());
	if (!bJsonInitialIndexCompleted)
	{
		return false;
	}
	ArcCoreAssetManagerJsonPrivate::FParsedFile Parsed;
	if (!ArcCoreAssetManagerJsonPrivate::ParseFile(ArcJsonAssetPaths::NormalizeFilename(Filename), Parsed)
		|| !AcceptJsonId(Parsed.Id, *Parsed.Type, Parsed.Filename, nullptr))
	{
		return false;
	}
	ArcCoreAssetManagerJsonPrivate::IndexParsed(*this, MakeArrayView(&Parsed, 1));
	StoreJsonRecord(Parsed.Id, *Parsed.Type, MoveTemp(Parsed.Record));
	JsonRecordChanged.Broadcast(Parsed.Id);
	return true;
}

bool UArcCoreAssetManager::RemoveJsonFile(const FString& Filename)
{
	check(IsInGameThread());
	if (!bJsonInitialIndexCompleted)
	{
		return false;
	}
	const FString Normalized = ArcJsonAssetPaths::NormalizeFilename(Filename);
	ForgetJsonDuplicate(Normalized);
	const FPrimaryAssetId* Found = JsonFilenameToId.Find(Normalized);
	if (!Found)
	{
		return false;
	}
	const FPrimaryAssetId Id = *Found;
	RemoveJsonRecord(Id, Normalized);
	return true;
}

void UArcCoreAssetManager::DumpJsonRecords(FPrimaryAssetType PrimaryAssetType) const
{
	TArray<FPrimaryAssetId> Ids;
	for (const FPrimaryAssetType& Type : GetJsonRecordTypes())
	{
		if (!PrimaryAssetType.IsValid() || Type == PrimaryAssetType)
		{
			GetPrimaryAssetIdList(Type, Ids);
		}
	}
	Ids.Sort([](const FPrimaryAssetId& A, const FPrimaryAssetId& B) -> bool
	{
		return A.ToString() < B.ToString();
	});

	UE_LOGF(LogArcJsonAssets, Display, "ArcJsonAssets.Dump: %d records", Ids.Num());
	for (const FPrimaryAssetId& Id : Ids)
	{
		const FArcJsonAssetTypeInfo* Type = FindJsonRecordType(Id.PrimaryAssetType);
		FString Export(TEXT("<not stored>"));
		const void* RecordMemory = Type && Type->Struct ? FindJsonRecord(Id, *Type->Struct) : nullptr;
		if (RecordMemory)
		{
			FInstancedStruct Record;
			Record.InitializeAs(Type->Struct, static_cast<const uint8*>(RecordMemory));
			const TArray<uint8> ExportUtf8 = ArcJsonAssetFile::FormatRecord(Record);
			Export = ExportUtf8.IsEmpty()
				? FString(TEXT("<format failed>"))
				: FString::ConstructFromPtrSize(reinterpret_cast<const UTF8CHAR*>(ExportUtf8.GetData()), ExportUtf8.Num());
		}

		FString Tags;
		FAssetData AssetData;
		if (GetPrimaryAssetData(Id, AssetData))
		{
			for (const TPair<FName, FString>& Tag : AssetData.TagsAndValues.CopyMap())
			{
				Tags += FString::Printf(TEXT("\n    %s = %s"), *Tag.Key.ToString(), *Tag.Value);
			}
		}

		UE_LOGF(LogArcJsonAssets, Display, "%ls\n  file: %ls\n  primary asset path: %ls\n  tags:%ls\n  record: %ls",
			*Id.ToString(), *GetJsonRecordFilename(Id), *GetPrimaryAssetPath(Id).ToString(), Tags.IsEmpty() ? TEXT(" none") : *Tags, *Export);
	}
}

#if WITH_EDITOR
void UArcCoreAssetManager::ReinitializeFromConfig()
{
	// Upstream requires an empty AssetPathMap and resets CachedAssetBundles (AssetManager.cpp:5494-5497), so JSON ids leave before Super and come back after.
	TArray<TPair<FPrimaryAssetId, FSoftObjectPath>> JsonAssets;
	for (const FPrimaryAssetType& Type : GetJsonRecordTypes())
	{
		TArray<FPrimaryAssetId> Ids;
		GetPrimaryAssetIdList(Type, Ids);
		for (const FPrimaryAssetId& Id : Ids)
		{
			JsonAssets.Emplace(Id, GetPrimaryAssetPath(Id));
		}
	}
	for (const TPair<FPrimaryAssetId, FSoftObjectPath>& Entry : JsonAssets)
	{
		AddDynamicAsset(Entry.Key, Entry.Value, FAssetBundleData());
	}

	Super::ReinitializeFromConfig();

	for (const TPair<FPrimaryAssetId, FSoftObjectPath>& Entry : JsonAssets)
	{
		AddDynamicAsset(Entry.Key, Entry.Value, ArcCoreAssetManagerJsonPrivate::GetMarkerBundle());
	}
}
#endif

void UArcCoreAssetManager::HandleJsonInitialScanCompleted()
{
	check(IsInGameThread());
	if (bJsonInitialIndexCompleted)
	{
		return;
	}
	bJsonInitialIndexCompleted = true;
	const FArcJsonAssetContentRoots Roots = ArcJsonAssetPaths::GetContentRoots();
	IndexJsonRoots(Roots.Roots, true);
}

void UArcCoreAssetManager::HandleJsonContentRootChanged(const FArcJsonAssetContentRoot& Root, bool bMounted)
{
	check(IsInGameThread());
	if (!bJsonInitialIndexCompleted)
	{
		return;
	}
	if (bMounted)
	{
		IndexJsonRoots(MakeArrayView(&Root, 1), false);
		return;
	}

	TArray<FString> Filenames;
	for (const TPair<FString, FPrimaryAssetId>& Pair : JsonFilenameToId)
	{
		if (ArcJsonAssetPaths::IsUnderRoot(Root, Pair.Key))
		{
			Filenames.Add(Pair.Key);
		}
	}
	ArcCoreAssetManagerJsonPrivate::RemoveWaitingUnderRoot(JsonDuplicateFilenames, Root);
	ArcCoreAssetManagerJsonPrivate::RemoveWaitingUnderRoot(JsonTypeConflictFilenames, Root);
	for (const FString& Filename : Filenames)
	{
		RemoveJsonFile(Filename);
	}
}

void UArcCoreAssetManager::IndexJsonRoots(TConstArrayView<FArcJsonAssetContentRoot> Roots, bool bAlwaysLogSummary)
{
	if (ArcJsonAssets::GetTypes().IsEmpty())
	{
		return;
	}

	const double WalkStart = FPlatformTime::Seconds();
	TArray<FString> Filenames;
	for (const FArcJsonAssetContentRoot& Root : Roots)
	{
		ArcCoreAssetManagerJsonPrivate::CollectFiles(Root, Filenames);
	}
	Filenames.Sort();
	const double WalkSeconds = FPlatformTime::Seconds() - WalkStart;

	TMap<FPrimaryAssetType, ArcCoreAssetManagerJsonPrivate::FTypeStats> Stats;
	TArray<ArcCoreAssetManagerJsonPrivate::FParsedFile> Accepted;
	FJsonPendingIndex Pending;
	for (const FString& Filename : Filenames)
	{
		const double FileStart = FPlatformTime::Seconds();
		const FArcJsonAssetTypeInfo* Type = ArcJsonAssets::FindTypeByFilename(Filename);
		if (!Type)
		{
			continue;
		}
		ArcCoreAssetManagerJsonPrivate::FTypeStats& TypeStats = Stats.FindOrAdd(Type->PrimaryAssetType);
		ArcCoreAssetManagerJsonPrivate::FParsedFile Parsed;
		if (ArcCoreAssetManagerJsonPrivate::ParseFile(Filename, Parsed) && AcceptJsonId(Parsed.Id, *Parsed.Type, Parsed.Filename, &Pending))
		{
			Pending.Owners.Add(TPair<FString, FName>(Parsed.Type->Extension, Parsed.Id.PrimaryAssetName), FJsonPendingOwner{ Parsed.Id, Parsed.Filename });
			if (Parsed.Id.PrimaryAssetType != Parsed.Type->PrimaryAssetType)
			{
				Pending.TypeExtensions.Add(Parsed.Id.PrimaryAssetType, Parsed.Type->Extension);
			}
			Accepted.Add(MoveTemp(Parsed));
			++TypeStats.Loaded;
		}
		else
		{
			++TypeStats.Rejected;
		}
		TypeStats.Seconds += FPlatformTime::Seconds() - FileStart;
	}

	const double IndexStart = FPlatformTime::Seconds();
	ArcCoreAssetManagerJsonPrivate::IndexParsed(*this, Accepted);
	const double IndexSeconds = FPlatformTime::Seconds() - IndexStart;

	for (ArcCoreAssetManagerJsonPrivate::FParsedFile& Parsed : Accepted)
	{
		StoreJsonRecord(Parsed.Id, *Parsed.Type, MoveTemp(Parsed.Record));
	}
	for (const ArcCoreAssetManagerJsonPrivate::FParsedFile& Parsed : Accepted)
	{
		JsonRecordChanged.Broadcast(Parsed.Id);
	}

	for (const FArcJsonAssetTypeInfo& Type : ArcJsonAssets::GetTypes())
	{
		const ArcCoreAssetManagerJsonPrivate::FTypeStats* TypeStats = Stats.Find(Type.PrimaryAssetType);
		if (!TypeStats && !bAlwaysLogSummary)
		{
			continue;
		}
		const ArcCoreAssetManagerJsonPrivate::FTypeStats Values = TypeStats ? *TypeStats : ArcCoreAssetManagerJsonPrivate::FTypeStats();
		UE_LOGF(LogArcJsonAssets, Display,
			"ArcJsonAssets: %ls (.%ls) loaded %d, rejected %d, %.1f ms parse (shared: %.1f ms walk, %.1f ms index, %d roots)",
			*Type.PrimaryAssetType.ToString(), *Type.Extension, Values.Loaded, Values.Rejected, Values.Seconds * 1000.0,
			WalkSeconds * 1000.0, IndexSeconds * 1000.0, Roots.Num());
	}
}

bool UArcCoreAssetManager::AcceptJsonId(const FPrimaryAssetId& Id, const FArcJsonAssetTypeInfo& Type, const FString& Filename, const FJsonPendingIndex* Pending)
{
	ForgetJsonDuplicate(Filename);
	const FPrimaryAssetType IdType = Id.PrimaryAssetType;
	if (IdType != Type.PrimaryAssetType)
	{
		if (const FArcJsonAssetTypeInfo* DefaultOwner = ArcCoreAssetManagerJsonPrivate::FindRegisteredDefaultType(IdType))
		{
			UE_LOGF(LogArcJsonAssets, Error, "ArcJsonAssets: %ls: type %ls is the default type of .%ls records",
				*Filename, *IdType.ToString(), *DefaultOwner->Extension);
			return false;
		}
		FPrimaryAssetTypeInfo AssetTypeInfo;
		if (GetPrimaryAssetTypeInfo(IdType, AssetTypeInfo) && !AssetTypeInfo.bIsDynamicAsset)
		{
			UE_LOGF(LogArcJsonAssets, Error, "ArcJsonAssets: %ls: type %ls is a scanned asset manager type", *Filename, *IdType.ToString());
			return false;
		}
		const FString* PendingExtension = Pending ? Pending->TypeExtensions.Find(IdType) : nullptr;
		const FJsonTypeUse* Use = JsonCustomTypes.Find(IdType);
		const FString* OwnerExtension = PendingExtension ? PendingExtension : (Use ? &Use->Extension : nullptr);
		if (OwnerExtension && !OwnerExtension->Equals(Type.Extension, ESearchCase::IgnoreCase))
		{
			UE_LOGF(LogArcJsonAssets, Error, "ArcJsonAssets: %ls: type %ls is used by .%ls records", *Filename, *IdType.ToString(), **OwnerExtension);
			JsonTypeConflictFilenames.FindOrAdd(IdType).AddUnique(Filename);
			return false;
		}
	}

	// Keyed by extension and GUID, not id: a copy whose type was edited is still a duplicate.
	const FJsonPendingOwner* PendingOwner = Pending ? Pending->Owners.Find(TPair<FString, FName>(Type.Extension, Id.PrimaryAssetName)) : nullptr;
	FPrimaryAssetId OwnerId;
	FString Owner;
	if (PendingOwner)
	{
		OwnerId = PendingOwner->Id;
		Owner = PendingOwner->Filename;
	}
	else
	{
		Owner = FindJsonGuidOwner(Type, Id.PrimaryAssetName, OwnerId);
	}
	if (!Owner.IsEmpty() && Owner != Filename)
	{
		UE_LOGF(LogArcJsonAssets, Error, "ArcJsonAssets: %ls: GUID of %ls is already used by %ls (%ls)",
			*Filename, *Id.ToString(), *Owner, *OwnerId.ToString());
		JsonDuplicateFilenames.FindOrAdd(OwnerId).AddUnique(Filename);
		return false;
	}

	if (const FPrimaryAssetId* PreviousId = JsonFilenameToId.Find(Filename))
	{
		if (*PreviousId != Id)
		{
			const FPrimaryAssetId Previous = *PreviousId;
			if (Previous.PrimaryAssetName == Id.PrimaryAssetName)
			{
				TArray<FString> Waiting;
				if (JsonDuplicateFilenames.RemoveAndCopyValue(Previous, Waiting))
				{
					JsonDuplicateFilenames.FindOrAdd(Id).Append(Waiting);
				}
			}
			RemoveJsonRecord(Previous, Filename);
		}
	}
	JsonFilenameToId.Add(Filename, Id);
	return true;
}

FString UArcCoreAssetManager::FindJsonGuidOwner(const FArcJsonAssetTypeInfo& Type, FName Guid, FPrimaryAssetId& OutOwnerId) const
{
	const FPrimaryAssetId DefaultId(Type.PrimaryAssetType, Guid);
	FString Owner = GetJsonRecordFilename(DefaultId);
	if (!Owner.IsEmpty())
	{
		OutOwnerId = DefaultId;
		return Owner;
	}
	for (const TPair<FPrimaryAssetType, FJsonTypeUse>& Pair : JsonCustomTypes)
	{
		if (!Pair.Value.Extension.Equals(Type.Extension, ESearchCase::IgnoreCase))
		{
			continue;
		}
		const FPrimaryAssetId CustomId(Pair.Key, Guid);
		Owner = GetJsonRecordFilename(CustomId);
		if (!Owner.IsEmpty())
		{
			OutOwnerId = CustomId;
			return Owner;
		}
	}
	return FString();
}

void UArcCoreAssetManager::StoreJsonRecord(const FPrimaryAssetId& Id, const FArcJsonAssetTypeInfo& Type, FInstancedStruct&& Record)
{
	if (FInstancedStruct* Existing = JsonRecords.Find(Id))
	{
		const UScriptStruct* Struct = Existing->GetScriptStruct();
		check(Struct && Struct == Record.GetScriptStruct());
		Struct->CopyScriptStruct(Existing->GetMutableMemory(), Record.GetMemory());
		return;
	}
	JsonRecords.Add(Id, MoveTemp(Record));
	if (Id.PrimaryAssetType != Type.PrimaryAssetType)
	{
		FJsonTypeUse& Use = JsonCustomTypes.FindOrAdd(Id.PrimaryAssetType);
		Use.Extension = Type.Extension;
		++Use.IdCount;
	}
}

void UArcCoreAssetManager::RemoveJsonRecord(const FPrimaryAssetId& Id, const FString& Filename)
{
	JsonFilenameToId.Remove(Filename);
	AddDynamicAsset(Id, ArcCoreAssetManagerJsonPrivate::MakeIndexObjectPath(Id), FAssetBundleData());
	bool bTypeLeftUse = false;
	if (JsonRecords.Remove(Id) > 0)
	{
		if (FJsonTypeUse* Use = JsonCustomTypes.Find(Id.PrimaryAssetType))
		{
			--Use->IdCount;
			if (Use->IdCount <= 0)
			{
				JsonCustomTypes.Remove(Id.PrimaryAssetType);
				bTypeLeftUse = true;
			}
		}
	}
	JsonRecordChanged.Broadcast(Id);

	TArray<FString> Waiting;
	JsonDuplicateFilenames.RemoveAndCopyValue(Id, Waiting);
	if (bTypeLeftUse)
	{
		TArray<FString> TypeWaiting;
		JsonTypeConflictFilenames.RemoveAndCopyValue(Id.PrimaryAssetType, TypeWaiting);
		Waiting.Append(TypeWaiting);
	}
	for (const FString& WaitingFilename : Waiting)
	{
		if (FPaths::FileExists(WaitingFilename))
		{
			ReindexJsonFile(WaitingFilename);
		}
	}
}

void UArcCoreAssetManager::ForgetJsonDuplicate(const FString& Filename)
{
	ArcCoreAssetManagerJsonPrivate::RemoveWaitingFilename(JsonDuplicateFilenames, Filename);
	ArcCoreAssetManagerJsonPrivate::RemoveWaitingFilename(JsonTypeConflictFilenames, Filename);
}

void UArcCoreAssetManager::InitializeNativeGameplayTags()
{
	SCOPED_BOOT_TIMING("UArcCoreAssetManager::InitializeNativeGameplayTags");

	FArcCoreGameplayTags::InitializeNativeTags();
	FArcMassGameFrameworkTags::InitializeNativeTags();
	Arcx::EventTags::InitializeNativeEventTagsBase();

	UGameplayTagsManager& Manager = UGameplayTagsManager::Get();
	// Notify manager that we are done adding native tags.
	Manager.DoneAddingNativeTags();
}

void UArcCoreAssetManager::InitializeGameplayCueManager()
{
	// GAS retired (Bucket D 7); UArcGameplayCueManager initialization no longer needed.
}

UPrimaryDataAsset* UArcCoreAssetManager::LoadGameDataOfClass(TSubclassOf<UPrimaryDataAsset> DataClass
															 , const TSoftObjectPtr<UPrimaryDataAsset>& DataClassPath
															 , FPrimaryAssetType PrimaryAssetType)
{
	UPrimaryDataAsset* Asset = nullptr;

	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("Loading GameData Object")
		, STAT_GameData
		, STATGROUP_LoadTime);
	if (!DataClassPath.IsNull())
	{
#if WITH_EDITOR
		FScopedSlowTask SlowTask(0
			, FText::Format(NSLOCTEXT("LyraEditor"
					, "BeginLoadingGameDataTask"
					, "Loading GameData {0}")
				, FText::FromName(DataClass->GetFName())));
		const bool bShowCancelButton = false;
		const bool bAllowInPIE = true;
		SlowTask.MakeDialog(bShowCancelButton
			, bAllowInPIE);
#endif
		UE_LOGF(LogArcAssetManager
			, Log
			,"Loading GameData: %ls ...", *DataClassPath.ToString());
		SCOPE_LOG_TIME_IN_SECONDS(TEXT("    ... GameData loaded!")
			, nullptr);

		// This can be called recursively in the editor because it is called on demand
		// from PostLoad so force a sync load for primary asset and async load the rest in
		// that case
		if (GIsEditor)
		{
			Asset = DataClassPath.LoadSynchronous();
			LoadPrimaryAssetsWithType(PrimaryAssetType);
		}
		else
		{
			TSharedPtr<FStreamableHandle> Handle = LoadPrimaryAssetsWithType(PrimaryAssetType);
			if (Handle.IsValid())
			{
				Handle->WaitUntilComplete(0.0f
					, false);

				// This should always work
				Asset = Cast<UPrimaryDataAsset>(Handle->GetLoadedAsset());
			}
		}
	}

	if (Asset)
	{
		GameDataMap.Add(DataClass
			, Asset);
	}
	else
	{
		// It is not acceptable to fail to load any GameData asset. It will result in soft
		// failures that are hard to diagnose.
		UE_LOGF(LogArcAssetManager
			, Fatal
			, "Failed to load GameData asset at %ls. Type %ls. This is not recoverable and likely means you do not "
				"have the correct data to run %ls.", *DataClassPath.ToString()
			, *PrimaryAssetType.ToString()
			, FApp::GetProjectName());
	}

	return Asset;
}

void UArcCoreAssetManager::DoAllStartupJobs()
{
	SCOPED_BOOT_TIMING("ULyraAssetManager::DoAllStartupJobs");
	const double AllStartupJobsStartTime = FPlatformTime::Seconds();

	if (IsRunningDedicatedServer())
	{
		// No need for periodic progress updates, just run the jobs
		for (const FArcAssetManagerStartupJob& StartupJob : StartupJobs)
		{
			StartupJob.DoJob();
		}
	}
	else
	{
		if (StartupJobs.Num() > 0)
		{
			float TotalJobValue = 0.0f;
			for (const FArcAssetManagerStartupJob& StartupJob : StartupJobs)
			{
				TotalJobValue += StartupJob.JobWeight;
			}

			float AccumulatedJobValue = 0.0f;
			for (FArcAssetManagerStartupJob& StartupJob : StartupJobs)
			{
				const float JobValue = StartupJob.JobWeight;
				StartupJob.SubstepProgressDelegate.BindLambda(
					[This = this, AccumulatedJobValue, JobValue, TotalJobValue] (float NewProgress)
					{
						const float SubstepAdjustment = FMath::Clamp(NewProgress
															, 0.0f
															, 1.0f) * JobValue;
						const float OverallPercentWithSubstep =
								(AccumulatedJobValue + SubstepAdjustment) / TotalJobValue;

						This->UpdateInitialGameContentLoadPercent(OverallPercentWithSubstep);
					});

				StartupJob.DoJob();

				StartupJob.SubstepProgressDelegate.Unbind();

				AccumulatedJobValue += JobValue;

				UpdateInitialGameContentLoadPercent(AccumulatedJobValue / TotalJobValue);
			}
		}
		else
		{
			UpdateInitialGameContentLoadPercent(1.0f);
		}
	}

	StartupJobs.Empty();

	UE_LOGF(LogArcAssetManager
		, Display
		,"All startup jobs took %.2f seconds to complete", FPlatformTime::Seconds() - AllStartupJobsStartTime);
}

void UArcCoreAssetManager::UpdateInitialGameContentLoadPercent(float GameContentPercent)
{
	// Could route this to the early startup loading screen
}

void UArcCoreAssetManager::SetExperienceBundlesAsync(UWorld* InWorld, const TArray<FName>& InExperienceBundles)
{
}

void UArcCoreAssetManager::SetExperienceBundlesSync(UWorld* InWorld, const TArray<FName>& InExperienceBundles)
{
	TArray<FName> OldBundles;
	ENetMode NetMode = InWorld->GetNetMode();

	if (NetMode == ENetMode::NM_Standalone)
	{
		for (const FName& BundleName : ExperienceBundles)
		{
			if (InExperienceBundles.Contains(BundleName) || DefaultServerBundles.Contains(BundleName)
				|| DefaultClientBundles.Contains(BundleName)
				|| DefaultGameBundles.Contains(BundleName))
			{
				continue;
			}
			OldBundles.AddUnique(BundleName);
		}

		ExperienceBundles.Empty();
		for (const FName& Bundle : DefaultServerBundles)
		{
			ExperienceBundles.AddUnique(Bundle);
		}
		for (const FName& Bundle : DefaultClientBundles)
		{
			ExperienceBundles.AddUnique(Bundle);
		}
	}

	if (NetMode == ENetMode::NM_DedicatedServer)
	{
		for (const FName& BundleName : ExperienceBundles)
		{
			if (InExperienceBundles.Contains(BundleName) || DefaultServerBundles.Contains(BundleName)
				|| DefaultGameBundles.Contains(BundleName))
			{
				continue;
			}
			OldBundles.AddUnique(BundleName);
		}
		ExperienceBundles.Empty();
		for (const FName& Bundle : DefaultServerBundles)
		{
			ExperienceBundles.AddUnique(Bundle);
		}
	}

	if (NetMode == ENetMode::NM_Client)
	{
		for (const FName& BundleName : ExperienceBundles)
		{
			if (InExperienceBundles.Contains(BundleName) || DefaultClientBundles.Contains(BundleName)
				|| DefaultGameBundles.Contains(BundleName))
			{
				continue;
			}
			OldBundles.AddUnique(BundleName);
		}
		ExperienceBundles.Empty();
		for (const FName& Bundle : DefaultClientBundles)
		{
			ExperienceBundles.AddUnique(Bundle);
		}
	}
	for (const FName& Bundle : InExperienceBundles)
	{
		ExperienceBundles.AddUnique(Bundle);
	}
	for (const FName& Bundle : DefaultGameBundles)
	{
		ExperienceBundles.AddUnique(Bundle);
	}
	TSharedPtr<FStreamableHandle> Handle = ChangeBundleStateForMatchingPrimaryAssets(ExperienceBundles, OldBundles);
	if (Handle.IsValid())
	{
		Handle->WaitUntilComplete();
	}
}

#if WITH_EDITOR
void UArcCoreAssetManager::PreBeginPIE(bool bStartSimulate)
{
	Super::PreBeginPIE(bStartSimulate);

	{
		FScopedSlowTask SlowTask(0
			, NSLOCTEXT("ArcEditor"
				, "BeginLoadingPIEData"
				, "Loading PIE Data"));
		const bool bShowCancelButton = false;
		const bool bAllowInPIE = true;
		SlowTask.MakeDialog(bShowCancelButton
			, bAllowInPIE);

		// Intentionally after GetGameData to avoid counting GameData time in this timer
		SCOPE_LOG_TIME_IN_SECONDS(TEXT("PreBeginPIE asset preloading complete")
			, nullptr);
	}
}
#endif
