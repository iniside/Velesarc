// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcUnifiedMeshVisualizationTrait.h"

#include "MassCommonFragments.h"
#include "MassEntityTemplateRegistry.h"
#include "Mass/ArchetypeCompositionTypes.h"
#include "MassLogging.h"
#include "ArcMassEntityVisualization.h"
#include "ArcMassVisualizationConfigFragments.h"
#include "ArcMass/SkinnedMeshVisualization/ArcMassSkinnedMeshFragments.h"
#include "ArcMass/CompositeVisualization/ArcCompositeMeshVisualizationFragments.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkinnedAsset.h"
#include "Materials/MaterialInterface.h"
#include "MassWorldUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcUnifiedMeshVisualizationTrait)

namespace ArcUnifiedMeshVisTrait_Impl
{
	// Non-loading resolve: residency is guaranteed by the caller's Client-bundle load,
	// never by a synchronous load here. Unloaded entries are dropped.
	void ResolveMaterialsDirect(const TArray<TSoftObjectPtr<UMaterialInterface>>& In, TArray<TObjectPtr<UMaterialInterface>>& Out)
	{
		for (const TSoftObjectPtr<UMaterialInterface>& SoftMat : In)
		{
			UMaterialInterface* Mat = SoftMat.Get();
			if (Mat)
			{
				Out.Add(Mat);
			}
		}
	}

	// Single source of truth for the rendering-props -> mesh-config conversion. Every
	// mode fills all fields (the legacy skinned trait drops the last nine — fixed here).
	FArcMassVisualizationMeshConfigFragment BuildVisMeshConfig(const FArcMassVisRenderingProps& Props)
	{
		FArcMassVisualizationMeshConfigFragment Out;
		Out.CastShadow = Props.bCastShadow;
		Out.CastShadowAsTwoSided = Props.bCastShadowAsTwoSided;
		Out.CastHiddenShadow = Props.bCastHiddenShadow;
		Out.CastFarShadow = Props.bCastFarShadow;
		Out.CastInsetShadow = Props.bCastInsetShadow;
		Out.CastContactShadow = Props.bCastContactShadow;
		Out.HiddenInGame = Props.bHiddenInGame;
		Out.ReceivesDecals = Props.bReceivesDecals;
		Out.NeverDistanceCull = Props.bNeverDistanceCull;
		Out.RenderCustomDepth = Props.bRenderCustomDepth;
		Out.CustomDepthStencilValue = Props.CustomDepthStencilValue;
		Out.TranslucencySortPriority = Props.TranslucencySortPriority;
		Out.AffectDynamicIndirectLighting = Props.bAffectDynamicIndirectLighting;
		Out.AffectIndirectLightingWhileHidden = Props.bAffectIndirectLightingWhileHidden;
		Out.LightingChannels = Props.LightingChannels;
		Out.VisibleInReflections = Props.bVisibleInReflections;
		Out.VisibleInRayTracing = Props.bVisibleInRayTracing;
		Out.VisibleInReflectionCaptures = Props.bVisibleInReflectionCaptures;
		Out.MinDrawDistance = Props.MinDrawDistance;
		Out.LDMaxDrawDistance = Props.LDMaxDrawDistance;
		Out.CustomDepthStencilWriteMask = Props.CustomDepthStencilWriteMask;
		return Out;
	}
}

void UArcUnifiedMeshVisualizationTrait::CountAuthoredEntries(int32& OutStatic, int32& OutSkinned) const
{
	OutStatic = 0;
	OutSkinned = 0;
	for (const FArcUnifiedStaticMeshEntry& Entry : StaticMeshes)
	{
		if (!Entry.StaticMesh.IsNull())
		{
			++OutStatic;
		}
	}
	for (const FArcUnifiedSkinnedMeshEntry& Entry : SkinnedMeshes)
	{
		if (!Entry.SkinnedAsset.IsNull())
		{
			++OutSkinned;
		}
	}
}

