// Copyright Epic Games, Inc. All Rights Reserved.

#include "Panel/ArcLoreAssetDisplay.h"

#include "ISourceControlModule.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "Misc/PackagePath.h"

#if WITH_EDITOR
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetRegistry/AssetData.h"
#include "WorldPartition/WorldPartitionActorDesc.h"
#include "WorldPartition/WorldPartitionActorDescUtils.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "Modules/ModuleManager.h"
#include "FileHelpers.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Editor.h"
#endif

// Unique, file-scoped namespace for private helpers (CLAUDE.md). NOTE: the public API
// below lives in the SHARED ArcLoreAssetDisplay namespace — only these internal helpers
// are file-scoped.
namespace ArcLoreAssetDisplay_Internal
{
	/** Convert an .uasset/.umap on-disk path to its long package name. Returns false for a
	 *  non-asset extension OR a path outside a mounted content root. */
	bool TryGetAssetLongName(const FString& InAbsPath, FString& OutLongName)
	{
		if (InAbsPath.IsEmpty())
		{
			return false;
		}

		const FString Extension = FPaths::GetExtension(InAbsPath);
		const bool bIsAssetPackage = Extension.Equals(TEXT("uasset"), ESearchCase::IgnoreCase)
			|| Extension.Equals(TEXT("umap"), ESearchCase::IgnoreCase);
		if (!bIsAssetPackage)
		{
			return false;
		}

		return FPackageName::TryConvertFilenameToLongPackageName(InAbsPath, OutLongName);
	}

	/** Given an OFPA long name, derive the owning map's LONG package name. The OFPA long
	 *  name looks like <Mount>/__ExternalActors__/<MapPath...>/X/Y/<GUID> (the trailing
	 *  /X/Y/<GUID> are exactly three shard segments). Strip the external folder token, drop
	 *  the last three shard segments, and rebuild the mount-correct owning-map package name.
	 *  Returns false when the long name is not an OFPA layout. */
	bool DeriveOwningMapLongName(const FString& InLongName, FString& OutMapLongName)
	{
		const FString ExternalActorsFolder = FPackagePath::GetExternalActorsFolderName();
		const FString ExternalObjectsFolder = FPackagePath::GetExternalObjectsFolderName();
		const bool bExternalActor = InLongName.Contains(ExternalActorsFolder);
		const bool bExternalObject = InLongName.Contains(ExternalObjectsFolder);
		if (!bExternalActor && !bExternalObject)
		{
			return false;
		}

		const FString& ExternalFolder = bExternalActor ? ExternalActorsFolder : ExternalObjectsFolder;

		TArray<FString> Tokens;
		InLongName.ParseIntoArray(Tokens, TEXT("/"), /*InCullEmpty=*/true);

		int32 ExternalIndex = INDEX_NONE;
		for (int32 TokenIndex = 0; TokenIndex < Tokens.Num(); ++TokenIndex)
		{
			if (Tokens[TokenIndex] == ExternalFolder)
			{
				ExternalIndex = TokenIndex;
				break;
			}
		}

		if (ExternalIndex == INDEX_NONE)
		{
			return false;
		}

		TArray<FString> MountParts;
		for (int32 TokenIndex = 0; TokenIndex < ExternalIndex; ++TokenIndex)
		{
			MountParts.Add(Tokens[TokenIndex]);
		}

		TArray<FString> AfterParts;
		for (int32 TokenIndex = ExternalIndex + 1; TokenIndex < Tokens.Num(); ++TokenIndex)
		{
			AfterParts.Add(Tokens[TokenIndex]);
		}

		// Need at least 3 shard segments to strip AND one map-name segment.
		if (AfterParts.Num() < 4)
		{
			return false;
		}

		TArray<FString> MapParts;
		for (int32 PartIndex = 0; PartIndex < AfterParts.Num() - 3; ++PartIndex)
		{
			MapParts.Add(AfterParts[PartIndex]);
		}

		FString MapPackage = TEXT("/") + FString::Join(MountParts, TEXT("/"));
		MapPackage += TEXT("/") + FString::Join(MapParts, TEXT("/"));
		OutMapLongName = MapPackage;
		return true;
	}
}

