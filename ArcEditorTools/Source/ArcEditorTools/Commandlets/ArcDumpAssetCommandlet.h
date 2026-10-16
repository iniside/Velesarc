// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "ArcDumpAssetCommandlet.generated.h"

/**
 * Headless asset-inspection commandlet. Writes the reflected state of a single
 * DataAsset / Blueprint (or a folder of them) to disk so the dump can be read
 * independently of the editor log spam.
 *
 * Invocation examples:
 *   UnrealEditor-Cmd.exe ArcGame.uproject -run=ArcDumpAsset \
 *     -AssetPath=/Game/Data/Items/DA_Sword -Out=g:/tmp/sword.json
 *
 *   UnrealEditor-Cmd.exe ArcGame.uproject -run=ArcDumpAsset \
 *     -Path=/Game/Data/Items -OutDir=g:/tmp/items
 *
 *   UnrealEditor-Cmd.exe ArcGame.uproject -run=ArcDumpAsset \
 *     -Path=/Game/Data/Items -List
 *
 * Flags:
 *   -AssetPath=<obj path>   single asset to dump
 *   -Path=<package path>    folder to scan (recursive by default)
 *   -Out=<file>             output file. With -Path, becomes JSONL.
 *   -OutDir=<dir>           output dir, one file per asset (only with -Path).
 *   -Format=json|text       json (default) or reflected key=value text.
 *   -List                   list eligible assets only, no dumping.
 *   -Force                  bypass DataAsset/Blueprint eligibility filter.
 *   -NoRecursive            disable folder recursion.
 *
 * Log lines are prefixed `[ArcDumpAsset]` so they can be grepped out of the
 * editor log: `ok <path> -> <outfile>`, `skip <path> <class>`, `fail <path> <reason>`.
 */
UCLASS()
class UArcDumpAssetCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UArcDumpAssetCommandlet();

	virtual int32 Main(const FString& Params) override;
};
