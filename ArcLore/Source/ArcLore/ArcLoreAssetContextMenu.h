// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

namespace ArcLoreAssetContextMenu
{
	/** Register the Lock/Unlock entries under "ContentBrowser.AssetContextMenu".
	 *  Call from inside a FToolMenuOwnerScoped block — teardown is handled by the
	 *  module's existing UToolMenus::UnregisterOwner(this) in ShutdownModule. */
	void Register();
}