namespace ArcLoreAssetDisplay
{
	bool SortKeyLess(
		const FString& NameKeyA, const FString& PathKeyA, const FString& TypeKeyA,
		const FString& NameKeyB, const FString& PathKeyB, const FString& TypeKeyB,
		EArcLoreSortKey Key,
		EColumnSortMode::Type Mode)
	{
		const FString* KeyA = &NameKeyA;
		const FString* KeyB = &NameKeyB;
		switch (Key)
		{
		case EArcLoreSortKey::Path:
			KeyA = &PathKeyA; KeyB = &PathKeyB; break;
		case EArcLoreSortKey::Type:
			KeyA = &TypeKeyA; KeyB = &TypeKeyB; break;
		case EArcLoreSortKey::Name:
		default:
			break;
		}

		const int32 Cmp = KeyA->Compare(*KeyB, ESearchCase::IgnoreCase);
		return (Mode == EColumnSortMode::Descending) ? (Cmp > 0) : (Cmp < 0);
	}

	FArcLoreDisplayInfo ResolveDisplayInfo(const FString& InAbsPath)
	{
		FArcLoreDisplayInfo Info;
		Info.Tooltip = InAbsPath;

		if (InAbsPath.IsEmpty())
		{
			Info.Primary = InAbsPath;
			return Info;
		}

		// Only true UE asset packages (.uasset/.umap) get extension-stripped friendly
		// names. ANY other file (config .ini, source, text) keeps its full filename WITH
		// the extension — the user expects "DefaultEngine.ini", not "DefaultEngine". Gate
		// on the actual extension, NOT on TryConvertFilenameToLongPackageName: a config
		// file under the project dir can still convert to a long name and would otherwise
		// fall into the plain-asset branch and be extension-stripped by GetShortName.
		const FString Extension = FPaths::GetExtension(InAbsPath);
		const bool bIsAssetPackage = Extension.Equals(TEXT("uasset"), ESearchCase::IgnoreCase)
			|| Extension.Equals(TEXT("umap"), ESearchCase::IgnoreCase);
		if (!bIsAssetPackage)
		{
			Info.Primary = FPaths::GetCleanFilename(InAbsPath);
			Info.Secondary = FPaths::GetCleanFilename(FPaths::GetPath(InAbsPath));
			return Info;
		}

		FString LongName;
		if (!FPackageName::TryConvertFilenameToLongPackageName(InAbsPath, LongName))
		{
			// An asset-extension file not under a mounted content root — keep the full
			// filename (with extension) and the full path in the tooltip.
			Info.Primary = FPaths::GetCleanFilename(InAbsPath);
			Info.Secondary = FPaths::GetCleanFilename(FPaths::GetPath(InAbsPath));
			return Info;
		}

		const FString ExternalActorsFolder = FPackagePath::GetExternalActorsFolderName();
		const FString ExternalObjectsFolder = FPackagePath::GetExternalObjectsFolderName();
		const bool bExternalActor = LongName.Contains(ExternalActorsFolder);
		const bool bExternalObject = LongName.Contains(ExternalObjectsFolder);

		// Default fallback for Primary; refined per-branch below.
		Info.Primary = FPaths::GetBaseFilename(InAbsPath);

		// ---- Map derivation (shared by the external-actor / external-object branches) ----
		FString MapShort;
		if (bExternalActor || bExternalObject)
		{
			FString MapLongName;
			if (ArcLoreAssetDisplay_Internal::DeriveOwningMapLongName(LongName, MapLongName))
			{
				MapShort = FPackageName::GetShortName(MapLongName);
			}
		}

#if WITH_EDITOR
		IAssetRegistry& AssetRegistry = IAssetRegistry::GetChecked();
#endif

		if (bExternalActor)
		{
#if WITH_EDITOR
			TArray<FAssetData> Assets;
			AssetRegistry.GetAssetsByPackageName(FName(*LongName), Assets, /*bIncludeOnlyDiskAssets=*/true);
			if (Assets.Num() > 0)
			{
				const FAssetData& AssetData = Assets[0];

				static const FName NAME_ActorLabel(TEXT("ActorLabel"));
				FString Label;
				if (!AssetData.GetTagValue(NAME_ActorLabel, Label) || Label.IsEmpty())
				{
					TUniquePtr<FWorldPartitionActorDesc> Desc =
						FWorldPartitionActorDescUtils::GetActorDescriptorFromAssetData(AssetData);
					if (Desc.IsValid())
					{
						Label = Desc->GetActorLabel().ToString();
					}
				}

				Info.Primary = Label.IsEmpty() ? FPaths::GetBaseFilename(InAbsPath) : Label;

				static const FName NAME_ActorMetaDataClass(TEXT("ActorMetaDataClass"));
				FString ClassPath;
				AssetData.GetTagValue(NAME_ActorMetaDataClass, ClassPath);
				if (!ClassPath.IsEmpty())
				{
					Info.Tooltip = FString::Printf(
						TEXT("%s\nClass: %s\nMap: %s\n%s"),
						*Info.Primary,
						*FPackageName::ObjectPathToObjectName(ClassPath),
						*MapShort,
						*InAbsPath);
				}
			}
			else
			{
				Info.Primary = FPaths::GetBaseFilename(InAbsPath);
			}
#else
			Info.Primary = FPaths::GetBaseFilename(InAbsPath);
#endif
			Info.Secondary = MapShort;
			return Info;
		}

		if (bExternalObject)
		{
#if WITH_EDITOR
			TArray<FAssetData> Assets;
			AssetRegistry.GetAssetsByPackageName(FName(*LongName), Assets, /*bIncludeOnlyDiskAssets=*/true);
			Info.Primary = Assets.Num() > 0
				? Assets[0].AssetName.ToString()
				: FPaths::GetBaseFilename(InAbsPath);
#else
			Info.Primary = FPaths::GetBaseFilename(InAbsPath);
#endif
			Info.Secondary = MapShort;
			return Info;
		}

		// ---- Plain asset branch ----
		Info.Primary = FPackageName::GetShortName(LongName);
		Info.Secondary = FPackageName::GetLongPackagePath(LongName);
		return Info;
	}

