// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "AI/Navigation/NavAgentSelector.h"
#include "AI/Navigation/NavigationInvokerPriority.h"
#include "ArcMassNavInvokerTypes.generated.h"

/** Per-entity fragment that drives Mass-owned navigation tile invoking. */
USTRUCT()
struct ARCMASS_API FMassNavInvokerFragment : public FMassFragment
{
	GENERATED_BODY()

	/** Radius within which nav tiles will be generated. */
	UPROPERTY(EditAnywhere, Category = "NavInvoker")
	float GenerationRadius = 3000.f;

	/** Radius beyond which nav tiles will be removed. Must be >= GenerationRadius. */
	UPROPERTY(EditAnywhere, Category = "NavInvoker")
	float RemovalRadius = 5000.f;

	/** Which navigation agents this invoker generates tiles for. */
	UPROPERTY(EditAnywhere, Category = "NavInvoker")
	FNavAgentSelector SupportedAgents;

	/** Priority used when dirtying tiles. */
	UPROPERTY(EditAnywhere, Category = "NavInvoker")
	ENavigationInvokerPriority Priority = ENavigationInvokerPriority::Default;

	/** Last coarse cell position used for dirty-tracking (runtime, not exposed). */
	FIntPoint LastCell = FIntPoint(INDEX_NONE, INDEX_NONE);

	/** Index into UArcMassNavInvokerSubsystem::InvokerSlots (-1 = unregistered). */
	int32 InvokerIndex = INDEX_NONE;

	/** Cached navmesh origin, set once at registration. Used by parallel processor. */
	FVector CachedNavmeshOrigin = FVector::ZeroVector;

	/** Cached tile dimension in world units, set once at registration. */
	float CachedTileDim = 0.f;
};

/** Lightweight invoker data stored per Mass entity. Mirrors FNavigationInvokerRaw fields
 *  but avoids the unexported constructor. */
struct FArcNavInvokerData
{
	FVector Location = FVector::ZeroVector;
	float RadiusMin = 0.f;
	float RadiusMax = 0.f;
	FNavAgentSelector SupportedAgents;
	ENavigationInvokerPriority Priority = ENavigationInvokerPriority::Default;
};

/** Pre-computed tile diff for a single invoker that crossed a cell boundary.
 *  Built by the parallel cell-detect processor, consumed by the signal-based apply processor. */
struct FArcNavTileDiffEntry
{
	int32 SlotIndex = INDEX_NONE;
	FVector NewLocation = FVector::ZeroVector;
	ENavigationInvokerPriority Priority = ENavigationInvokerPriority::Default;
	TArray<FIntPoint, TInlineAllocator<16>> TileGains;
	TArray<FIntPoint, TInlineAllocator<16>> TileLosses;
};

/** Tag placed on static entities whose invoker only needs to be registered once. */
USTRUCT()
struct ARCMASS_API FMassNavInvokerStaticTag : public FMassTag
{
	GENERATED_BODY()
};

namespace UE::ArcMass::Signals
{
	const FName NavInvokerCellChanged = FName(TEXT("NavInvokerCellChanged"));
}

namespace ArcNavInvoker
{
	/** Convert a world-space position to a coarse cell coordinate using CellSize. */
	inline FIntPoint WorldToCell(const FVector& Position, float CellSize)
	{
		return FIntPoint(
			FMath::FloorToInt32(Position.X / CellSize),
			FMath::FloorToInt32(Position.Y / CellSize)
		);
	}

	/**
	 * Iterate all navmesh tile grid coordinates that fall within Radius of Position.
	 * Mirrors the distance check used by ARecastNavMesh::UpdateActiveTiles:
	 *   RelativeLocation = NavmeshOrigin - Position
	 *   tile center = RelLoc + FVector(X*TileDim + TileDim/2, Y*TileDim + TileDim/2, 0)
	 *   tile is included if 2D distance from RelLoc to tile center < TileCenterHalfDiag + Radius
	 *
	 * @param Position        Invoker world-space location.
	 * @param Radius          Generation or removal radius.
	 * @param NavmeshOrigin   ARecastNavMesh::NavMeshOriginOffset (UE world space).
	 * @param TileDim         Tile size in world units (ARecastNavMesh::GetTileSizeUU()).
	 * @param Callback        Called with FIntPoint tile coords for every tile in radius.
	 */
	template<typename TCallback>
	void ForEachTileInRadius(const FVector& Position, float Radius, const FVector& NavmeshOrigin, float TileDim, TCallback&& Callback)
	{
		const FVector RelativeLocation = NavmeshOrigin - Position;
		const FVector::FReal TileCenterHalfDiag = TileDim * UE_SQRT_2 / 2.f;
		const FVector::FReal CheckDistSq = FMath::Square(TileCenterHalfDiag + Radius);

		const int32 MinTileX = FMath::FloorToInt32((RelativeLocation.X - Radius) / TileDim);
		const int32 MaxTileX = FMath::CeilToInt32((RelativeLocation.X + Radius) / TileDim);
		const int32 MinTileY = FMath::FloorToInt32((RelativeLocation.Y - Radius) / TileDim);
		const int32 MaxTileY = FMath::CeilToInt32((RelativeLocation.Y + Radius) / TileDim);

		for (int32 X = MinTileX; X <= MaxTileX; ++X)
		{
			for (int32 Y = MinTileY; Y <= MaxTileY; ++Y)
			{
				const FVector TileCenter(X * TileDim + TileDim / 2.f, Y * TileDim + TileDim / 2.f, 0.f);
				const FVector::FReal DistSq = (RelativeLocation - TileCenter).SizeSquared2D();
				if (DistSq < CheckDistSq)
				{
					Callback(FIntPoint(X, Y));
				}
			}
		}
	}

	/**
	 * Point-check whether a single tile coordinate falls within Radius of Position.
	 * Uses the same distance formula as ForEachTileInRadius but for a single tile.
	 */
	inline bool IsTileInRadius(
		const FVector& Position,
		float Radius,
		const FIntPoint& TileCoord,
		const FVector& NavmeshOrigin,
		float TileDim)
	{
		const FVector RelativeLocation = NavmeshOrigin - Position;
		const FVector::FReal TileCenterHalfDiag = TileDim * UE_SQRT_2 / 2.f;
		const FVector::FReal CheckDistSq = FMath::Square(TileCenterHalfDiag + Radius);
		const FVector TileCenter(
			TileCoord.X * TileDim + TileDim / 2.f,
			TileCoord.Y * TileDim + TileDim / 2.f, 0.f);
		return (RelativeLocation - TileCenter).SizeSquared2D() < CheckDistSq;
	}
} // namespace ArcNavInvoker