EArcUnifiedMeshVisMode UArcUnifiedMeshVisualizationTrait::GetInferredMode() const
{
	int32 NumStatic = 0;
	int32 NumSkinned = 0;
	CountAuthoredEntries(NumStatic, NumSkinned);

	if (NumStatic == 0 && NumSkinned == 0)
	{
		return EArcUnifiedMeshVisMode::None;
	}
	if (NumStatic == 1 && NumSkinned == 0)
	{
		return EArcUnifiedMeshVisMode::Static;
	}
	if (NumStatic == 0 && NumSkinned == 1)
	{
		return EArcUnifiedMeshVisMode::Skinned;
	}
	return EArcUnifiedMeshVisMode::Composite;
}

void UArcUnifiedMeshVisualizationTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	BuildContext.RequireFragment<FTransformFragment>();

	// Dedicated servers never load meshes and carry no visualization fragments at all.
	if (World.GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	// Mode is decided on authored entries only; residency failures below never change it
	// (the editor classifies placed entities by the same inference).
	const EArcUnifiedMeshVisMode Mode = GetInferredMode();
	if (Mode == EArcUnifiedMeshVisMode::None)
	{
		return;
	}

	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);

	if (Mode == EArcUnifiedMeshVisMode::Static)
	{
		const FArcUnifiedStaticMeshEntry* Entry = nullptr;
		for (const FArcUnifiedStaticMeshEntry& Candidate : StaticMeshes)
		{
			if (!Candidate.StaticMesh.IsNull())
			{
				Entry = &Candidate;
				break;
			}
		}

		// Residency contract: the spawner path must have loaded the Client bundle before
		// building this template. The template is cached per ConfigGuid and frozen — a
		// meshless build here stays meshless for the world's lifetime.
		UStaticMesh* LoadedMesh = Entry->StaticMesh.Get();
		if (!LoadedMesh)
		{
			UE_LOGF(LogMass, Error,"%s: mesh '%ls' not resident for trait '%ls' — caller must load the Client bundle before spawning; entity will have no visualization.",
				__FUNCTION__, *Entry->StaticMesh.ToSoftObjectPath().ToString(), *GetPathNameSafe(this));
			return;
		}

		BuildContext.AddFragment<FArcVisRepresentationFragment>();
		BuildContext.AddTag<FArcVisEntityTag>();

		FArcVisConfigFragment VisConfig;
		VisConfig.bCastShadows = RenderingProps.bCastShadow;
		BuildContext.AddConstSharedFragment(EntityManager.GetOrCreateConstSharedFragment(VisConfig));

		FArcMassStaticMeshConfigFragment StaticMeshConfigFrag(LoadedMesh);
		BuildContext.AddConstSharedFragment(EntityManager.GetOrCreateConstSharedFragment(StaticMeshConfigFrag));

		FArcVisComponentTransformFragment CompTransformFrag;
		CompTransformFrag.ComponentRelativeTransform = Entry->RelativeTransform;
		BuildContext.AddConstSharedFragment(EntityManager.GetOrCreateConstSharedFragment(CompTransformFrag));

		BuildContext.AddConstSharedFragment(EntityManager.GetOrCreateConstSharedFragment(
			ArcUnifiedMeshVisTrait_Impl::BuildVisMeshConfig(RenderingProps)));

		TArray<TObjectPtr<UMaterialInterface>> ResolvedMats;
		ArcUnifiedMeshVisTrait_Impl::ResolveMaterialsDirect(Entry->MaterialOverrides, ResolvedMats);
		if (ResolvedMats.Num() > 0)
		{
			FArcOverrideMaterialsFragment MatFrag;
			MatFrag.OverrideMaterials = ResolvedMats;
			BuildContext.AddConstSharedFragment(EntityManager.GetOrCreateConstSharedFragment(MatFrag));
		}
	}
	else if (Mode == EArcUnifiedMeshVisMode::Skinned)
	{
		const FArcUnifiedSkinnedMeshEntry* Entry = nullptr;
		for (const FArcUnifiedSkinnedMeshEntry& Candidate : SkinnedMeshes)
		{
			if (!Candidate.SkinnedAsset.IsNull())
			{
				Entry = &Candidate;
				break;
			}
		}

		// Same residency contract as the static branch.
		USkinnedAsset* LoadedSkinned = Entry->SkinnedAsset.Get();
		if (!LoadedSkinned)
		{
			UE_LOGF(LogMass, Error,"%s: skinned asset '%ls' not resident for trait '%ls' — caller must load the Client bundle before spawning; entity will have no visualization.",
				__FUNCTION__, *Entry->SkinnedAsset.ToSoftObjectPath().ToString(), *GetPathNameSafe(this));
			return;
		}

		BuildContext.AddFragment<FArcVisRepresentationFragment>();
		BuildContext.AddTag<FArcVisEntityTag>();

		FArcVisConfigFragment VisConfig;
		VisConfig.bCastShadows = RenderingProps.bCastShadow;
		BuildContext.AddConstSharedFragment(EntityManager.GetOrCreateConstSharedFragment(VisConfig));

		FArcMassSkinnedMeshFragment SkinnedMeshFrag;
		SkinnedMeshFrag.SkinnedAsset = LoadedSkinned;
		SkinnedMeshFrag.TransformProvider = TransformProvider;
		BuildContext.AddConstSharedFragment(EntityManager.GetOrCreateConstSharedFragment(SkinnedMeshFrag));

		FArcVisComponentTransformFragment CompTransformFrag;
		CompTransformFrag.ComponentRelativeTransform = Entry->RelativeTransform;
		BuildContext.AddConstSharedFragment(EntityManager.GetOrCreateConstSharedFragment(CompTransformFrag));

		BuildContext.AddConstSharedFragment(EntityManager.GetOrCreateConstSharedFragment(
			ArcUnifiedMeshVisTrait_Impl::BuildVisMeshConfig(RenderingProps)));

		TArray<TObjectPtr<UMaterialInterface>> ResolvedMats;
		ArcUnifiedMeshVisTrait_Impl::ResolveMaterialsDirect(Entry->MaterialOverrides, ResolvedMats);
		if (ResolvedMats.Num() > 0)
		{
			FArcOverrideMaterialsFragment MatFrag;
			MatFrag.OverrideMaterials = ResolvedMats;
			BuildContext.AddConstSharedFragment(EntityManager.GetOrCreateConstSharedFragment(MatFrag));
		}
	}
	else // Composite — two or more entries, or a static/skinned mix.
	{
		BuildContext.AddFragment<FArcVisRepresentationFragment>();
		BuildContext.AddFragment<FArcCompositeMeshVisInstanceLinkFragment>();
		BuildContext.AddTag<FArcCompositeMeshVisEntityTag>();

		FArcVisConfigFragment VisConfig;
		VisConfig.bCastShadows = RenderingProps.bCastShadow;
		BuildContext.AddConstSharedFragment(EntityManager.GetOrCreateConstSharedFragment(VisConfig));

		const FArcMassVisualizationMeshConfigFragment SharedMeshConfig =
			ArcUnifiedMeshVisTrait_Impl::BuildVisMeshConfig(RenderingProps);

		// Statics first, then skinned — the same ordering the editor view (GetMeshEntries)
		// and the placed-entity composite loop use, so sub-indices line up everywhere.
		FArcCompositeMeshConfigFragment CompositeMeshConfig;
		for (const FArcUnifiedStaticMeshEntry& Entry : StaticMeshes)
		{
			if (Entry.StaticMesh.IsNull())
			{
				continue;
			}
			UStaticMesh* LoadedMesh = Entry.StaticMesh.Get();
			if (!LoadedMesh)
			{
				UE_LOGF(LogMass, Error,"%s: mesh '%ls' not resident for trait '%ls' — entry skipped; caller must load the Client bundle before spawning.",
					__FUNCTION__, *Entry.StaticMesh.ToSoftObjectPath().ToString(), *GetPathNameSafe(this));
				continue;
			}

			FArcCompositeMeshEntryConfig EntryConfig;
			EntryConfig.Mesh = LoadedMesh;
			EntryConfig.RelativeTransform = Entry.RelativeTransform;
			ArcUnifiedMeshVisTrait_Impl::ResolveMaterialsDirect(Entry.MaterialOverrides, EntryConfig.MaterialOverrides);
			EntryConfig.MeshConfig = SharedMeshConfig;
			CompositeMeshConfig.Entries.Add(MoveTemp(EntryConfig));
		}
		for (const FArcUnifiedSkinnedMeshEntry& Entry : SkinnedMeshes)
		{
			if (Entry.SkinnedAsset.IsNull())
			{
				continue;
			}
			USkinnedAsset* LoadedSkinned = Entry.SkinnedAsset.Get();
			if (!LoadedSkinned)
			{
				UE_LOGF(LogMass, Error,"%s: skinned asset '%ls' not resident for trait '%ls' — entry skipped; caller must load the Client bundle before spawning.",
					__FUNCTION__, *Entry.SkinnedAsset.ToSoftObjectPath().ToString(), *GetPathNameSafe(this));
				continue;
			}

			FArcCompositeMeshEntryConfig EntryConfig;
			EntryConfig.SkinnedAsset = LoadedSkinned;
			EntryConfig.TransformProvider = TransformProvider;
			EntryConfig.RelativeTransform = Entry.RelativeTransform;
			ArcUnifiedMeshVisTrait_Impl::ResolveMaterialsDirect(Entry.MaterialOverrides, EntryConfig.MaterialOverrides);
			EntryConfig.MeshConfig = SharedMeshConfig;
			CompositeMeshConfig.Entries.Add(MoveTemp(EntryConfig));
		}

		if (CompositeMeshConfig.Entries.Num() > 0)
		{
			BuildContext.AddConstSharedFragment(EntityManager.GetOrCreateConstSharedFragment(CompositeMeshConfig));
		}
	}
}

