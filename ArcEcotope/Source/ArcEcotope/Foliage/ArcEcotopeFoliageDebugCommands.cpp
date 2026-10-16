// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcEcotopeModule.h"
#include "ArcTerrainActor.h"
#include "ArcTerrainSubsystem.h"
#include "ArcTerrainTypes.h"
#include "Engine/Engine.h"
#include "Engine/Level.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Foliage/ArcEcotopeFoliageAtlas.h"
#include "Foliage/ArcEcotopeFoliageFields.h"
#include "Foliage/ArcEcotopeFoliagePrimitive.h"
#include "Foliage/ArcEcotopeFoliageWriteShader.h"
#include "Foliage/ArcEcotopeGroundFoliage.h"
#include "GlobalShader.h"
#include "GPUSceneWriter.h"
#include "HAL/IConsoleManager.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "UObject/UObjectGlobals.h"

namespace ArcEcotopeFoliageDebugCommandsPrivate
{
	constexpr int32 GridSize = 32;
	const TCHAR* const DebugMeshPath = TEXT("/Engine/BasicShapes/Cube.Cube");

	/** Spacing decides whether the lattice spans more than one terrain cell, which is what makes the
	 *  "no foliage over a cell with no baked section" half of the proof observable at all. */
	TAutoConsoleVariable<float> CVarDebugSpacingCm(
		TEXT("arcecotope.Foliage.DebugSpacingCm"),
		200.0f,
		TEXT("Lattice spacing, cm, used by arcecotope.Foliage.DebugSpawn."),
		ECVF_Default);

	TAutoConsoleVariable<float> CVarDebugMaxSlopeDegrees(
		TEXT("arcecotope.Foliage.DebugMaxSlopeDegrees"),
		35.0f,
		TEXT("Slope rejection threshold, degrees, used by arcecotope.Foliage.DebugSpawn."),
		ECVF_Default);

	TAutoConsoleVariable<float> CVarDebugMinScale(
		TEXT("arcecotope.Foliage.DebugMinScale"),
		0.8f,
		TEXT("Lower end of the uniform instance scale used by arcecotope.Foliage.DebugSpawn."),
		ECVF_Default);

	TAutoConsoleVariable<float> CVarDebugMaxScale(
		TEXT("arcecotope.Foliage.DebugMaxScale"),
		1.25f,
		TEXT("Upper end of the uniform instance scale used by arcecotope.Foliage.DebugSpawn."),
		ECVF_Default);

	TAutoConsoleVariable<float> CVarDebugMaxYawDeg(
		TEXT("arcecotope.Foliage.DebugMaxYawDeg"),
		180.0f,
		TEXT("Yaw is drawn from +/- this many degrees by arcecotope.Foliage.DebugSpawn."),
		ECVF_Default);

	TAutoConsoleVariable<int32> CVarDebugAlignToNormal(
		TEXT("arcecotope.Foliage.DebugAlignToNormal"),
		1,
		TEXT("Non-zero puts the instance up axis on the sampled terrain normal in arcecotope.Foliage.DebugSpawn."),
		ECVF_Default);

	TAutoConsoleVariable<FString> CVarDebugLayerChannel(
		TEXT("arcecotope.Foliage.DebugLayerChannel"),
		TEXT(""),
		TEXT("Terrain layer channel name the arcecotope.Foliage.DebugSpawn filter samples; empty disables the filter."),
		ECVF_Default);

	TAutoConsoleVariable<float> CVarDebugMinLayerWeight(
		TEXT("arcecotope.Foliage.DebugMinLayerWeight"),
		0.25f,
		TEXT("Minimum layer weight arcecotope.Foliage.DebugSpawn accepts when a layer channel is set."),
		ECVF_Default);

	TUniquePtr<FArcEcotopeFoliagePrimitive> DebugPrimitive;
	TWeakObjectPtr<UWorld> DebugPrimitiveWorld;
	FDelegateHandle WorldCleanupHandle;

	void ClearDebugPrimitive()
	{
		if (DebugPrimitive)
		{
			FArcEcotopeFoliagePrimitive::Release(MoveTemp(DebugPrimitive));
		}
		DebugPrimitiveWorld.Reset();

		if (WorldCleanupHandle.IsValid())
		{
			FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
			WorldCleanupHandle.Reset();
		}
	}

