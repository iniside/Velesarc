// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "ArcEcotopeSiteMetadata.generated.h"

enum class EArcEcotopeSiteField : uint8;

/**
 * Storage-independent scalar contract for one frozen ArcEcotope site.
 *
 * GridDimensions is the base-grid shape. Level L divides each base dimension
 * by 1 << L, rounding up, so trailing cells remain clipped to LocalBounds and
 * every base cell maps to exactly one cell at every supported level.
 */
USTRUCT(BlueprintType)
struct ARCECOTOPE_API FArcEcotopeSiteMetadata
{
	GENERATED_BODY()

	static constexpr uint8 MaxGridLevel = 8;

	/**
	 * Layout version, independent of SiteSnapshotHash. Current site writers stamp
	 * the current value; older values fail that gate.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Site")
	int32 SiteSchemaVersion = 1;

	/** The sampled volume bounds in actor-local coordinates. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Site")
	FBox LocalBounds = FBox(EForceInit::ForceInit);

	/** Cell edge length in actor-local centimeters. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Site")
	float CellSizeCm = 0.0f;

	/** Base-grid dimensions. Cells are indexed X + Y * GridDimensions.X. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Site")
	FIntPoint GridDimensions = FIntPoint::ZeroValue;

	/** Mip level of the simulation record grid. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Site")
	uint8 RecordGridLevel = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Site")
	uint8 MoistureLevel = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Site")
	uint8 FertilityLevel = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Site")
	uint8 SunLoadLevel = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Site")
	uint8 WindExposureLevel = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Site")
	uint8 SkyViewFactorLevel = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Site")
	uint8 TemperatureLevel = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Site")
	uint8 ModificationCutLevel = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Site")
	uint8 BareRockLevel = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Site")
	uint8 ChannelExclusionLevel = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Site")
	uint8 EstablishmentWearLevel = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Site")
	uint8 SubstrateMobilityLevel = 0;

	/** Canonical logical-site identity. */
	UPROPERTY(VisibleAnywhere, Category = "Site")
	uint64 SiteSnapshotHash = 0;

	/** Storage-independent identity of the finalized logical field stream. */
	UPROPERTY(VisibleAnywhere, Category = "Site")
	uint64 LogicalFieldHash = 0;

	static int32 GetCurrentSiteSchemaVersion();
	FIntPoint GetBaseDimensions() const { return GridDimensions; }
	uint8 GetFieldLevel(EArcEcotopeSiteField Field) const;

	int32 GetCellCount() const;
	bool TryGetCellIndex(int32 CellX, int32 CellY, int32& OutCellIndex) const;
	bool TryGetCellCoordinates(int32 CellIndex, FIntPoint& OutCellCoordinates) const;
	bool TryGetCellIndexForLocalPosition(const FVector2D& LocalPosition, int32& OutCellIndex) const;
	FVector GetCellCenterLocal(int32 CellX, int32 CellY) const;

	FIntPoint GetLevelDimensions(uint8 Level) const;
	int32 GetLevelCellCount(uint8 Level) const;
	double GetLevelCellSizeCm(uint8 Level) const;
	bool TryGetLevelCellIndex(const FVector2D& LocalPosition, uint8 Level, int32& OutIndex) const;
	FVector GetLevelCellCenterLocal(int32 LevelCellIndex, uint8 Level) const;
	bool TryGetLevelCellRect(
		int32 LevelCellIndex,
		uint8 Level,
		FVector2D& OutMinLocal,
		FVector2D& OutMaxLocal) const;

	/** Returns INDEX_NONE when either level is unusable. Coarse to fine selects the minimum-corner child. */
	int32 MapCellIndexBetweenLevels(int32 InIndex, uint8 FromLevel, uint8 ToLevel) const;

	/** Clamped against LocalBounds, so a coarse cell equals the sum of its children's areas. */
	double GetLevelCellAreaSquareCentimeters(int32 LevelCellIndex, uint8 Level) const;

	// Base-grid convenience names used by site consumers.
	bool TryGetBaseCellIndex(int32 CellX, int32 CellY, int32& OutCellIndex) const
	{
		return TryGetCellIndex(CellX, CellY, OutCellIndex);
	}

	bool TryGetBaseCellCoordinates(int32 CellIndex, FIntPoint& OutCellCoordinates) const
	{
		return TryGetCellCoordinates(CellIndex, OutCellCoordinates);
	}

	bool TryGetBaseCellIndexForLocalPosition(const FVector2D& LocalPosition, int32& OutCellIndex) const
	{
		return TryGetCellIndexForLocalPosition(LocalPosition, OutCellIndex);
	}
};
