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

#include "ArcSvgToolset.generated.h"

/**
 * ArcMarkupUI SVG MCP Toolset — exposes .svg import via UArcSvgAssetFactory on the
 * ModelContextProtocol server, so an agent can create UArcSvgAsset assets headlessly
 * (the engine TextureFactory rejects .svg, and the SVG factory has no other automated entry point).
 */
UCLASS()
class UArcSvgToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	/**
	 * Import an .svg file from disk into a UArcSvgAsset via UArcSvgAssetFactory.
	 *
	 * SourceFile:  absolute disk path to the .svg source.
	 * PackagePath: destination content folder (e.g. "/ArcRPG/UI/Icons/SVG").
	 * AssetName:   name for the created asset (e.g. "SVG_Backpack").
	 *
	 * Overwrites any existing asset of that name and saves the package.
	 * Returns JSON: {"success":true,"asset":"<object path>"} on success, {"error":"..."} on failure.
	 */
	UFUNCTION(meta = (AICallable))
	static FString ImportSvgAsset(const FString& SourceFile, const FString& PackagePath, const FString& AssetName);
};