	void HandleWorldCleanup(UWorld* InWorld, bool /*bSessionEnded*/, bool /*bCleanupResources*/)
	{
		if (DebugPrimitiveWorld.Get() == InWorld)
		{
			ClearDebugPrimitive();
		}
	}

	/** First terrain with a finite, positive CellSize. */
	AArcTerrain* FindTerrain(UWorld& InWorld)
	{
		for (TActorIterator<AArcTerrain> TerrainIt(&InWorld); TerrainIt; ++TerrainIt)
		{
			AArcTerrain* Terrain = *TerrainIt;
			if (!IsValid(Terrain))
			{
				continue;
			}

			const FArcTerrainConfig& Config = Terrain->Config;
			if (!FMath::IsFinite(Config.CellSize()) || Config.CellSize() <= 0.0)
			{
				continue;
			}

			return Terrain;
		}

		return nullptr;
	}

	void LogAtlas(const AArcTerrain& Terrain, const FArcEcotopeFoliageTerrainData& Data)
	{
		FString Residency;
		for (int32 SlotIndex = 0; SlotIndex < Data.Slots.Num(); ++SlotIndex)
		{
			const FIntPoint Key = Data.AtlasCellCount.X > 0
				? FIntPoint(Data.AtlasOriginCell.X + SlotIndex % Data.AtlasCellCount.X,
					Data.AtlasOriginCell.Y + SlotIndex / Data.AtlasCellCount.X)
				: FIntPoint::ZeroValue;
			Residency += FString::Printf(TEXT(" (%d,%d)=%s"), Key.X, Key.Y,
				Data.Slots[SlotIndex].bResident ? TEXT("resident") : TEXT("hole"));
		}

		UE_LOGF(LogArcEcotope, Log,"[FoliageDebug] terrain='%ls' atlas %dx%d heightTile=%.0f subRect=%dx%d heightCm=(%.1f, %.1f) cells:%ls",
			*Terrain.GetName(), Data.AtlasCellCount.X, Data.AtlasCellCount.Y, Data.HeightTileRes,
			Data.HeightSubRect.X, Data.HeightSubRect.Y, Data.LocalHeightMinCm, Data.LocalHeightMaxCm,
			*Residency);
	}

	void SpawnDebugPrimitive(UWorld* InWorld)
	{
		if (InWorld == nullptr || InWorld->Scene == nullptr)
		{
			UE_LOGF(LogArcEcotope, Warning,"[FoliageDebug] No renderable world.");
			return;
		}

		ClearDebugPrimitive();

		UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, DebugMeshPath);
		if (Mesh == nullptr)
		{
			UE_LOGF(LogArcEcotope, Warning,"[FoliageDebug] Failed to load %ls.", DebugMeshPath);
			return;
		}

		AArcTerrain* Terrain = FindTerrain(*InWorld);
		UArcTerrainSubsystem* TerrainSubsystem = InWorld->GetSubsystem<UArcTerrainSubsystem>();
		if (Terrain == nullptr || TerrainSubsystem == nullptr)
		{
			UE_LOGF(LogArcEcotope, Warning,"[FoliageDebug] No usable AArcTerrain in %ls.", *InWorld->GetName());
			return;
		}

		const float GridStepCm = FMath::Max(CVarDebugSpacingCm.GetValueOnGameThread(), 1.0f);

		// The lattice is anchored at the WORLD origin, not at the region: cell (0,0) spans
		// [0, GridStepCm) whatever the region covers, which is what keeps a plant's world cell — and so
		// its random draws — fixed while the region moves.
		const FIntPoint RegionOriginCell(-GridSize / 2, -GridSize / 2);
		const double RegionMinX = static_cast<double>(RegionOriginCell.X) * GridStepCm;
		const double RegionMinY = static_cast<double>(RegionOriginCell.Y) * GridStepCm;
		const double RegionMaxX = RegionMinX + static_cast<double>(GridSize) * GridStepCm;
		const double RegionMaxY = RegionMinY + static_cast<double>(GridSize) * GridStepCm;

