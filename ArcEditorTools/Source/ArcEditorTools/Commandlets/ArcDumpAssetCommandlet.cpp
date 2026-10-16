// Copyright Lukasz Baran. All Rights Reserved.

#include "Commandlets/ArcDumpAssetCommandlet.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/DataAsset.h"
#include "HAL/FileManager.h"
#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcDumpAsset, Log, All);

#define ARCDUMP_LOG_PREFIX TEXT("[ArcDumpAsset]")

namespace ArcDumpAssetCommandletImpl
{
	struct FOptions
	{
		FString SingleAssetPath;
		FString FolderPath;
		FString OutFile;
		FString OutDir;
		FString Format = TEXT("json");
		bool bList = false;
		bool bForce = false;
		bool bRecursive = true;
	};

	struct FCounters
	{
		int32 Ok = 0;
		int32 Skipped = 0;
		int32 Failed = 0;
	};

	bool IsEligibleClass(const UClass* Class)
	{
		if (Class == nullptr)
		{
			return false;
		}
		return Class->IsChildOf(UDataAsset::StaticClass())
			|| Class->IsChildOf(UBlueprint::StaticClass())
			|| Class->IsChildOf(UBlueprintGeneratedClass::StaticClass());
	}

	bool IsEligibleAssetData(const FAssetData& AssetData)
	{
		const UClass* Class = AssetData.GetClass();
		if (Class && IsEligibleClass(Class))
		{
			return true;
		}
		// GetClass() can return null for blueprint-generated classes that aren't loaded; fall back to name match.
		const FString ClassName = AssetData.AssetClassPath.GetAssetName().ToString();
		return ClassName == TEXT("Blueprint")
			|| ClassName == TEXT("BlueprintGeneratedClass")
			|| ClassName.EndsWith(TEXT("DataAsset"));
	}

	FString ReflectToText(const UObject* Object)
	{
		if (Object == nullptr)
		{
			return FString();
		}
		FString Out;
		const UClass* Class = Object->GetClass();
		Out += FString::Printf(TEXT("# %s : %s\n"), *Object->GetPathName(), *Class->GetName());
		for (TFieldIterator<FProperty> It(Class); It; ++It)
		{
			const FProperty* Prop = *It;
			if (Prop == nullptr)
			{
				continue;
			}
			FString Value;
			const void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(Object);
			Prop->ExportTextItem_Direct(Value, ValuePtr, nullptr, nullptr, PPF_None);
			Out += FString::Printf(TEXT("%s = %s\n"), *Prop->GetName(), *Value);
		}
		return Out;
	}

