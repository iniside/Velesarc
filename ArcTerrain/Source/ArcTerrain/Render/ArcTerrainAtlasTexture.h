// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/Texture.h"
#include "RenderResource.h"
#include "TextureResource.h"

#include "ArcTerrainAtlasTexture.generated.h"

/**
 * Texture resource that owns nothing and points at the tile atlas array currently held by the
 * render thread. It always resolves to a REAL Texture2DArray: a 1x1x1 dummy stands in until the
 * atlas exists, because a material parameter left unresolved would fall back to the engine's 2D
 * black texture and bind a Texture2D into a Texture2DArray slot.
 */
class FArcTerrainAtlasTextureResource : public FTextureResource
{
public:
	//~ Begin FTextureResource interface
	virtual void InitRHI(FRHICommandListBase& RHICmdList) override;
	virtual void ReleaseRHI() override;
	virtual uint32 GetSizeX() const override { return static_cast<uint32>(SizeXY); }
	virtual uint32 GetSizeY() const override { return static_cast<uint32>(SizeXY); }
	//~ End FTextureResource interface

	/** Render thread. Null reverts to the dummy array. */
	void SetAtlasRHI_RenderThread(FRHICommandListBase& RHICmdList, FRHITexture* InAtlasTexture);

	void SetSize(int32 InSizeXY) { SizeXY = FMath::Max(InSizeXY, 1); }

	/** Diagnostics only: the facade UObject this resource was created for, and its declared slice count. */
	void SetDiagnosticIdentity(const FString& InOwnerName, int32 InNumSlices)
	{
		OwnerName = InOwnerName;
		NumSlices = InNumSlices;
	}

private:
	FTextureRHIRef DummyTextureRHI;
	int32 SizeXY = 1;
	FString OwnerName;
	int32 NumSlices = 1;

	struct FSetAtlasRHISnapshot
	{
		bool bIsDummy = false;
		bool bIsArray = false;
		int32 DeclaredSizeXY = 0;
		int32 DeclaredNumSlices = 0;
		int32 BoundExtentX = 0;
		int32 BoundExtentY = 0;
		int32 BoundArraySize = 0;
		int32 BoundNumMips = 0;
		int32 BoundFormat = 0;
		int32 BoundDimension = 0;
		uint64 ReferenceRHI = 0;

		bool operator==(const FSetAtlasRHISnapshot& Other) const = default;
	};

	TOptional<FSetAtlasRHISnapshot> LastSetAtlasRHISnapshot;
};

/**
 * UTexture facade over one tile-atlas array, so a material instance can bind the atlas through an
 * ordinary Texture2DArray parameter. The atlas itself is an RDG pooled render target with no UObject
 * of its own, and MID parameters only accept UTexture — this is the adapter between the two.
 *
 * Transient, created per terrain by the render-binding owner; the RHI texture is pushed from the
 * render thread and the UObject never touches it.
 */
UCLASS(NotBlueprintable, NotPlaceable, Transient, MinimalAPI)
class UArcTerrainAtlasTexture : public UTexture
{
	GENERATED_BODY()

public:
	//~ Begin UTexture interface
	virtual ETextureClass GetTextureClass() const override { return ETextureClass::Array; }
	virtual EMaterialValueType GetMaterialType() const override { return MCT_Texture2DArray; }
	virtual FTextureResource* CreateResource() override;
	virtual float GetSurfaceWidth() const override { return static_cast<float>(SizeXY); }
	virtual float GetSurfaceHeight() const override { return static_cast<float>(SizeXY); }
	virtual float GetSurfaceDepth() const override { return 0.0f; }
	virtual uint32 GetSurfaceArraySize() const override { return static_cast<uint32>(NumSlices); }
	//~ End UTexture interface

	/** Game thread: declare the array geometry and (re)create the resource when it changed. */
	ARCTERRAIN_API void SetAtlasGeometry(int32 InSizeXY, int32 InNumSlices);

	/** The render-thread side. Null before the first SetAtlasGeometry; deref only on the render thread. */
	FArcTerrainAtlasTextureResource* GetAtlasTextureResource()
	{
		return static_cast<FArcTerrainAtlasTextureResource*>(GetResource());
	}

private:
	int32 SizeXY = 1;
	int32 NumSlices = 1;
};
