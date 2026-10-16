// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTerrainSubsystem.h"
#include "ArcTerrainActor.h"
#include "ArcTerrainChannelSet.h"
#include "ArcTerrainDebugDraw.h"
#include "ArcTerrainDiagnostics.h"
#include "ArcTerrainSettings.h"
#include "ArcTerrainInvokers.h"
#include "ArcTerrainModule.h"
#include "Collision/ArcTerrainCollisionBuilder.h"
#include "Collision/ArcTerrainCollisionComponent.h"
#include "Collision/ArcTerrainHeightfieldRef.h"
#include "Composite/ArcTerrainCompositeShaders.h"
#include "Composite/ArcTerrainCpuComposite.h"
#include "DataMaps/ArcTerrainDataMapSample.h"
#include "Lighting/ArcTerrainFarOcclusionComponent.h"
#include "Lighting/ArcTerrainLightingComponent.h"
#include "Render/ArcTerrainAtlasPrepare.h"
#include "Runtime/ArcTerrainRuntimeWorkCoordinator.h"
#include "Render/ArcTerrainCBTRenderer.h"
#include "Render/ArcTerrainCellMath.h"
#include "Render/ArcTerrainDensityTiers.h"
#include "Render/ArcTerrainHeightBounds.h"
#include "Render/ArcTerrainMaterialBindings.h"
#include "Render/ArcTerrainRenderComponent.h"
#include "Render/ArcTerrainVertexFactory.h"
#include "Stamps/ArcTerrainMeshModifier.h"
#include "Stamps/ArcTerrainStampComponent.h"
#include "Surface/ArcTerrainSurfaceSample.h"
#include "Weights/ArcTerrainWeightSlicePacking.h" // GArcTerrainWeightMaxSlices (atlas slice budget)
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h" // FTextureSource dims (native density resolve) + CPU source cache
#include "Engine/Texture2DArray.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "MaterialDomain.h"
#include "MaterialShared.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/App.h"
#include "Misc/Optional.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "PooledRenderTarget.h"
#include "RHIGPUReadback.h"
#include "RHIGlobals.h"
#include "RenderResource.h"
#include "RenderUtils.h"
#include "RenderingThread.h"
#include "TextureResource.h"
#include "Tasks/Task.h"
#include "Templates/Function.h"
#include "UObject/ObjectKey.h"

#if WITH_EDITOR
#include "ArcTerrainTileBake.h" // tile lattice + ComputeTileCacheKey
#include "ArcTerrainTileBlobs.h" // the three rails' DDC payloads
#include "ArcTerrainTileCache.h" // async DDC probe handle polling (height + weight bake rails)
#include "ArcTerrainTileIO.h"
#include "ArcTerrainTileNames.h"
#include "Async/Async.h"
#include "TimerManager.h"
#include "DataMaps/ArcTerrainDataMapBake.h"
#include "DataMaps/ArcTerrainDataMapKey.h"
#include "Editor.h"
#include "Engine/Selection.h"
#include "Stamps/ArcTerrainTexturePatchModifier.h"
#include "Subsystems/ImportSubsystem.h"
#include "Weights/ArcTerrainWeightBuilder.h" // FArcTerrainWeightPlane + ComputeCellWeightKey
#include "UObject/UnrealType.h" // FPropertyChangedEvent + GET_MEMBER_NAME_CHECKED (settings-changed handler)
#endif

// FILE-SCOPED and unique (CLAUDE.md C++ Code Style) — nothing here may gain external linkage.
namespace ArcTerrainFarBlockRule_3e07
{
	// Floor division, not truncation: cells west/south of the origin would otherwise merge into block 0.
	FArcTerrainFarBlockKey BlockKeyFromCell(FArcTerrainCellKey CellKey, int32 BlockCells)
	{
		return FArcTerrainFarBlockKey(
			FMath::DivideAndRoundDown(CellKey.X, BlockCells),
			FMath::DivideAndRoundDown(CellKey.Y, BlockCells));
	}

	FArcTerrainCellKey BlockOriginCell(FArcTerrainFarBlockKey BlockKey, int32 BlockCells)
	{
		return FArcTerrainCellKey(BlockKey.X * BlockCells, BlockKey.Y * BlockCells);
	}
}

namespace ArcTerrainLightingLocal_4d17
{
	constexpr int32 GNearBand = 0;
	constexpr int32 GMidBand = 1;

	float BandToleranceCm(const AArcTerrain& Terrain, int32 Band)
	{
		return Band == GNearBand ? Terrain.LightingNearToleranceCm : Terrain.LightingMidToleranceCm;
	}

	double NearBandEdge(const AArcTerrain& Terrain)
	{
		return static_cast<double>(FMath::Max(Terrain.LightingRadius, 0.0f))
			* static_cast<double>(FMath::Clamp(Terrain.LightingNearBandFraction, 0.0f, 1.0f));
	}

	int32 ResolveBandNoHysteresis(const AArcTerrain& Terrain, double DistSq)
	{
		const double Edge = NearBandEdge(Terrain);
		return DistSq <= Edge * Edge ? GNearBand : GMidBand;
	}

	int32 ResolveBandWithHysteresis(const AArcTerrain& Terrain, double DistSq, int32 CurrentBand)
	{
		const double Edge = CurrentBand == GNearBand
			? NearBandEdge(Terrain) * static_cast<double>(FMath::Max(Terrain.LightingRadiusHysteresis, 1.0f))
			: NearBandEdge(Terrain);
		return DistSq <= Edge * Edge ? GNearBand : GMidBand;
	}

	constexpr int32 GNearSinkBandCount = 8;

	int32 ResolveSinkBandNoHysteresis(double Dist, double BandWidth)
	{
		if (BandWidth <= 0.0)
		{
			return 0;
		}
		return FMath::Clamp(FMath::FloorToInt32(Dist / BandWidth), 0, GNearSinkBandCount - 1);
	}

	int32 ResolveSinkBandWithHysteresis(double Dist, double BandWidth, float Hysteresis, int32 CurrentBand)
	{
		const int32 Band = ResolveSinkBandNoHysteresis(Dist, BandWidth);
		if (CurrentBand == INDEX_NONE || Band <= CurrentBand)
		{
			return Band;
		}
		const double HeldEdge = static_cast<double>(CurrentBand + 1) * BandWidth
			* static_cast<double>(FMath::Max(Hysteresis, 1.0f));
		return Dist <= HeldEdge ? CurrentBand : Band;
	}

	float SinkCmForBand(int32 Band, float SinkPerDistance, double BandWidth)
	{
		return static_cast<float>(
			static_cast<double>(SinkPerDistance) * static_cast<double>(Band + 1) * BandWidth);
	}

	double SinkBandMinDistance(int32 Band, double BandWidth)
	{
		return static_cast<double>(Band) * BandWidth;
	}

	double SinkBandMaxDistance(int32 Band, int32 BandCount, double BandWidth, double Hysteresis,
		double RingMaxDistance)
	{
		if (Band >= BandCount - 1)
		{
			return RingMaxDistance;
		}
		return FMath::Min(RingMaxDistance, static_cast<double>(Band + 1) * BandWidth * Hysteresis);
	}

	constexpr double GSeamToleranceMultiple = 4.0;

	double MaxSkirtToleranceRatio(int32 BandCount, double BandWidth, double RingMaxDistance,
		double UnitExtent, double Hysteresis, double SinkPerDistance, double ToleranceSplitDistance,
		double InnerToleranceCm, double OuterToleranceCm)
	{
		double MaxRatio = GSeamToleranceMultiple;
		for (int32 BandA = 0; BandA < BandCount; ++BandA)
		{
			for (int32 BandB = BandA; BandB < BandCount; ++BandB)
			{
				for (int32 SideA = 0; SideA < 2; ++SideA)
				{
					for (int32 SideB = 0; SideB < 2; ++SideB)
					{
						const double MinA = FMath::Max(SinkBandMinDistance(BandA, BandWidth),
							SideA == 0 ? 0.0 : ToleranceSplitDistance);
						const double MaxA = FMath::Min(
							SinkBandMaxDistance(BandA, BandCount, BandWidth, Hysteresis, RingMaxDistance),
							SideA == 0 ? ToleranceSplitDistance : RingMaxDistance);
						const double MinB = FMath::Max(SinkBandMinDistance(BandB, BandWidth),
							SideB == 0 ? 0.0 : ToleranceSplitDistance);
						const double MaxB = FMath::Min(
							SinkBandMaxDistance(BandB, BandCount, BandWidth, Hysteresis, RingMaxDistance),
							SideB == 0 ? ToleranceSplitDistance : RingMaxDistance);
						if (MinA > MaxA || MinB > MaxB)
						{
							continue;
						}

						const double DistanceGap = FMath::Max(0.0, FMath::Max(MinB - MaxA, MinA - MaxB));
						if (DistanceGap > UnitExtent)
						{
							continue;
						}

						const double SeamToleranceCm = FMath::Max(SideA == 0 ? InnerToleranceCm : OuterToleranceCm,
							SideB == 0 ? InnerToleranceCm : OuterToleranceCm);
						if (SeamToleranceCm <= 0.0)
						{
							continue;
						}

						const double SinkStepCm =
							SinkPerDistance * static_cast<double>(BandB - BandA) * BandWidth;
						MaxRatio = FMath::Max(MaxRatio,
							(SinkStepCm + GSeamToleranceMultiple * SeamToleranceCm) / SeamToleranceCm);
					}
				}
			}
		}
		return MaxRatio;
	}

	void SinkMesh(ArcTerrainLightingMesh::FMesh& Mesh, float SinkCm)
	{
		if (SinkCm <= 0.0f)
		{
			return;
		}
		for (FVector3f& Position : Mesh.Positions)
		{
			Position.Z -= SinkCm;
		}
		Mesh.LocalBounds = Mesh.LocalBounds.ShiftBy(FVector3f(0.0f, 0.0f, -SinkCm));
	}

	FArcTerrainCellKey NeighbourCellKey(FArcTerrainCellKey Key, int32 Edge)
	{
		switch (static_cast<ArcTerrainLightingMesh::ESkirtEdge>(Edge))
		{
		case ArcTerrainLightingMesh::ESkirtEdge::NegX: return FArcTerrainCellKey(Key.X - 1, Key.Y);
		case ArcTerrainLightingMesh::ESkirtEdge::PosX: return FArcTerrainCellKey(Key.X + 1, Key.Y);
		case ArcTerrainLightingMesh::ESkirtEdge::NegY: return FArcTerrainCellKey(Key.X, Key.Y - 1);
		default: return FArcTerrainCellKey(Key.X, Key.Y + 1);
		}
	}

	void SetPackingComponent(FColor& Color, int32 Component, uint8 Value)
	{
		switch (Component)
		{
		case 0: Color.R = Value; break;
		case 1: Color.G = Value; break;
		case 2: Color.B = Value; break;
		default: Color.A = Value; break;
		}
	}

	FColor MakeBaseWeightClear(int32 ArrayIndex, int32 BaseChannelIndex)
	{
		FColor Clear(0, 0, 0, 0);
		if (BaseChannelIndex != INDEX_NONE && ArrayIndex == BaseChannelIndex / 4)
		{
			SetPackingComponent(Clear, BaseChannelIndex % 4, 255);
		}
		return Clear;
	}

	UTexture2DArray* CreateLightingTextureArray(int32 Side, EPixelFormat Format)
	{
		UTexture2DArray* Texture = UTexture2DArray::CreateTransient(Side, Side, 1, Format);
		if (Texture == nullptr)
		{
			return nullptr;
		}
		Texture->SRGB = false;
		Texture->NeverStream = true;
		Texture->Filter = TF_Bilinear;
		Texture->AddressX = TA_Clamp;
		Texture->AddressY = TA_Clamp;
		Texture->AddressZ = TA_Clamp;
		return Texture;
	}

	UTexture2D* CreatePackedHeightTexture()
	{
		UTexture2D* Texture = UTexture2D::CreateTransient(ArcTerrainLightingMesh::GHeightfieldResolution,
			ArcTerrainLightingMesh::GHeightfieldResolution, PF_R8G8);
		if (Texture == nullptr)
		{
			return nullptr;
		}
		Texture->SRGB = false;
		Texture->NeverStream = true;
		Texture->Filter = TF_Bilinear;
		Texture->AddressX = TA_Clamp;
		Texture->AddressY = TA_Clamp;
		return Texture;
	}

	void WriteMip0(UTexture& Texture, FTexturePlatformData* PlatformData, const void* Source, int64 NumBytes)
	{
		if (PlatformData == nullptr || PlatformData->Mips.Num() == 0)
		{
			return;
		}
		FTexture2DMipMap& Mip = PlatformData->Mips[0];
		if (Mip.BulkData.GetBulkDataSize() != NumBytes)
		{
			return;
		}
		void* Destination = Mip.BulkData.Lock(LOCK_READ_WRITE);
		FMemory::Memcpy(Destination, Source, NumBytes);
		Mip.BulkData.Unlock();
		Texture.UpdateResource();
	}

	int32 PackedHeightNumBytes()
	{
		return ArcTerrainLightingMesh::GHeightfieldResolution
			* ArcTerrainLightingMesh::GHeightfieldResolution * 2;
	}

	void WritePackedHeight(UTexture2D& Texture, TConstArrayView<uint8> PackedRG)
	{
		WriteMip0(Texture, Texture.GetPlatformData(), PackedRG.GetData(),
			static_cast<int64>(PackedRG.Num()));
	}

	UTexture2D* MakeFlatPackedHeightTexture(AArcTerrain& Terrain)
	{
		UTexture2D* Texture = CreatePackedHeightTexture();
		if (Texture == nullptr)
		{
			return nullptr;
		}

		constexpr double MinRepresentableCm =
			-32768.0 / 128.0 * static_cast<double>(ArcTerrainLightingMesh::GHeightfieldScaleZ);
		constexpr double MaxRepresentableCm =
			32767.0 / 128.0 * static_cast<double>(ArcTerrainLightingMesh::GHeightfieldScaleZ);
		const double BaseHeight = static_cast<double>(Terrain.HeightOptions.BaseHeightCm);
		ensureMsgf(BaseHeight >= MinRepresentableCm && BaseHeight <= MaxRepresentableCm,
			TEXT("ArcTerrain flat lighting: BaseHeightCm %.2f is outside the representable heightfield band ")
			TEXT("[%.2f, %.2f] cm; EncodePackedHeight saturates and the heightfield will not match the mesh."),
			BaseHeight, MinRepresentableCm, MaxRepresentableCm);

		TArray<float> ConstantApron;
		ConstantApron.Init(Terrain.HeightOptions.BaseHeightCm, 4 * 4);
		TArray<uint8> PackedRG;
		ArcTerrainLightingMesh::EncodePackedHeight(ConstantApron, 1, PackedRG);
		WritePackedHeight(*Texture, PackedRG);

		Terrain.FlatLightingHeightTexture = Texture;
		Terrain.FlatLightingHeightCm = Terrain.HeightOptions.BaseHeightCm;
		return Texture;
	}

	void RepointFlatPackedHeightTextures(AArcTerrain& Terrain, FArcTerrainState& State)
	{
		UTexture2D* Texture = MakeFlatPackedHeightTexture(Terrain);
		if (Texture == nullptr)
		{
			return;
		}
		for (TPair<FArcTerrainCellKey, FArcTerrainLightingPatch>& PatchPair : State.LightingPatches)
		{
			++PatchPair.Value.DirtySerial;

			UArcTerrainLightingComponent* Component = PatchPair.Value.Component.Get();
			if (Component == nullptr || !PatchPair.Value.bAppliedFlat
				|| Component->GetPackedHeightTexture() == Texture)
			{
				continue;
			}
			Component->SetPackedHeightTexture(Texture);
			Component->MarkRenderStateDirty();
		}
	}

