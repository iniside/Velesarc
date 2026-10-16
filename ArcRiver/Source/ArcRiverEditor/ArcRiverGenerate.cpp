// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcRiverGenerate.h"

#include "ArcRiverGeneratorComponent.h"
#include "ArcRiverModule.h"
#include "ArcTerrainActor.h"
#include "Build/ArcRiverWaterEmitter.h"
#include "Engine/Texture2D.h"
#include "Extraction/ArcRiverGeoreference.h"
#include "Extraction/ArcRiverNetworkExtraction.h"
#include "Framework/Notifications/NotificationManager.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopedSlowTask.h"
#include "Stamps/ArcTerrainTexturePatchModifier.h"
#include "Tiles/ArcTerrainSourceCache.h"
#include "WaterZoneActor.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "ArcRiverGenerate"

namespace ArcRiverGeneratePrivate
{
	bool ResolveModifier(
		UArcRiverGeneratorComponent& Generator,
		const UArcTerrainTexturePatchModifier*& OutModifier,
		FText& OutError)
	{
		AActor* const Owner = Generator.GetOwner();
		if (Owner == nullptr)
		{
			OutError = LOCTEXT("NoOwner", "The river generator component has no owning actor.");
			return false;
		}

		TArray<UArcTerrainTexturePatchModifier*> Found;
		Owner->GetComponents<UArcTerrainTexturePatchModifier>(Found);
		if (Found.Num() != 1)
		{
			if (Found.IsEmpty())
			{
				OutError = FText::Format(
					LOCTEXT("NoModifier",
						"Actor '{0}' carries no Terrain Texture Patch Modifier; add one holding the Gaea Rivers mask "
						"beside this generator."),
					FText::FromString(Owner->GetActorNameOrLabel()));
				return false;
			}

			TArray<FString> Names;
			Names.Reserve(Found.Num());
			for (const UArcTerrainTexturePatchModifier* const Modifier : Found)
			{
				Names.Add(Modifier->GetName());
			}

			OutError = FText::Format(
				LOCTEXT("AmbiguousModifier",
					"Actor '{0}' carries {1} Terrain Texture Patch Modifiers ({2}); ArcRiver cannot tell which one "
					"holds the Rivers mask. Move the generator onto an actor with exactly one."),
				FText::FromString(Owner->GetActorNameOrLabel()),
				FText::AsNumber(Found.Num()),
				FText::FromString(FString::Join(Names, TEXT(", "))));
			return false;
		}

		OutModifier = Found[0];
		return true;
	}

	bool ResolveSources(
		UArcRiverGeneratorComponent& Generator,
		const UArcTerrainTexturePatchModifier*& OutModifier,
		const AArcTerrain*& OutTerrain,
		FText& OutError)
	{
		if (!ResolveModifier(Generator, OutModifier, OutError))
		{
			return false;
		}

		OutTerrain = Generator.AffectedTerrain.LoadSynchronous();
		if (OutTerrain == nullptr)
		{
			OutError = LOCTEXT("NoTerrain", "AffectedTerrain must name the ArcTerrain actor the rivers are cut into.");
			return false;
		}

		return true;
	}

	bool ResolveWaterZone(UArcRiverGeneratorComponent& Generator, FText& OutError)
	{
		if (Generator.WaterZone.IsNull())
		{
			OutError = LOCTEXT("NoWaterZone",
				"This generator names no Water Zone. Press Create Water Zone first: the engine's own zone sits at the "
				"world origin and does not cover the stamp, so the rivers would register with a zone that renders no "
				"water over them.");
			return false;
		}

		if (Generator.WaterZone.LoadSynchronous() == nullptr)
		{
			OutError = FText::Format(
				LOCTEXT("StaleWaterZone",
					"The generator's Water Zone '{0}' no longer resolves to a live actor. Press Create Water Zone to "
					"place one on the stamp footprint."),
				FText::FromString(Generator.WaterZone.ToString()));
			return false;
		}

		return true;
	}

