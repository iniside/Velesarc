// Copyright Lukasz Baran. All Rights Reserved.

#include "Preview/ArcTerrainPreviewManager.h"

#include "ArcTerrainActor.h"
#include "ArcTerrainChannelSet.h"
#include "ArcTerrainModule.h"
#include "ArcTerrainSubsystem.h"
#include "Render/ArcTerrainAtlasPublish.h"
#include "Render/ArcTerrainAtlasTexture.h"
#include "Render/ArcTerrainCellMath.h"
#include "Render/ArcTerrainHeightBounds.h"
#include "Render/ArcTerrainMaterialBindings.h"
#include "Render/ArcTerrainRenderComponent.h"
#include "Render/ArcTerrainTileAtlas.h"

#include "Engine/Engine.h"
#include "Engine/Texture.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialParameters.h"
#include "MaterialShared.h"
#include "RHIGlobals.h"

namespace ArcTerrainPreviewManagerPrivate_2d18
{
	/**
	 * The STATIC face table and the world bounds it spans, plus the residency rings a leaf resolves its
	 * slices through. Neither the table nor the bounds is a function of residency: every face of the grid
	 * exists always, carries its global texel ORIGIN, and reads the fill slice wherever nothing is
	 * resident under it.
	 */
	void BuildRenderBindings(const FArcTerrainTileAtlas& Atlas, const FArcTerrainConfig& Config,
		const FArcTerrainFaceGrid& Grid, const FArcTerrainResidencyRings& Rings,
		FVector2f ConservativeHeightRangeCm, const FArcTerrainRenderIdentity& Identity,
		FArcTerrainRenderBindings& OutBindings)
	{
		const FArcTerrainTileAtlasLayout& Layout = Atlas.GetLayout();
		OutBindings.Identity = Identity;
		const double TexelToLocal =
			static_cast<double>(ArcTerrainCellMath::CellTexelToLocal(Config.CellSize(), Layout.HeightQuads));

		OutBindings.SectorQuads = Layout.HeightQuads << Layout.SectorShift;
		OutBindings.FaceQuads = Grid.FaceQuads;
		OutBindings.SectorQuadsShift = Layout.GetSectorQuadsShift();
		OutBindings.WeightQuadsShift = Layout.GetWeightQuadsShift();
		OutBindings.TexelToLocal = static_cast<float>(TexelToLocal);
		OutBindings.AtlasResources = Atlas.GetResources();
		OutBindings.Rings = Rings;
		OutBindings.bResourcesValid = true;
		OutBindings.bLogWeightVerbose = ArcTerrainDiag::IsWeightCompositeVerbose();
		OutBindings.LayoutWeightFillSlice = Layout.GetWeightFillSlice();

		const int32 NumFaces = Grid.NumFaces();
		OutBindings.Faces.SetNum(NumFaces);
		for (int32 FaceId = 0; FaceId < NumFaces; ++FaceId)
		{
			OutBindings.Faces[FaceId].TexelOrigin = Grid.FaceTexelOrigin(FaceId);
		}

		const double MinTexel = static_cast<double>(-Grid.HalfFaces) * Grid.FaceQuads;
		const double MaxTexel = static_cast<double>(Grid.NumFacesPerEdge - Grid.HalfFaces) * Grid.FaceQuads;
		OutBindings.LocalBounds = FBox(
			FVector(MinTexel * TexelToLocal, MinTexel * TexelToLocal, ConservativeHeightRangeCm.X),
			FVector(MaxTexel * TexelToLocal, MaxTexel * TexelToLocal, ConservativeHeightRangeCm.Y));
	}

}

void FArcTerrainPreviewManager::Initialize(UArcTerrainSubsystem& InSubsystem)
{
	Subsystem = &InSubsystem;
}

UMaterialInterface* FArcTerrainPreviewManager::GetTerrainMaterial(AArcTerrain& Terrain)
{
	if (const TWeakObjectPtr<UMaterialInterface>* Cached = ResolvedMaterials.Find(&Terrain))
	{
		if (UMaterialInterface* CachedMaterial = Cached->Get())
		{
			return CachedMaterial;
		}
	}

	UMaterialInterface* Resolved = ResolveTerrainMaterial(Terrain);
	if (Resolved != nullptr)
	{
		ResolvedMaterials.Add(&Terrain, Resolved);
	}
	return Resolved;
}