	void RunLightingBuild(FArcTerrainLightingBuild& Build)
	{
		if (Build.HeightsWithApron.IsEmpty())
		{
			ArcTerrainLightingMesh::BuildFlatCellMesh(
				Build.FlatHeightCm, Build.CellSize, Build.SkirtDepthCm, Build.Mesh);
			const int32 HeightSide = ArcTerrainLightingMesh::GLightingHeightQuads + 3;
			Build.HeightRaster.Init(Build.FlatHeightCm, HeightSide * HeightSide);
		}
		else
		{
			const FArcTerrainSurfaceCellCache Cell = ArcTerrainSurface::BuildCellCache(
				Build.Key, Build.HeightsWithApron, Build.SourceHeightQuads, Build.CellSize);
			ArcTerrainLightingMesh::BuildCellMesh(
				Cell, Build.CellSize, Build.ToleranceCm, Build.SkirtDepthCm, Build.Mesh);
			ArcTerrainLightingMesh::ResampleHeightPlane(
				Build.HeightsWithApron, Build.SourceHeightQuads, Build.HeightRaster);
			ArcTerrainLightingMesh::EncodePackedHeight(
				Build.HeightsWithApron, Build.SourceHeightQuads, Build.PackedHeight);
		}

		SinkMesh(Build.Mesh, Build.SinkCm);

		Build.WeightRasters.SetNum(Build.WeightPlanesWithApron.Num());
		for (int32 PlaneIndex = 0; PlaneIndex < Build.WeightPlanesWithApron.Num(); ++PlaneIndex)
		{
			ArcTerrainLightingMesh::ResampleWeightPlane(Build.WeightPlanesWithApron[PlaneIndex],
				Build.SourceWeightQuads, Build.WeightRasters[PlaneIndex]);
		}
	}

	bool IsBuildLatched(const FArcTerrainLightingPatch& Patch, uint32 HeightPayload, uint32 WeightPayload,
		float ToleranceCm, int32 HeightQuads, int32 WeightQuads)
	{
		return Patch.bBuildFailed
			&& Patch.FailedDirtySerial == Patch.DirtySerial
			&& Patch.FailedHeightPayload == HeightPayload
			&& Patch.FailedWeightPayload == WeightPayload
			&& Patch.FailedToleranceCm == ToleranceCm
			&& Patch.FailedHeightQuads == HeightQuads
			&& Patch.FailedWeightQuads == WeightQuads;
	}

	void LatchBuildFailure(FArcTerrainLightingPatch& Patch, uint64 DirtySerial, uint32 HeightPayload,
		uint32 WeightPayload, float ToleranceCm, int32 HeightQuads, int32 WeightQuads)
	{
		Patch.bBuildFailed = true;
		Patch.FailedDirtySerial = DirtySerial;
		Patch.FailedHeightPayload = HeightPayload;
		Patch.FailedWeightPayload = WeightPayload;
		Patch.FailedToleranceCm = ToleranceCm;
		Patch.FailedHeightQuads = HeightQuads;
		Patch.FailedWeightQuads = WeightQuads;
	}

	void FailLightingApply(UArcTerrainLightingComponent* Component, bool bNewComponent,
		FArcTerrainLightingPatch& Patch, const FArcTerrainLightingBuild& Build)
	{
		if (bNewComponent && Component != nullptr)
		{
			Component->DestroyComponent();
		}
		LatchBuildFailure(Patch, Build.BuiltDirtySerial, Build.BuiltHeightPayload, Build.BuiltWeightPayload,
			Build.ToleranceCm, Build.SourceHeightQuads, Build.SourceWeightQuads);
		ArcTerrainDiag::NoteLightingStale();
	}

	void LogWeightUnresolvableOnce(const AArcTerrain& Terrain, FArcTerrainCellKey Key)
	{
		static TSet<TTuple<uint32, FArcTerrainCellKey>> LoggedUnresolvableCells;
		bool bAlreadyLogged = false;
		LoggedUnresolvableCells.Add(MakeTuple(Terrain.GetUniqueID(), Key), &bAlreadyLogged);
		if (!bAlreadyLogged)
		{
			UE_LOGF(LogArcTerrain, Warning,
				"[Lighting] Terrain '%ls' cell (%d,%d): no baked weight plane names a channel of the "
				"current set, so the patch renders base-channel weights. Logged once per terrain cell.",
				*Terrain.GetName(), Key.X, Key.Y);
		}
	}

}

namespace ArcTerrainLightingDelta_a1f3
{
	// Symmetric per-interval budget, the same trade-off GMaxSectorWindowSpan already makes for
	// residency/streaming/collision. Default CellExtent M256 gives CellSize 25 600 cm, and the largest
	// near threshold LightingRadius 30 000 * LightingRadiusHysteresis 1.35 = 40 500 cm reaches ~2 units
	// either side; the far variant runs this same helper over BlockSize = LightingFarBlockCells 4 *
	// 25 600 = 102 400 cm against LightingFarRadius 200 000 * 1.35, ~3 either side. 32 therefore only
	// bites under a degenerate terrain scale, where it clips the ring instead of sweeping toward int32.
	constexpr int32 GMaxLightingKeySpan = 32;

	void InclusiveKeyInterval(double LocalCoord, double Reach, double UnitSize, int32& OutMin, int32& OutMax)
	{
		OutMin = 1;
		OutMax = 0;
		if (Reach < 0.0 || UnitSize <= 0.0)
		{
			return;
		}
		const double MinCoord = LocalCoord - Reach;
		const double MaxCoord = LocalCoord + Reach;
		double MinUnit = FMath::Floor(MinCoord / UnitSize);
		const double MaxUnit = FMath::Floor(MaxCoord / UnitSize);
		const double CentreUnit = FMath::Floor(LocalCoord / UnitSize);

		constexpr double CellIndexLimit = 1.0e9;
		if (!FMath::IsFinite(MinUnit) || !FMath::IsFinite(MaxUnit) || !FMath::IsFinite(CentreUnit)
			|| FMath::Abs(MinUnit) > CellIndexLimit || FMath::Abs(MaxUnit) > CellIndexLimit
			|| FMath::Abs(CentreUnit) > CellIndexLimit)
		{
			return;
		}

		if (FMath::Fmod(MinCoord, UnitSize) == 0.0)
		{
			MinUnit -= 1.0;
		}

		const double Budget = static_cast<double>(GMaxLightingKeySpan);
		OutMin = static_cast<int32>(FMath::Max(MinUnit, CentreUnit - Budget));
		OutMax = static_cast<int32>(FMath::Min(MaxUnit, CentreUnit + Budget));
	}

	void AppendIntervalDiff(int32 OldMin, int32 OldMax, int32 NewMin, int32 NewMax, int32 RowY,
		TSet<FIntPoint>& OutKeys)
	{
		const bool bOldValid = OldMin <= OldMax;
		const bool bNewValid = NewMin <= NewMax;
		if (!bOldValid && !bNewValid)
		{
			return;
		}
		if (!bOldValid)
		{
			for (int32 KeyX = NewMin; KeyX <= NewMax; ++KeyX)
			{
				OutKeys.Add(FIntPoint(KeyX, RowY));
			}
			return;
		}
		if (!bNewValid)
		{
			for (int32 KeyX = OldMin; KeyX <= OldMax; ++KeyX)
			{
				OutKeys.Add(FIntPoint(KeyX, RowY));
			}
			return;
		}
		const int32 OverlapMin = FMath::Max(OldMin, NewMin);
		const int32 OverlapMax = FMath::Min(OldMax, NewMax);
		for (int32 KeyX = OldMin; KeyX <= OldMax; ++KeyX)
		{
			if (KeyX < OverlapMin || KeyX > OverlapMax)
			{
				OutKeys.Add(FIntPoint(KeyX, RowY));
			}
		}
		for (int32 KeyX = NewMin; KeyX <= NewMax; ++KeyX)
		{
			if (KeyX < OverlapMin || KeyX > OverlapMax)
			{
				OutKeys.Add(FIntPoint(KeyX, RowY));
			}
		}
	}

	void RowXInterval(double LocalX, double LocalY, int32 RowY, double UnitSize, double Radius,
		double ScaleX, double ScaleY, int32& OutMin, int32& OutMax)
	{
		OutMin = 1;
		OutMax = 0;
		if (Radius <= 0.0)
		{
			return;
		}
		const double RowMin = static_cast<double>(RowY) * UnitSize;
		const double RowMax = RowMin + UnitSize;
		const double ClosestY = FMath::Clamp(LocalY, RowMin, RowMax);
		const double DY = FMath::Abs(LocalY - ClosestY) * ScaleY;
		if (!(DY < Radius))
		{
			return;
		}
		const double XReach = FMath::Sqrt(Radius * Radius - DY * DY) / ScaleX;
		InclusiveKeyInterval(LocalX, XReach, UnitSize, OutMin, OutMax);
	}

	void GatherChangedDistanceBandKeys(
		const FVector& OldWorldInvoker,
		const FVector& NewWorldInvoker,
		double UnitSize,
		TConstArrayView<double> Thresholds,
		const FTransform& TerrainTransform,
		TSet<FIntPoint>& OutKeys)
	{
		if (OldWorldInvoker.Equals(NewWorldInvoker, 0.0) || UnitSize <= 0.0)
		{
			return;
		}

		const FVector Scale = TerrainTransform.GetScale3D();
		const double ScaleX = FMath::Max(FMath::Abs(Scale.X), UE_DOUBLE_SMALL_NUMBER);
		const double ScaleY = FMath::Max(FMath::Abs(Scale.Y), UE_DOUBLE_SMALL_NUMBER);
		const FVector OldLocal = TerrainTransform.InverseTransformPosition(OldWorldInvoker);
		const FVector NewLocal = TerrainTransform.InverseTransformPosition(NewWorldInvoker);

		for (const double Radius : Thresholds)
		{
			if (Radius <= 0.0)
			{
				continue;
			}
			int32 OldMinY = 1;
			int32 OldMaxY = 0;
			int32 NewMinY = 1;
			int32 NewMaxY = 0;
			InclusiveKeyInterval(OldLocal.Y, Radius / ScaleY, UnitSize, OldMinY, OldMaxY);
			InclusiveKeyInterval(NewLocal.Y, Radius / ScaleY, UnitSize, NewMinY, NewMaxY);
			const bool bOldRowsValid = OldMinY <= OldMaxY;
			const bool bNewRowsValid = NewMinY <= NewMaxY;
			FIntPoint RowRanges[2];
			int32 NumRowRanges = 0;
			if (bOldRowsValid && bNewRowsValid)
			{
				if (OldMaxY + 1 < NewMinY || NewMaxY + 1 < OldMinY)
				{
					RowRanges[0] = FIntPoint(OldMinY, OldMaxY);
					RowRanges[1] = FIntPoint(NewMinY, NewMaxY);
					NumRowRanges = 2;
				}
				else
				{
					RowRanges[0] = FIntPoint(FMath::Min(OldMinY, NewMinY), FMath::Max(OldMaxY, NewMaxY));
					NumRowRanges = 1;
				}
			}
			else if (bOldRowsValid)
			{
				RowRanges[0] = FIntPoint(OldMinY, OldMaxY);
				NumRowRanges = 1;
			}
			else if (bNewRowsValid)
			{
				RowRanges[0] = FIntPoint(NewMinY, NewMaxY);
				NumRowRanges = 1;
			}
			else
			{
				continue;
			}

			for (int32 RangeIndex = 0; RangeIndex < NumRowRanges; ++RangeIndex)
			{
				for (int32 RowY = RowRanges[RangeIndex].X; RowY <= RowRanges[RangeIndex].Y; ++RowY)
				{
					int32 OldMinX = 1;
					int32 OldMaxX = 0;
					int32 NewMinX = 1;
					int32 NewMaxX = 0;
					RowXInterval(OldLocal.X, OldLocal.Y, RowY, UnitSize, Radius, ScaleX, ScaleY,
						OldMinX, OldMaxX);
					RowXInterval(NewLocal.X, NewLocal.Y, RowY, UnitSize, Radius, ScaleX, ScaleY,
						NewMinX, NewMaxX);
					// Interval difference only: never the union AABB of old and new reaches.
					AppendIntervalDiff(OldMinX, OldMaxX, NewMinX, NewMaxX, RowY, OutKeys);
				}
			}
		}
	}

	void GatherExactRadiusKeys(const FVector& WorldInvoker, double Radius, double UnitSize,
		const FTransform& TerrainTransform, TSet<FIntPoint>& OutKeys)
	{
		if (UnitSize <= 0.0 || Radius < 0.0)
		{
			return;
		}
		const FVector Scale = TerrainTransform.GetScale3D();
		const double MinAxisScale = FMath::Max(
			FMath::Min(FMath::Abs(Scale.X), FMath::Abs(Scale.Y)), UE_DOUBLE_SMALL_NUMBER);
		const double Extent = Radius / MinAxisScale;
		const FVector Local = TerrainTransform.InverseTransformPosition(WorldInvoker);
		int32 MinX = 1;
		int32 MaxX = 0;
		int32 MinY = 1;
		int32 MaxY = 0;
		InclusiveKeyInterval(Local.X, Extent, UnitSize, MinX, MaxX);
		InclusiveKeyInterval(Local.Y, Extent, UnitSize, MinY, MaxY);
		if (MinX > MaxX || MinY > MaxY)
		{
			return;
		}
		TArray<FVector> Invokers;
		Invokers.Add(WorldInvoker);
		const double RadiusSq = Radius * Radius;
		for (int32 KeyY = MinY; KeyY <= MaxY; ++KeyY)
		{
			for (int32 KeyX = MinX; KeyX <= MaxX; ++KeyX)
			{
				const double MinLocalX = static_cast<double>(KeyX) * UnitSize;
				const double MinLocalY = static_cast<double>(KeyY) * UnitSize;
				const double DistSq = ArcTerrainSubsystemPrivate::RectInvokerDistSq(
					MinLocalX, MinLocalY, MinLocalX + UnitSize, MinLocalY + UnitSize, TerrainTransform,
					Invokers);
				if (DistSq <= RadiusSq)
				{
					OutKeys.Add(FIntPoint(KeyX, KeyY));
				}
			}
		}
	}

	FBox TileLocalBounds(const FArcTerrainTileGrid& Grid, FIntPoint TileKey)
	{
		const double TileSize = static_cast<double>(Grid.TileExtentM) * 100.0;
		return FBox(
			FVector(static_cast<double>(TileKey.X) * TileSize, static_cast<double>(TileKey.Y) * TileSize,
				-ArcTerrain::GCellZHalfExtent),
			FVector(static_cast<double>(TileKey.X + 1) * TileSize, static_cast<double>(TileKey.Y + 1) * TileSize,
				ArcTerrain::GCellZHalfExtent));
	}

	bool PositionsMatch(TConstArrayView<FVector> A, TConstArrayView<FVector> B)
	{
		if (A.Num() != B.Num())
		{
			return false;
		}
		for (int32 Index = 0; Index < A.Num(); ++Index)
		{
			if (!A[Index].Equals(B[Index], 0.0))
			{
				return false;
			}
		}
		return true;
	}

	bool TryPairInvokerPositions(
		TConstArrayView<FVector> OldPositions,
		TConstArrayView<FVector> NewPositions,
		TArray<int32>& OutOldToNew)
	{
		OutOldToNew.Reset();
		if (OldPositions.Num() != NewPositions.Num())
		{
			return false;
		}
		TArray<uint8> NewUsed;
		NewUsed.Init(0, NewPositions.Num());
		OutOldToNew.Init(INDEX_NONE, OldPositions.Num());
		for (int32 OldIndex = 0; OldIndex < OldPositions.Num(); ++OldIndex)
		{
			for (int32 NewIndex = 0; NewIndex < NewPositions.Num(); ++NewIndex)
			{
				if (NewUsed[NewIndex] != 0)
				{
					continue;
				}
				if (OldPositions[OldIndex].Equals(NewPositions[NewIndex], 0.0))
				{
					NewUsed[NewIndex] = 1;
					OutOldToNew[OldIndex] = NewIndex;
					break;
				}
			}
		}
		int32 UnmatchedOld = INDEX_NONE;
		int32 UnmatchedNew = INDEX_NONE;
		int32 UnmatchedOldCount = 0;
		int32 UnmatchedNewCount = 0;
		for (int32 OldIndex = 0; OldIndex < OldPositions.Num(); ++OldIndex)
		{
			if (OutOldToNew[OldIndex] == INDEX_NONE)
			{
				++UnmatchedOldCount;
				UnmatchedOld = OldIndex;
			}
		}
		for (int32 NewIndex = 0; NewIndex < NewPositions.Num(); ++NewIndex)
		{
			if (NewUsed[NewIndex] == 0)
			{
				++UnmatchedNewCount;
				UnmatchedNew = NewIndex;
			}
		}
		if (UnmatchedOldCount != UnmatchedNewCount)
		{
			return false;
		}
		if (UnmatchedOldCount == 0)
		{
			return true;
		}
		if (UnmatchedOldCount == 1)
		{
			OutOldToNew[UnmatchedOld] = UnmatchedNew;
			return true;
		}
		return false;
	}
}