	bool DecodeMask(
		const UArcTerrainTexturePatchModifier& Modifier,
		TArray<float>& OutMask,
		FIntPoint& OutDims,
		FText& OutError)
	{
		const FArcTerrainDataMapSource& Rivers = Modifier.Rivers;
		if (Rivers.Texture == nullptr)
		{
			OutError = LOCTEXT("NoRiversTexture",
				"The source modifier carries no Rivers mask texture; assign the Gaea Rivers output to its Rivers data map.");
			return false;
		}

		const FTextureSource& Source = Rivers.Texture->Source;
		if (!Source.IsValid())
		{
			OutError = FText::Format(
				LOCTEXT("InvalidRiversSource", "The Rivers mask texture '{0}' carries no editor source data."),
				FText::FromString(Rivers.Texture->GetName()));
			return false;
		}
		if (!FArcTerrainSourceCache::IsDecodableSourceFormat(Source.GetFormat()))
		{
			OutError = FText::Format(
				LOCTEXT("UndecodableRiversSource",
					"The Rivers mask texture '{0}' is stored in a source format the terrain channel decode cannot read. "
					"Re-import it as an 8-bit or 16-bit grayscale or RGBA image."),
				FText::FromString(Rivers.Texture->GetName()));
			return false;
		}

		const int64 SourceSizeX = Source.GetSizeX();
		const int64 SourceSizeY = Source.GetSizeY();
		if (SourceSizeX < 2 || SourceSizeY < 2)
		{
			OutError = LOCTEXT("MaskTooSmall", "The Rivers mask must be at least 2 texels on each side.");
			return false;
		}
		if (SourceSizeX > static_cast<int64>(ArcRiverNetworkExtraction::MaxMaskDim)
			|| SourceSizeY > static_cast<int64>(ArcRiverNetworkExtraction::MaxMaskDim))
		{
			OutError = FText::Format(
				LOCTEXT("MaskTooLarge",
					"The Rivers mask texture '{0}' is {1}x{2}, above the {3} per-side limit. Re-export it at or below "
					"{3} texels per side; decoding it would cost gigabytes on the game thread."),
				FText::FromString(Rivers.Texture->GetName()),
				FText::AsNumber(SourceSizeX),
				FText::AsNumber(SourceSizeY),
				FText::AsNumber(ArcRiverNetworkExtraction::MaxMaskDim));
			return false;
		}

		FArcTerrainSourceCache Cache;
		const FArcTerrainSourceCpuFloatsPtr Floats =
			Cache.GetOrCreateCpuFloatsFromTexture(*Rivers.Texture, Rivers.SourceChannel, Source.GetId());
		if (!Floats.IsValid())
		{
			OutError = FText::Format(
				LOCTEXT("RiversDecodeFailed", "The Rivers mask texture '{0}' could not be decoded to floats."),
				FText::FromString(Rivers.Texture->GetName()));
			return false;
		}

		OutDims = Floats->Dims;
		OutMask = Floats->Values;
		for (float& Value : OutMask)
		{
			Value = (Value - Rivers.ZeroInEncoding) * Rivers.EncodingScale;
		}
		return true;
	}

