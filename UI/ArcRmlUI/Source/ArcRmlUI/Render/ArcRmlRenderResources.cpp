// Copyright Lukasz Baran. All Rights Reserved.

#include "Render/ArcRmlRenderResources.h"

#include "PipelineStateCache.h"
#include "RenderGraphUtils.h"
#include "RHICommandList.h"
#include "RHIResourceUtils.h"

namespace ArcRmlRenderResourcesPrivate
{
	FArcRmlGeometryResource CreateGeometry(FRHICommandListBase& RHICmdList, TConstArrayView<FArcRmlVertex> Vertices, TConstArrayView<uint32> Indices)
	{
		FArcRmlGeometryResource Resource;
		Resource.VertexBuffer = UE::RHIResourceUtils::CreateVertexBufferFromArray(RHICmdList, TEXT("ArcRmlVertexBuffer"), Vertices);
		Resource.IndexBuffer = UE::RHIResourceUtils::CreateIndexBufferFromArray(RHICmdList, TEXT("ArcRmlIndexBuffer"), Indices);
		Resource.NumVertices = static_cast<uint32>(Vertices.Num());
		Resource.NumIndices = static_cast<uint32>(Indices.Num());
		return Resource;
	}
}

void FArcRmlRenderResources::ApplyCreates(FRHICommandListBase& RHICmdList, const FArcRmlRenderFrame& Frame)
{
	for (const FArcRmlResourceOp& Op : Frame.ResourceOps)
	{
		switch (Op.Type)
		{
		case EArcRmlResourceOpType::CreateGeometry:
		{
			const FArcRmlGeometryPayload& Payload = Frame.Geometries[Op.PayloadIndex];
			Geometries.Add(Op.Handle, ArcRmlRenderResourcesPrivate::CreateGeometry(RHICmdList, Payload.Vertices, Payload.Indices));
			break;
		}
		case EArcRmlResourceOpType::CreateTexture:
		{
			const FArcRmlTexturePayload& Payload = Frame.Textures[Op.PayloadIndex];
			const FRHITextureCreateDesc Desc = FRHITextureCreateDesc::Create2D(TEXT("ArcRmlTexture"), Payload.Size, PF_R8G8B8A8)
				.SetFlags(ETextureCreateFlags::ShaderResource)
				.SetInitialState(ERHIAccess::SRVMask);
			FTextureRHIRef Texture = RHICmdList.CreateTexture(Desc);
			const FUpdateTextureRegion2D Region(0, 0, 0, 0, static_cast<uint32>(Payload.Size.X), static_cast<uint32>(Payload.Size.Y));
			RHICmdList.UpdateTexture2D(Texture, 0, Region, static_cast<uint32>(Payload.Size.X) * 4, Payload.Pixels.GetData());
			Textures.Add(Op.Handle, FArcRmlTextureResource{CreateRenderTarget(Texture, TEXT("ArcRmlTexture")), nullptr});
			break;
		}
		case EArcRmlResourceOpType::CreateAssetTexture:
			Textures.Add(Op.Handle, FArcRmlTextureResource{nullptr, Frame.AssetTextures[Op.PayloadIndex]});
			break;
		case EArcRmlResourceOpType::CreateFilter:
			Filters.Add(Op.Handle, Frame.Filters[Op.PayloadIndex]);
			break;
		case EArcRmlResourceOpType::CreateShader:
			Shaders.Add(Op.Handle, Frame.Shaders[Op.PayloadIndex]);
			break;
		default:
			break;
		}
	}
}

void FArcRmlRenderResources::ApplyReleases(const FArcRmlRenderFrame& Frame)
{
	for (const FArcRmlResourceOp& Op : Frame.ResourceOps)
	{
		switch (Op.Type)
		{
		case EArcRmlResourceOpType::ReleaseGeometry:
			Geometries.Remove(Op.Handle);
			break;
		case EArcRmlResourceOpType::ReleaseTexture:
			Textures.Remove(Op.Handle);
			break;
		case EArcRmlResourceOpType::ReleaseFilter:
			Filters.Remove(Op.Handle);
			break;
		case EArcRmlResourceOpType::ReleaseShader:
			Shaders.Remove(Op.Handle);
			break;
		default:
			break;
		}
	}
}

void FArcRmlRenderResources::Reset()
{
	Geometries.Empty();
	Textures.Empty();
	Filters.Empty();
	Shaders.Empty();
	FullscreenQuad = FArcRmlGeometryResource();
	VertexDeclaration.SafeRelease();
}

const FArcRmlGeometryResource& FArcRmlRenderResources::GetFullscreenQuad(FRHICommandListBase& RHICmdList)
{
	if (!FullscreenQuad.VertexBuffer.IsValid())
	{
		FArcRmlVertex Vertices[4];
		Vertices[0].Position = FVector2f(-1.0f, -1.0f);
		Vertices[1].Position = FVector2f(1.0f, -1.0f);
		Vertices[2].Position = FVector2f(1.0f, 1.0f);
		Vertices[3].Position = FVector2f(-1.0f, 1.0f);
		const uint32 Indices[6] = {0, 1, 2, 0, 2, 3};
		FullscreenQuad = ArcRmlRenderResourcesPrivate::CreateGeometry(RHICmdList, TConstArrayView<FArcRmlVertex>(Vertices, 4), TConstArrayView<uint32>(Indices, 6));
	}
	return FullscreenQuad;
}

FRHIVertexDeclaration* FArcRmlRenderResources::GetVertexDeclaration()
{
	if (!VertexDeclaration.IsValid())
	{
		const uint16 Stride = sizeof(FArcRmlVertex);
		FVertexDeclarationElementList Elements;
		Elements.Add(FVertexElement(0, STRUCT_OFFSET(FArcRmlVertex, Position), VET_Float2, 0, Stride));
		Elements.Add(FVertexElement(0, STRUCT_OFFSET(FArcRmlVertex, Color), VET_UByte4N, 1, Stride));
		Elements.Add(FVertexElement(0, STRUCT_OFFSET(FArcRmlVertex, TexCoord), VET_Float2, 2, Stride));
		VertexDeclaration = PipelineStateCache::GetOrCreateVertexDeclaration(Elements);
	}
	return VertexDeclaration;
}
