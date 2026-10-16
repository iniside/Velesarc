// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcTerrainTypes.h"
#include "DataMaps/ArcTerrainDataMapBake.h" // FArcTerrainDataMapWindow
#include "DataMaps/ArcTerrainDataMapTypes.h"

#if WITH_EDITOR

#include "IO/IoHash.h"

struct FArcTerrainState;
class UArcTerrainStampComponent;

namespace ArcTerrainDataMaps
{
	/** One (mask, modifier) pair contributing to a cell, reduced to the fields that decide the bake.
	 *  Gathered WITHOUT decoding any texture — the probe reads FTextureSource identity and dimensions on
	 *  the mask texture, the height texture and every weight layer, but decodes no pixels; it must stay
	 *  cheap enough to run per tick. */
	struct FArcTerrainDataMapContribution
	{
		EArcTerrainDataMap Map = EArcTerrainDataMap::Deposits;

		FString ModifierClassPath;

		FVector Translation = FVector::ZeroVector;
		FQuat Rotation = FQuat::Identity;
		FVector Scale = FVector::OneVector;

		FVector2D CoverageXY = FVector2D::ZeroVector;
		double Priority = 0.0;

		bool bBlendDataMaps = false;
		uint8 BlendMode = 0;

		/** FTextureSource::GetId(), or zero when the source is invalid — a re-import moves it, which is
		 *  the ONLY signal ArcTerrain gets that a mask texture's content changed. */
		FGuid ContentId;
		uint8 SourceChannel = 0;
		float EncodingScale = 1.0f;
		float ZeroInEncoding = 0.0f;
		FIntPoint SourceDims = FIntPoint::ZeroValue;

		/** Falloff mask fields off FArcStampSourceDesc — the bake multiplies every mask texel by them. */
		float EdgeFalloffBand = 0.0f;
		float EdgeSharpness = 1.0f;
		float CornerRadius = 0.0f;
		bool bCircleMask = false;
		FVector2D CircleCenterOffset = FVector2D::ZeroVector;
		float CircleRadius = 0.0f;
		float CircleFalloffBand = 0.0f;

		/** The source format decodes AND the dims are usable, so this contributor drives mask density. */
		bool bDrivesDensity = false;
	};

	/** Everything ComputeCellDataKey folds for one cell. */
	struct FArcTerrainDataMapKeyInputs
	{
		/** Stamp order (GatherSortedStamps), masks in EArcTerrainDataMap order within a stamp — the
		 *  fold order the bake uses, so a re-ordering changes the key. */
		TArray<FArcTerrainDataMapContribution> Contributions;

		/** Resolved quad count per contributing mask, ascending by mask. */
		TArray<TPair<EArcTerrainDataMap, int32>> MaskQuads;

		/** The TERRAIN-WIDE quantization windows the payload encodes through. Not derivable from
		 *  Contributions, which cover these bounds only: a modifier outside them widens the window and
		 *  therefore re-encodes every tile. */
		TArray<FArcTerrainDataMapWindow> MaskWindows;
	};

	/**
	 * Gather the contributing (mask, modifier) set from one tile's ordered stamp and terrain-window views.
	 * Empty Contributions means NO modifier assigns a mask there — the un-register condition, and exactly
	 * the case in which the bake emits no mask.
	 *
	 * GAME THREAD ONLY (FTextureSource access).
	 */
	ARCTERRAIN_API void GatherDataMapKeyInputsForStamps(
		const FArcTerrainConfig& Config,
		const FTransform& TerrainTransform,
		TConstArrayView<UArcTerrainStampComponent* const> SortedStamps,
		TConstArrayView<FArcTerrainDataMapWindow> MaskWindows,
		FArcTerrainDataMapKeyInputs& OutInputs);

	/** Content key of one cell's data section. Its own FormatVersionKey GUID — nothing about the mesh,
	 *  the weights or the composite enters it, and nothing here can invalidate them. */
	ARCTERRAIN_API FIoHash ComputeCellDataKey(FArcTerrainCellKey Key, const FArcTerrainConfig& Config,
		const FArcTerrainDataMapKeyInputs& Inputs);
}

#endif // WITH_EDITOR