	void Notify(const FText& Message, const SNotificationItem::ECompletionState State)
	{
		FNotificationInfo Info(Message);
		Info.ExpireDuration = 10.0f;

		const TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Info);
		if (Item.IsValid())
		{
			Item->SetCompletionState(State);
		}
	}

	void NotifyCleared(const int32 NumRemoved, const FText& Scope)
	{
		Notify(
			FText::Format(
				LOCTEXT("ClearSucceeded",
					"ArcRiver removed {0} water body river(s) {1}. Only loaded World Partition cells were touched: "
					"rivers in streamed-out cells remain until those cells are loaded and Clear is run again."),
				FText::AsNumber(NumRemoved),
				Scope),
			SNotificationItem::CS_Success);
	}

	bool RunGenerate(UArcRiverGeneratorComponent& Generator, int32& OutSpawnedCount, FText& OutError)
	{
		OutSpawnedCount = 0;

		const UArcTerrainTexturePatchModifier* Modifier = nullptr;
		const AArcTerrain* Terrain = nullptr;
		if (!ResolveSources(Generator, Modifier, Terrain, OutError))
		{
			return false;
		}
		if (!ResolveWaterZone(Generator, OutError))
		{
			return false;
		}

		FScopedSlowTask SlowTask(4.0f, LOCTEXT("GeneratingRivers", "Generating rivers from the Rivers mask"));
		SlowTask.MakeDialog(/*bShowCancelButton=*/true);

		SlowTask.EnterProgressFrame(1.0f, LOCTEXT("DecodingMask", "Decoding the Rivers mask"));
		TArray<float> Mask;
		FIntPoint Dims = FIntPoint::ZeroValue;
		if (!DecodeMask(*Modifier, Mask, Dims, OutError))
		{
			return false;
		}

		const FVector2D CoverageXY = Modifier->CoverageXY;
		if (!(CoverageXY.X > 0.0) || !(CoverageXY.Y > 0.0))
		{
			OutError = LOCTEXT("BadCoverage", "The source modifier's CoverageXY must be positive on both axes.");
			return false;
		}

		const double CmPerTexel = ArcRiverGeoreference::ComputeCmPerTexel(*Modifier, Dims);
		if (!(CmPerTexel > 0.0))
		{
			OutError = LOCTEXT("BadTexelScale", "The Rivers mask footprint resolves a non-positive centimetres-per-texel.");
			return false;
		}

		// 8-connected skeleton stairs sit ≤0.5 texel off the diagonal; a world-cm tolerance below one texel keeps them.
		const float SimplifyToleranceTexels = FMath::Max(
			static_cast<float>(static_cast<double>(Generator.SimplifyTolerance) / CmPerTexel),
			1.5f);
		const int32 MinChainTexels = FMath::Max(
			2, FMath::RoundToInt32(static_cast<double>(Generator.MinRiverLength) / CmPerTexel));
		const float MinHalfWidthTexels =
			static_cast<float>(static_cast<double>(Generator.MinHalfWidth) / CmPerTexel);

		UE_CLOGF(MinHalfWidthTexels <= 1.0f, LogArcRiver, Warning,
			"MinHalfWidth=%.1fcm is %.2f mask texels at %.2f cm per texel, and every water texel already sits at "
			"least one texel from a bank, so the width filter removes nothing. Raise MinHalfWidth above %.0fcm to "
			"drop one-texel-wide threads, or re-export the mask at a finer texel size.",
			Generator.MinHalfWidth, MinHalfWidthTexels, CmPerTexel, CmPerTexel);

		SlowTask.EnterProgressFrame(1.0f, LOCTEXT("ExtractingChains", "Extracting river centrelines"));
		TArray<ArcRiverNetworkExtraction::FArcRiverChain> Chains;
		TArray<FIntPoint> JunctionTexels;
		ArcRiverNetworkExtraction::FArcRiverExtractionStats ExtractStats;
		FText ExtractError;
		ArcRiverNetworkExtraction::ExtractChains(
			Mask, Dims, Generator.ChannelThreshold, MinHalfWidthTexels, SimplifyToleranceTexels, MinChainTexels,
			Chains, JunctionTexels, ExtractStats, ExtractError,
			[&SlowTask](float /*Progress01*/) { return !SlowTask.ShouldCancel(); });

		if (SlowTask.ShouldCancel())
		{
			OutError = LOCTEXT("CancelledExtracting", "River generation was cancelled; nothing was placed.");
			return false;
		}
		if (!ExtractError.IsEmpty())
		{
			OutError = ExtractError;
			return false;
		}

		UE_LOGF(LogArcRiver, Log,
			"River extraction: mask %dx%d, CmPerTexel=%.4f, MinRiverLength=%.1fcm -> MinChainTexels=%d, "
			"SimplifyTolerance=%.1fcm -> SimplifyToleranceTexels=%.2f, MinHalfWidth=%.1fcm -> "
			"MinHalfWidthTexels=%.2f, water texels=%d -> %d after the width filter, skeleton texels=%d, "
			"chains before prune=%d, chains after prune=%d in %d pass(es), junctions=%d, stems=%d",
			Dims.X, Dims.Y, CmPerTexel, Generator.MinRiverLength, MinChainTexels, Generator.SimplifyTolerance,
			SimplifyToleranceTexels, Generator.MinHalfWidth, MinHalfWidthTexels, ExtractStats.WaterTexelCount,
			ExtractStats.WaterTexelsAfterWidthFilter, ExtractStats.SkeletonTexelCount, ExtractStats.ChainsBeforePrune,
			ExtractStats.ChainsAfterPrune, ExtractStats.PrunePasses, JunctionTexels.Num(),
			ExtractStats.StemCount);

		if (Chains.IsEmpty())
		{
			OutError = LOCTEXT("NoChains",
				"No river centreline survived the prune. Lower MinRiverLength, adjust ChannelThreshold, or lower "
				"MinHalfWidth.");
			return false;
		}

		if (Chains.Num() > Generator.MaxRivers)
		{
			UE_LOGF(LogArcRiver, Error,
				"The Rivers mask yields %d river(s), above MaxRivers=%d, at ChannelThreshold=%.3f, "
				"MinRiverLength=%.1fcm and MinHalfWidth=%.1fcm. Nothing was spawned.",
				Chains.Num(), Generator.MaxRivers, Generator.ChannelThreshold, Generator.MinRiverLength,
				Generator.MinHalfWidth);
			OutError = FText::Format(
				LOCTEXT("TooManyRivers",
					"The Rivers mask yields {0} rivers, above the {1} MaxRivers cap, and nothing was spawned. At "
					"ChannelThreshold {2} the mask is frayed into that many separate channels: raise ChannelThreshold "
					"to thin the fringe, raise MinHalfWidth ({3} cm) to drop the narrow threads, or raise "
					"MinRiverLength ({4} cm) to drop the short ones."),
				FText::AsNumber(Chains.Num()),
				FText::AsNumber(Generator.MaxRivers),
				FText::AsNumber(Generator.ChannelThreshold),
				FText::AsNumber(Generator.MinHalfWidth),
				FText::AsNumber(Generator.MinRiverLength));
			return false;
		}

		SlowTask.EnterProgressFrame(1.0f, LOCTEXT("Georeferencing", "Solving river heights"));
		ArcRiverGeoreference::FGeoreferenceInput GeoInput;
		GeoInput.Modifier = Modifier;
		GeoInput.Terrain = Terrain;
		GeoInput.MaskDims = Dims;
		GeoInput.WidthToDepthRatio = Generator.WidthToDepthRatio;

		TArray<ArcRiverGeoreference::FArcRiverPolyline> Polylines;
		if (!ArcRiverGeoreference::BuildPolylines(Chains, JunctionTexels, GeoInput, Polylines, OutError))
		{
			return false;
		}

		double MinDiveCm = TNumericLimits<double>::Max();
		double MaxDiveCm = TNumericLimits<double>::Lowest();
		double DiveSumCm = 0.0;
		int32 DiveSamples = 0;
		for (const ArcRiverGeoreference::FArcRiverPolyline& Polyline : Polylines)
		{
			const int32 NumPoints = Polyline.WorldPoints.Num();
			if (NumPoints != Polyline.GroundZ.Num())
			{
				continue;
			}
			for (int32 PointIndex = 0; PointIndex < NumPoints; ++PointIndex)
			{
				const double DiveCm =
					static_cast<double>(Polyline.GroundZ[PointIndex]) - Polyline.WorldPoints[PointIndex].Z;
				MinDiveCm = FMath::Min(MinDiveCm, DiveCm);
				MaxDiveCm = FMath::Max(MaxDiveCm, DiveCm);
				DiveSumCm += DiveCm;
				++DiveSamples;
			}
		}
		if (DiveSamples > 0)
		{
			UE_LOGF(LogArcRiver, Log,"River georeference: %d polyline(s), GroundZ-WorldZ cm min=%.1f max=%.1f mean=%.1f",
				Polylines.Num(), MinDiveCm, MaxDiveCm, DiveSumCm / static_cast<double>(DiveSamples));
		}
		else
		{
			UE_LOGF(LogArcRiver, Log,"River georeference: %d polyline(s)", Polylines.Num());
		}

		if (SlowTask.ShouldCancel())
		{
			OutError = LOCTEXT("CancelledSolving", "River generation was cancelled; nothing was placed.");
			return false;
		}

		SlowTask.EnterProgressFrame(1.0f, LOCTEXT("SpawningWater", "Spawning the water bodies"));
		if (!ArcRiverWaterEmitter::SpawnWaterBodies(
			Generator, Polylines, [&SlowTask]() { return !SlowTask.ShouldCancel(); }, OutSpawnedCount, OutError))
		{
			ArcRiverWaterEmitter::DestroyWaterBodies(Generator);
			return false;
		}

		return true;
	}
}

