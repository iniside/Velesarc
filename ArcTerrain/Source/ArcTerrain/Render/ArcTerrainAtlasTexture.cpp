// Copyright Lukasz Baran. All Rights Reserved.

#include "Render/ArcTerrainAtlasTexture.h"

#include "ArcTerrainModule.h"
#include "PixelFormat.h"
#include "RHICommandList.h"
#include "RHIStaticStates.h"
#include "RHITextureReference.h"
#include "RenderingThread.h"

namespace ArcTerrainAtlasTexturePrivate_9b31
{
	FString DescribeRHI(FRHITexture* Texture)
	{
		if (Texture == nullptr)
		{
			return TEXT("<null>");
		}
		const FRHITextureDesc& Desc = Texture->GetDesc();
		return FString::Printf(TEXT("extent=%dx%d arraysize=%d mips=%d format=%s dimension=%d"),
			Desc.Extent.X, Desc.Extent.Y, static_cast<int32>(Desc.ArraySize), static_cast<int32>(Desc.NumMips),
			GetPixelFormatString(Desc.Format), static_cast<int32>(Desc.Dimension));
	}

	uint64 PointerId(const void* Pointer)
	{
		return static_cast<uint64>(reinterpret_cast<UPTRINT>(Pointer));
	}

	template <typename SnapshotType>
	bool ShouldLogAndUpdate(TOptional<SnapshotType>& LastSnapshot, const SnapshotType& NewSnapshot, bool bVerbose)
	{
		const bool bChanged = !LastSnapshot.IsSet() || !(LastSnapshot.GetValue() == NewSnapshot);
		LastSnapshot = NewSnapshot;
		return bVerbose || bChanged;
	}
}

void FArcTerrainAtlasTextureResource::InitRHI(FRHICommandListBase& RHICmdList)
{
	const FRHITextureCreateDesc DummyDesc =
		FRHITextureCreateDesc::Create2DArray(TEXT("ArcTerrainAtlasTexture.Dummy"))
		.SetExtent(1, 1)
		.SetArraySize(1)
		.SetFormat(PF_B8G8R8A8)
		.SetFlags(ETextureCreateFlags::ShaderResource)
		.SetInitialState(ERHIAccess::SRVMask);
	DummyTextureRHI = RHICmdList.CreateTexture(DummyDesc);

	const bool bHadAtlas = TextureRHI.IsValid();
	if (!TextureRHI.IsValid())
	{
		TextureRHI = DummyTextureRHI;
	}

	SamplerStateRHI = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	DeferredPassSamplerStateRHI = SamplerStateRHI;

	// The material samples the owning UTexture's TextureReference.TextureReferenceRHI, not TextureRHI.
	FRHITextureReference* const ReferenceRHI = GetTextureReference();
	RHICmdList.UpdateTextureReference(ReferenceRHI, TextureRHI.GetReference());

	UE_LOGF(LogArcTerrain, Warning,
		"[AtlasFacade] '%ls' InitRHI: declared %dx%d slices=%d | atlas already set=%ls bound=%ls | "
		"reference=%ls%ls",
		*OwnerName, SizeXY, SizeXY, NumSlices, bHadAtlas ? TEXT("yes") : TEXT("no"),
		*ArcTerrainAtlasTexturePrivate_9b31::DescribeRHI(TextureRHI),
		ReferenceRHI != nullptr ? TEXT("valid") : TEXT("NULL"),
		bHadAtlas ? TEXT("")
			: TEXT("  <<<< this facade starts on the 1x1 DUMMY: until SetAtlasRHI_RenderThread lands, every ")
				TEXT("sample of it returns one constant texel"));
}

void FArcTerrainAtlasTextureResource::ReleaseRHI()
{
	if (FRHITextureReference* const ReferenceRHI = GetTextureReference())
	{
		RHIClearTextureReference(ReferenceRHI);
	}

	TextureRHI.SafeRelease();
	DummyTextureRHI.SafeRelease();
	LastSetAtlasRHISnapshot.Reset();
	FTextureResource::ReleaseRHI();
}

