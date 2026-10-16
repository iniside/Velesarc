// Copyright Lukasz Baran. All Rights Reserved.

#include "Render/ArcTerrainAtlasPublish.h"

#include "ArcTerrainActor.h"
#include "ArcTerrainSubsystem.h"
#include "ArcTerrainTypes.h"
#include "Preview/ArcTerrainPreviewManager.h"
#include "Render/ArcTerrainAtlasTexture.h"
#include "Render/ArcTerrainTileAtlas.h"

#include "Engine/World.h"
#include "Misc/LargeWorldRenderPosition.h"
#include "PixelFormat.h"
#include "RenderingThread.h"
#include "RHIGlobals.h"

namespace ArcTerrainAtlasPublishPrivate_7c41
{
	int32 MaxSectorSideForMask()
	{
		return FMath::FloorToInt32(FMath::Sqrt(static_cast<float>(ArcTerrainPublishedMaxSlots)));
	}

	void SplitTerrainTransformLWC(const FTransform& TerrainTransform, FVector3f& OutLWCTile,
		FMatrix44f& OutLocalToWorld, FMatrix44f& OutWorldToLocal)
	{
		const FLargeWorldRenderPosition Origin(TerrainTransform.GetLocation());
		FTransform TileRelative = TerrainTransform;
		TileRelative.SetLocation(FVector(Origin.GetOffset()));

		const FMatrix LocalToWorld = TileRelative.ToMatrixWithScale();
		OutLWCTile = Origin.GetTile();
		OutLocalToWorld = FMatrix44f(LocalToWorld);
		OutWorldToLocal = FMatrix44f(LocalToWorld.Inverse());
	}
}

FArcTerrainAtlasSnapshot::FArcTerrainAtlasSnapshot()
{
	for (int32 SlotIndex = 0; SlotIndex < ArcTerrainPublishedMaxSlots; ++SlotIndex)
	{
		SliceAndOriginPerSlot[SlotIndex] = FVector4f(ForceInitToZero);
	}
}

bool FArcTerrainAtlasSnapshot::IsValid() const
{
	return SectorSide > 0
		&& SectorSide * SectorSide <= ArcTerrainPublishedMaxSlots
		&& CellSizeCm > 0.0f
		&& HeightQuads > 0
		&& HeightSliceSide > 0;
}

bool UArcTerrainSubsystem::BuildHeightAtlasSnapshot(const AArcTerrain& Terrain, FVector CentreWorldPos,
	FArcTerrainAtlasSnapshot& OutSnapshot) const
{
	using namespace ArcTerrainAtlasPublishPrivate_7c41;

	check(IsInGameThread());

	OutSnapshot = FArcTerrainAtlasSnapshot();

	const FArcTerrainState* State = Terrains.Find(&Terrain);
	if (State == nullptr || !State->TileAtlas.IsValid())
	{
		return false;
	}

	const FArcTerrainTileAtlas& Atlas = *State->TileAtlas;
	const FArcTerrainTileAtlasLayout& Layout = Atlas.GetLayout();
	const FArcTerrainConfig& Config = Terrain.Config;
	if (!Layout.IsValid() || !Atlas.GetResources().IsValid() || Config.CellSize() <= 0.0)
	{
		return false;
	}

	if (!State->CbtPublishCache.Rings.HeightTiers.IsValidIndex(0))
	{
		return false;
	}
	const int32 RingSide = State->CbtPublishCache.Rings.HeightTiers[0].GetSide();
	const int32 SectorSide = FMath::Min(RingSide, MaxSectorSideForMask());
	if (SectorSide <= 0)
	{
		return false;
	}

	SplitTerrainTransformLWC(Terrain.GetTerrainTransform(), OutSnapshot.LWCTile, OutSnapshot.LocalToWorld,
		OutSnapshot.WorldToLocal);

	const FVector CentreLocal = Terrain.GetTerrainTransform().InverseTransformPosition(CentreWorldPos);
	const FArcTerrainCellKey CentreCell = ArcTerrain::CellKeyFromLocal(CentreLocal, Config);
	const FIntPoint CentreSector = ArcTerrainAtlas::GroupKeyFromCell(CentreCell, Layout.SectorShift);

	OutSnapshot.OriginSector = FIntPoint(CentreSector.X - SectorSide / 2, CentreSector.Y - SectorSide / 2);
	OutSnapshot.SectorSide = SectorSide;
	OutSnapshot.CellSizeCm = static_cast<float>(Config.CellSize());
	OutSnapshot.SectorShift = Layout.SectorShift;
	OutSnapshot.HeightQuads = Layout.HeightQuads;
	OutSnapshot.HeightSliceSide = Layout.GetHeightSliceSide();
	OutSnapshot.FillSlice = Layout.GetHeightFillSlice();
	OutSnapshot.ResourceGeneration = Atlas.GetResourceGeneration();

	const int32 HeightArraySize = Layout.GetHeightArraySize();
	for (int32 SlotY = 0; SlotY < SectorSide; ++SlotY)
	{
		for (int32 SlotX = 0; SlotX < SectorSide; ++SlotX)
		{
			const FIntPoint SectorKey(OutSnapshot.OriginSector.X + SlotX, OutSnapshot.OriginSector.Y + SlotY);
			const FArcTerrainSectorId SectorId{ 0, SectorKey };
			int32 HeightSlice = INDEX_NONE;
			if (!Atlas.TryGetReadyHeightSlice(SectorId, HeightSlice) || HeightSlice >= HeightArraySize)
			{
				continue;
			}

			const int32 SlotIndex = SlotY * SectorSide + SlotX;
			const FIntPoint SectorTexelOrigin(
				(SectorKey.X << Layout.SectorShift) * Layout.HeightQuads,
				(SectorKey.Y << Layout.SectorShift) * Layout.HeightQuads);
			OutSnapshot.SliceAndOriginPerSlot[SlotIndex] = FVector4f(
				static_cast<float>(HeightSlice),
				static_cast<float>(SectorTexelOrigin.X),
				static_cast<float>(SectorTexelOrigin.Y),
				0.0f);
			OutSnapshot.ValidSlotMask |= 1u << static_cast<uint32>(SlotIndex);
		}
	}

	return OutSnapshot.IsValid();
}