namespace ArcTerrainLightingIndex_6b19
{
	using namespace ArcTerrainLightingLocal_4d17;
	using namespace ArcTerrainLightingDelta_a1f3;
	using namespace ArcTerrainSubsystemPrivate;

	double ReadyDistSq(const FArcTerrainState& State, const AArcTerrain& Terrain, FArcTerrainCellKey Key)
	{
		return CellInvokerDistSq(Key, Terrain.Config.CellSize(), Terrain.GetTerrainTransform(),
			State.LightingSelectionCache.Positions);
	}

	void RemoveReady(FArcTerrainState& State, FArcTerrainCellKey Key)
	{
		if (State.LightingReadySet.Remove(Key) > 0)
		{
			State.LightingReadyOrdered.RemoveAll(
				[Key](const FArcTerrainLightingReadyItem& Item)
				{
					return Item.Key == Key;
				});
		}
	}

	void UnregisterTileDependents(FArcTerrainState& State, FArcTerrainCellKey Key)
	{
		if (FArcTerrainLightingPatch* Patch = State.LightingPatches.Find(Key))
		{
			if (TSet<FArcTerrainCellKey>* HeightDependents =
				State.LightingHeightTileDependents.Find(Patch->HeightTileKey))
			{
				HeightDependents->Remove(Key);
				if (HeightDependents->IsEmpty())
				{
					State.LightingHeightTileDependents.Remove(Patch->HeightTileKey);
				}
			}
			if (TSet<FArcTerrainCellKey>* WeightDependents =
				State.LightingWeightTileDependents.Find(Patch->WeightTileKey))
			{
				WeightDependents->Remove(Key);
				if (WeightDependents->IsEmpty())
				{
					State.LightingWeightTileDependents.Remove(Patch->WeightTileKey);
				}
			}
		}
	}

	void RegisterTileDependent(TMap<FIntPoint, TSet<FArcTerrainCellKey>>& Dependents, FIntPoint TileKey,
		FArcTerrainCellKey Key)
	{
		Dependents.FindOrAdd(TileKey).Add(Key);
	}

	void EnqueueReady(FArcTerrainState& State, const AArcTerrain& Terrain, FArcTerrainCellKey Key)
	{
		if (State.LightingReadySet.Contains(Key))
		{
			return;
		}
		FArcTerrainLightingReadyItem Item;
		Item.Key = Key;
		Item.DistSq = ReadyDistSq(State, Terrain, Key);
		State.LightingReadySet.Add(Key);
		int32 InsertIndex = State.LightingReadyOrdered.Num();
		while (InsertIndex > 0 && Item.DistSq < State.LightingReadyOrdered[InsertIndex - 1].DistSq)
		{
			--InsertIndex;
		}
		State.LightingReadyOrdered.Insert(Item, InsertIndex);
		ArcTerrainDiag::NoteLightingReady();
	}

	FArcTerrainLightingPatch& EnsurePatch(FArcTerrainState& State, FArcTerrainCellKey Key)
	{
		if (FArcTerrainLightingPatch* Existing = State.LightingPatches.Find(Key))
		{
			return *Existing;
		}
		FArcTerrainLightingPatch& Patch = State.LightingPatches.Add(Key);
		Patch.PatchGeneration = State.NextLightingPatchGeneration++;
		return Patch;
	}

	void ClearScheduling(FArcTerrainState& State)
	{
		State.LightingReadySet.Reset();
		State.LightingReadyOrdered.Reset();
		State.LightingRetryHeap.Reset();
		State.PendingLightingAuthorityEvents.Reset();
		State.LightingWindowUnion.Reset();
		State.LightingHeightTileDependents.Reset();
		State.LightingWeightTileDependents.Reset();
		State.LightingSelectionCache = FArcTerrainLightingSelectionCache();
		State.NextLightingRetry = 0.0;
		State.bLightingRanksDirty = false;
	}

	void RecreateFlatPackedHeightTexture(AArcTerrain& Terrain, FArcTerrainState& State)
	{
		RepointFlatPackedHeightTextures(Terrain, State);
	}

	void SeedCurrentWindow(FArcTerrainState& State, const AArcTerrain& Terrain)
	{
		for (const FArcTerrainCellKey& Key : State.LightingWindowUnion)
		{
			if (FArcTerrainLightingPatch* Patch = State.LightingPatches.Find(Key))
			{
				++Patch->DirtySerial;
			}
			EnqueueReady(State, Terrain, Key);
		}
	}

	void OrphanInFlight(FArcTerrainState& State)
	{
		for (TPair<FArcTerrainCellKey, FArcTerrainLightingPatch>& PatchPair : State.LightingPatches)
		{
			++PatchPair.Value.PatchGeneration;
			PatchPair.Value.InFlight.Reset();
		}
	}

	void RebuildRanks(FArcTerrainState& State, const AArcTerrain& Terrain)
	{
		for (FArcTerrainLightingReadyItem& Item : State.LightingReadyOrdered)
		{
			Item.DistSq = ReadyDistSq(State, Terrain, Item.Key);
		}
		State.LightingReadyOrdered.Sort(
			[](const FArcTerrainLightingReadyItem& A, const FArcTerrainLightingReadyItem& B)
			{
				return A.DistSq < B.DistSq;
			});
		State.bLightingRanksDirty = false;
	}

	void CollectThresholds(const AArcTerrain& Terrain, double LightingRadius, double UnloadRadius,
		TArray<double>& OutThresholds)
	{
		const double NearEdge = NearBandEdge(Terrain);
		const double NearSinkBandWidth = LightingRadius
			/ static_cast<double>(GNearSinkBandCount);
		const double Hysteresis = static_cast<double>(FMath::Max(Terrain.LightingRadiusHysteresis, 1.0f));
		OutThresholds.Reset();
		OutThresholds.Add(LightingRadius);
		OutThresholds.Add(UnloadRadius);
		OutThresholds.Add(NearEdge);
		OutThresholds.Add(NearEdge * Hysteresis);
		for (int32 Band = 1; Band <= GNearSinkBandCount; ++Band)
		{
			OutThresholds.Add(static_cast<double>(Band) * NearSinkBandWidth);
			OutThresholds.Add(static_cast<double>(Band) * NearSinkBandWidth * Hysteresis);
		}
	}

	void LeaveKey(FArcTerrainState& State, FArcTerrainCellKey Key,
		TFunctionRef<void(FArcTerrainLightingPatch&)> DestroyFn)
	{
		RemoveReady(State, Key);
		UnregisterTileDependents(State, Key);
		if (FArcTerrainLightingPatch* Patch = State.LightingPatches.Find(Key))
		{
			DestroyFn(*Patch);
			State.LightingPatches.Remove(Key);
		}
		State.LightingWindowUnion.Remove(Key);
	}

	void ReconcileKey(AArcTerrain& Terrain, FArcTerrainState& State,
		FArcTerrainCellKey Key, TConstArrayView<FVector> InvokerPositions, double LightingRadius,
		double UnloadRadius, double NearSinkBandWidth,
		TFunctionRef<void(FArcTerrainLightingPatch&)> DestroyFn, bool bEnqueueNeighbours)
	{
		const double CellSize = Terrain.Config.CellSize();
		const FTransform TerrainTransform = Terrain.GetTerrainTransform();
		const double DistSq = CellInvokerDistSq(Key, CellSize, TerrainTransform, InvokerPositions);
		FArcTerrainLightingPatch* Existing = State.LightingPatches.Find(Key);
		const bool bHeld = Existing != nullptr || State.LightingWindowUnion.Contains(Key);
		const double GateRadius = bHeld ? UnloadRadius : LightingRadius;
		if (DistSq > GateRadius * GateRadius)
		{
			if (Existing != nullptr || State.LightingWindowUnion.Contains(Key))
			{
				LeaveKey(State, Key, DestroyFn);
			}
			return;
		}

		State.LightingWindowUnion.Add(Key);
		FArcTerrainLightingPatch& Patch = EnsurePatch(State, Key);
		if (!Patch.Component.IsValid())
		{
			Patch.AppliedHeightPayload = 0;
			Patch.AppliedWeightPayload = 0;
			Patch.AppliedDirtySerial = 0;
			Patch.AppliedToleranceCm = 0.0f;
			Patch.AppliedSinkCm = 0.0f;
			Patch.bAppliedFlat = false;
		}

		bool bDirty = false;
		const int32 SkirtBand = ResolveBandNoHysteresis(Terrain, DistSq);
		if (Patch.SkirtBand != SkirtBand)
		{
			if (Patch.SkirtBand != INDEX_NONE)
			{
				++Patch.DirtySerial;
				bDirty = true;
				if (bEnqueueNeighbours)
				{
					for (int32 Edge = 0; Edge < ArcTerrainLightingMesh::GSkirtEdgeCount; ++Edge)
					{
						const FArcTerrainCellKey NeighbourKey = NeighbourCellKey(Key, Edge);
						if (FArcTerrainLightingPatch* Neighbour = State.LightingPatches.Find(NeighbourKey))
						{
							++Neighbour->DirtySerial;
							if (State.LightingWindowUnion.Contains(NeighbourKey))
							{
								EnqueueReady(State, Terrain, NeighbourKey);
							}
						}
					}
				}
			}
			Patch.SkirtBand = SkirtBand;
		}

		const int32 PreviousToleranceBand = Patch.ToleranceBand;
		const int32 PreviousSinkBand = Patch.SinkBand;
		Patch.ToleranceBand = ResolveBandWithHysteresis(Terrain, DistSq, Patch.ToleranceBand);
		Patch.SinkBand = ResolveSinkBandWithHysteresis(FMath::Sqrt(DistSq), NearSinkBandWidth,
			Terrain.LightingRadiusHysteresis, Patch.SinkBand);
		if (PreviousToleranceBand != Patch.ToleranceBand || PreviousSinkBand != Patch.SinkBand)
		{
			++Patch.DirtySerial;
			bDirty = true;
		}

		if (bDirty || !Patch.Component.IsValid() || Patch.AppliedDirtySerial != Patch.DirtySerial)
		{
			EnqueueReady(State, Terrain, Key);
		}
	}

	void AppendLightingNeighbours(const TSet<FIntPoint>& ChangedKeys, TSet<FIntPoint>& OutKeys)
	{
		for (const FIntPoint& Key : ChangedKeys)
		{
			OutKeys.Add(Key);
			for (int32 Edge = 0; Edge < ArcTerrainLightingMesh::GSkirtEdgeCount; ++Edge)
			{
				OutKeys.Add(NeighbourCellKey(Key, Edge));
			}
		}
	}
}

namespace ArcTerrainFarBlockLocal_9a56
{
	bool IsFarFieldRingSupported()
	{
		const TConsoleVariableData<int32>* LumenHardwareRayTracing =
			IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Lumen.HardwareRayTracing"));
		return IsRayTracingEnabled()
			&& (GRHISupportsRayTracingShaders || GRHISupportsInlineRayTracing)
			&& LumenHardwareRayTracing != nullptr
			&& LumenHardwareRayTracing->GetValueOnAnyThread() != 0;
	}

	constexpr int32 GFarSinkBandCount = 8;

	int32 ResolveFarSinkBandNoHysteresis(double Dist, double BandWidth)
	{
		if (BandWidth <= 0.0)
		{
			return 0;
		}
		return FMath::Clamp(FMath::FloorToInt32(Dist / BandWidth), 0, GFarSinkBandCount - 1);
	}

	int32 ResolveFarSinkBandWithHysteresis(double Dist, double BandWidth, float Hysteresis, int32 CurrentBand)
	{
		const int32 Band = ResolveFarSinkBandNoHysteresis(Dist, BandWidth);
		if (CurrentBand == INDEX_NONE || Band <= CurrentBand)
		{
			return Band;
		}
		const double HeldEdge = static_cast<double>(CurrentBand + 1) * BandWidth
			* static_cast<double>(FMath::Max(Hysteresis, 1.0f));
		return Dist <= HeldEdge ? CurrentBand : Band;
	}

	float FarSinkCm(int32 Band, float SinkPerDistance, double BandWidth)
	{
		return static_cast<float>(
			static_cast<double>(SinkPerDistance) * static_cast<double>(Band + 1) * BandWidth);
	}

	bool IsFarBuildLatched(const FArcTerrainFarBlockPatch& Patch, uint32 SourceHash, float ToleranceCm,
		int32 SourceQuads)
	{
		return Patch.bBuildFailed
			&& Patch.FailedDirtySerial == Patch.DirtySerial
			&& Patch.FailedSourceHash == SourceHash
			&& Patch.FailedToleranceCm == ToleranceCm
			&& Patch.FailedSourceQuads == SourceQuads;
	}

	void LatchFarBuildFailure(FArcTerrainFarBlockPatch& Patch, uint64 DirtySerial, uint32 SourceHash,
		float ToleranceCm, int32 SourceQuads)
	{
		Patch.bBuildFailed = true;
		Patch.FailedDirtySerial = DirtySerial;
		Patch.FailedSourceHash = SourceHash;
		Patch.FailedToleranceCm = ToleranceCm;
		Patch.FailedSourceQuads = SourceQuads;
	}
}

namespace ArcTerrainFarBlockIndex_2c08
{
	using namespace ArcTerrainFarBlockLocal_9a56;
	using namespace ArcTerrainFarBlockRule_3e07;
	using namespace ArcTerrainLightingDelta_a1f3;
	using namespace ArcTerrainSubsystemPrivate;

	double ReadyDistSq(const FArcTerrainState& State, const AArcTerrain& Terrain, FArcTerrainFarBlockKey Key)
	{
		const double BlockSize = static_cast<double>(FMath::Max(Terrain.LightingFarBlockCells, 1))
			* Terrain.Config.CellSize();
		const double MinX = static_cast<double>(Key.X) * BlockSize;
		const double MinY = static_cast<double>(Key.Y) * BlockSize;
		return RectInvokerDistSq(MinX, MinY, MinX + BlockSize, MinY + BlockSize,
			Terrain.GetTerrainTransform(), State.FarBlockSelectionCache.Positions);
	}

	void RemoveReady(FArcTerrainState& State, FArcTerrainFarBlockKey Key)
	{
		if (State.FarBlockReadySet.Remove(Key) > 0)
		{
			State.FarBlockReadyOrdered.RemoveAll(
				[Key](const FArcTerrainFarBlockReadyItem& Item)
				{
					return Item.Key == Key;
				});
		}
	}

	void UnregisterTileDependents(FArcTerrainState& State, FArcTerrainFarBlockKey Key)
	{
		TArray<FIntPoint> EmptyTiles;
		for (TPair<FIntPoint, TSet<FArcTerrainFarBlockKey>>& Pair : State.FarHeightTileDependents)
		{
			Pair.Value.Remove(Key);
			if (Pair.Value.IsEmpty())
			{
				EmptyTiles.Add(Pair.Key);
			}
		}
		for (const FIntPoint& TileKey : EmptyTiles)
		{
			State.FarHeightTileDependents.Remove(TileKey);
		}
	}

	void EnqueueReady(FArcTerrainState& State, const AArcTerrain& Terrain, FArcTerrainFarBlockKey Key)
	{
		if (State.FarBlockReadySet.Contains(Key))
		{
			return;
		}
		FArcTerrainFarBlockReadyItem Item;
		Item.Key = Key;
		Item.DistSq = ReadyDistSq(State, Terrain, Key);
		State.FarBlockReadySet.Add(Key);
		int32 InsertIndex = State.FarBlockReadyOrdered.Num();
		while (InsertIndex > 0 && Item.DistSq < State.FarBlockReadyOrdered[InsertIndex - 1].DistSq)
		{
			--InsertIndex;
		}
		State.FarBlockReadyOrdered.Insert(Item, InsertIndex);
		ArcTerrainDiag::NoteFarReady();
	}