bool ArcRiverGenerate::CreateWaterZone(UArcRiverGeneratorComponent& Generator, FText& OutError)
{
	using namespace ArcRiverGeneratePrivate;

	const UArcTerrainTexturePatchModifier* Modifier = nullptr;
	if (!ResolveModifier(Generator, Modifier, OutError)
		|| !ArcRiverWaterEmitter::CreateOrUpdateWaterZone(Generator, *Modifier, OutError))
	{
		Notify(OutError, SNotificationItem::CS_Fail);
		return false;
	}

	const AWaterZone* const PlacedZone = Generator.WaterZone.Get();
	Notify(
		FText::Format(
			LOCTEXT("WaterZoneSucceeded",
				"ArcRiver fitted water zone '{0}' to the stamp footprint. SAVE THE LEVEL, and regenerate the rivers so "
				"they register with it."),
			FText::FromString(PlacedZone != nullptr ? PlacedZone->GetActorNameOrLabel() : FString())),
		SNotificationItem::CS_Success);
	return true;
}

bool ArcRiverGenerate::GenerateRivers(UArcRiverGeneratorComponent& Generator, FText& OutError)
{
	using namespace ArcRiverGeneratePrivate;

	int32 SpawnedCount = 0;
	if (!RunGenerate(Generator, SpawnedCount, OutError))
	{
		Notify(OutError, SNotificationItem::CS_Fail);
		return false;
	}

	Notify(
		FText::Format(
			LOCTEXT("GenerateSucceeded",
				"ArcRiver placed {0} river(s), replacing only what this generator had placed in loaded World "
				"Partition cells. SAVE THE LEVEL: the generated water spline meshes only persist through the level "
				"package, and nothing regenerates them on load."),
			FText::AsNumber(SpawnedCount)),
		SNotificationItem::CS_Success);
	return true;
}

bool ArcRiverGenerate::ClearRivers(UArcRiverGeneratorComponent& Generator, FText& /*OutError*/)
{
	using namespace ArcRiverGeneratePrivate;

	NotifyCleared(
		ArcRiverWaterEmitter::DestroyWaterBodies(Generator),
		LOCTEXT("ClearScope", "placed by this generator"));
	return true;
}

bool ArcRiverGenerate::ClearAllRivers(UArcRiverGeneratorComponent& Generator, FText& /*OutError*/)
{
	using namespace ArcRiverGeneratePrivate;

	NotifyCleared(
		ArcRiverWaterEmitter::DestroyAllWaterBodies(Generator),
		LOCTEXT("ClearAllScope", "in the level, whatever placed them"));
	return true;
}

#undef LOCTEXT_NAMESPACE