UArcTerrainAtlasTexture* UArcTerrainSubsystem::GetHeightAtlasFacade(AArcTerrain& Terrain)
{
	check(IsInGameThread());

	return PreviewManager.GetHeightAtlasFacade(Terrain);
}

namespace ArcTerrainAtlasPublish
{
	bool BuildAtlasSnapshot(const AArcTerrain& Terrain, FVector CentreWorldPos,
		FArcTerrainAtlasSnapshot& OutSnapshot)
	{
		const UWorld* World = Terrain.GetWorld();
		const UArcTerrainSubsystem* Subsystem = World != nullptr
			? World->GetSubsystem<UArcTerrainSubsystem>() : nullptr;
		if (Subsystem == nullptr)
		{
			OutSnapshot = FArcTerrainAtlasSnapshot();
			return false;
		}
		return Subsystem->BuildHeightAtlasSnapshot(Terrain, CentreWorldPos, OutSnapshot);
	}

	UArcTerrainAtlasTexture* AcquireHeightAtlasFacade(AArcTerrain& Terrain)
	{
		UWorld* World = Terrain.GetWorld();
		UArcTerrainSubsystem* Subsystem = World != nullptr
			? World->GetSubsystem<UArcTerrainSubsystem>() : nullptr;
		return Subsystem != nullptr ? Subsystem->GetHeightAtlasFacade(Terrain) : nullptr;
	}

	bool IsHeightAtlasFilterable()
	{
		return EnumHasAllFlags(GPixelFormats[PF_R32_FLOAT].Capabilities,
			EPixelFormatCapabilities::TextureFilterable);
	}

	void PublishAtlasTextures(const TSharedPtr<FArcTerrainTileAtlasResources, ESPMode::ThreadSafe>& Resources,
		FArcTerrainAtlasTextureResource* HeightResource,
		TArray<FArcTerrainAtlasTextureResource*>&& WeightResources)
	{
		if (!Resources.IsValid())
		{
			return;
		}

		ENQUEUE_RENDER_COMMAND(ArcTerrainPublishAtlasTextures)(
			[Resources, HeightResource, WeightResources = MoveTemp(WeightResources)](
				FRHICommandListImmediate& RHICmdList)
			{
				if (HeightResource != nullptr)
				{
					HeightResource->SetAtlasRHI_RenderThread(RHICmdList,
						Resources->HeightAtlas.IsValid() ? Resources->HeightAtlas->GetRHI() : nullptr);
				}
				for (int32 ArrayIndex = 0; ArrayIndex < WeightResources.Num(); ++ArrayIndex)
				{
					if (WeightResources[ArrayIndex] == nullptr)
					{
						continue;
					}
					const bool bHasArray = Resources->WeightAtlases.IsValidIndex(ArrayIndex)
						&& Resources->WeightAtlases[ArrayIndex].IsValid();
					FRHITexture* const AtlasRHI = bHasArray
						? Resources->WeightAtlases[ArrayIndex]->GetRHI() : nullptr;
					WeightResources[ArrayIndex]->SetAtlasRHI_RenderThread(RHICmdList, AtlasRHI);
				}
			});
	}
}
