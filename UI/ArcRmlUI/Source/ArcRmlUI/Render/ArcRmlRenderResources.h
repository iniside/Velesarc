// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RHIResources.h"
#include "RendererInterface.h"
#include "Render/ArcRmlRenderTypes.h"

class FRHICommandListBase;

class FTextureReference;

struct FArcRmlTextureResource
{
	TRefCountPtr<IPooledRenderTarget> Pooled;
	const FTextureReference* AssetReference = nullptr;
};

struct FArcRmlGeometryResource
{
	FBufferRHIRef VertexBuffer;
	FBufferRHIRef IndexBuffer;
	uint32 NumVertices = 0;
	uint32 NumIndices = 0;
};

class FArcRmlRenderResources
{
public:
	void ApplyCreates(FRHICommandListBase& RHICmdList, const FArcRmlRenderFrame& Frame);
	void ApplyReleases(const FArcRmlRenderFrame& Frame);
	void Reset();

	const FArcRmlGeometryResource& GetFullscreenQuad(FRHICommandListBase& RHICmdList);
	FRHIVertexDeclaration* GetVertexDeclaration();

	TMap<uint64, FArcRmlGeometryResource> Geometries;
	TMap<uint64, FArcRmlTextureResource> Textures;
	TMap<uint64, FArcRmlFilterDesc> Filters;
	TMap<uint64, FArcRmlShaderDesc> Shaders;

private:
	FArcRmlGeometryResource FullscreenQuad;
	FVertexDeclarationRHIRef VertexDeclaration;
};