		const float MinScale = FMath::Max(CVarDebugMinScale.GetValueOnGameThread(), 0.01f);
		const float MaxScale = FMath::Max(CVarDebugMaxScale.GetValueOnGameThread(), MinScale);
		const float MaxYawRad = FMath::DegreesToRadians(
			FMath::Clamp(CVarDebugMaxYawDeg.GetValueOnGameThread(), 0.0f, 180.0f));
		const float CosMaxSlope = FMath::Cos(FMath::DegreesToRadians(
			FMath::Clamp(CVarDebugMaxSlopeDegrees.GetValueOnGameThread(), 0.0f, 90.0f)));
		const float MinLayerWeight = FMath::Clamp(CVarDebugMinLayerWeight.GetValueOnGameThread(), 0.0f, 1.0f);
		const uint32 AlignToNormal = CVarDebugAlignToNormal.GetValueOnGameThread() != 0 ? 1u : 0u;

		const FString LayerChannelName = CVarDebugLayerChannel.GetValueOnGameThread();
		const FName LayerChannel = LayerChannelName.IsEmpty() ? NAME_None : FName(*LayerChannelName);

		constexpr double SampleHalfHeight = 1000000.0;
		const FBox SampleBounds(
			FVector(RegionMinX - GridStepCm, RegionMinY - GridStepCm, -SampleHalfHeight),
			FVector(RegionMaxX + GridStepCm, RegionMaxY + GridStepCm, SampleHalfHeight));

		FArcEcotopeFoliageTerrainData TerrainData;
		if (!ArcEcotopeFoliageAtlas::BuildTerrainData(*Terrain, SampleBounds, LayerChannel, TerrainData))
		{
			UE_LOGF(LogArcEcotope, Warning,"[FoliageDebug] Terrain '%ls' yielded no sampleable cell (layer channel '%ls'): no loaded height "
					 "tile under the lattice, the channel name does not resolve, or no loaded tile carries it. "
					 "The preceding LogArcEcotope lines say which.",
				*Terrain->GetName(), *LayerChannel.ToString());
			return;
		}

		LogAtlas(*Terrain, TerrainData);

		TUniquePtr<FArcEcotopeFoliagePrimitive> Primitive = MakeUnique<FArcEcotopeFoliagePrimitive>();

		// The renderer compares the primitive object's PIE prefix against the world's, so it must be a
		// world-owned object rather than the content-package mesh.
		if (!Primitive->Initialize(
				InWorld,
				InWorld->PersistentLevel,
				Mesh,
				TConstArrayView<TObjectPtr<UMaterialInterface>>(),
				FArcEcotopeFoliageRenderSettings()))
		{
			UE_LOGF(LogArcEcotope, Warning,"[FoliageDebug] Primitive initialization failed.");
			return;
		}

		const FTransform TerrainTransform = Terrain->GetTerrainTransform();
		const double WindowZA =
			TerrainTransform.TransformPosition(FVector(0.0, 0.0, TerrainData.LocalHeightMinCm)).Z;
		const double WindowZB =
			TerrainTransform.TransformPosition(FVector(0.0, 0.0, TerrainData.LocalHeightMaxCm)).Z;

		FBox LatticeBox(
			FVector(RegionMinX, RegionMinY, FMath::Min(WindowZA, WindowZB)),
			FVector(RegionMaxX, RegionMaxY, FMath::Max(WindowZA, WindowZB)));
		LatticeBox = LatticeBox.ExpandBy(Mesh->GetBounds().SphereRadius * MaxScale);

		Primitive->SetTransformAndBounds(FTransform::Identity, FBoxSphereBounds(LatticeBox));

		const int32 NumInstances = GridSize * GridSize;
		const int32 NumAllocatedInstances = Primitive->EnsureInstanceAllocation(NumInstances);
		if (NumAllocatedInstances <= 0 || Primitive->GetSceneProxy() == nullptr)
		{
			UE_LOGF(LogArcEcotope, Warning,"[FoliageDebug] Scene proxy was not created (mesh render data not ready?).");
			Primitive->RemoveFromScene();
			return;
		}

