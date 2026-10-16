// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UArcTerrainChannelSet;
class UMaterialInstanceDynamic;
class UTexture;
enum EShaderPlatform : uint16;

namespace ArcTerrainMaterialBindings
{
	enum class EResult : uint8
	{
		Succeeded,
		ResourceUnavailable,
		Failed,
	};

	EResult Bind(UMaterialInstanceDynamic& MID, EShaderPlatform ShaderPlatform,
		UTexture* HeightAtlas, TConstArrayView<UTexture*> WeightAtlases,
		const UArcTerrainChannelSet* ChannelSet, float HeightInvRes, float WeightInvRes,
		float CellSizeCm, bool bDumpMaterialParameters, bool bDumpWeightConsistency);
}
