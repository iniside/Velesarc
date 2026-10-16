// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

namespace ArcMassConfigScan
{
	/**
	 * Enumerates every UArcMassEntityConfigAsset via the AssetRegistry, flags any asset
	 * whose FMassEntityConfig contains more than one trait of the same class (template-merge
	 * corruption), logs a Warning per offending asset, and writes a CSV report to the
	 * session scratchpad. Read-only — no assets are modified or saved.
	 */
	void RunDuplicateTraitScan();
} // namespace ArcMassConfigScan
