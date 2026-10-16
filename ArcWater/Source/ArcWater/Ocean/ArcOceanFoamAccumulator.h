// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PooledRenderTarget.h"
#include "SceneViewExtension.h"

class FArcOceanSceneProxy;
class FRDGBuilder;
class FSceneInterface;
class FSceneView;
class FSceneViewFamily;

namespace ArcOcean
{
	inline constexpr int32 FoamResolution = 512;
	inline constexpr int32 FoamThreadsPerGroup = 8;
	inline constexpr float FoamTexelSizeCm = 100.0f;

	static_assert(FoamResolution % FoamThreadsPerGroup == 0,
		"ArcOceanFoamAdvect.usf has no bounds guard: every dispatched thread must land on a texel.");

	ARCWATER_API void RegisterFoamViewExtension();
	ARCWATER_API void UnregisterFoamViewExtension();
}

class FArcOceanFoamAccumulator
{
public:
	static ARCWATER_API FArcOceanFoamAccumulator& Get();

	void RegisterProxy(FArcOceanSceneProxy* InProxy);
	void UnregisterProxy(FArcOceanSceneProxy* InProxy);

	void Update(FRDGBuilder& GraphBuilder, const FSceneView& InView);

private:
	struct FSceneState
	{
		// Advection gathers from displaced coordinates, so the frame's source and destination cannot be one texture.
		TRefCountPtr<IPooledRenderTarget> Targets[2];

		FVector WindowOriginCm = FVector::ZeroVector;
		uint32 LastUpdateFrame = MAX_uint32;
		uint32 WrittenMask = 0u;
		int32 WriteIndex = 0;
		bool bWindowValid = false;
	};

	TArray<FArcOceanSceneProxy*> Proxies;
	TMap<const FSceneInterface*, FSceneState> States;
};

class FArcOceanFoamViewExtension : public FSceneViewExtensionBase
{
public:
	explicit FArcOceanFoamViewExtension(const FAutoRegister& AutoRegister)
		: FSceneViewExtensionBase(AutoRegister)
	{
	}

	//~ Begin ISceneViewExtension interface
	virtual void PreRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily) override;
	//~ End ISceneViewExtension interface
};
