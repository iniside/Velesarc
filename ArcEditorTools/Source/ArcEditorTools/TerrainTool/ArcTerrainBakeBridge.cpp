// Copyright Lukasz Baran. All Rights Reserved.

#include "TerrainTool/ArcTerrainBakeBridge.h"

#include "TerrainTool/ArcTextureGraphBake.h"
#include "TerrainTool/Graph/ArcHeightmapGraph.h"
#include "TerrainTool/Graph/ArcHeightmapGraphEval.h"

#include "Stamps/ArcTerrainBakedStamp.h"
#include "Assets/ArcTerrainBakedField.h"

#include "TextureGraph.h"
#include "Misc/Guid.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcTerrainBakeBridge, Log, All);

#define LOCTEXT_NAMESPACE "ArcTerrainBakeBridge"

namespace ArcTerrainBakeBridge_Local
{
	/** Copy an editor baked field into the Runtime mirror + roll a fresh ContentId (keys GPU cache + DDC). */
	void ConvertField(const FArcBakedHeightField& In, FArcTerrainBakedField& Out)
	{
		Out.Values = In.Values;
		Out.Dims = In.Dims;
		Out.ContentId = FGuid::NewGuid();
	}

	/** Bake a UArcHeightmapGraph: re-evaluate for fresh data, falling back to the graph's stored bake. */
	bool BakeHeightmapGraph(UArcHeightmapGraph& Graph, UArcTerrainBakedStamp& Stamp)
	{
		FArcHeightmapEvalContext Ctx =
			FArcHeightmapEvalContext::FromCoverage(
				Stamp.CoverageXY.X, Stamp.CoverageXY.Y, Graph.DefaultResolution);

		FArcHeightmapEvalResult Result;
		FString Error;
		if (EvaluateGraph(Graph, Ctx, Result, Error))
		{
			FArcBakedHeightField Baked;
			ArcHeightmapGraph::FromField(Result.Height, Baked);
			ConvertField(Baked, Stamp.BakedField);

			Stamp.BakedLayers.Reset();
			for (const TPair<FName, UE::Geometry::FSampledScalarField2f>& LayerPair : Result.Layers)
			{
				FArcBakedHeightField LayerBaked;
				ArcHeightmapGraph::FromField(LayerPair.Value, LayerBaked);
				ConvertField(LayerBaked, Stamp.BakedLayers.Add(LayerPair.Key));
			}
			return true;
		}

		UE_LOG(LogArcTerrainBakeBridge, Warning,
			TEXT("Heightmap graph '%s' eval failed (%s) — copying stored baked data."),
			*Graph.GetName(), *Error);

		if (!Graph.BakedField.IsValid())
		{
			UE_LOG(LogArcTerrainBakeBridge, Warning,
				TEXT("Heightmap graph '%s' has no valid stored bake."), *Graph.GetName());
			return false;
		}

		ConvertField(Graph.BakedField, Stamp.BakedField);
		Stamp.BakedLayers.Reset();
		for (const TPair<FName, FArcBakedHeightField>& LayerPair : Graph.BakedLayers)
		{
			ConvertField(LayerPair.Value, Stamp.BakedLayers.Add(LayerPair.Key));
		}
		return true;
	}

	/** Bake a UTextureGraph via the existing blocking render + readback path. */
	bool BakeTextureGraph(UTextureGraph& TextureGraph, UArcTerrainBakedStamp& Stamp)
	{
		ArcTextureGraphBake::FBakeResult BakeResult;
		FString Error;
		if (!ArcTextureGraphBake::Bake(&TextureGraph, Stamp.HeightOutputName, BakeResult, Error))
		{
			UE_LOG(LogArcTerrainBakeBridge, Warning,
				TEXT("TextureGraph '%s' bake failed: %s"), *TextureGraph.GetName(), *Error);
			return false;
		}

		ConvertField(BakeResult.Height, Stamp.BakedField);
		Stamp.BakedLayers.Reset();
		for (const TPair<FName, FArcBakedHeightField>& LayerPair : BakeResult.Layers)
		{
			ConvertField(LayerPair.Value, Stamp.BakedLayers.Add(LayerPair.Key));
		}
		return true;
	}

	bool HandleBake(UArcTerrainBakedStamp& Stamp)
	{
		UObject* Source = Stamp.BakeSource.LoadSynchronous();
		if (Source == nullptr)
		{
			UE_LOG(LogArcTerrainBakeBridge, Warning, TEXT("Baked stamp has no BakeSource assigned."));
			return false;
		}

		if (UArcHeightmapGraph* Graph = Cast<UArcHeightmapGraph>(Source))
		{
			return BakeHeightmapGraph(*Graph, Stamp);
		}

		if (UTextureGraph* TextureGraph = Cast<UTextureGraph>(Source))
		{
			return BakeTextureGraph(*TextureGraph, Stamp);
		}

		UE_LOG(LogArcTerrainBakeBridge, Warning,
			TEXT("BakeSource '%s' is neither a UArcHeightmapGraph nor a UTextureGraph."), *Source->GetName());
		return false;
	}
}

namespace ArcTerrainBakeBridge
{
	void Register()
	{
		UArcTerrainBakedStamp::OnBakeRequested.BindStatic(&ArcTerrainBakeBridge_Local::HandleBake);
	}

	void Unregister()
	{
		UArcTerrainBakedStamp::OnBakeRequested.Unbind();
	}
}

#undef LOCTEXT_NAMESPACE