UMaterialInterface* FArcTerrainPreviewManager::ResolveTerrainMaterial(AArcTerrain& Terrain)
{
	// (a) Editor-assigned terrain material — any ORDINARY surface material works (displacement and
	// normals live in the vertex factory); the ArcTerrain nodes remain available for layered materials.
	if (UMaterialInterface* Assigned = Terrain.GetTerrainMaterial())
	{
		return Assigned;
	}
	return UMaterial::GetDefaultMaterial(MD_Surface);
}

bool FArcTerrainPreviewManager::HasAdmittedRender(const AArcTerrain& Terrain) const
{
	const FTerrainRender* Render = Renders.Find(&Terrain);
	if (Render == nullptr || Render->bUnadmitted || !Render->bHasBindingSignature)
	{
		return false;
	}
	const UArcTerrainRenderComponent* Component = Render->Component.Get();
	const UMaterialInstanceDynamic* MID = Render->MID.Get();
	const UArcTerrainAtlasTexture* HeightAtlasTexture = Render->HeightAtlasTexture.Get();
	if (Component == nullptr || MID == nullptr || HeightAtlasTexture == nullptr)
	{
		return false;
	}
	for (const TWeakObjectPtr<UArcTerrainAtlasTexture>& WeightAtlasTexture : Render->WeightAtlasTextures)
	{
		if (!WeightAtlasTexture.IsValid())
		{
			return false;
		}
	}
	return true;
}

UArcTerrainAtlasTexture* FArcTerrainPreviewManager::GetHeightAtlasFacade(AArcTerrain& Terrain) const
{
	const FTerrainRender* Render = Renders.Find(&Terrain);
	if (Render == nullptr || Render->bUnadmitted)
	{
		return nullptr;
	}
	return Render->HeightAtlasTexture.Get();
}

void FArcTerrainPreviewManager::InvalidateBindingSignature(AArcTerrain& Terrain)
{
	if (FTerrainRender* Render = Renders.Find(&Terrain))
	{
		Render->bHasBindingSignature = false;
		++Render->MaterialRevision;
	}
}