	FArcTerrainFarBlockPatch& EnsurePatch(FArcTerrainState& State, FArcTerrainFarBlockKey Key)
	{
		if (FArcTerrainFarBlockPatch* Existing = State.FarBlockPatches.Find(Key))
		{
			return *Existing;
		}
		FArcTerrainFarBlockPatch& Patch = State.FarBlockPatches.Add(Key);
		Patch.PatchGeneration = State.NextFarBlockGeneration++;
		return Patch;
	}

	void ClearScheduling(FArcTerrainState& State)
	{
		State.FarBlockReadySet.Reset();
		State.FarBlockReadyOrdered.Reset();
		State.FarBlockRetryHeap.Reset();
		State.PendingFarAuthorityEvents.Reset();
		State.FarBlockWindowUnion.Reset();
		State.FarHeightTileDependents.Reset();
		State.FarBlockSelectionCache = FArcTerrainFarBlockSelectionCache();
		State.NextFarBlockRetry = 0.0;
		State.bFarBlockRanksDirty = false;
	}

	void SeedCurrentWindow(FArcTerrainState& State, const AArcTerrain& Terrain)
	{
		for (const FArcTerrainFarBlockKey& Key : State.FarBlockWindowUnion)
		{
			if (FArcTerrainFarBlockPatch* Patch = State.FarBlockPatches.Find(Key))
			{
				++Patch->DirtySerial;
			}
			EnqueueReady(State, Terrain, Key);
		}
	}

	void OrphanInFlight(FArcTerrainState& State)
	{
		for (TPair<FArcTerrainFarBlockKey, FArcTerrainFarBlockPatch>& PatchPair : State.FarBlockPatches)
		{
			++PatchPair.Value.PatchGeneration;
			PatchPair.Value.InFlight.Reset();
		}
	}

	void RebuildRanks(FArcTerrainState& State, const AArcTerrain& Terrain)
	{
		for (FArcTerrainFarBlockReadyItem& Item : State.FarBlockReadyOrdered)
		{
			Item.DistSq = ReadyDistSq(State, Terrain, Item.Key);
		}
		State.FarBlockReadyOrdered.Sort(
			[](const FArcTerrainFarBlockReadyItem& A, const FArcTerrainFarBlockReadyItem& B)
			{
				return A.DistSq < B.DistSq;
			});
		State.bFarBlockRanksDirty = false;
	}

	void CollectThresholds(const AArcTerrain& Terrain, double FarRadius, TArray<double>& OutThresholds)
	{
		const double FarSinkBandWidth = FarRadius / static_cast<double>(GFarSinkBandCount);
		const double Hysteresis = static_cast<double>(FMath::Max(Terrain.LightingRadiusHysteresis, 1.0f));
		OutThresholds.Reset();
		OutThresholds.Add(FarRadius);
		for (int32 Band = 1; Band <= GFarSinkBandCount; ++Band)
		{
			OutThresholds.Add(static_cast<double>(Band) * FarSinkBandWidth);
			OutThresholds.Add(static_cast<double>(Band) * FarSinkBandWidth * Hysteresis);
		}
	}

	void LeaveKey(FArcTerrainState& State, FArcTerrainFarBlockKey Key,
		TFunctionRef<void(FArcTerrainFarBlockPatch&)> DestroyFn)
	{
		RemoveReady(State, Key);
		UnregisterTileDependents(State, Key);
		if (FArcTerrainFarBlockPatch* Patch = State.FarBlockPatches.Find(Key))
		{
			DestroyFn(*Patch);
			State.FarBlockPatches.Remove(Key);
		}
		State.FarBlockWindowUnion.Remove(Key);
	}

	void ReconcileKey(AArcTerrain& Terrain, FArcTerrainState& State,
		FArcTerrainFarBlockKey Key, TConstArrayView<FVector> InvokerPositions, double FarRadius,
		double BlockSize, double FarSinkBandWidth,
		TFunctionRef<void(FArcTerrainFarBlockPatch&)> DestroyFn)
	{
		const FTransform TerrainTransform = Terrain.GetTerrainTransform();
		const double MinX = static_cast<double>(Key.X) * BlockSize;
		const double MinY = static_cast<double>(Key.Y) * BlockSize;
		const double DistSq = RectInvokerDistSq(
			MinX, MinY, MinX + BlockSize, MinY + BlockSize, TerrainTransform, InvokerPositions);
		if (DistSq > FarRadius * FarRadius)
		{
			if (State.FarBlockPatches.Contains(Key) || State.FarBlockWindowUnion.Contains(Key))
			{
				LeaveKey(State, Key, DestroyFn);
			}
			return;
		}

		State.FarBlockWindowUnion.Add(Key);
		FArcTerrainFarBlockPatch& Patch = EnsurePatch(State, Key);
		if (!Patch.Component.IsValid())
		{
			Patch.AppliedDirtySerial = 0;
			Patch.AppliedSourceHash = 0;
			Patch.AppliedSinkCm = 0.0f;
		}

		const int32 PreviousSinkBand = Patch.SinkBand;
		Patch.SinkBand = ResolveFarSinkBandWithHysteresis(FMath::Sqrt(DistSq), FarSinkBandWidth,
			Terrain.LightingRadiusHysteresis, Patch.SinkBand);
		if (PreviousSinkBand != Patch.SinkBand)
		{
			++Patch.DirtySerial;
		}

		if (!Patch.Component.IsValid() || Patch.AppliedDirtySerial != Patch.DirtySerial)
		{
			EnqueueReady(State, Terrain, Key);
		}
	}
}

void UArcTerrainSubsystem::TickLightingPatches(TConstArrayView<FVector> InvokerPositions,
	double& OutLightingSeconds, double& OutFarBlockSeconds)
{
	OutLightingSeconds = 0.0;
	OutFarBlockSeconds = 0.0;

	for (TPair<TWeakObjectPtr<AArcTerrain>, FArcTerrainState>& TerrainPair : Terrains)
	{
		if (AArcTerrain* Terrain = TerrainPair.Key.Get())
		{
			const double LightingStart = FPlatformTime::Seconds();
			TickTerrainLightingPatches(*Terrain, TerrainPair.Value, InvokerPositions);
			OutLightingSeconds += FPlatformTime::Seconds() - LightingStart;
			const double FarBlockStart = FPlatformTime::Seconds();
			TickTerrainFarBlockPatches(*Terrain, TerrainPair.Value, InvokerPositions);
			OutFarBlockSeconds += FPlatformTime::Seconds() - FarBlockStart;
		}
	}
}