void FArcTerrainAtlasTextureResource::SetAtlasRHI_RenderThread(FRHICommandListBase& RHICmdList,
	FRHITexture* InAtlasTexture)
{
	FRHITexture* const DesiredTexture =
		InAtlasTexture != nullptr ? InAtlasTexture : DummyTextureRHI.GetReference();
	if (TextureRHI.GetReference() == DesiredTexture)
	{
		return;
	}

	TextureRHI = DesiredTexture;

	FRHITextureReference* const ReferenceRHI = GetTextureReference();
	RHICmdList.UpdateTextureReference(ReferenceRHI, TextureRHI.GetReference());

	const bool bIsDummy = InAtlasTexture == nullptr;
	const FRHITextureDesc* const BoundDesc = TextureRHI.IsValid() ? &TextureRHI->GetDesc() : nullptr;
	const bool bIsArray = BoundDesc != nullptr && BoundDesc->Dimension == ETextureDimension::Texture2DArray;

	FSetAtlasRHISnapshot NewSnapshot;
	NewSnapshot.bIsDummy = bIsDummy;
	NewSnapshot.bIsArray = bIsArray;
	NewSnapshot.DeclaredSizeXY = SizeXY;
	NewSnapshot.DeclaredNumSlices = NumSlices;
	NewSnapshot.BoundExtentX = BoundDesc != nullptr ? BoundDesc->Extent.X : 0;
	NewSnapshot.BoundExtentY = BoundDesc != nullptr ? BoundDesc->Extent.Y : 0;
	NewSnapshot.BoundArraySize = BoundDesc != nullptr ? static_cast<int32>(BoundDesc->ArraySize) : 0;
	NewSnapshot.BoundNumMips = BoundDesc != nullptr ? static_cast<int32>(BoundDesc->NumMips) : 0;
	NewSnapshot.BoundFormat = BoundDesc != nullptr ? static_cast<int32>(BoundDesc->Format) : 0;
	NewSnapshot.BoundDimension = BoundDesc != nullptr ? static_cast<int32>(BoundDesc->Dimension) : 0;
	NewSnapshot.ReferenceRHI = ArcTerrainAtlasTexturePrivate_9b31::PointerId(ReferenceRHI);

	if (ArcTerrainAtlasTexturePrivate_9b31::ShouldLogAndUpdate(LastSetAtlasRHISnapshot, NewSnapshot,
		ArcTerrainDiag::IsAtlasFacadeVerbose()))
	{
		UE_LOGF(LogArcTerrain, Warning,
		"[AtlasFacade] '%ls' SetAtlasRHI: incoming=%ls declared %dx%d slices=%d | bound=%ls (%ls) | "
		"reference=%ls%ls%ls%ls",
			*OwnerName, bIsDummy ? TEXT("NULL") : TEXT("atlas"), SizeXY, SizeXY, NumSlices,
			bIsDummy ? TEXT("1x1 DUMMY") : TEXT("real atlas"),
			*ArcTerrainAtlasTexturePrivate_9b31::DescribeRHI(TextureRHI),
			ReferenceRHI != nullptr ? TEXT("valid") : TEXT("NULL"),
			bIsDummy
				? TEXT("  <<<< the material samples the 1x1 dummy: every weight fetch returns one constant texel")
				: TEXT(""),
			bIsArray ? TEXT("")
				: TEXT("  <<<< the bound RHI texture is NOT a Texture2DArray: the Texture2DArray sampler slot is ")
					TEXT("mismatched"),
			ReferenceRHI != nullptr ? TEXT("")
				: TEXT("  <<<< the owning UTexture has no TextureReferenceRHI: the shader binds the reference, so ")
					TEXT("this atlas never reaches the GPU"));
	}
}

FTextureResource* UArcTerrainAtlasTexture::CreateResource()
{
	FArcTerrainAtlasTextureResource* Resource = new FArcTerrainAtlasTextureResource();
	Resource->SetSize(SizeXY);
	Resource->SetDiagnosticIdentity(GetName(), NumSlices);

	UE_LOGF(LogArcTerrain, Warning,
		"[AtlasFacade] '%ls' CreateResource: SizeXY=%d NumSlices=%d surface=%.0fx%.0f surfacearraysize=%u "
		"textureclass=%d materialtype=%d%ls",
		*GetName(), SizeXY, NumSlices, GetSurfaceWidth(), GetSurfaceHeight(), GetSurfaceArraySize(),
		static_cast<int32>(GetTextureClass()), static_cast<int32>(GetMaterialType()),
		GetMaterialType() == MCT_Texture2DArray ? TEXT("")
			: TEXT("  <<<< the facade does not advertise MCT_Texture2DArray: the material parameter refuses ")
				TEXT("it and falls back to the compile default"));
	return Resource;
}

void UArcTerrainAtlasTexture::SetAtlasGeometry(int32 InSizeXY, int32 InNumSlices)
{
	const int32 NewSizeXY = FMath::Max(InSizeXY, 1);
	const int32 NewNumSlices = FMath::Max(InNumSlices, 1);
	if (GetResource() != nullptr && NewSizeXY == SizeXY && NewNumSlices == NumSlices)
	{
		return;
	}

	SizeXY = NewSizeXY;
	NumSlices = NewNumSlices;
	UpdateResource();
}
