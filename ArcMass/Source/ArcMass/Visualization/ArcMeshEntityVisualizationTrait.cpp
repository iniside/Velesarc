// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMeshEntityVisualizationTrait.h"

#include "MassCommonFragments.h"
#include "MassEntityTemplateRegistry.h"
#include "Mesh/MassEngineMeshFragments.h"
#include "ArcMassEntityVisualization.h"
#include "ArcMassVisualizationConfigFragments.h"
#include "Engine/StaticMesh.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcMeshEntityVisualizationTrait)

void UArcMeshEntityVisualizationTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	BuildContext.RequireFragment<FTransformFragment>();

	BuildContext.AddFragment<FArcVisRepresentationFragment>();
	BuildContext.AddTag<FArcVisEntityTag>();

	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);

	// Vis config (const shared) — no ActorClass
	FArcVisConfigFragment VisConfig;
	VisConfig.bCastShadows = bCastShadow;
	BuildContext.AddConstSharedFragment(EntityManager.GetOrCreateConstSharedFragment(VisConfig));

	if (!Mesh)
	{
		return;
	}

	// Static mesh (const shared)
	FArcMassStaticMeshConfigFragment StaticMeshConfigFrag(Mesh.Get());
	BuildContext.AddConstSharedFragment(EntityManager.GetOrCreateConstSharedFragment(StaticMeshConfigFrag));

	// Component-relative transform (const shared)
	FArcVisComponentTransformFragment CompTransformFrag;
	CompTransformFrag.ComponentRelativeTransform = ComponentTransform;
	BuildContext.AddConstSharedFragment(EntityManager.GetOrCreateConstSharedFragment(CompTransformFrag));

	// Visualization mesh flags (const shared)
	FArcMassVisualizationMeshConfigFragment VisMeshConfigFrag;
	VisMeshConfigFrag.CastShadow = bCastShadow;
	VisMeshConfigFrag.CastShadowAsTwoSided = bCastShadowAsTwoSided;
	VisMeshConfigFrag.CastHiddenShadow = bCastHiddenShadow;
	VisMeshConfigFrag.CastFarShadow = bCastFarShadow;
	VisMeshConfigFrag.CastInsetShadow = bCastInsetShadow;
	VisMeshConfigFrag.CastContactShadow = bCastContactShadow;
	VisMeshConfigFrag.HiddenInGame = bHiddenInGame;
	VisMeshConfigFrag.ReceivesDecals = bReceivesDecals;
	VisMeshConfigFrag.NeverDistanceCull = bNeverDistanceCull;
	VisMeshConfigFrag.RenderCustomDepth = bRenderCustomDepth;
	VisMeshConfigFrag.CustomDepthStencilValue = CustomDepthStencilValue;
	VisMeshConfigFrag.TranslucencySortPriority = TranslucencySortPriority;
	VisMeshConfigFrag.AffectDynamicIndirectLighting = bAffectDynamicIndirectLighting;
	VisMeshConfigFrag.AffectIndirectLightingWhileHidden = bAffectIndirectLightingWhileHidden;
	BuildContext.AddConstSharedFragment(EntityManager.GetOrCreateConstSharedFragment(VisMeshConfigFrag));

	// Override materials (const shared, optional)
	if (MaterialOverrides.Num() > 0)
	{
		FMassOverrideMaterialsFragment MatFrag;
		MatFrag.OverrideMaterials = MaterialOverrides;
		BuildContext.AddConstSharedFragment(EntityManager.GetOrCreateConstSharedFragment(MatFrag));
	}

}