bool FArcTerrainPreviewManager::EnsureTerrainRender(AArcTerrain& Terrain, const FArcTerrainConfig& Config,
	const FArcTerrainTileAtlas& Atlas, const FArcTerrainResidencyRings& Rings,
	const FArcTerrainRenderIdentity& Identity)
{
	using namespace ArcTerrainPreviewManagerPrivate_2d18;

	const FArcTerrainTileAtlasLayout& Layout = Atlas.GetLayout();
	const FArcTerrainFaceGrid Grid = ArcTerrain::ResolveFaceGrid(
		Config.TexelSizeCm(), Terrain.FaceQuadsShift, Terrain.WorldExtentCm);
	FArcTerrainRenderBindings NewBindings;
	if (Layout.IsValid() && Grid.IsValid() && Atlas.GetResources().IsValid())
	{
		const FVector2f HeightRangeCm = ArcTerrain::ResolveConservativeHeightRangeCm(
			Terrain.ConservativeMinHeightCm, Terrain.ConservativeMaxHeightCm);
		BuildRenderBindings(Atlas, Config, Grid, Rings, HeightRangeCm, Identity, NewBindings);
	}
	if (!NewBindings.IsValid())
	{
		UnadmitTerrainRender(Terrain);
		return false;
	}

	FTerrainRender& Render = Renders.FindOrAdd(&Terrain);
	UMaterialInterface* BaseMaterial = GetTerrainMaterial(Terrain);
	UArcTerrainRenderComponent* ExistingComponent = Render.Component.Get();
	UMaterialInstanceDynamic* ExistingMID = Render.MID.Get();
	const UWorld* const World = Terrain.GetWorld();
	const ERHIFeatureLevel::Type FeatureLevel =
		World != nullptr ? World->GetFeatureLevel() : GMaxRHIFeatureLevel;
	const EShaderPlatform ShaderPlatform = GetFeatureLevelShaderPlatform(FeatureLevel);
	const int32 HeightSliceSide = Layout.GetHeightSliceSide();
	const int32 WeightSliceSide = Layout.GetWeightSliceSide();
	FArcTerrainRenderBindingSignature NewSignature;
	NewSignature.AtlasLayout = Layout;
	NewSignature.AtlasResourceIdentity = Atlas.GetResources().Get();
	NewSignature.AtlasResourceGeneration = Atlas.GetResourceGeneration();
	if (const UArcTerrainChannelSet* ChannelSet = Config.ChannelSet)
	{
		NewSignature.ChannelLayout = ChannelSet->Channels;
	}
	NewSignature.Material = BaseMaterial;
	FMaterialResource* const MaterialResource = BaseMaterial != nullptr
		? BaseMaterial->GetMaterialResource(ShaderPlatform) : nullptr;
	NewSignature.MaterialResourceIdentity = MaterialResource;
	NewSignature.MaterialShaderMapIdentity = MaterialResource != nullptr
		? MaterialResource->GetGameThreadShaderMap() : nullptr;
	NewSignature.MaterialId = MaterialResource != nullptr ? MaterialResource->GetMaterialId() : FGuid();
	NewSignature.MaterialRevision = Render.MaterialRevision;
	NewSignature.HeightInvRes = 1.0f / static_cast<float>(FMath::Max(HeightSliceSide, 1));
	NewSignature.WeightInvRes = 1.0f / static_cast<float>(FMath::Max(WeightSliceSide, 1));
	NewSignature.CellSizeCm = static_cast<float>(Config.CellSize());
	NewSignature.bDiagnosticBinding = ArcTerrainDiag::IsWeightCompositeVerbose();

	UArcTerrainAtlasTexture* ExistingHeightFacade = Render.HeightAtlasTexture.Get();
	bool bFacadesValid = ExistingHeightFacade != nullptr;
	for (const TWeakObjectPtr<UArcTerrainAtlasTexture>& WeightAtlasTexture : Render.WeightAtlasTextures)
	{
		if (!WeightAtlasTexture.IsValid())
		{
			bFacadesValid = false;
			break;
		}
	}
	if (Render.bHasBindingSignature
		&& !Render.bUnadmitted
		&& ExistingComponent != nullptr
		&& ExistingMID != nullptr
		&& bFacadesValid
		&& Render.BindingSignature == NewSignature)
	{
		ExistingComponent->UpdateBindings(MoveTemp(NewBindings));
		return false;
	}

	if (BaseMaterial == nullptr)
	{
		return false;
	}
	UnadmitTerrainRender(Terrain);

	UArcTerrainRenderComponent* Component = ExistingComponent;
	const bool bNewComponent = Component == nullptr;
	if (Component == nullptr)
	{
		Component = NewObject<UArcTerrainRenderComponent>(&Terrain, NAME_None, RF_Transient);
		Component->SetGenerateOverlapEvents(false);
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetupAttachment(Terrain.GetRootComponent());
	}

	// Atlas facades. Their only GC root is the MID's texture parameter override, so they are created
	// alongside it and rebound unconditionally below.
	UArcTerrainAtlasTexture* HeightAtlasTexture = Render.HeightAtlasTexture.Get();
	if (HeightAtlasTexture == nullptr)
	{
		HeightAtlasTexture = NewObject<UArcTerrainAtlasTexture>(Component, NAME_None, RF_Transient);
		Render.HeightAtlasTexture = HeightAtlasTexture;
	}
	HeightAtlasTexture->SetAtlasGeometry(Layout.GetHeightSliceSide(), Layout.GetHeightArraySize());

	Render.WeightAtlasTextures.SetNum(FMath::Max(Layout.NumWeightArrays, 0));
	TArray<FArcTerrainAtlasTextureResource*> WeightResources;
	WeightResources.Reserve(Render.WeightAtlasTextures.Num());
	for (int32 ArrayIndex = 0; ArrayIndex < Render.WeightAtlasTextures.Num(); ++ArrayIndex)
	{
		UArcTerrainAtlasTexture* WeightAtlasTexture = Render.WeightAtlasTextures[ArrayIndex].Get();
		if (WeightAtlasTexture == nullptr)
		{
			WeightAtlasTexture = NewObject<UArcTerrainAtlasTexture>(Component, NAME_None, RF_Transient);
			Render.WeightAtlasTextures[ArrayIndex] = WeightAtlasTexture;
		}
		WeightAtlasTexture->SetAtlasGeometry(Layout.GetWeightSliceSide(), Layout.GetWeightArraySize());
		WeightResources.Add(WeightAtlasTexture->GetAtlasTextureResource());
	}

	ArcTerrainAtlasPublish::PublishAtlasTextures(Atlas.GetResources(),
		HeightAtlasTexture->GetAtlasTextureResource(), MoveTemp(WeightResources));

	TArray<TObjectPtr<UArcTerrainAtlasTexture>> RootedTextures;
	RootedTextures.Reserve(Render.WeightAtlasTextures.Num() + 1);
	RootedTextures.Add(HeightAtlasTexture);
	for (const TWeakObjectPtr<UArcTerrainAtlasTexture>& WeightAtlasTexture : Render.WeightAtlasTextures)
	{
		RootedTextures.Add(WeightAtlasTexture.Get());
	}
	const bool bFreshMID = ExistingMID == nullptr;
	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BaseMaterial, Component);
	if (MID == nullptr)
	{
		return false;
	}

	const bool bLogWeightBind = ArcTerrainDiag::IsWeightCompositeVerbose();
	if (bLogWeightBind)
	{
		UE_LOGF(LogArcTerrain, Warning,
			"ArcTerrainPreviewManager '%ls': binding atlas params to MID='%ls', "
			"HeightAtlasTexture='%ls', NumWeightArrays=%d",
			*Terrain.GetName(), *MID->GetName(), *HeightAtlasTexture->GetName(),
			Layout.NumWeightArrays);
	}

	TArray<UTexture*> WeightAtlasBindings;
	WeightAtlasBindings.Reserve(Render.WeightAtlasTextures.Num());
	for (int32 ArrayIndex = 0; ArrayIndex < Render.WeightAtlasTextures.Num(); ++ArrayIndex)
	{
		const FName ParamName(*FString::Printf(TEXT("ArcTerrainWeights%d"), ArrayIndex));
		UArcTerrainAtlasTexture* WeightAtlasTexture = Render.WeightAtlasTextures[ArrayIndex].Get();
		WeightAtlasBindings.Add(WeightAtlasTexture);
		if (WeightAtlasTexture == nullptr)
		{
			if (bLogWeightBind)
			{
				UE_LOGF(LogArcTerrain, Warning,
					"ArcTerrainPreviewManager '%ls': SKIPPED bind for param '%ls' — WeightAtlasTextures[%d] "
					"is null",
					*Terrain.GetName(), *ParamName.ToString(), ArrayIndex);
			}
			continue;
		}
		if (bLogWeightBind)
		{
			FArcTerrainAtlasTextureResource* AtlasResource = WeightAtlasTexture->GetAtlasTextureResource();
			UE_LOGF(LogArcTerrain, Warning,
				"ArcTerrainPreviewManager '%ls': bind param '%ls' <- array %d, texture='%ls', "
				"resource=%ls, size=%dx%d, slices=%d",
				*Terrain.GetName(), *ParamName.ToString(), ArrayIndex, *WeightAtlasTexture->GetName(),
				AtlasResource != nullptr ? TEXT("valid") : TEXT("NULL"),
				static_cast<int32>(WeightAtlasTexture->GetSurfaceWidth()),
				static_cast<int32>(WeightAtlasTexture->GetSurfaceHeight()),
				static_cast<int32>(WeightAtlasTexture->GetSurfaceArraySize()));
		}
	}
	const float HeightInvRes = 1.0f / static_cast<float>(FMath::Max(HeightSliceSide, 1));
	const float WeightInvRes = 1.0f / static_cast<float>(FMath::Max(WeightSliceSide, 1));
	const float CellSizeCm = static_cast<float>(Config.CellSize());
	if (bLogWeightBind)
	{
		const float WeightUVScale = 1.0f - 3.0f * WeightInvRes;
		const float WeightUVBias = 1.5f * WeightInvRes;
		const float SliceSideF = static_cast<float>(FMath::Max(WeightSliceSide, 1));
		UE_LOGF(LogArcTerrain, Warning,
			"ArcTerrainPreviewManager '%ls': scalars ArcTerrainHeightInvRes=%.9f (heightsliceside=%d = "
			"(HeightQuads %d << SectorShift %d) + 3) ArcTerrainWeightInvRes=%.9f (weightsliceside=%d = "
			"(WeightQuads %d << WeightSliceShift %d) + 3) ArcTerrainCellSize=%.4f "
			"(Config.CellSize()=%.6f) | weight UV remap UV*%.9f + %.9f maps UV 0 -> texel %d, "
			"UV 1 -> texel %d",
			*Terrain.GetName(), HeightInvRes, HeightSliceSide, Layout.HeightQuads, Layout.SectorShift,
			WeightInvRes, WeightSliceSide, Layout.WeightQuads, Layout.WeightSliceShift, CellSizeCm,
			Config.CellSize(), WeightUVScale, WeightUVBias,
			FMath::FloorToInt32(WeightUVBias * SliceSideF),
			FMath::FloorToInt32((WeightUVScale + WeightUVBias) * SliceSideF));
	}

	const ArcTerrainMaterialBindings::EResult BindingResult = ArcTerrainMaterialBindings::Bind(
		*MID, ShaderPlatform, HeightAtlasTexture,
		WeightAtlasBindings, Config.ChannelSet, HeightInvRes, WeightInvRes, CellSizeCm,
		bFreshMID || bLogWeightBind, bLogWeightBind);
	if (BindingResult != ArcTerrainMaterialBindings::EResult::Succeeded)
	{
		return false;
	}

	Component->SetAtlasTextures(MoveTemp(RootedTextures));
	Component->SetMaterial(0, MID);
	Component->UpdateBindings(CopyTemp(NewBindings));
	if (bNewComponent)
	{
		Component->RegisterComponent();
		Render.Component = Component;
	}
	Render.MID = MID;
	Render.bUnadmitted = false;

	if (bFreshMID || bLogWeightBind)
	{
		UMaterialInterface* const ComponentMaterial = Component->GetMaterial(0);
		UE_LOGF(LogArcTerrain, Warning,
			"[CompMaterial] terrain='%ls' component material[0]='%ls' ptr=0x%p class='%ls' | MID='%ls' "
			"ptr=0x%p | identical=%ls%ls",
			*Terrain.GetName(),
			ComponentMaterial != nullptr ? *ComponentMaterial->GetName() : TEXT("<null>"),
			static_cast<const void*>(ComponentMaterial),
			ComponentMaterial != nullptr ? *ComponentMaterial->GetClass()->GetName() : TEXT("<none>"),
			*MID->GetName(), static_cast<const void*>(MID),
			ComponentMaterial == MID ? TEXT("yes") : TEXT("NO"),
			ComponentMaterial == MID ? TEXT("")
				: TEXT("  <<<< the render component does NOT draw with the instance this manager binds: every ")
					TEXT("atlas parameter goes to a material nothing renders"));
	}
	Render.BindingSignature = MoveTemp(NewSignature);
	Render.bHasBindingSignature = true;
	return true;
}

