// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FJsonObject;
class UArcTQSQueryDefinition;

namespace ArcTQSDefinitionJson
{
	ARCTARGETQUERYEDITOR_API TSharedRef<FJsonObject> ListNodeTypes(const FString& Role, const FString& Filter);

	ARCTARGETQUERYEDITOR_API TSharedRef<FJsonObject> ExportDefinition(const UArcTQSQueryDefinition& Definition);
	ARCTARGETQUERYEDITOR_API TSharedRef<FJsonObject> ExportAsset(const FString& AssetPath);

	ARCTARGETQUERYEDITOR_API TSharedRef<FJsonObject> ApplyToAsset(const FString& AssetPath, const TSharedRef<FJsonObject>& Json);

	ARCTARGETQUERYEDITOR_API TSharedRef<FJsonObject> CompileDefinition(UArcTQSQueryDefinition& Definition);
	ARCTARGETQUERYEDITOR_API TSharedRef<FJsonObject> CompileAsset(const FString& AssetPath);

	ARCTARGETQUERYEDITOR_API TSharedRef<FJsonObject> SetQueryReference(
		const FString& ObjectPath,
		const FString& PropertyName,
		const FString& DefinitionPath,
		const TSharedPtr<FJsonObject>& Parameters);
}