TArray<FArcUnifiedMeshEntry> UArcUnifiedMeshVisualizationTrait::GetMeshEntries() const
{
	TArray<FArcUnifiedMeshEntry> Result;
	Result.Reserve(StaticMeshes.Num() + SkinnedMeshes.Num());
	for (const FArcUnifiedStaticMeshEntry& Entry : StaticMeshes)
	{
		FArcUnifiedMeshEntry& View = Result.AddDefaulted_GetRef();
		View.StaticMesh = Entry.StaticMesh;
		View.RelativeTransform = Entry.RelativeTransform;
		View.MaterialOverrides = Entry.MaterialOverrides;
		View.RenderingProps = RenderingProps;
	}
	for (const FArcUnifiedSkinnedMeshEntry& Entry : SkinnedMeshes)
	{
		FArcUnifiedMeshEntry& View = Result.AddDefaulted_GetRef();
		View.SkinnedAsset = Entry.SkinnedAsset;
		View.TransformProvider = TransformProvider;
		View.RelativeTransform = Entry.RelativeTransform;
		View.MaterialOverrides = Entry.MaterialOverrides;
		View.RenderingProps = RenderingProps;
	}
	return Result;
}

#if WITH_EDITOR

UStaticMesh* UArcUnifiedMeshVisualizationTrait::GetSingleStaticMeshDirect() const
{
	for (const FArcUnifiedStaticMeshEntry& Entry : StaticMeshes)
	{
		if (!Entry.StaticMesh.IsNull())
		{
			return Entry.StaticMesh.Get();
		}
	}
	return nullptr;
}