	bool DumpToJsonString(UObject* Object, FString& OutJson)
	{
		if (Object == nullptr)
		{
			return false;
		}

		const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetStringField(TEXT("__path"), Object->GetPathName());
		Root->SetStringField(TEXT("__class"), Object->GetClass()->GetPathName());

		const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
		const bool bConverted = FJsonObjectConverter::UStructToJsonObject(
			Object->GetClass(),
			Object,
			Body,
			CPF_Edit | CPF_BlueprintVisible,
			0);
		if (!bConverted)
		{
			return false;
		}
		Root->SetObjectField(TEXT("data"), Body);

		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutJson);
		return FJsonSerializer::Serialize(Root, Writer);
	}

	UObject* LoadAssetFromData(const FAssetData& AssetData)
	{
		UObject* Object = AssetData.GetAsset();
		if (UBlueprint* BP = Cast<UBlueprint>(Object))
		{
			// Prefer the CDO of the generated class for blueprints — that's the edited state.
			if (BP->GeneratedClass && BP->GeneratedClass->GetDefaultObject(false))
			{
				return BP->GeneratedClass->GetDefaultObject(false);
			}
		}
		return Object;
	}

	FString MakeOutFileName(const FAssetData& AssetData, const FString& OutDir, const FString& Extension)
	{
		const FString Safe = AssetData.AssetName.ToString().Replace(TEXT("/"), TEXT("_"));
		return FPaths::Combine(OutDir, Safe + Extension);
	}

	bool ProcessAsset(
		const FAssetData& AssetData,
		const FOptions& Options,
		FArchive* JsonlStream,
		FCounters& Counters)
	{
		const FString AssetPathStr = AssetData.GetObjectPathString();

		if (!Options.bForce && !IsEligibleAssetData(AssetData))
		{
			const FString ClassName = AssetData.AssetClassPath.GetAssetName().ToString();
			UE_LOG(LogArcDumpAsset, Display, TEXT("%s skip %s %s"), ARCDUMP_LOG_PREFIX, *AssetPathStr, *ClassName);
			++Counters.Skipped;
			return false;
		}

		UObject* Object = LoadAssetFromData(AssetData);
		if (Object == nullptr)
		{
			UE_LOG(LogArcDumpAsset, Warning, TEXT("%s fail %s load-returned-null"), ARCDUMP_LOG_PREFIX, *AssetPathStr);
			++Counters.Failed;
			return false;
		}

		const bool bJson = Options.Format != TEXT("text");
		FString Payload;
		bool bOk = false;
		if (bJson)
		{
			bOk = DumpToJsonString(Object, Payload);
			if (!bOk)
			{
				// Fallback: reflected text. Wrap as a tiny JSON envelope so JSONL stays line-parseable.
				const FString Text = ReflectToText(Object);
				const TSharedRef<FJsonObject> Envelope = MakeShared<FJsonObject>();
				Envelope->SetStringField(TEXT("__path"), Object->GetPathName());
				Envelope->SetStringField(TEXT("__class"), Object->GetClass()->GetPathName());
				Envelope->SetStringField(TEXT("__fallback"), TEXT("reflected-text"));
				Envelope->SetStringField(TEXT("text"), Text);
				const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Payload);
				bOk = FJsonSerializer::Serialize(Envelope, Writer);
			}
		}
		else
		{
			Payload = ReflectToText(Object);
			bOk = !Payload.IsEmpty();
		}

		if (!bOk)
		{
			UE_LOG(LogArcDumpAsset, Warning, TEXT("%s fail %s serialize"), ARCDUMP_LOG_PREFIX, *AssetPathStr);
			++Counters.Failed;
			return false;
		}

		const FString Extension = bJson ? TEXT(".json") : TEXT(".txt");
		FString OutFile;
		if (JsonlStream != nullptr && bJson)
		{
			// JSONL: collapse newlines, write one line per asset.
			FString OneLine = Payload.Replace(TEXT("\r"), TEXT("")).Replace(TEXT("\n"), TEXT(" "));
			OneLine += TEXT("\n");
			FTCHARToUTF8 Utf8(*OneLine);
			JsonlStream->Serialize(const_cast<ANSICHAR*>(Utf8.Get()), Utf8.Length());
			OutFile = Options.OutFile + TEXT(" (jsonl)");
		}
		else if (!Options.OutDir.IsEmpty())
		{
			OutFile = MakeOutFileName(AssetData, Options.OutDir, Extension);
			if (!FFileHelper::SaveStringToFile(Payload, *OutFile))
			{
				UE_LOG(LogArcDumpAsset, Warning, TEXT("%s fail %s write %s"), ARCDUMP_LOG_PREFIX, *AssetPathStr, *OutFile);
				++Counters.Failed;
				return false;
			}
		}
		else if (!Options.OutFile.IsEmpty())
		{
			OutFile = Options.OutFile;
			if (!FFileHelper::SaveStringToFile(Payload, *OutFile))
			{
				UE_LOG(LogArcDumpAsset, Warning, TEXT("%s fail %s write %s"), ARCDUMP_LOG_PREFIX, *AssetPathStr, *OutFile);
				++Counters.Failed;
				return false;
			}
		}
		else
		{
			// No output destination — log a one-line excerpt so caller knows we ran.
			OutFile = TEXT("<no -Out specified>");
		}

		UE_LOG(LogArcDumpAsset, Display, TEXT("%s ok %s -> %s"), ARCDUMP_LOG_PREFIX, *AssetPathStr, *OutFile);
		++Counters.Ok;
		return true;
	}

	void ParseOptions(const FString& Params, FOptions& Out)
	{
		FParse::Value(*Params, TEXT("AssetPath="), Out.SingleAssetPath);
		FParse::Value(*Params, TEXT("Path="), Out.FolderPath);
		FParse::Value(*Params, TEXT("Out="), Out.OutFile);
		FParse::Value(*Params, TEXT("OutDir="), Out.OutDir);
		FParse::Value(*Params, TEXT("Format="), Out.Format);
		Out.bList = FParse::Param(*Params, TEXT("List"));
		Out.bForce = FParse::Param(*Params, TEXT("Force"));
		Out.bRecursive = !FParse::Param(*Params, TEXT("NoRecursive"));
		Out.Format = Out.Format.ToLower();
		if (Out.Format.IsEmpty())
		{
			Out.Format = TEXT("json");
		}
	}
}

