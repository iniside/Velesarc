// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FJsonObject;
class UArcTacticsConfig;

namespace ArcTacticsConfigJson
{
	ARCTACTICSEDITOR_API TSharedRef<FJsonObject> ListNodeTypes(const FString& Role, const FString& Filter);

	ARCTACTICSEDITOR_API TSharedRef<FJsonObject> ExportConfig(const UArcTacticsConfig& Config);
	ARCTACTICSEDITOR_API TSharedRef<FJsonObject> ExportAsset(const FString& AssetPath);

	ARCTACTICSEDITOR_API TSharedRef<FJsonObject> ApplyToConfig(UArcTacticsConfig& Config, const TSharedRef<FJsonObject>& Json);
	ARCTACTICSEDITOR_API TSharedRef<FJsonObject> ApplyToAsset(const FString& AssetPath, const TSharedRef<FJsonObject>& Json);

	ARCTACTICSEDITOR_API TSharedRef<FJsonObject> CompileConfig(UArcTacticsConfig& Config);
	ARCTACTICSEDITOR_API TSharedRef<FJsonObject> CompileAsset(const FString& AssetPath);
}