USkinnedAsset* UArcUnifiedMeshVisualizationTrait::GetSingleSkinnedAssetDirect() const
{
	for (const FArcUnifiedSkinnedMeshEntry& Entry : SkinnedMeshes)
	{
		if (!Entry.SkinnedAsset.IsNull())
		{
			return Entry.SkinnedAsset.Get();
		}
	}
	return nullptr;
}

FTransform UArcUnifiedMeshVisualizationTrait::GetSingleRelativeTransform() const
{
	for (const FArcUnifiedStaticMeshEntry& Entry : StaticMeshes)
	{
		if (!Entry.StaticMesh.IsNull())
		{
			return Entry.RelativeTransform;
		}
	}
	for (const FArcUnifiedSkinnedMeshEntry& Entry : SkinnedMeshes)
	{
		if (!Entry.SkinnedAsset.IsNull())
		{
			return Entry.RelativeTransform;
		}
	}
	return FTransform::Identity;
}

TArray<TObjectPtr<UMaterialInterface>> UArcUnifiedMeshVisualizationTrait::GetSingleMaterialOverridesDirect() const
{
	TArray<TObjectPtr<UMaterialInterface>> Result;
	for (const FArcUnifiedStaticMeshEntry& Entry : StaticMeshes)
	{
		if (!Entry.StaticMesh.IsNull())
		{
			ArcUnifiedMeshVisTrait_Impl::ResolveMaterialsDirect(Entry.MaterialOverrides, Result);
			return Result;
		}
	}
	for (const FArcUnifiedSkinnedMeshEntry& Entry : SkinnedMeshes)
	{
		if (!Entry.SkinnedAsset.IsNull())
		{
			ArcUnifiedMeshVisTrait_Impl::ResolveMaterialsDirect(Entry.MaterialOverrides, Result);
			return Result;
		}
	}
	return Result;
}