		Primitive->UpdateInstancesGPU(NumInstances,
			[NumInstances, NumAllocatedInstances, RegionOriginCell, GridStepCm, MinScale, MaxScale, MaxYawRad,
				CosMaxSlope, MinLayerWeight, AlignToNormal, TerrainData](FRDGBuilder& GraphBuilder, const FGPUSceneWriteDelegateParams& Params)
			{
				if (Params.PersistentPrimitiveId == static_cast<uint32>(INDEX_NONE))
				{
					return;
				}

				FArcEcotopeFoliageScatterCS::FParameters* Parameters =
					GraphBuilder.AllocParameters<FArcEcotopeFoliageScatterCS::FParameters>();
				Parameters->InPrimitiveId = Params.PersistentPrimitiveId;
				Parameters->InNumInstances = static_cast<uint32>(NumInstances);
				Parameters->InNumInstancesAllocated = static_cast<uint32>(NumAllocatedInstances);
				Parameters->InGridX = static_cast<uint32>(GridSize);
				Parameters->InPayloadDataFlags = Params.PackedInstanceSceneDataFlags;
				Parameters->InCustomDataCount = Params.NumCustomDataFloats;
				Parameters->InAlignToNormal = AlignToNormal;
				Parameters->InMeshIndex = 0u;
				Parameters->InMeshCount = 1u;
				Parameters->InRegionOriginCell = RegionOriginCell;
				Parameters->InGridStepCm = GridStepCm;
				Parameters->InProbeHeightWS = 0.0f;
				Parameters->InMinScale = MinScale;
				Parameters->InMaxScale = MaxScale;
				Parameters->InMaxYawRad = MaxYawRad;
				Parameters->InCosMaxSlope = CosMaxSlope;
				Parameters->InMinLayerWeight = MinLayerWeight;
				// The debug spawn belongs to no volume: zero strength makes the canopy term one, and the
				// null field leaves the moisture and fertility gates open.
				Parameters->InUnderstoryShade = 1.0f;
				Parameters->InShadePreference = 0.0f;
				Parameters->InShadeStrength = 0.0f;
				Parameters->InMinMoisture = 0.0f;
				Parameters->InMinFertility = 0.0f;
				ArcEcotopeFoliageAtlas::BuildTerrainAtlas(GraphBuilder, TerrainData, Parameters->TerrainParameters);
				ArcEcotopeFoliageFields::BindField(GraphBuilder, FArcEcotopeFoliageFieldRef(), Parameters->FieldParameters);
				// GPUWriteParams is the deprecated field, not the struct type: the opaque UB replacing it
				// is unusable on DX11 (ENABLE_SCENE_DATA_DX11_UB_ERROR_WORKAROUND).
				PRAGMA_DISABLE_DEPRECATION_WARNINGS
				Parameters->GPUSceneWriterParameters = Params.GPUWriteParams;
				PRAGMA_ENABLE_DEPRECATION_WARNINGS

				TShaderMapRef<FArcEcotopeFoliageScatterCS> Shader(GetGlobalShaderMap(GMaxRHIShaderPlatform));

				// Sized from the ALLOCATED span, not the live count: the tail slots a shrink left behind
				// still have to be written, and the kernel hides them.
				const FIntVector GroupCount = FComputeShaderUtils::GetGroupCountWrapped(
					FMath::DivideAndRoundUp<int32>(NumAllocatedInstances, FArcEcotopeFoliageScatterCS::NumThreadsPerGroup));

				FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("ArcEcotopeFoliageScatter"),
					ERDGPassFlags::Compute | ERDGPassFlags::NeverCull, Shader, Parameters, GroupCount);
			});

		DebugPrimitive = MoveTemp(Primitive);
		DebugPrimitiveWorld = InWorld;
		WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddStatic(&HandleWorldCleanup);

		UE_LOGF(LogArcEcotope, Log,"[FoliageDebug] Scattered %dx%d cells (%d instances, %d allocated, %.0f cm step, layer '%ls') on terrain '%ls' in %ls.",
			GridSize, GridSize, NumInstances, NumAllocatedInstances, GridStepCm, *LayerChannel.ToString(),
			*Terrain->GetName(), *InWorld->GetName());
	}

	FAutoConsoleCommandWithWorld SpawnCommand(
		TEXT("arcecotope.Foliage.DebugSpawn"),
		TEXT("Spawns one GPU-only foliage primitive and scatters a 32x32 world lattice onto the terrain surface around the origin."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&SpawnDebugPrimitive));

	FAutoConsoleCommandWithWorld ClearCommand(
		TEXT("arcecotope.Foliage.DebugClear"),
		TEXT("Removes the debug foliage primitive."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* /*InWorld*/) { ClearDebugPrimitive(); }));
}