	EArcLoreFileKind ClassifyFile(const FString& InAbsPath)
	{
		FString LongName;
		if (!ArcLoreAssetDisplay_Internal::TryGetAssetLongName(InAbsPath, LongName))
		{
			return EArcLoreFileKind::NonAsset;
		}

		const FString ExternalActorsFolder = FPackagePath::GetExternalActorsFolderName();
		const FString ExternalObjectsFolder = FPackagePath::GetExternalObjectsFolderName();
		if (LongName.Contains(ExternalActorsFolder))
		{
			return EArcLoreFileKind::ExternalActor;
		}
		if (LongName.Contains(ExternalObjectsFolder))
		{
			return EArcLoreFileKind::ExternalObject;
		}
		return EArcLoreFileKind::PlainAsset;
	}

	bool TryGetOwningMapPackage(const FString& InAbsPath, FString& OutMapPackageName)
	{
		FString LongName;
		if (!ArcLoreAssetDisplay_Internal::TryGetAssetLongName(InAbsPath, LongName))
		{
			return false;
		}
		return ArcLoreAssetDisplay_Internal::DeriveOwningMapLongName(LongName, OutMapPackageName);
	}

	FString ResolveAssetTypeName(const FString& InAbsPath)
	{
		FString LongName;
		if (!ArcLoreAssetDisplay_Internal::TryGetAssetLongName(InAbsPath, LongName))
		{
			return FPaths::GetExtension(InAbsPath).ToUpper();
		}

#if WITH_EDITOR
		IAssetRegistry& AR = IAssetRegistry::GetChecked();
		TArray<FAssetData> Assets;
		AR.GetAssetsByPackageName(FName(*LongName), Assets, /*bIncludeOnlyOnDiskAssets=*/true);
		if (Assets.Num() > 0 && !Assets[0].AssetClassPath.GetAssetName().IsNone())
		{
			return Assets[0].AssetClassPath.GetAssetName().ToString();
		}
		return FPaths::GetExtension(InAbsPath).ToUpper();
#else
		return FPaths::GetExtension(InAbsPath).ToUpper();
#endif
	}