void UArcUnifiedMeshVisualizationTrait::CollectSoftPaths(TArray<FSoftObjectPath>& OutPaths) const
{
	for (const FArcUnifiedStaticMeshEntry& Entry : StaticMeshes)
	{
		if (!Entry.StaticMesh.IsNull())
		{
			OutPaths.AddUnique(Entry.StaticMesh.ToSoftObjectPath());
		}
		for (const TSoftObjectPtr<UMaterialInterface>& SoftMat : Entry.MaterialOverrides)
		{
			if (!SoftMat.IsNull())
			{
				OutPaths.AddUnique(SoftMat.ToSoftObjectPath());
			}
		}
	}
	for (const FArcUnifiedSkinnedMeshEntry& Entry : SkinnedMeshes)
	{
		if (!Entry.SkinnedAsset.IsNull())
		{
			OutPaths.AddUnique(Entry.SkinnedAsset.ToSoftObjectPath());
		}
		for (const TSoftObjectPtr<UMaterialInterface>& SoftMat : Entry.MaterialOverrides)
		{
			if (!SoftMat.IsNull())
			{
				OutPaths.AddUnique(SoftMat.ToSoftObjectPath());
			}
		}
	}
}

void UArcUnifiedMeshVisualizationTrait::SetMeshEntryTransform(int32 Index, const FTransform& InTransform)
{
	if (StaticMeshes.IsValidIndex(Index))
	{
		StaticMeshes[Index].RelativeTransform = InTransform;
		return;
	}
	const int32 SkinnedIndex = Index - StaticMeshes.Num();
	if (SkinnedMeshes.IsValidIndex(SkinnedIndex))
	{
		SkinnedMeshes[SkinnedIndex].RelativeTransform = InTransform;
	}
}

void UArcUnifiedMeshVisualizationTrait::SetStaticMeshEntries(TConstArrayView<FArcUnifiedStaticMeshEntry> InEntries)
{
	StaticMeshes.Reset(InEntries.Num());
	StaticMeshes.Append(InEntries.GetData(), InEntries.Num());
}

#endif // WITH_EDITOR
