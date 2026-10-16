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

#pragma once

#include "ToolsetRegistry/UToolsetRegistry.h"

#include "ArcFontToolset.generated.h"

/**
 * Font MCP Toolset — exposes .ttf/.otf import and runtime (Slate) composite-font
 * authoring on the ModelContextProtocol server, so an agent can create UMG-ready
 * fonts headlessly. The generic AssetTools toolset has no font import path, and a
 * usable UMG font needs a UFont with a typeface table mapping named weights to
 * UFontFace assets — which is fiddly to assemble by hand.
 *
 * All methods return JSON: {"success":true,"asset":"<object path>"} on success,
 * or {"error":"..."} on failure.
 */
UCLASS()
class UArcFontToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	/**
	 * Import a single .ttf/.otf font file into a UFontFace asset (forces
	 * UFontFileImportFactory so the import is deterministic and automated).
	 *
	 * SourceFile:  absolute disk path to the .ttf/.otf source.
	 * PackagePath: destination content folder (e.g. "/ArcRPG/UI/Fonts").
	 * AssetName:   name for the created face asset (e.g. "FontFace_PlexSans_Regular").
	 *
	 * Overwrites any existing asset of that name and saves the package.
	 */
	UFUNCTION(meta = (AICallable))
	static FString ImportFontFace(const FString& SourceFile, const FString& PackagePath, const FString& AssetName);

	/**
	 * Build a runtime (Slate) UFont whose default typeface maps named weights to
	 * already-imported UFontFace assets — the asset a UMG TextBlock's Font Family
	 * picker expects, with each entry selectable via TypefaceFontName.
	 *
	 * PackagePath:   destination content folder (e.g. "/ArcRPG/UI/Fonts").
	 * AssetName:     name for the created UFont (e.g. "Font_IBMPlexSans").
	 * FontFacePaths: object paths of the UFontFace assets, in order.
	 * TypefaceNames: the typeface entry name for each face (e.g. "Regular","Medium","SemiBold").
	 *                Must be the same length as FontFacePaths.
	 *
	 * Recreates the font if one already exists at the path, and saves the package.
	 */
	UFUNCTION(meta = (AICallable))
	static FString CreateRuntimeFont(const FString& PackagePath, const FString& AssetName,
		const TArray<FString>& FontFacePaths, const TArray<FString>& TypefaceNames);

	/**
	 * One-call convenience: import each source file as a UFontFace, then assemble a
	 * runtime UFont from them. Face assets are named "FontFace_<AssetName>_<TypefaceName>"
	 * (with the "Font_" prefix stripped from AssetName if present).
	 *
	 * PackagePath:   destination content folder (e.g. "/ArcRPG/UI/Fonts").
	 * AssetName:     name for the created UFont (e.g. "Font_IBMPlexSans").
	 * SourceFiles:   absolute disk paths to the .ttf/.otf sources, in order.
	 * TypefaceNames: the typeface entry name for each source. Same length as SourceFiles.
	 */
	UFUNCTION(meta = (AICallable))
	static FString ImportFontFamily(const FString& PackagePath, const FString& AssetName,
		const TArray<FString>& SourceFiles, const TArray<FString>& TypefaceNames);
};
