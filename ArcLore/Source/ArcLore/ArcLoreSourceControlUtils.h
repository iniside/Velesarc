// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ArcLoreSourceControlRevision.h"

class FArcLoreSourceControlState;

class FArcLoreSourceControlCommand;

namespace ArcLoreSourceControlUtils
{

/** Walk parent directories starting at InStartDir until one containing a ".lore" subdirectory is found (the Lore working-copy root). */
bool FindLoreRepositoryRoot(const FString& InStartDir, FString& OutRoot);

/** Minimal line-scan of <root>/.lore/config.toml for the "identity" and "remote_url" scalar keys (no TOML library). Robust to missing keys. */
bool ParseLoreConfig(const FString& InConfigPath, FString& OutIdentity, FString& OutRemoteUrl);

}