void UArcTerrainSubsystem::TickTerrainLightingPatches(AArcTerrain& Terrain, FArcTerrainState& State,
	TConstArrayView<FVector> InvokerPositions)
{
	using namespace ArcTerrainLightingLocal_4d17;
	using namespace ArcTerrainLightingDelta_a1f3;
	using namespace ArcTerrainLightingIndex_6b19;
	using namespace ArcTerrainSubsystemPrivate;

	const FArcTerrainConfig& Config = Terrain.Config;
	const double CellSize = Config.CellSize();
	const int32 HeightQuads = CompositeQuads(Config);
	if (CellSize <= 0.0 || HeightQuads <= 0)
	{
		return;
	}
	if (!ensureMsgf(FMath::IsPowerOfTwo(HeightQuads),
		TEXT("ArcTerrain '%s': the lighting decimator bisects a (Quads+1)^2 lattice, which needs a ")
		TEXT("power-of-two QuadsPerCell; got %d, so the terrain gets no lighting representation."),
		*Terrain.GetName(), HeightQuads))
	{
		return;
	}
	if (State.CollisionTerrainId == 0)
	{
		State.CollisionTerrainId = NextCollisionTerrainId++;
	}

	const double LightingRadius = static_cast<double>(FMath::Max(Terrain.LightingRadius, 0.0f));
	const double UnloadRadius =
		LightingRadius * static_cast<double>(FMath::Max(Terrain.LightingRadiusHysteresis, 1.0f));
	const float NearSinkPerDistance = FMath::Max(Terrain.LightingNearSinkPerDistance, 0.0f);
	const double NearSinkBandWidth = LightingRadius / static_cast<double>(GNearSinkBandCount);
	const FTransform TerrainTransform = Terrain.GetTerrainTransform();
	const double Now = FPlatformTime::Seconds();
	UMaterialInterface* LightingMaterial = Terrain.GetTerrainMaterial();
	if (LightingMaterial == nullptr)
	{
		LightingMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
	}
	const UWorld* const World = Terrain.GetWorld();
	const ERHIFeatureLevel::Type FeatureLevel =
		World != nullptr ? World->GetFeatureLevel() : GMaxRHIFeatureLevel;
	const EShaderPlatform ShaderPlatform = GetFeatureLevelShaderPlatform(FeatureLevel);
	FMaterialResource* const MaterialResource = LightingMaterial != nullptr
		? LightingMaterial->GetMaterialResource(ShaderPlatform) : nullptr;
	const FMaterialShaderMap* const MaterialShaderMap = MaterialResource != nullptr
		? MaterialResource->GetGameThreadShaderMap() : nullptr;
	const FGuid MaterialId = MaterialResource != nullptr ? MaterialResource->GetMaterialId() : FGuid();
	const bool bMaterialGenerationChanged = MaterialResource != nullptr && MaterialShaderMap != nullptr
		&& (State.LightingSelectionCache.Material != LightingMaterial
			|| State.LightingSelectionCache.MaterialResourceIdentity != MaterialResource
			|| State.LightingSelectionCache.MaterialShaderMapIdentity != MaterialShaderMap
			|| State.LightingSelectionCache.MaterialId != MaterialId);
	const bool bRetryDue = State.NextLightingRetry > 0.0 && Now >= State.NextLightingRetry;
	const bool bHasReady = State.LightingReadySet.Num() > 0;
	const bool bHasEvents = State.PendingLightingAuthorityEvents.Num() > 0;
	const bool bPositionChanged = !PositionsMatch(State.LightingSelectionCache.Positions, InvokerPositions);
	const bool bTransformChanged = !State.LightingSelectionCache.TerrainTransform.Equals(TerrainTransform);
	const bool bRadiusChanged = State.LightingSelectionCache.LightingRadius != LightingRadius
		|| State.LightingSelectionCache.UnloadRadius != UnloadRadius
		|| State.LightingSelectionCache.CellSize != CellSize
		|| State.LightingSelectionCache.NearBandEdge != NearBandEdge(Terrain)
		|| State.LightingSelectionCache.NearSinkBandWidth != NearSinkBandWidth
		|| State.LightingSelectionCache.Hysteresis != Terrain.LightingRadiusHysteresis;
	const bool bConfigGenChanged =
		State.LightingSelectionCache.ConfigGeneration != State.LightingConfigGeneration;
	const bool bInvokerPresenceChanged =
		State.LightingSelectionCache.bHasInvoker != (InvokerPositions.Num() > 0);
	if (!bPositionChanged && !bTransformChanged && !bRadiusChanged && !bConfigGenChanged
		&& !bInvokerPresenceChanged && !bMaterialGenerationChanged && !bHasEvents && !bHasReady && !bRetryDue)
	{
		return;
	}

	TFunction<void(FArcTerrainLightingPatch&)> DestroyLighting =
		[this](FArcTerrainLightingPatch& Patch)
		{
			DestroyLightingPatch(Patch);
		};
	if (bMaterialGenerationChanged)
	{
		State.LightingSelectionCache.Material = LightingMaterial;
		State.LightingSelectionCache.MaterialResourceIdentity = MaterialResource;
		State.LightingSelectionCache.MaterialShaderMapIdentity = MaterialShaderMap;
		State.LightingSelectionCache.MaterialId = MaterialId;
		++State.LightingMaterialRevision;
		for (const FArcTerrainCellKey& Key : State.LightingWindowUnion)
		{
			if (FArcTerrainLightingPatch* Patch = State.LightingPatches.Find(Key))
			{
				++Patch->DirtySerial;
			}
			EnqueueReady(State, Terrain, Key);
		}
	}

	const bool bInvokerCountChanged =
		State.LightingSelectionCache.Positions.Num() != InvokerPositions.Num();
	const bool bFullWindow = bTransformChanged || bRadiusChanged || bInvokerPresenceChanged
		|| bInvokerCountChanged
		|| State.LightingSelectionCache.Positions.Num() == 0
		|| (InvokerPositions.Num() == 0 && State.LightingWindowUnion.Num() > 0);
	if (bFullWindow || bPositionChanged || bConfigGenChanged)
	{
		const double ReconcileStartSeconds = ArcTerrainLeafTiming_8f21::Begin();
		TArray<FVector> PreviousPositions = State.LightingSelectionCache.Positions;
		State.LightingSelectionCache.Positions.Reset();
		for (const FVector& Position : InvokerPositions)
		{
			State.LightingSelectionCache.Positions.Add(Position);
		}
		State.LightingSelectionCache.TerrainTransform = TerrainTransform;
		State.LightingSelectionCache.LightingRadius = LightingRadius;
		State.LightingSelectionCache.UnloadRadius = UnloadRadius;
		State.LightingSelectionCache.CellSize = CellSize;
		State.LightingSelectionCache.NearBandEdge = NearBandEdge(Terrain);
		State.LightingSelectionCache.NearSinkBandWidth = NearSinkBandWidth;
		State.LightingSelectionCache.Hysteresis = Terrain.LightingRadiusHysteresis;
		State.LightingSelectionCache.ConfigGeneration = State.LightingConfigGeneration;
		State.LightingSelectionCache.bHasInvoker = InvokerPositions.Num() > 0;
		State.bLightingRanksDirty = true;

		TArray<int32> OldToNew;
		const bool bInvokersUnpaired =
			!bFullWindow && !bConfigGenChanged
			&& !TryPairInvokerPositions(PreviousPositions, InvokerPositions, OldToNew);
		if (bFullWindow || bConfigGenChanged || bInvokersUnpaired)
		{
			TSet<FArcTerrainCellKey> EvaluateKeys = State.LightingWindowUnion;
			if (InvokerPositions.Num() > 0)
			{
				for (const FVector& Invoker : InvokerPositions)
				{
					GatherExactRadiusKeys(Invoker, UnloadRadius, CellSize, TerrainTransform, EvaluateKeys);
				}
			}
			for (const FArcTerrainCellKey& Key : EvaluateKeys)
			{
				ReconcileKey(Terrain, State, Key, InvokerPositions, LightingRadius, UnloadRadius,
					NearSinkBandWidth, DestroyLighting, true);
			}
			if (InvokerPositions.Num() == 0)
			{
				TArray<FArcTerrainCellKey> Remaining;
				Remaining.Reserve(State.LightingWindowUnion.Num());
				for (const FArcTerrainCellKey& Key : State.LightingWindowUnion)
				{
					Remaining.Add(Key);
				}
				for (const FArcTerrainCellKey& Key : Remaining)
				{
					LeaveKey(State, Key, DestroyLighting);
				}
			}
		}
		else
		{
			TArray<double> Thresholds;
			CollectThresholds(Terrain, LightingRadius, UnloadRadius, Thresholds);
			TSet<FIntPoint> ChangedKeys;
			for (int32 OldIndex = 0; OldIndex < PreviousPositions.Num(); ++OldIndex)
			{
				GatherChangedDistanceBandKeys(PreviousPositions[OldIndex],
					InvokerPositions[OldToNew[OldIndex]], CellSize, Thresholds, TerrainTransform,
					ChangedKeys);
			}
			TSet<FIntPoint> EvaluateKeys;
			AppendLightingNeighbours(ChangedKeys, EvaluateKeys);
			for (const FIntPoint& Key : EvaluateKeys)
			{
				ReconcileKey(Terrain, State, Key, InvokerPositions, LightingRadius, UnloadRadius,
					NearSinkBandWidth, DestroyLighting, true);
			}
		}
		ArcTerrainLeafTiming_8f21::End(ArcTerrainDiag::ELeafTiming::LightingWindowReconcile,
			ReconcileStartSeconds);
	}

	if (State.PendingLightingAuthorityEvents.Num() > 0)
	{
		TArray<FArcTerrainTileAuthorityEvent> Events = MoveTemp(State.PendingLightingAuthorityEvents);
		State.PendingLightingAuthorityEvents.Reset();
		FArcTerrainTileGrid HeightGrid;
		FArcTerrainTileGrid WeightGrid;
		const bool bHasHeightGrid =
			ArcTerrainTileAccess::ResolveGrid(Terrain, EArcTerrainRail::Height, 0, HeightGrid);
		const bool bHasWeightGrid =
			ArcTerrainTileAccess::ResolveGrid(Terrain, EArcTerrainRail::Weight, 0, WeightGrid);
		for (const FArcTerrainTileAuthorityEvent& Event : Events)
		{
			if (Event.Reason == EArcTerrainTileAuthorityReason::PendingHeightReset
				|| Event.Reason == EArcTerrainTileAuthorityReason::LayoutChanged)
			{
				for (const FArcTerrainCellKey& Key : State.LightingWindowUnion)
				{
					if (FArcTerrainLightingPatch* Patch = State.LightingPatches.Find(Key))
					{
						++Patch->SourceRevision;
						++Patch->DirtySerial;
					}
					EnqueueReady(State, Terrain, Key);
				}
				continue;
			}
			if (Event.Reason == EArcTerrainTileAuthorityReason::StampBoundsChanged
				|| Event.Rail == EArcTerrainRail::Data)
			{
				continue;
			}
			const bool bHeightEvent = Event.Rail == EArcTerrainRail::Height && Event.Tier == 0 && bHasHeightGrid;
			const bool bWeightEvent = Event.Rail == EArcTerrainRail::Weight && Event.Tier == 0 && bHasWeightGrid;
			if (!bHeightEvent && !bWeightEvent)
			{
				continue;
			}
			const FArcTerrainTileGrid& EventGrid = bHeightEvent ? HeightGrid : WeightGrid;
			TArray<FArcTerrainCellKey> Covered;
			ArcTerrain::CellsIntersectingLocalBounds(TileLocalBounds(EventGrid, Event.TileKey), Config, Covered);
			TSet<FArcTerrainCellKey>* Dependents = bHeightEvent
				? State.LightingHeightTileDependents.Find(Event.TileKey)
				: State.LightingWeightTileDependents.Find(Event.TileKey);
			TSet<FArcTerrainCellKey> WakeKeys;
			for (const FArcTerrainCellKey& CellKey : Covered)
			{
				if (State.LightingWindowUnion.Contains(CellKey))
				{
					WakeKeys.Add(CellKey);
				}
			}
			if (Dependents != nullptr)
			{
				for (const FArcTerrainCellKey& Dependent : *Dependents)
				{
					WakeKeys.Add(Dependent);
				}
			}
			for (const FArcTerrainCellKey& Key : WakeKeys)
			{
				if (FArcTerrainLightingPatch* Patch = State.LightingPatches.Find(Key))
				{
					++Patch->SourceRevision;
					++Patch->DirtySerial;
				}
				EnqueueReady(State, Terrain, Key);
			}
		}
	}

	if (bRetryDue)
	{
		double NextRetry = 0.0;
		while (State.LightingRetryHeap.Num() > 0)
		{
			const FArcTerrainLightingRetryItem& Top = State.LightingRetryHeap.HeapTop();
			if (Top.RetryAfter > Now)
			{
				NextRetry = Top.RetryAfter;
				break;
			}
			FArcTerrainLightingRetryItem Due;
			State.LightingRetryHeap.HeapPop(Due,
				[](const FArcTerrainLightingRetryItem& A, const FArcTerrainLightingRetryItem& B)
				{
					return A.RetryAfter < B.RetryAfter;
				}, EAllowShrinking::No);
			if (State.LightingWindowUnion.Contains(Due.Key))
			{
				EnqueueReady(State, Terrain, Due.Key);
			}
		}
		State.NextLightingRetry = NextRetry;
	}

	if (State.bLightingRanksDirty)
	{
		RebuildRanks(State, Terrain);
	}

	const int32 MaxLightingBuilds = FMath::Max(Terrain.MaxLightingBuildsPerFrame, 1);
	if (FArcTerrainRuntimeWorkCoordinator* Coordinator = FArcTerrainRuntimeWorkCoordinator::TryGet())
	{
		Coordinator->SetTerrainLaneLaunchLimit(GetRuntimeOwnerId(), State.CollisionTerrainId,
			EArcTerrainRuntimeLane::Lighting, MaxLightingBuilds);
	}

	if (State.LightingReadyOrdered.Num() == 0)
	{
		return;
	}

	const int32 WeightQuads = ResolveWeightQuads(Config, State);
	FArcTerrainTileGrid HeightGrid;
	FArcTerrainTileGrid WeightGrid;
	const bool bHasHeightGrid =
		ArcTerrainTileAccess::ResolveGrid(Terrain, EArcTerrainRail::Height, 0, HeightGrid);
	const bool bHasWeightGrid =
		ArcTerrainTileAccess::ResolveGrid(Terrain, EArcTerrainRail::Weight, 0, WeightGrid);
	const UArcTerrainChannelSet* ChannelSet = Config.ChannelSet;

	if (Terrain.FlatLightingHeightTexture != nullptr
		&& Terrain.FlatLightingHeightCm != Terrain.HeightOptions.BaseHeightCm)
	{
		RepointFlatPackedHeightTextures(Terrain, State);
	}

	const float SkirtMultiplier = FMath::Max(Terrain.LightingSkirtToleranceMultiplier, 0.0f);
	TArray<FArcTerrainCellKey> LaunchKeys;
	LaunchKeys.Reserve(State.LightingReadyOrdered.Num());
	for (const FArcTerrainLightingReadyItem& Item : State.LightingReadyOrdered)
	{
		FArcTerrainLightingPatch* Patch = State.LightingPatches.Find(Item.Key);
		if (Patch == nullptr)
		{
			LaunchKeys.Add(Item.Key);
			continue;
		}
		if (Patch->InFlight.IsValid())
		{
			continue;
		}
		LaunchKeys.Add(Item.Key);
	}

	const double SnapshotStartSeconds = ArcTerrainLeafTiming_8f21::Begin();
	for (const FArcTerrainCellKey& Key : LaunchKeys)
	{
		RemoveReady(State, Key);
		FArcTerrainLightingPatch* PatchPtr = State.LightingPatches.Find(Key);
		if (PatchPtr == nullptr)
		{
			continue;
		}
		FArcTerrainLightingPatch& Patch = *PatchPtr;
		if (Patch.InFlight.IsValid())
		{
			EnqueueReady(State, Terrain, Key);
			continue;
		}

		const double DistSq = CellInvokerDistSq(Key, CellSize, TerrainTransform,
			State.LightingSelectionCache.Positions);
		Patch.ToleranceBand = ResolveBandWithHysteresis(Terrain, DistSq, Patch.ToleranceBand);
		Patch.SinkBand = ResolveSinkBandWithHysteresis(FMath::Sqrt(DistSq), NearSinkBandWidth,
			Terrain.LightingRadiusHysteresis, Patch.SinkBand);
		const float ToleranceCm = BandToleranceCm(Terrain, Patch.ToleranceBand);
		const float SinkCm = SinkCmForBand(Patch.SinkBand, NearSinkPerDistance, NearSinkBandWidth);

		FIntPoint HeightTileKey = FIntPoint::ZeroValue;
		FTileSourceCapture HeightSource;
		if (bHasHeightGrid)
		{
			HeightTileKey = ArcTerrainTileAccess::TileKeyFromCell(HeightGrid, Key);
			HeightSource = CaptureTileSource(Terrain, EArcTerrainRail::Height, 0, HeightTileKey, &HeightGrid);
		}
		if (HeightSource.State == ETileSourceState::KnownUnloaded
			|| (HeightSource.State == ETileSourceState::Absent && Patch.bHadLoadedHeight))
		{
			// Released known tiles wait for arrival instead of republishing flat over valid data.
			Patch.HeightTileKey = HeightTileKey;
			RegisterTileDependent(State.LightingHeightTileDependents, HeightTileKey, Key);
			ArcTerrainDiag::NoteLightingDeferred();
			continue;
		}

		FIntPoint WeightTileKey = FIntPoint::ZeroValue;
		FTileSourceCapture WeightSource;
		if (bHasWeightGrid && ChannelSet != nullptr)
		{
			WeightTileKey = ArcTerrainTileAccess::TileKeyFromCell(WeightGrid, Key);
			WeightSource = CaptureTileSource(Terrain, EArcTerrainRail::Weight, 0, WeightTileKey, &WeightGrid);
		}
		if (WeightSource.State == ETileSourceState::KnownUnloaded
			|| (WeightSource.State == ETileSourceState::Absent && Patch.bHadLoadedWeight))
		{
			Patch.WeightTileKey = WeightTileKey;
			RegisterTileDependent(State.LightingWeightTileDependents, WeightTileKey, Key);
			ArcTerrainDiag::NoteLightingDeferred();
			continue;
		}

		const uint32 HeightPayload = HeightSource.PayloadId;
		const uint32 WeightPayload = WeightSource.PayloadId;
		const bool bWantsFlat = HeightSource.State != ETileSourceState::Loaded;
		if (Patch.Component.IsValid()
			&& Patch.AppliedDirtySerial == Patch.DirtySerial
			&& Patch.AppliedHeightPayload == HeightPayload
			&& Patch.AppliedWeightPayload == WeightPayload
			&& Patch.AppliedToleranceCm == ToleranceCm
			&& Patch.AppliedSinkCm == SinkCm
			&& Patch.bAppliedFlat == bWantsFlat)
		{
			continue;
		}

		if (IsBuildLatched(Patch, HeightPayload, WeightPayload, ToleranceCm, HeightQuads, WeightQuads))
		{
			continue;
		}

		FArcTerrainLightingBuildPtr Build = MakeShared<FArcTerrainLightingBuild, ESPMode::ThreadSafe>();
		Build->Key = Key;
		Build->CellSize = CellSize;
		Build->FlatHeightCm = Terrain.HeightOptions.BaseHeightCm;
		Build->ToleranceCm = ToleranceCm;
		Build->SinkCm = SinkCm;
		Build->SourceHeightQuads = HeightQuads;
		Build->SourceWeightQuads = WeightQuads;
		Build->OwnerId = GetUniqueID();
		Build->TerrainId = State.CollisionTerrainId;
		Build->PatchGeneration = Patch.PatchGeneration;
		Build->ConfigGeneration = State.LightingConfigGeneration;
		Build->SourceRevision = Patch.SourceRevision;
		Build->BuiltMaterialRevision = State.LightingMaterialRevision;

		const float OwnSkirtToleranceCm =
			BandToleranceCm(Terrain, ResolveBandNoHysteresis(Terrain, DistSq));
		for (int32 Edge = 0; Edge < ArcTerrainLightingMesh::GSkirtEdgeCount; ++Edge)
		{
			const FArcTerrainCellKey NeighbourKey = NeighbourCellKey(Key, Edge);
			const double NeighbourDistSq =
				CellInvokerDistSq(NeighbourKey, CellSize, TerrainTransform,
					State.LightingSelectionCache.Positions);
			const float NeighbourToleranceCm =
				BandToleranceCm(Terrain, ResolveBandNoHysteresis(Terrain, NeighbourDistSq));
			Build->SkirtDepthCm[Edge] =
				SkirtMultiplier * FMath::Max(OwnSkirtToleranceCm, NeighbourToleranceCm);
		}

		if (HeightSource.State == ETileSourceState::Loaded)
		{
			Patch.bHadLoadedHeight = true;
			Patch.HeightTileKey = HeightTileKey;
		}
		if (WeightSource.State == ETileSourceState::Loaded && ChannelSet != nullptr)
		{
			Patch.bHadLoadedWeight = true;
			Patch.WeightTileKey = WeightTileKey;
		}

		Build->BuiltDirtySerial = Patch.DirtySerial;
		Build->BuiltHeightPayload = HeightPayload;
		Build->BuiltWeightPayload = WeightPayload;

		FArcTerrainRuntimeWorkCoordinator* Coordinator = FArcTerrainRuntimeWorkCoordinator::TryGet();
		if (Coordinator == nullptr)
		{
			EnqueueReady(State, Terrain, Key);
			continue;
		}
		FArcTerrainRuntimeReservationRequest Request;
		Request.OwnerId = GetRuntimeOwnerId();
		Request.TerrainId = State.CollisionTerrainId;
		Request.Lane = EArcTerrainRuntimeLane::Lighting;
		Request.ExactOwnedInputBytes = sizeof(FArcTerrainLightingBuild);
		const int32 HeightApron = HeightQuads + 3;
		const int32 WeightApron = WeightQuads + 3;
		const int32 LightH = ArcTerrainLightingMesh::GLightingHeightQuads + 3;
		const int32 LightW = ArcTerrainLightingMesh::GLightingWeightQuads + 3;
		Request.PreReservedResultBytes =
			static_cast<uint64>(HeightApron) * HeightApron * sizeof(float)
			+ static_cast<uint64>(ArcTerrainWeights::GArcTerrainWeightMaxSlices) * WeightApron * WeightApron
			+ static_cast<uint64>(LightH) * LightH * sizeof(float)
			+ static_cast<uint64>(ArcTerrainWeights::GArcTerrainWeightMaxSlices) * LightW * LightW
			+ static_cast<uint64>(LightH) * LightH * 2u
			+ static_cast<uint64>(LightH) * LightH
				* (sizeof(FVector3f) * 4u + sizeof(FVector2f) * 3u)
			+ static_cast<uint64>(LightH) * LightH * 6u * sizeof(uint32);
		if (HeightSource.Payload.IsValid())
		{
			FArcTerrainRetainedPayload Retain;
			Retain.PayloadId = HeightSource.PayloadId;
			Retain.Bytes = HeightSource.Payload->RetainedBytes();
			Retain.Payload = HeightSource.Payload;
			Request.Payloads.Add(Retain);
		}
		if (WeightSource.Payload.IsValid())
		{
			FArcTerrainRetainedPayload Retain;
			Retain.PayloadId = WeightSource.PayloadId;
			Retain.Bytes = WeightSource.Payload->RetainedBytes();
			Retain.Payload = WeightSource.Payload;
			Request.Payloads.Add(Retain);
		}
		FArcTerrainRuntimeReservationToken Token;
		if (!Coordinator->TryReserveSubmission(Request, Token))
		{
			EnqueueReady(State, Terrain, Key);
			continue;
		}

		TArray<FArcTerrainChannelHashIndex> ChannelMap;
		if (ChannelSet != nullptr)
		{
			ArcTerrainAtlasPrepare::CopyChannelHashIndexMap(*ChannelSet, ChannelMap);
		}
		const int32 ExpectedHeightRes = HeightQuads + 3;
		const int32 ExpectedWeightRes = WeightQuads + 3;
		Patch.InFlight = Build;
		ArcTerrainDiag::NoteLightingInFlight();
		TSharedPtr<FArcTerrainLightingWorkerResult, ESPMode::ThreadSafe> WorkerResult =
			MakeShared<FArcTerrainLightingWorkerResult, ESPMode::ThreadSafe>();
		WorkerResult->Build = Build;
		Coordinator->Submit(MoveTemp(Token), UE::Tasks::ETaskPriority::BackgroundNormal,
			[WorkerResult, Build, HeightGrid, WeightGrid, HeightSource, WeightSource, ChannelMap,
				ExpectedHeightRes, ExpectedWeightRes](FArcTerrainRuntimeWorkContext& Context)
			{
				if (Context.IsAbandoned())
				{
					FArcTerrainRuntimeResult Result;
					Result.Status = EArcTerrainRuntimeWorkStatus::Cancelled;
					Result.Payload = WorkerResult;
					return Result;
				}
				if (HeightSource.State == ETileSourceState::Loaded && HeightSource.Payload.IsValid())
				{
					int32 Resolution = 0;
					if (!ArcTerrainTileAccess::ExtractCellFloats(HeightGrid, *HeightSource.Payload, 0,
							Build->Key, Build->HeightsWithApron, Resolution)
						|| Resolution != ExpectedHeightRes)
					{
						WorkerResult->bHeightCutFailed = true;
						WorkerResult->ObservedHeightResolution = Resolution;
					}
					else
					{
						WorkerResult->ObservedHeightResolution = Resolution;
					}
				}
				if (!WorkerResult->bHeightCutFailed
					&& WeightSource.State == ETileSourceState::Loaded && WeightSource.Payload.IsValid())
				{
					FArcTerrainAtlasPrepareStatus WeightStatus;
					if (!ArcTerrainAtlasPrepare::CutCellWeightPlanes(Build->Key, WeightGrid,
							WeightSource.Payload, ChannelMap, ExpectedWeightRes,
							Build->WeightPlanesWithApron, Build->PlaneChannelIndices, WeightStatus))
					{
						Build->WeightPlanesWithApron.Reset();
						Build->PlaneChannelIndices.Reset();
						WorkerResult->bWeightUnresolvable = WeightStatus.bUnresolvable;
						WorkerResult->bWeightCutFailed = !WeightStatus.bUnresolvable;
						WorkerResult->ObservedWeightResolution = WeightStatus.ObservedResolution;
					}
					else
					{
						WorkerResult->ObservedWeightResolution = WeightStatus.ObservedResolution;
					}
				}
				if (!WorkerResult->bHeightCutFailed && !WorkerResult->bWeightCutFailed)
				{
					ArcTerrainLightingLocal_4d17::RunLightingBuild(*Build);
				}
				FArcTerrainRuntimeResult Result;
				Result.Status = Context.IsAbandoned()
					? EArcTerrainRuntimeWorkStatus::Cancelled
					: EArcTerrainRuntimeWorkStatus::Succeeded;
				Result.Payload = WorkerResult;
				return Result;
			});
	}
	ArcTerrainLeafTiming_8f21::End(ArcTerrainDiag::ELeafTiming::LightingPayloadSnapshot,
		SnapshotStartSeconds);
}

