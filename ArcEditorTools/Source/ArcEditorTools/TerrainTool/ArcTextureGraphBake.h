// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TerrainTool/Graph/ArcHeightmapGraph.h"   // FArcBakedHeightField

class UTextureGraph;

namespace ArcTextureGraphBake
{
    struct FBakeResult
    {
        FArcBakedHeightField Height;                  // normalized output value, native output resolution
        TMap<FName, FArcBakedHeightField> Layers;     // keyed by output node title name (all non-height outputs)
    };

    // Renders TextureGraph on the game thread (blocking) and reads each named output back to CPU floats.
    // HeightOutputName selects which Output node is the heightfield; every other Output becomes a layer.
    // Returns false + fills OutError on any failure. EDITOR + real-RHI only (no -nullrhi/commandlet).
    bool Bake(UTextureGraph* TextureGraph, FName HeightOutputName,
              FBakeResult& OutResult, FString& OutError);
}