void FArcTerrainPreviewManager::UnadmitTerrainRender(AArcTerrain& Terrain)
{
	FTerrainRender* Render = Renders.Find(&Terrain);
	UArcTerrainRenderComponent* Component = Render != nullptr ? Render->Component.Get() : nullptr;
	if (Component == nullptr || Render->bUnadmitted)
	{
		return;
	}

	FArcTerrainRenderBindings Parked = Component->GetBindings();
	Parked.bResourcesValid = false;
	Component->UpdateBindings(MoveTemp(Parked));
	Render->bUnadmitted = true;
	Render->bHasBindingSignature = false;

	UE_LOGF(LogArcTerrain, Log,
		"ArcTerrain '%ls': render resources or material bindings changed; the terrain stays parked "
		"until the next successful bind.",
		*Terrain.GetName());
}

void FArcTerrainPreviewManager::ReleaseTerrainRender(const TWeakObjectPtr<AArcTerrain>& Terrain)
{
	ResolvedMaterials.Remove(Terrain);
	FTerrainRender Removed;
	if (Renders.RemoveAndCopyValue(Terrain, Removed))
	{
		if (UArcTerrainRenderComponent* Component = Removed.Component.Get())
		{
			Component->DestroyComponent();
		}
	}
}

void FArcTerrainPreviewManager::RefreshTerrainMaterial(AArcTerrain& Terrain)
{
	// Drop the cached resolution so the next resolve re-reads the actor's (possibly just-cleared) slot,
	// and force MID recreation against the newly-resolved base material. The component and its bindings
	// survive; the next EnsureTerrainRender rebinds every parameter.
	ResolvedMaterials.Remove(&Terrain);

	FTerrainRender* Render = Renders.Find(&Terrain);
	if (Render == nullptr)
	{
		return;
	}
	Render->MID = nullptr;
	Render->bHasBindingSignature = false;
	++Render->MaterialRevision;
	if (UArcTerrainRenderComponent* Component = Render->Component.Get())
	{
		Component->SetMaterial(0, nullptr);
	}
}

void FArcTerrainPreviewManager::Shutdown()
{
	for (TPair<TWeakObjectPtr<AArcTerrain>, FTerrainRender>& TerrainPair : Renders)
	{
		if (UArcTerrainRenderComponent* Component = TerrainPair.Value.Component.Get())
		{
			Component->DestroyComponent();
		}
	}
	Renders.Empty();
	ResolvedMaterials.Empty();
}