void UArcTerrainSubsystem::ApplyLightingBuild(AArcTerrain& Terrain, FArcTerrainCellKey Key,
	FArcTerrainLightingPatch& Patch, FArcTerrainLightingBuild& Build)
{
	using namespace ArcTerrainLightingLocal_4d17;

	const int32 HeightSide = ArcTerrainLightingMesh::GLightingHeightQuads + 3;
	const int32 WeightSide = ArcTerrainLightingMesh::GLightingWeightQuads + 3;
	const bool bFlatBuild = Build.HeightsWithApron.IsEmpty();
	if (Build.Mesh.Indices.IsEmpty() || Build.HeightRaster.Num() != HeightSide * HeightSide
		|| (!bFlatBuild && Build.PackedHeight.Num() != PackedHeightNumBytes()))
	{
		LatchBuildFailure(Patch, Build.BuiltDirtySerial, Build.BuiltHeightPayload, Build.BuiltWeightPayload,
			Build.ToleranceCm, Build.SourceHeightQuads, Build.SourceWeightQuads);
		ArcTerrainDiag::NoteLightingStale();
		return;
	}

	const FArcTerrainConfig& Config = Terrain.Config;
	const int32 NumChannels = Config.ChannelSet != nullptr ? Config.ChannelSet->Channels.Num() : 0;
	const int32 NumWeightArrays = FMath::Max(FMath::DivideAndRoundUp(NumChannels, 4), 1);
	const int32 BaseChannelIndex = ArcTerrain::ResolveBaseChannelIndex(NumChannels);

	UArcTerrainLightingComponent* Component = Patch.Component.Get();
	const bool bNewComponent = Component == nullptr;
	if (bNewComponent)
	{
		Component = NewObject<UArcTerrainLightingComponent>(&Terrain, NAME_None, RF_Transient);
	}
	else if (Component->IsRegistered())
	{
		Component->UnregisterComponent();
	}

	bool bRebindMaterial = bNewComponent
		|| Patch.AppliedMaterialRevision != Build.BuiltMaterialRevision;

	UTexture2DArray* HeightTexture = Component->GetHeightTexture();
	if (HeightTexture == nullptr)
	{
		HeightTexture = CreateLightingTextureArray(HeightSide, PF_R32_FLOAT);
		if (HeightTexture == nullptr)
		{
			FailLightingApply(Component, bNewComponent, Patch, Build);
			return;
		}
		Component->SetHeightTexture(HeightTexture);
		bRebindMaterial = true;
	}
	WriteMip0(*HeightTexture, HeightTexture->GetPlatformData(), Build.HeightRaster.GetData(),
		static_cast<int64>(Build.HeightRaster.Num()) * sizeof(float));

	TArray<TObjectPtr<UTexture2DArray>> WeightTextures = Component->GetWeightTextures();
	bool bWeightTexturesRecreated = WeightTextures.Num() != NumWeightArrays;
	for (const TObjectPtr<UTexture2DArray>& WeightTexture : WeightTextures)
	{
		bWeightTexturesRecreated |= WeightTexture == nullptr;
	}
	if (bWeightTexturesRecreated)
	{
		WeightTextures.Reset(NumWeightArrays);
		for (int32 ArrayIndex = 0; ArrayIndex < NumWeightArrays; ++ArrayIndex)
		{
			UTexture2DArray* WeightTexture = CreateLightingTextureArray(WeightSide, PF_B8G8R8A8);
			if (WeightTexture == nullptr)
			{
				FailLightingApply(Component, bNewComponent, Patch, Build);
				return;
			}
			WeightTextures.Add(WeightTexture);
		}
		bRebindMaterial = true;
	}

	TArray<FColor> PackedWeights;
	for (int32 ArrayIndex = 0; ArrayIndex < NumWeightArrays; ++ArrayIndex)
	{
		PackedWeights.Init(MakeBaseWeightClear(ArrayIndex, BaseChannelIndex), WeightSide * WeightSide);
		for (int32 PlaneIndex = 0; PlaneIndex < Build.PlaneChannelIndices.Num(); ++PlaneIndex)
		{
			const int32 ChannelIndex = Build.PlaneChannelIndices[PlaneIndex];
			if (ChannelIndex / 4 != ArrayIndex || !Build.WeightRasters.IsValidIndex(PlaneIndex))
			{
				continue;
			}
			const TArray<uint8>& Raster = Build.WeightRasters[PlaneIndex];
			if (Raster.Num() != PackedWeights.Num())
			{
				continue;
			}
			const int32 PackingComponent = ChannelIndex % 4;
			for (int32 Texel = 0; Texel < Raster.Num(); ++Texel)
			{
				SetPackingComponent(PackedWeights[Texel], PackingComponent, Raster[Texel]);
			}
		}
		WriteMip0(*WeightTextures[ArrayIndex], WeightTextures[ArrayIndex]->GetPlatformData(),
			PackedWeights.GetData(), static_cast<int64>(PackedWeights.Num()) * sizeof(FColor));
	}
	if (bWeightTexturesRecreated)
	{
		Component->SetWeightTextures(CopyTemp(WeightTextures));
	}

	if (bFlatBuild)
	{
		UTexture2D* FlatTexture = Terrain.FlatLightingHeightTexture;
		if (FlatTexture == nullptr)
		{
			FlatTexture = MakeFlatPackedHeightTexture(Terrain);
			if (FlatTexture == nullptr)
			{
				FailLightingApply(Component, bNewComponent, Patch, Build);
				return;
			}
		}
		Component->SetPackedHeightTexture(FlatTexture);
	}
	else
	{
		UTexture2D* PackedTexture = Component->GetPackedHeightTexture();
		if (PackedTexture == nullptr || PackedTexture == Terrain.FlatLightingHeightTexture)
		{
			PackedTexture = CreatePackedHeightTexture();
			if (PackedTexture == nullptr)
			{
				FailLightingApply(Component, bNewComponent, Patch, Build);
				return;
			}
			Component->SetPackedHeightTexture(PackedTexture);
		}
		WritePackedHeight(*PackedTexture, Build.PackedHeight);
	}

	UMaterialInterface* BaseMaterial = Terrain.GetTerrainMaterial();
	if (BaseMaterial == nullptr)
	{
		BaseMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
	}
	UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Component->GetMaterial(0));
	if (MID == nullptr || MID->Parent != BaseMaterial)
	{
		bRebindMaterial = true;
	}

	if (bRebindMaterial)
	{
		MID = UMaterialInstanceDynamic::Create(BaseMaterial, Component);
		if (MID == nullptr)
		{
			FailLightingApply(Component, bNewComponent, Patch, Build);
			return;
		}

		TArray<UTexture*> WeightAtlasBindings;
		WeightAtlasBindings.Reserve(WeightTextures.Num());
		for (int32 ArrayIndex = 0; ArrayIndex < WeightTextures.Num(); ++ArrayIndex)
		{
			WeightAtlasBindings.Add(WeightTextures[ArrayIndex]);
		}
		const UWorld* const World = Terrain.GetWorld();
		const ERHIFeatureLevel::Type FeatureLevel =
			World != nullptr ? World->GetFeatureLevel() : GMaxRHIFeatureLevel;
		const EShaderPlatform ShaderPlatform = GetFeatureLevelShaderPlatform(FeatureLevel);
		const ArcTerrainMaterialBindings::EResult BindingResult = ArcTerrainMaterialBindings::Bind(
			*MID, ShaderPlatform, HeightTexture,
			WeightAtlasBindings, Config.ChannelSet,
			1.0f / static_cast<float>(HeightSide),
			1.0f / static_cast<float>(WeightSide),
			static_cast<float>(Config.CellSize()), false, false);
		if (BindingResult == ArcTerrainMaterialBindings::EResult::ResourceUnavailable)
		{
			if (bNewComponent)
			{
				Component->DestroyComponent();
			}
			if (FArcTerrainState* State = Terrains.Find(&Terrain))
			{
				State->LightingSelectionCache.MaterialResourceIdentity = nullptr;
				State->LightingSelectionCache.MaterialShaderMapIdentity = nullptr;
				State->LightingSelectionCache.MaterialId.Invalidate();
			}
			ArcTerrainDiag::NoteLightingStale();
			return;
		}
		if (BindingResult == ArcTerrainMaterialBindings::EResult::Failed)
		{
			FailLightingApply(Component, bNewComponent, Patch, Build);
			return;
		}
		Component->SetMaterial(0, MID);
	}

	Component->SetCellSize(Config.CellSize());
	Component->SetMesh(MoveTemp(Build.Mesh));

	if (bNewComponent)
	{
		Component->SetupAttachment(Terrain.GetRootComponent());
		Component->SetRelativeLocation(FVector(
			static_cast<double>(Key.X) * Config.CellSize(),
			static_cast<double>(Key.Y) * Config.CellSize(),
			0.0));
		Patch.Component = Component;
	}
	if (!Component->IsRegistered())
	{
		Component->RegisterComponent();
	}

	Patch.AppliedDirtySerial = Build.BuiltDirtySerial;
	Patch.AppliedMaterialRevision = Build.BuiltMaterialRevision;
	Patch.AppliedHeightPayload = Build.BuiltHeightPayload;
	Patch.AppliedWeightPayload = Build.BuiltWeightPayload;
	Patch.AppliedToleranceCm = Build.ToleranceCm;
	Patch.AppliedSinkCm = Build.SinkCm;
	Patch.bAppliedFlat = bFlatBuild;
	Patch.bBuildFailed = false;
	ArcTerrainDiag::NoteLightingCompletion();
}

void UArcTerrainSubsystem::DestroyLightingPatch(FArcTerrainLightingPatch& Patch)
{
	if (UArcTerrainLightingComponent* Component = Patch.Component.Get())
	{
		Component->DestroyComponent();
	}
	Patch.Component = nullptr;
	Patch.InFlight.Reset();
	Patch.AppliedHeightPayload = 0;
	Patch.AppliedWeightPayload = 0;
	Patch.AppliedDirtySerial = 0;
	Patch.AppliedToleranceCm = 0.0f;
	Patch.AppliedSinkCm = 0.0f;
	Patch.bAppliedFlat = false;
	Patch.bHadLoadedHeight = false;
	Patch.bHadLoadedWeight = false;
	Patch.HeightTileKey = FIntPoint::ZeroValue;
	Patch.WeightTileKey = FIntPoint::ZeroValue;
	Patch.bBuildFailed = false;
	Patch.FailedHeightPayload = 0;
	Patch.FailedWeightPayload = 0;
	Patch.FailedDirtySerial = 0;
	Patch.FailedToleranceCm = 0.0f;
	Patch.FailedHeightQuads = 0;
	Patch.FailedWeightQuads = 0;
	Patch.ToleranceBand = INDEX_NONE;
	Patch.SkirtBand = INDEX_NONE;
	Patch.SinkBand = INDEX_NONE;
}

