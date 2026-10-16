// Copyright Lukasz Baran. All Rights Reserved.

#include "Config/ArcMassConfigDuplicateScan.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/AssetData.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "MassEntityTraitBase.h"
#include "MassEntityConfigAsset.h"
#include "ArcMass/Config/ArcMassEntityConfigAsset.h"

namespace ArcMassConfigScan_Private
{
	static FAutoConsoleCommand GScanDuplicateTraitsCmd(
		TEXT("ArcMass.ScanConfigDuplicateTraits"),
		TEXT("Scan all UArcMassEntityConfigAsset assets for duplicate-class traits (template-merge corruption). "
		     "Read-only. Logs warnings and writes a CSV report."),
		FConsoleCommandDelegate::CreateStatic(&ArcMassConfigScan::RunDuplicateTraitScan)
	);
} // namespace ArcMassConfigScan_Private

namespace ArcMassConfigScan
{

void RunDuplicateTraitScan()
{
	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();

	// Wait for any in-flight scan to finish so we see the full asset set.
	AssetRegistry.SearchAllAssets(/*bSynchronousSearch=*/true);

	TArray<FAssetData> Assets;
	AssetRegistry.GetAssetsByClass(
		UArcMassEntityConfigAsset::StaticClass()->GetClassPathName(),
		Assets,
		/*bSearchSubClasses=*/true);

	int32 TotalScanned = Assets.Num();
	int32 AssetsWithDuplicates = 0;

	// CSV accumulator: header + one row per offending (asset, class) pair.
	FString CsvContent = TEXT("AssetPath,TraitClass,Count\n");

	for (const FAssetData& AssetData : Assets)
	{
		UArcMassEntityConfigAsset* ConfigAsset = Cast<UArcMassEntityConfigAsset>(AssetData.GetAsset());
		if (ConfigAsset == nullptr)
		{
			continue;
		}

		TConstArrayView<UMassEntityTraitBase*> Traits = ConfigAsset->GetConfig().GetTraits();

		// Count occurrences of each trait class on this asset.
		TMap<UClass*, int32> ClassCounts;
		ClassCounts.Reserve(Traits.Num());
		for (UMassEntityTraitBase* Trait : Traits)
		{
			if (Trait == nullptr)
			{
				continue;
			}
			int32& Count = ClassCounts.FindOrAdd(Trait->GetClass(), 0);
			++Count;
		}

		// Collect duplicates (count > 1).
		bool bAssetHasDuplicates = false;
		for (const TPair<UClass*, int32>& Pair : ClassCounts)
		{
			if (Pair.Value <= 1)
			{
				continue;
			}

			if (!bAssetHasDuplicates)
			{
				bAssetHasDuplicates = true;
				++AssetsWithDuplicates;
				UE_LOGF(LogTemp, Warning,"ArcMass.ScanConfigDuplicateTraits: duplicate traits in '%ls':",
					*AssetData.GetObjectPathString());
			}

			UE_LOGF(LogTemp, Warning,"  Class=%ls  Count=%d",
				*Pair.Key->GetPathName(),
				Pair.Value);

			CsvContent += FString::Printf(
				TEXT("%s,%s,%d\n"),
				*AssetData.GetObjectPathString(),
				*Pair.Key->GetPathName(),
				Pair.Value);
		}
	}

	// Write CSV to scratchpad.
	const FString CsvPath = TEXT("C:/Users/lukas/AppData/Local/Temp/claude/G--ArcGame/886cae05-3e3d-4795-abef-07b447c7b2ef/scratchpad/arcmass-config-dup-traits.csv");
	const bool bSaved = FFileHelper::SaveStringToFile(CsvContent, *CsvPath);
	if (bSaved)
	{
		UE_LOGF(LogTemp, Log,"ArcMass.ScanConfigDuplicateTraits: CSV written to '%ls'",
			*CsvPath);
	}
	else
	{
		UE_LOGF(LogTemp, Warning,"ArcMass.ScanConfigDuplicateTraits: failed to write CSV to '%ls'",
			*CsvPath);
	}

	UE_LOGF(LogTemp, Log,"ArcMass.ScanConfigDuplicateTraits: %d assets scanned, %d with duplicate-class traits.",
		TotalScanned,
		AssetsWithDuplicates);
}

} // namespace ArcMassConfigScan