UArcDumpAssetCommandlet::UArcDumpAssetCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = false;
	ShowErrorCount = true;
}

int32 UArcDumpAssetCommandlet::Main(const FString& Params)
{
	using namespace ArcDumpAssetCommandletImpl;

	FOptions Options;
	ParseOptions(Params, Options);

	if (Options.SingleAssetPath.IsEmpty() && Options.FolderPath.IsEmpty())
	{
		UE_LOG(LogArcDumpAsset, Error, TEXT("%s fail no -AssetPath or -Path supplied"), ARCDUMP_LOG_PREFIX);
		return 1;
	}

	IAssetRegistry& AssetRegistry = FAssetRegistryModule::GetRegistry();

	TArray<FAssetData> Assets;
	if (!Options.SingleAssetPath.IsEmpty())
	{
		const FSoftObjectPath SoftPath(Options.SingleAssetPath);
		FAssetData Single = AssetRegistry.GetAssetByObjectPath(SoftPath);
		if (!Single.IsValid())
		{
			// Some callers pass /Game/Foo/Bar without the .Bar suffix; retry by package path.
			TArray<FAssetData> Candidates;
			AssetRegistry.GetAssetsByPackageName(*Options.SingleAssetPath, Candidates);
			if (Candidates.Num() > 0)
			{
				Single = Candidates[0];
			}
		}
		if (!Single.IsValid())
		{
			UE_LOG(LogArcDumpAsset, Error, TEXT("%s fail %s not-found"), ARCDUMP_LOG_PREFIX, *Options.SingleAssetPath);
			return 1;
		}
		Assets.Add(Single);
	}
	else
	{
		// Ensure the target path has been scanned before iterating.
		TArray<FString> ScanPaths;
		ScanPaths.Add(Options.FolderPath);
		AssetRegistry.ScanPathsSynchronous(ScanPaths, /*bForceRescan=*/false);

		FARFilter Filter;
		Filter.PackagePaths.Add(*Options.FolderPath);
		Filter.bRecursivePaths = Options.bRecursive;
		AssetRegistry.GetAssets(Filter, Assets);
	}

	if (Options.bList)
	{
		int32 Eligible = 0;
		for (const FAssetData& Data : Assets)
		{
			const bool bElig = Options.bForce || IsEligibleAssetData(Data);
			const FString ClassName = Data.AssetClassPath.GetAssetName().ToString();
			UE_LOG(LogArcDumpAsset, Display, TEXT("%s list %s %s %s"),
				ARCDUMP_LOG_PREFIX,
				bElig ? TEXT("eligible") : TEXT("skip"),
				*Data.GetObjectPathString(),
				*ClassName);
			if (bElig)
			{
				++Eligible;
			}
		}
		UE_LOG(LogArcDumpAsset, Display, TEXT("%s done list: %d eligible / %d total"),
			ARCDUMP_LOG_PREFIX, Eligible, Assets.Num());
		return Assets.Num() == 0 ? 1 : 0;
	}

	FCounters Counters;
	TUniquePtr<FArchive> JsonlStream;
	const bool bJsonlMode = !Options.FolderPath.IsEmpty()
		&& !Options.OutFile.IsEmpty()
		&& Options.OutDir.IsEmpty()
		&& Options.Format != TEXT("text");
	if (bJsonlMode)
	{
		JsonlStream.Reset(IFileManager::Get().CreateFileWriter(*Options.OutFile));
		if (!JsonlStream.IsValid())
		{
			UE_LOG(LogArcDumpAsset, Error, TEXT("%s fail open %s"), ARCDUMP_LOG_PREFIX, *Options.OutFile);
			return 1;
		}
	}
	if (!Options.OutDir.IsEmpty())
	{
		IFileManager::Get().MakeDirectory(*Options.OutDir, /*Tree=*/true);
	}

	for (const FAssetData& Data : Assets)
	{
		ProcessAsset(Data, Options, JsonlStream.Get(), Counters);
	}

	if (JsonlStream.IsValid())
	{
		JsonlStream->Close();
	}

	UE_LOG(LogArcDumpAsset, Display, TEXT("%s done: %d ok, %d skipped, %d failed"),
		ARCDUMP_LOG_PREFIX, Counters.Ok, Counters.Skipped, Counters.Failed);

	return Counters.Ok > 0 ? 0 : 1;
}