// FILE-SCOPED and unique (CLAUDE.md C++ Code Style) — nothing here may gain external linkage.
void UArcTerrainSubsystem::TickTerrainFarBlockPatches(AArcTerrain& Terrain, FArcTerrainState& State,
	TConstArrayView<FVector> InvokerPositions)
{
	using namespace ArcTerrainFarBlockLocal_9a56;
	using namespace ArcTerrainFarBlockRule_3e07;
	using namespace ArcTerrainFarBlockIndex_2c08;
	using namespace ArcTerrainLightingDelta_a1f3;
	using namespace ArcTerrainSubsystemPrivate;

	if (!IsFarFieldRingSupported())
	{
		return;
	}

	const FArcTerrainConfig& Config = Terrain.Config;
	const double CellSize = Config.CellSize();
	const int32 HeightQuads = CompositeQuads(Config);
	const int32 BlockCells = FMath::Max(Terrain.LightingFarBlockCells, 1);
	const double FarRadius = static_cast<double>(FMath::Max(Terrain.LightingFarRadius, 0.0f));
	if (CellSize <= 0.0 || HeightQuads <= 0 || !FMath::IsPowerOfTwo(HeightQuads) || FarRadius <= 0.0)
	{
		return;
	}
	if (State.CollisionTerrainId == 0)
	{
		State.CollisionTerrainId = NextCollisionTerrainId++;
	}

	const FTransform TerrainTransform = Terrain.GetTerrainTransform();
	const double BlockSize = static_cast<double>(BlockCells) * CellSize;
	const float FarSinkPerDistance = FMath::Max(Terrain.LightingFarSinkPerDistance, 0.0f);
	const double FarSinkBandWidth = FarRadius / static_cast<double>(GFarSinkBandCount);
	const double Now = FPlatformTime::Seconds();
	const bool bRetryDue = State.NextFarBlockRetry > 0.0 && Now >= State.NextFarBlockRetry;
	const bool bHasReady = State.FarBlockReadySet.Num() > 0;
	const bool bHasEvents = State.PendingFarAuthorityEvents.Num() > 0;
	const bool bPositionChanged = !PositionsMatch(State.FarBlockSelectionCache.Positions, InvokerPositions);
	const bool bTransformChanged = !State.FarBlockSelectionCache.TerrainTransform.Equals(TerrainTransform);
	const bool bRadiusChanged = State.FarBlockSelectionCache.FarRadius != FarRadius
		|| State.FarBlockSelectionCache.BlockSize != BlockSize
		|| State.FarBlockSelectionCache.FarSinkBandWidth != FarSinkBandWidth
		|| State.FarBlockSelectionCache.Hysteresis != Terrain.LightingRadiusHysteresis
		|| State.FarBlockSelectionCache.BlockCells != BlockCells;
	const bool bConfigGenChanged =
		State.FarBlockSelectionCache.ConfigGeneration != State.FarBlockConfigGeneration;
	const bool bInvokerPresenceChanged =
		State.FarBlockSelectionCache.bHasInvoker != (InvokerPositions.Num() > 0);
	if (!bPositionChanged && !bTransformChanged && !bRadiusChanged && !bConfigGenChanged
		&& !bInvokerPresenceChanged && !bHasEvents && !bHasReady && !bRetryDue)
	{
		return;
	}

	TFunction<void(FArcTerrainFarBlockPatch&)> DestroyFar =
		[this](FArcTerrainFarBlockPatch& Patch)
		{
			DestroyFarBlockPatch(Patch);
		};

	const bool bInvokerCountChanged =
		State.FarBlockSelectionCache.Positions.Num() != InvokerPositions.Num();
	const bool bFullWindow = bTransformChanged || bRadiusChanged || bInvokerPresenceChanged
		|| bInvokerCountChanged
		|| State.FarBlockSelectionCache.Positions.Num() == 0
		|| (InvokerPositions.Num() == 0 && State.FarBlockWindowUnion.Num() > 0);
	if (bFullWindow || bPositionChanged || bConfigGenChanged)
	{
		const double ReconcileStartSeconds = ArcTerrainLeafTiming_8f21::Begin();
		TArray<FVector> PreviousPositions = State.FarBlockSelectionCache.Positions;
		State.FarBlockSelectionCache.Positions.Reset();
		for (const FVector& Position : InvokerPositions)
		{
			State.FarBlockSelectionCache.Positions.Add(Position);
		}
		State.FarBlockSelectionCache.TerrainTransform = TerrainTransform;
		State.FarBlockSelectionCache.FarRadius = FarRadius;
		State.FarBlockSelectionCache.BlockSize = BlockSize;
		State.FarBlockSelectionCache.FarSinkBandWidth = FarSinkBandWidth;
		State.FarBlockSelectionCache.Hysteresis = Terrain.LightingRadiusHysteresis;
		State.FarBlockSelectionCache.BlockCells = BlockCells;
		State.FarBlockSelectionCache.ConfigGeneration = State.FarBlockConfigGeneration;
		State.FarBlockSelectionCache.bHasInvoker = InvokerPositions.Num() > 0;
		State.bFarBlockRanksDirty = true;

		TArray<int32> OldToNew;
		const bool bInvokersUnpaired =
			!bFullWindow && !bConfigGenChanged
			&& !TryPairInvokerPositions(PreviousPositions, InvokerPositions, OldToNew);
		if (bFullWindow || bConfigGenChanged || bInvokersUnpaired)
		{
			TSet<FArcTerrainFarBlockKey> EvaluateKeys = State.FarBlockWindowUnion;
			if (InvokerPositions.Num() > 0)
			{
				for (const FVector& Invoker : InvokerPositions)
				{
					GatherExactRadiusKeys(Invoker, FarRadius, BlockSize, TerrainTransform, EvaluateKeys);
				}
			}
			for (const FArcTerrainFarBlockKey& Key : EvaluateKeys)
			{
				ReconcileKey(Terrain, State, Key, InvokerPositions, FarRadius, BlockSize,
					FarSinkBandWidth, DestroyFar);
			}
			if (InvokerPositions.Num() == 0)
			{
				TArray<FArcTerrainFarBlockKey> Remaining;
				Remaining.Reserve(State.FarBlockWindowUnion.Num());
				for (const FArcTerrainFarBlockKey& Key : State.FarBlockWindowUnion)
				{
					Remaining.Add(Key);
				}
				for (const FArcTerrainFarBlockKey& Key : Remaining)
				{
					LeaveKey(State, Key, DestroyFar);
				}
			}
		}
		else
		{
			TArray<double> Thresholds;
			CollectThresholds(Terrain, FarRadius, Thresholds);
			TSet<FIntPoint> ChangedKeys;
			for (int32 OldIndex = 0; OldIndex < PreviousPositions.Num(); ++OldIndex)
			{
				GatherChangedDistanceBandKeys(PreviousPositions[OldIndex],
					InvokerPositions[OldToNew[OldIndex]], BlockSize, Thresholds, TerrainTransform,
					ChangedKeys);
			}
			for (const FIntPoint& Key : ChangedKeys)
			{
				ReconcileKey(Terrain, State, Key, InvokerPositions, FarRadius, BlockSize,
					FarSinkBandWidth, DestroyFar);
			}
		}
		ArcTerrainLeafTiming_8f21::End(ArcTerrainDiag::ELeafTiming::FarWindowReconcile,
			ReconcileStartSeconds);
	}

	if (State.PendingFarAuthorityEvents.Num() > 0)
	{
		TArray<FArcTerrainTileAuthorityEvent> Events = MoveTemp(State.PendingFarAuthorityEvents);
		State.PendingFarAuthorityEvents.Reset();
		FArcTerrainTileGrid HeightGrid;
		const bool bHasHeightGrid =
			ArcTerrainTileAccess::ResolveGrid(Terrain, EArcTerrainRail::Height, 0, HeightGrid);
		for (const FArcTerrainTileAuthorityEvent& Event : Events)
		{
			if (Event.Reason == EArcTerrainTileAuthorityReason::PendingHeightReset
				|| Event.Reason == EArcTerrainTileAuthorityReason::LayoutChanged)
			{
				for (const FArcTerrainFarBlockKey& Key : State.FarBlockWindowUnion)
				{
					if (FArcTerrainFarBlockPatch* Patch = State.FarBlockPatches.Find(Key))
					{
						++Patch->SourceRevision;
						++Patch->DirtySerial;
					}
					EnqueueReady(State, Terrain, Key);
				}
				continue;
			}
			if (Event.Reason == EArcTerrainTileAuthorityReason::StampBoundsChanged
				|| Event.Rail != EArcTerrainRail::Height || Event.Tier != 0 || !bHasHeightGrid)
			{
				continue;
			}
			TArray<FArcTerrainCellKey> Covered;
			ArcTerrain::CellsIntersectingLocalBounds(TileLocalBounds(HeightGrid, Event.TileKey), Config,
				Covered);
			TSet<FArcTerrainFarBlockKey> WakeKeys;
			for (const FArcTerrainCellKey& CellKey : Covered)
			{
				const FArcTerrainFarBlockKey BlockKey = BlockKeyFromCell(CellKey, BlockCells);
				if (State.FarBlockWindowUnion.Contains(BlockKey))
				{
					WakeKeys.Add(BlockKey);
				}
			}
			if (TSet<FArcTerrainFarBlockKey>* Dependents =
				State.FarHeightTileDependents.Find(Event.TileKey))
			{
				for (const FArcTerrainFarBlockKey& Dependent : *Dependents)
				{
					WakeKeys.Add(Dependent);
				}
			}
			for (const FArcTerrainFarBlockKey& Key : WakeKeys)
			{
				if (FArcTerrainFarBlockPatch* Patch = State.FarBlockPatches.Find(Key))
				{
					++Patch->SourceRevision;
					++Patch->DirtySerial;
				}
				EnqueueReady(State, Terrain, Key);
			}
		}
	}

	if (bRetryDue)
	{
		double NextRetry = 0.0;
		while (State.FarBlockRetryHeap.Num() > 0)
		{
			const FArcTerrainFarBlockRetryItem& Top = State.FarBlockRetryHeap.HeapTop();
			if (Top.RetryAfter > Now)
			{
				NextRetry = Top.RetryAfter;
				break;
			}
			FArcTerrainFarBlockRetryItem Due;
			State.FarBlockRetryHeap.HeapPop(Due,
				[](const FArcTerrainFarBlockRetryItem& A, const FArcTerrainFarBlockRetryItem& B)
				{
					return A.RetryAfter < B.RetryAfter;
				}, EAllowShrinking::No);
			if (State.FarBlockWindowUnion.Contains(Due.Key))
			{
				EnqueueReady(State, Terrain, Due.Key);
			}
		}
		State.NextFarBlockRetry = NextRetry;
	}

	if (State.bFarBlockRanksDirty)
	{
		RebuildRanks(State, Terrain);
	}

	const int32 MaxFarBuilds = FMath::Max(Terrain.MaxFarBlockBuildsPerFrame, 1);
	if (FArcTerrainRuntimeWorkCoordinator* Coordinator = FArcTerrainRuntimeWorkCoordinator::TryGet())
	{
		Coordinator->SetTerrainLaneLaunchLimit(GetRuntimeOwnerId(), State.CollisionTerrainId,
			EArcTerrainRuntimeLane::Far, MaxFarBuilds);
	}

	if (State.FarBlockReadyOrdered.Num() == 0)
	{
		return;
	}

	FArcTerrainTileGrid HeightGrid;
	const bool bHasHeightGrid =
		ArcTerrainTileAccess::ResolveGrid(Terrain, EArcTerrainRail::Height, 0, HeightGrid);
	const int32 CellsPerBlock = BlockCells * BlockCells;
	const float FarToleranceCm = FMath::Max(Terrain.LightingFarBlockToleranceCm, 0.0f);
	const float FarSkirtDepthCm = FMath::Max(Terrain.LightingSkirtToleranceMultiplier, 0.0f) * FarToleranceCm;
	TArray<FArcTerrainFarBlockKey> LaunchKeys;
	LaunchKeys.Reserve(State.FarBlockReadyOrdered.Num());
	for (const FArcTerrainFarBlockReadyItem& Item : State.FarBlockReadyOrdered)
	{
		FArcTerrainFarBlockPatch* Patch = State.FarBlockPatches.Find(Item.Key);
		if (Patch == nullptr)
		{
			LaunchKeys.Add(Item.Key);
			continue;
		}
		if (Patch->InFlight.IsValid())
		{
			continue;
		}
		LaunchKeys.Add(Item.Key);
	}

	const double SnapshotStartSeconds = ArcTerrainLeafTiming_8f21::Begin();
	for (const FArcTerrainFarBlockKey& BlockKey : LaunchKeys)
	{
		RemoveReady(State, BlockKey);
		FArcTerrainFarBlockPatch* PatchPtr = State.FarBlockPatches.Find(BlockKey);
		if (PatchPtr == nullptr)
		{
			continue;
		}
		FArcTerrainFarBlockPatch& Patch = *PatchPtr;
		if (Patch.InFlight.IsValid())
		{
			EnqueueReady(State, Terrain, BlockKey);
			continue;
		}

		const double MinX = static_cast<double>(BlockKey.X) * BlockSize;
		const double MinY = static_cast<double>(BlockKey.Y) * BlockSize;
		const double DistSq = RectInvokerDistSq(
			MinX, MinY, MinX + BlockSize, MinY + BlockSize, TerrainTransform,
			State.FarBlockSelectionCache.Positions);
		Patch.SinkBand = ResolveFarSinkBandWithHysteresis(FMath::Sqrt(DistSq), FarSinkBandWidth,
			Terrain.LightingRadiusHysteresis, Patch.SinkBand);
		const float SinkCm = FarSinkCm(Patch.SinkBand, FarSinkPerDistance, FarSinkBandWidth);

		const FArcTerrainCellKey OriginCell = BlockOriginCell(BlockKey, BlockCells);
		bool bBlockResident = true;
		uint32 SourceHash = 0;
		TArray<FTileSourceCapture> CellSources;
		CellSources.Reserve(CellsPerBlock);
		TArray<FIntPoint> DeferredTiles;
		for (int32 LocalIndex = 0; LocalIndex < CellsPerBlock && bBlockResident; ++LocalIndex)
		{
			const FArcTerrainCellKey CellKey(
				OriginCell.X + LocalIndex % BlockCells, OriginCell.Y + LocalIndex / BlockCells);
			FTileSourceCapture HeightSource;
			FIntPoint HeightTileKey = FIntPoint::ZeroValue;
			if (bHasHeightGrid)
			{
				HeightTileKey = ArcTerrainTileAccess::TileKeyFromCell(HeightGrid, CellKey);
				HeightSource = CaptureTileSource(Terrain, EArcTerrainRail::Height, 0, HeightTileKey, &HeightGrid);
			}
			if (HeightSource.State == ETileSourceState::KnownUnloaded
				|| (HeightSource.State == ETileSourceState::Absent
					&& Patch.LoadedHeightTiles.Contains(HeightTileKey)))
			{
				DeferredTiles.Add(HeightTileKey);
				bBlockResident = false;
				continue;
			}
			SourceHash = HashCombine(SourceHash, HeightSource.PayloadId);
			CellSources.Add(MoveTemp(HeightSource));
		}

		if (!bBlockResident)
		{
			for (const FIntPoint& TileKey : DeferredTiles)
			{
				State.FarHeightTileDependents.FindOrAdd(TileKey).Add(BlockKey);
			}
			ArcTerrainDiag::NoteFarDeferred();
			continue;
		}

		if (Patch.Component.IsValid()
			&& Patch.AppliedDirtySerial == Patch.DirtySerial
			&& Patch.AppliedSourceHash == SourceHash
			&& Patch.AppliedSinkCm == SinkCm)
		{
			continue;
		}

		if (IsFarBuildLatched(Patch, SourceHash, FarToleranceCm, HeightQuads))
		{
			continue;
		}

		FArcTerrainFarBlockBuildPtr Build = MakeShared<FArcTerrainFarBlockBuild, ESPMode::ThreadSafe>();
		Build->BlockKey = BlockKey;
		Build->BlockOriginCell = OriginCell;
		Build->SourceQuads = HeightQuads;
		Build->CellSize = CellSize;
		Build->ToleranceCm = FarToleranceCm;
		Build->SkirtDepthCm = FarSkirtDepthCm;
		Build->SinkCm = SinkCm;
		Build->OwnerId = GetUniqueID();
		Build->TerrainId = State.CollisionTerrainId;
		Build->PatchGeneration = Patch.PatchGeneration;
		Build->ConfigGeneration = State.FarBlockConfigGeneration;
		Build->SourceRevision = Patch.SourceRevision;
		Build->BuiltDirtySerial = Patch.DirtySerial;
		Build->BuiltSourceHash = SourceHash;

		for (int32 LocalIndex = 0; LocalIndex < CellsPerBlock; ++LocalIndex)
		{
			const FArcTerrainCellKey CellKey(
				OriginCell.X + LocalIndex % BlockCells, OriginCell.Y + LocalIndex / BlockCells);
			const FTileSourceCapture& HeightSource = CellSources[LocalIndex];
			if (HeightSource.State == ETileSourceState::Loaded && bHasHeightGrid)
			{
				Patch.LoadedHeightTiles.Add(ArcTerrainTileAccess::TileKeyFromCell(HeightGrid, CellKey));
			}
		}

		FArcTerrainRuntimeWorkCoordinator* Coordinator = FArcTerrainRuntimeWorkCoordinator::TryGet();
		if (Coordinator == nullptr)
		{
			EnqueueReady(State, Terrain, BlockKey);
			continue;
		}
		FArcTerrainRuntimeReservationRequest Request;
		Request.OwnerId = GetRuntimeOwnerId();
		Request.TerrainId = State.CollisionTerrainId;
		Request.Lane = EArcTerrainRuntimeLane::Far;
		Request.ExactOwnedInputBytes = sizeof(FArcTerrainFarBlockBuild)
			+ static_cast<uint64>(CellsPerBlock) * sizeof(FTileSourceCapture);
		const int32 HeightApron = HeightQuads + 3;
		Request.PreReservedResultBytes =
			static_cast<uint64>(CellsPerBlock) * HeightApron * HeightApron * sizeof(float)
			+ static_cast<uint64>(CellsPerBlock) * HeightApron * HeightApron
				* (sizeof(FVector3f) * 4u + sizeof(FVector2f) * 3u)
			+ static_cast<uint64>(CellsPerBlock) * HeightApron * HeightApron * 6u * sizeof(uint32);
		for (const FTileSourceCapture& HeightSource : CellSources)
		{
			if (HeightSource.Payload.IsValid())
			{
				FArcTerrainRetainedPayload Retain;
				Retain.PayloadId = HeightSource.PayloadId;
				Retain.Bytes = HeightSource.Payload->RetainedBytes();
				Retain.Payload = HeightSource.Payload;
				Request.Payloads.Add(Retain);
			}
		}
		FArcTerrainRuntimeReservationToken Token;
		if (!Coordinator->TryReserveSubmission(Request, Token))
		{
			EnqueueReady(State, Terrain, BlockKey);
			continue;
		}

		const float FlatHeightCm = Terrain.HeightOptions.BaseHeightCm;
		const int32 ExpectedHeightRes = HeightQuads + 3;
		Patch.InFlight = Build;
		ArcTerrainDiag::NoteFarInFlight();
		TSharedPtr<FArcTerrainFarBlockWorkerResult, ESPMode::ThreadSafe> WorkerResult =
			MakeShared<FArcTerrainFarBlockWorkerResult, ESPMode::ThreadSafe>();
		WorkerResult->Build = Build;
		Coordinator->Submit(MoveTemp(Token), UE::Tasks::ETaskPriority::BackgroundNormal,
			[WorkerResult, Build, HeightGrid, CellSources, BlockCells, OriginCell, FlatHeightCm,
				ExpectedHeightRes](FArcTerrainRuntimeWorkContext& Context)
			{
				if (Context.IsAbandoned())
				{
					FArcTerrainRuntimeResult Result;
					Result.Status = EArcTerrainRuntimeWorkStatus::Cancelled;
					Result.Payload = WorkerResult;
					return Result;
				}
				const int32 CellCount = CellSources.Num();
				Build->Sources.Reserve(CellCount);
				for (int32 LocalIndex = 0; LocalIndex < CellCount; ++LocalIndex)
				{
					const FArcTerrainCellKey CellKey(
						OriginCell.X + LocalIndex % BlockCells, OriginCell.Y + LocalIndex / BlockCells);
					const FTileSourceCapture& HeightSource = CellSources[LocalIndex];
					ArcTerrainLightingMesh::FFarBlockSource Source;
					Source.Key = CellKey;
					if (HeightSource.State == ETileSourceState::Loaded && HeightSource.Payload.IsValid())
					{
						int32 Resolution = 0;
						if (!ArcTerrainTileAccess::ExtractCellFloats(HeightGrid, *HeightSource.Payload, 0,
								CellKey, Source.HeightsWithApron, Resolution)
							|| Resolution != ExpectedHeightRes)
						{
							WorkerResult->bCutFailed = true;
							WorkerResult->FailedCell = CellKey;
							WorkerResult->ObservedResolution = Resolution;
							break;
						}
						Source.bFlat = false;
					}
					else
					{
						Source.bFlat = true;
						Source.FlatHeightCm = FlatHeightCm;
					}
					Build->Sources.Add(MoveTemp(Source));
				}
				if (!WorkerResult->bCutFailed)
				{
					ArcTerrainLightingMesh::BuildFarBlockMesh(Build->Sources, Build->SourceQuads,
						Build->CellSize, Build->BlockOriginCell, Build->ToleranceCm, Build->SkirtDepthCm,
						Build->SinkCm, Build->Mesh);
				}
				FArcTerrainRuntimeResult Result;
				Result.Status = Context.IsAbandoned()
					? EArcTerrainRuntimeWorkStatus::Cancelled
					: EArcTerrainRuntimeWorkStatus::Succeeded;
				Result.Payload = WorkerResult;
				return Result;
			});
	}
	ArcTerrainLeafTiming_8f21::End(ArcTerrainDiag::ELeafTiming::FarPayloadSnapshot,
		SnapshotStartSeconds);
}
void UArcTerrainSubsystem::ApplyFarBlockBuild(AArcTerrain& Terrain, FArcTerrainFarBlockPatch& Patch,
	FArcTerrainFarBlockBuild& Build)
{
	using namespace ArcTerrainFarBlockLocal_9a56;

	if (Build.Mesh.Indices.IsEmpty())
	{
		LatchFarBuildFailure(Patch, Build.BuiltDirtySerial, Build.BuiltSourceHash, Build.ToleranceCm,
			Build.SourceQuads);
		ArcTerrainDiag::NoteFarStale();
		return;
	}

	UArcTerrainFarOcclusionComponent* Component = Patch.Component.Get();
	const bool bNewComponent = Component == nullptr;
	if (bNewComponent)
	{
		Component = NewObject<UArcTerrainFarOcclusionComponent>(&Terrain, NAME_None, RF_Transient);
	}

	Component->SetMesh(MoveTemp(Build.Mesh));
	Component->SetRelativeLocation(FVector(
		static_cast<double>(Build.BlockOriginCell.X) * Build.CellSize,
		static_cast<double>(Build.BlockOriginCell.Y) * Build.CellSize,
		0.0));

	if (bNewComponent)
	{
		Component->SetupAttachment(Terrain.GetRootComponent());
		Component->RegisterComponent();
		Patch.Component = Component;
	}

	Patch.AppliedDirtySerial = Build.BuiltDirtySerial;
	Patch.AppliedSourceHash = Build.BuiltSourceHash;
	Patch.AppliedSinkCm = Build.SinkCm;
	Patch.bBuildFailed = false;
	ArcTerrainDiag::NoteFarCompletion();
}

