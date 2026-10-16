// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
// EColumnSortMode (namespaced enum) used by the shared sort comparator / SortFileItems below.
#include "Widgets/Views/SHeaderRow.h"
// FPaths used by the SortFileItems template body (header-defined → needs the type here).
#include "Misc/Paths.h"

/**
 * Shared display / open / browse utilities for on-disk Lore file paths, reused by the
 * Lore source-control panel and the Branch History panel. Everything lives in a SHARED
 * namespace (declared here, defined in the .cpp) because two translation units consume
 * it — a file-scoped namespace would not work across them.
 *
 * The resolver maps raw OFPA package filenames (the "__ExternalActors__" / "__ExternalObjects__"
 * GUID-named .uasset files) to a useful name + owning map, plain assets to a short name +
 * package folder, and any other file to its clean filename. The open / browse helpers act on
 * the SAME classification: external actors/objects target their OWNING MAP, plain assets the
 * asset itself, anything else is a Verbose no-op.
 */
namespace ArcLoreAssetDisplay
{
	/** Resolved, human-useful display strings for one on-disk file path. Primary is the
	 *  name shown in the Name column, Secondary the Path column, Tooltip the full path. */
	struct FArcLoreDisplayInfo
	{
		FString Primary;
		FString Secondary;
		FString Tooltip;
	};

	/** Broad classification of an on-disk file path, gated on the actual extension. Drives
	 *  both the resolver's per-branch logic and the open / browse helpers. */
	enum class EArcLoreFileKind : uint8
	{
		ExternalActor,
		ExternalObject,
		PlainAsset,
		NonAsset
	};

	/** Resolve a useful display name for an on-disk file path. External-actor OFPA
	 *  packages ("__ExternalActors__") resolve to the actor's label + owning map;
	 *  external-object packages ("__ExternalObjects__") to the asset short name + owning
	 *  map; plain assets to the asset short name + its package folder. The full on-disk
	 *  path is always returned as the tooltip. Any failed step falls back to the base
	 *  filename for Primary — never crashes. */
	FArcLoreDisplayInfo ResolveDisplayInfo(const FString& InAbsPath);

	/** Asset class short name (e.g. "WidgetBlueprint", "Texture2D") for an on-disk file path
	 *  WITHOUT loading the asset or its class. Non-asset / deleted / unmounted paths fall back to
	 *  the uppercased file extension (e.g. "INI"). Editor-only (AssetRegistry); returns "" if the
	 *  path has no extension and isn't a known asset. */
	FString ResolveAssetTypeName(const FString& InAbsPath);

	/** Classify a path into one of the four kinds (extension-gated, same logic the resolver
	 *  uses). .uasset/.umap containing "__ExternalActors__" => ExternalActor; containing
	 *  "__ExternalObjects__" => ExternalObject; other .uasset/.umap => PlainAsset; else
	 *  NonAsset. */
	EArcLoreFileKind ClassifyFile(const FString& InAbsPath);

	/** For an external-actor / external-object OFPA path, derive the owning MAP's long
	 *  package name (e.g. "/Game/Maps/MyMap"). Returns false for non-OFPA / unresolvable
	 *  paths (OutMapPackageName left untouched). */
	bool TryGetOwningMapPackage(const FString& InAbsPath, FString& OutMapPackageName);

	/** Navigate the Content Browser to the asset behind a path. External actors/objects
	 *  browse to the OWNING MAP asset; plain assets to the asset itself; NonAsset /
	 *  unresolved is a Verbose no-op. */
	void BrowseToFile(const FString& InAbsPath);

	/** Open the thing behind a path. External actors/objects open the OWNING MAP as a level;
	 *  plain assets open in the asset editor; NonAsset / unresolved is a Verbose no-op. If a
	 *  map .umap filename cannot be resolved, falls back to BrowseToFile. */
	void OpenFile(const FString& InAbsPath);

	/** Sort key shared by every file list (folds the per-list Name/Path/Type columns). Lives
	 *  here so both the Lore source-control panel and the Branch History panel consume one copy. */
	enum class EArcLoreSortKey : uint8 { Name, Path, Type };

	/** Compare two precomputed sort keys for the active key + direction.
	 *  Case-insensitive; ascending => (cmp<0) first. Pure (reads only the passed keys). */
	bool SortKeyLess(
		const FString& NameKeyA, const FString& PathKeyA, const FString& TypeKeyA,
		const FString& NameKeyB, const FString& PathKeyB, const FString& TypeKeyB,
		EArcLoreSortKey Key,
		EColumnSortMode::Type Mode);

	/** Sort an array of TSharedPtr<T> file items in place by the active sort key/mode.
	 *  Type keys are pre-resolved for EVERY item in a pre-pass (so the comparator below
	 *  is PURE — it never resolves inside Sort). GetAbs yields the on-disk path the
	 *  Name/Path keys derive from; the two ref-accessors expose the item's cached Type
	 *  fields so resolution is memoised across sorts/renders. */
	template <typename T>
	void SortFileItems(
		TArray<TSharedPtr<T>>& Items,
		EArcLoreSortKey Key,
		EColumnSortMode::Type Mode,
		TFunctionRef<FString(const T&)> GetAbs,
		TFunctionRef<FString& (T&)> GetTypeRef,
		TFunctionRef<bool& (T&)> GetTypeResolvedRef)
	{
		if (Mode == EColumnSortMode::None)
		{
			return;
		}

		// Pre-pass: resolve CachedType for every item so the Type key is populated.
		for (const TSharedPtr<T>& ItemPtr : Items)
		{
			T& Item = *ItemPtr;
			if (!GetTypeResolvedRef(Item))
			{
				GetTypeRef(Item) = ArcLoreAssetDisplay::ResolveAssetTypeName(GetAbs(Item));
				GetTypeResolvedRef(Item) = true;
			}
		}

		Items.Sort([&GetAbs, &GetTypeRef, Key, Mode](const TSharedPtr<T>& A, const TSharedPtr<T>& B) -> bool
		{
			const FString AbsA = GetAbs(*A);
			const FString AbsB = GetAbs(*B);
			return SortKeyLess(
				FPaths::GetBaseFilename(AbsA), FPaths::GetPath(AbsA), GetTypeRef(*A),
				FPaths::GetBaseFilename(AbsB), FPaths::GetPath(AbsB), GetTypeRef(*B),
				Key, Mode);
		});
	}
}