	void BrowseToFile(const FString& InAbsPath)
	{
#if WITH_EDITOR
		const EArcLoreFileKind Kind = ClassifyFile(InAbsPath);
		if (Kind == EArcLoreFileKind::NonAsset)
		{
			UE_LOGF(LogSourceControl, Verbose,"ArcLore: '%ls' is not an asset; cannot browse.", *InAbsPath);
			return;
		}

		// External actors/objects browse to the OWNING MAP package; plain assets to the
		// asset's own package.
		FString PackageName;
		if (Kind == EArcLoreFileKind::ExternalActor || Kind == EArcLoreFileKind::ExternalObject)
		{
			if (!TryGetOwningMapPackage(InAbsPath, PackageName))
			{
				UE_LOGF(LogSourceControl, Verbose,"ArcLore: cannot derive owning map for '%ls'; cannot browse.", *InAbsPath);
				return;
			}
		}
		else if (!ArcLoreAssetDisplay_Internal::TryGetAssetLongName(InAbsPath, PackageName))
		{
			UE_LOGF(LogSourceControl, Verbose,"ArcLore: '%ls' does not resolve to a content package; cannot browse.", *InAbsPath);
			return;
		}

		IAssetRegistry* AssetRegistry = IAssetRegistry::Get();
		if (AssetRegistry == nullptr)
		{
			UE_LOGF(LogSourceControl, Verbose,"ArcLore: AssetRegistry unavailable; cannot browse '%ls'.", *PackageName);
			return;
		}

		TArray<FAssetData> Assets;
		AssetRegistry->GetAssetsByPackageName(FName(*PackageName), Assets);
		if (Assets.Num() == 0)
		{
			UE_LOGF(LogSourceControl, Verbose,"ArcLore: no assets found for package '%ls'.", *PackageName);
			return;
		}

		FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
		ContentBrowserModule.Get().SyncBrowserToAssets(Assets);
#endif
	}

	void OpenFile(const FString& InAbsPath)
	{
#if WITH_EDITOR
		const EArcLoreFileKind Kind = ClassifyFile(InAbsPath);
		if (Kind == EArcLoreFileKind::NonAsset)
		{
			UE_LOGF(LogSourceControl, Verbose,"ArcLore: '%ls' is not an asset; cannot open.", *InAbsPath);
			return;
		}

		// External actors/objects open their OWNING MAP as a level.
		if (Kind == EArcLoreFileKind::ExternalActor || Kind == EArcLoreFileKind::ExternalObject)
		{
			FString MapPackage;
			if (!TryGetOwningMapPackage(InAbsPath, MapPackage))
			{
				UE_LOGF(LogSourceControl, Verbose,"ArcLore: cannot derive owning map for '%ls'; cannot open.", *InAbsPath);
				return;
			}

			FString MapFilename;
			if (!FPackageName::TryConvertLongPackageNameToFilename(MapPackage, MapFilename, FPackageName::GetMapPackageExtension()))
			{
				// Can't resolve the on-disk .umap — degrade to revealing it in the browser.
				UE_LOGF(LogSourceControl, Verbose,"ArcLore: cannot resolve .umap for '%ls'; browsing instead.", *MapPackage);
				BrowseToFile(InAbsPath);
				return;
			}

			FEditorFileUtils::LoadMap(MapFilename, /*LoadAsTemplate=*/false, /*bShowProgress=*/true);
			return;
		}

		// Plain asset: open in the asset editor by its long package name.
		FString LongName;
		if (!ArcLoreAssetDisplay_Internal::TryGetAssetLongName(InAbsPath, LongName))
		{
			UE_LOGF(LogSourceControl, Verbose,"ArcLore: '%ls' does not resolve to a content package; cannot open.", *InAbsPath);
			return;
		}

		if (GEditor == nullptr)
		{
			UE_LOGF(LogSourceControl, Verbose,"ArcLore: GEditor unavailable; cannot open '%ls'.", *LongName);
			return;
		}

		UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
		if (AssetEditorSubsystem == nullptr)
		{
			UE_LOGF(LogSourceControl, Verbose,"ArcLore: AssetEditorSubsystem unavailable; cannot open '%ls'.", *LongName);
			return;
		}

		// Resolve the package to the asset's object path, then open it. The FString
		// overload of OpenEditorForAsset loads the asset if needed.
		IAssetRegistry* AssetRegistry = IAssetRegistry::Get();
		if (AssetRegistry != nullptr)
		{
			TArray<FAssetData> Assets;
			AssetRegistry->GetAssetsByPackageName(FName(*LongName), Assets);
			if (Assets.Num() > 0)
			{
				AssetEditorSubsystem->OpenEditorForAsset(Assets[0].GetSoftObjectPath().ToString());
				return;
			}
		}

		// Fall back to the package name (the FString overload tolerates a package path).
		AssetEditorSubsystem->OpenEditorForAsset(LongName);
#endif
	}
}