void UArcTerrainSubsystem::DestroyFarBlockPatch(FArcTerrainFarBlockPatch& Patch)
{
	if (UArcTerrainFarOcclusionComponent* Component = Patch.Component.Get())
	{
		Component->DestroyComponent();
	}
	Patch.Component = nullptr;
	Patch.InFlight.Reset();
	Patch.AppliedDirtySerial = 0;
	Patch.AppliedSourceHash = 0;
	Patch.AppliedSinkCm = 0.0f;
	Patch.SinkBand = INDEX_NONE;
	Patch.LoadedHeightTiles.Reset();
	Patch.bBuildFailed = false;
	Patch.FailedDirtySerial = 0;
	Patch.FailedSourceHash = 0;
	Patch.FailedToleranceCm = 0.0f;
	Patch.FailedSourceQuads = 0;
}

void UArcTerrainSubsystem::BakeLightingDerivedKnobs(AArcTerrain& Terrain)
{
	const double DetailPerDistance =
		static_cast<double>(FMath::Max(Terrain.LightingDetailPerDistance, 0.0f));
	const double SinkPerDistance = static_cast<double>(FMath::Max(Terrain.LightingSinkPerDistance, 0.0f));
	const double LightingRadius = static_cast<double>(FMath::Max(Terrain.LightingRadius, 0.0f));
	const double FarRadius = static_cast<double>(FMath::Max(Terrain.LightingFarRadius, 0.0f));
	const double NearBandEdgeCm = LightingRadius
		* static_cast<double>(FMath::Clamp(Terrain.LightingNearBandFraction, 0.0f, 1.0f));
	const double Hysteresis = static_cast<double>(FMath::Max(Terrain.LightingRadiusHysteresis, 1.0f));
	const double CellSize = Terrain.Config.CellSize();
	const double BlockSize = CellSize * static_cast<double>(FMath::Max(Terrain.LightingFarBlockCells, 1));

	Terrain.LightingNearToleranceCm = static_cast<float>(DetailPerDistance * NearBandEdgeCm);
	Terrain.LightingMidToleranceCm = static_cast<float>(DetailPerDistance * LightingRadius);
	Terrain.LightingFarBlockToleranceCm = static_cast<float>(DetailPerDistance * FarRadius);
	Terrain.LightingNearSinkPerDistance = static_cast<float>(SinkPerDistance);
	Terrain.LightingFarSinkPerDistance = static_cast<float>(SinkPerDistance);

	const double NearRatio = ArcTerrainLightingLocal_4d17::MaxSkirtToleranceRatio(
		ArcTerrainLightingLocal_4d17::GNearSinkBandCount,
		LightingRadius / static_cast<double>(ArcTerrainLightingLocal_4d17::GNearSinkBandCount),
		LightingRadius * Hysteresis, CellSize * UE_DOUBLE_SQRT_2, Hysteresis, SinkPerDistance,
		NearBandEdgeCm, static_cast<double>(Terrain.LightingNearToleranceCm),
		static_cast<double>(Terrain.LightingMidToleranceCm));
	const double FarRatio = ArcTerrainLightingLocal_4d17::MaxSkirtToleranceRatio(
		ArcTerrainFarBlockLocal_9a56::GFarSinkBandCount,
		FarRadius / static_cast<double>(ArcTerrainFarBlockLocal_9a56::GFarSinkBandCount),
		FarRadius, BlockSize * UE_DOUBLE_SQRT_2, Hysteresis, SinkPerDistance, FarRadius,
		static_cast<double>(Terrain.LightingFarBlockToleranceCm),
		static_cast<double>(Terrain.LightingFarBlockToleranceCm));

	Terrain.LightingSkirtToleranceMultiplier = static_cast<float>(FMath::Max(NearRatio, FarRatio));
}

void UArcTerrainSubsystem::ApplyLightingRuntimeCompletion(FArcTerrainRuntimeCompletion&& Completion)
{
	using namespace ArcTerrainLightingLocal_4d17;
	using namespace ArcTerrainLightingIndex_6b19;

	TSharedPtr<FArcTerrainLightingWorkerResult, ESPMode::ThreadSafe> WorkerResult =
		StaticCastSharedPtr<FArcTerrainLightingWorkerResult>(Completion.Payload);
	Completion.Reservation.Release();
	if (!WorkerResult.IsValid() || !WorkerResult->Build.IsValid())
	{
		return;
	}

	FArcTerrainLightingBuild& Build = *WorkerResult->Build;
	for (TPair<TWeakObjectPtr<AArcTerrain>, FArcTerrainState>& TerrainPair : Terrains)
	{
		AArcTerrain* Terrain = TerrainPair.Key.Get();
		if (Terrain == nullptr || TerrainPair.Value.CollisionTerrainId != Build.TerrainId)
		{
			continue;
		}
		FArcTerrainLightingPatch* Patch = TerrainPair.Value.LightingPatches.Find(Build.Key);
		if (Patch == nullptr || Patch->InFlight != WorkerResult->Build)
		{
			return;
		}
		Patch->InFlight.Reset();
		const double ApplyStartSeconds = ArcTerrainLeafTiming_8f21::Begin();
		if (Completion.Status == EArcTerrainRuntimeWorkStatus::Cancelled
			|| Completion.Status == EArcTerrainRuntimeWorkStatus::BudgetExceeded)
		{
			EnqueueReady(TerrainPair.Value, *Terrain, Build.Key);
			ArcTerrainLeafTiming_8f21::End(ArcTerrainDiag::ELeafTiming::LightingCompletionApply,
				ApplyStartSeconds);
			break;
		}
		if (WorkerResult->bHeightCutFailed)
		{
			ensureMsgf(false,
				TEXT("ArcTerrain '%s': cell (%d,%d) could not be cut from its loaded height tile at ")
				TEXT("resolution %d (expected %d); it gets no lighting representation."),
				*Terrain->GetName(), Build.Key.X, Build.Key.Y, WorkerResult->ObservedHeightResolution,
				Build.SourceHeightQuads + 3);
			LatchBuildFailure(*Patch, Build.BuiltDirtySerial, Build.BuiltHeightPayload,
				Build.BuiltWeightPayload, Build.ToleranceCm, Build.SourceHeightQuads,
				Build.SourceWeightQuads);
			ArcTerrainLeafTiming_8f21::End(ArcTerrainDiag::ELeafTiming::LightingCompletionApply,
				ApplyStartSeconds);
			break;
		}
		if (WorkerResult->bWeightCutFailed)
		{
			ensureMsgf(false,
				TEXT("ArcTerrain '%s': cell (%d,%d) could not cut its weight planes from the loaded ")
				TEXT("weight tile at resolution %d; it gets no lighting representation."),
				*Terrain->GetName(), Build.Key.X, Build.Key.Y, Build.SourceWeightQuads + 3);
			LatchBuildFailure(*Patch, Build.BuiltDirtySerial, Build.BuiltHeightPayload,
				Build.BuiltWeightPayload, Build.ToleranceCm, Build.SourceHeightQuads,
				Build.SourceWeightQuads);
			ArcTerrainLeafTiming_8f21::End(ArcTerrainDiag::ELeafTiming::LightingCompletionApply,
				ApplyStartSeconds);
			break;
		}
		if (WorkerResult->bWeightUnresolvable)
		{
			LogWeightUnresolvableOnce(*Terrain, Build.Key);
		}
		if (Patch->PatchGeneration != Build.PatchGeneration
			|| TerrainPair.Value.LightingConfigGeneration != Build.ConfigGeneration
			|| TerrainPair.Value.LightingMaterialRevision != Build.BuiltMaterialRevision
			|| Patch->SourceRevision != Build.SourceRevision
			|| Patch->DirtySerial != Build.BuiltDirtySerial)
		{
			ArcTerrainDiag::NoteLightingStale();
			ArcTerrainLeafTiming_8f21::End(ArcTerrainDiag::ELeafTiming::LightingCompletionApply,
				ApplyStartSeconds);
			break;
		}
		ApplyLightingBuild(*Terrain, Build.Key, *Patch, Build);
		ArcTerrainLeafTiming_8f21::End(ArcTerrainDiag::ELeafTiming::LightingCompletionApply,
			ApplyStartSeconds);
		break;
	}
}

void UArcTerrainSubsystem::ApplyFarRuntimeCompletion(FArcTerrainRuntimeCompletion&& Completion)
{
	using namespace ArcTerrainFarBlockLocal_9a56;
	using namespace ArcTerrainFarBlockIndex_2c08;

	TSharedPtr<FArcTerrainFarBlockWorkerResult, ESPMode::ThreadSafe> WorkerResult =
		StaticCastSharedPtr<FArcTerrainFarBlockWorkerResult>(Completion.Payload);
	Completion.Reservation.Release();
	if (!WorkerResult.IsValid() || !WorkerResult->Build.IsValid())
	{
		return;
	}

	FArcTerrainFarBlockBuild& Build = *WorkerResult->Build;
	for (TPair<TWeakObjectPtr<AArcTerrain>, FArcTerrainState>& TerrainPair : Terrains)
	{
		AArcTerrain* Terrain = TerrainPair.Key.Get();
		if (Terrain == nullptr || TerrainPair.Value.CollisionTerrainId != Build.TerrainId)
		{
			continue;
		}
		FArcTerrainFarBlockPatch* Patch = TerrainPair.Value.FarBlockPatches.Find(Build.BlockKey);
		if (Patch == nullptr || Patch->InFlight != WorkerResult->Build)
		{
			return;
		}
		Patch->InFlight.Reset();
		const double ApplyStartSeconds = ArcTerrainLeafTiming_8f21::Begin();
		if (Completion.Status == EArcTerrainRuntimeWorkStatus::Cancelled
			|| Completion.Status == EArcTerrainRuntimeWorkStatus::BudgetExceeded)
		{
			EnqueueReady(TerrainPair.Value, *Terrain, Build.BlockKey);
			ArcTerrainLeafTiming_8f21::End(ArcTerrainDiag::ELeafTiming::FarCompletionApply,
				ApplyStartSeconds);
			break;
		}
		if (WorkerResult->bCutFailed)
		{
			ensureMsgf(false,
				TEXT("ArcTerrain '%s': far block (%d,%d) could not cut cell (%d,%d) from its loaded ")
				TEXT("height tile at resolution %d (expected %d); the block gets no occlusion mesh."),
				*Terrain->GetName(), Build.BlockKey.X, Build.BlockKey.Y,
				WorkerResult->FailedCell.X, WorkerResult->FailedCell.Y,
				WorkerResult->ObservedResolution, Build.SourceQuads + 3);
			LatchFarBuildFailure(*Patch, Build.BuiltDirtySerial, Build.BuiltSourceHash, Build.ToleranceCm,
				Build.SourceQuads);
			ArcTerrainLeafTiming_8f21::End(ArcTerrainDiag::ELeafTiming::FarCompletionApply,
				ApplyStartSeconds);
			break;
		}
		if (Patch->PatchGeneration != Build.PatchGeneration
			|| TerrainPair.Value.FarBlockConfigGeneration != Build.ConfigGeneration
			|| Patch->SourceRevision != Build.SourceRevision
			|| Patch->DirtySerial != Build.BuiltDirtySerial)
		{
			ArcTerrainDiag::NoteFarStale();
			ArcTerrainLeafTiming_8f21::End(ArcTerrainDiag::ELeafTiming::FarCompletionApply,
				ApplyStartSeconds);
			break;
		}
		ApplyFarBlockBuild(*Terrain, *Patch, Build);
		ArcTerrainLeafTiming_8f21::End(ArcTerrainDiag::ELeafTiming::FarCompletionApply,
			ApplyStartSeconds);
		break;
	}
}
