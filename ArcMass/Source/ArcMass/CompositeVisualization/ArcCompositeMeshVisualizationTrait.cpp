// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcCompositeMeshVisualizationTrait.h"

#include "ArcCompositeMeshVisualizationFragments.h"
#include "MassCommonFragments.h"
#include "MassActorSubsystem.h"
#include "MassEntityTemplateRegistry.h"
#include "ArcMass/Visualization/ArcMassEntityVisualization.h"
#include "ArcMass/Visualization/ArcMassVisualizationConfigFragments.h"
#include "Engine/StaticMesh.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcCompositeMeshVisualizationTrait)

void UArcCompositeMeshVisualizationTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	BuildContext.RequireFragment<FTransformFragment>();
	BuildContext.RequireFragment<FMassActorFragment>();

	BuildContext.AddFragment<FArcVisRepresentationFragment>();
	BuildContext.AddTag<FArcVisEntityTag>();

	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);

	// Vis config (const shared)
	FArcVisConfigFragment VisConfig;
	VisConfig.bCastShadows = false;
	for (const FArcCompositeMeshEntry& Entry : MeshEntries)
	{
		if (Entry.bCastShadow)
		{
			VisConfig.bCastShadows = true;
			break;
		}
	}
	BuildContext.AddConstSharedFragment(EntityManager.GetOrCreateConstSharedFragment(VisConfig));

	// Composite mesh config (const shared)
	FArcCompositeMeshConfigFragment CompositeMeshConfig;
	for (const FArcCompositeMeshEntry& Entry : MeshEntries)
	{
		if (!Entry.Mesh)
		{
			continue;
		}

		FArcCompositeMeshEntryConfig& EntryConfig = CompositeMeshConfig.Entries.AddDefaulted_GetRef();
		EntryConfig.Mesh = Entry.Mesh.Get();
		EntryConfig.RelativeTransform = Entry.RelativeTransform;
		EntryConfig.MaterialOverrides = Entry.MaterialOverrides;

		FArcMassVisualizationMeshConfigFragment& MeshConfig = EntryConfig.MeshConfig;
		MeshConfig.CastShadow = Entry.bCastShadow;
		MeshConfig.CastShadowAsTwoSided = Entry.bCastShadowAsTwoSided;
		MeshConfig.CastHiddenShadow = Entry.bCastHiddenShadow;
		MeshConfig.CastFarShadow = Entry.bCastFarShadow;
		MeshConfig.CastInsetShadow = Entry.bCastInsetShadow;
		MeshConfig.CastContactShadow = Entry.bCastContactShadow;
		MeshConfig.HiddenInGame = Entry.bHiddenInGame;
		MeshConfig.ReceivesDecals = Entry.bReceivesDecals;
		MeshConfig.NeverDistanceCull = Entry.bNeverDistanceCull;
		MeshConfig.RenderCustomDepth = Entry.bRenderCustomDepth;
		MeshConfig.CustomDepthStencilValue = Entry.CustomDepthStencilValue;
		MeshConfig.TranslucencySortPriority = Entry.TranslucencySortPriority;
		MeshConfig.AffectDynamicIndirectLighting = Entry.bAffectDynamicIndirectLighting;
		MeshConfig.AffectIndirectLightingWhileHidden = Entry.bAffectIndirectLightingWhileHidden;
	}

	if (CompositeMeshConfig.Entries.Num() > 0)
	{
		BuildContext.AddConstSharedFragment(EntityManager.GetOrCreateConstSharedFragment(CompositeMeshConfig));
	}

}

#if WITH_EDITOR
bool UArcCompositeMeshVisualizationTrait::IsValid() const
{
	for (const FArcCompositeMeshEntry& Entry : MeshEntries)
	{
		if (Entry.Mesh)
		{
			return true;
		}
	}
	return false;
}

void UArcCompositeMeshVisualizationTrait::SetMeshEntryTransform(int32 Index, const FTransform& InTransform)
{
	if (MeshEntries.IsValidIndex(Index))
	{
		MeshEntries[Index].RelativeTransform = InTransform;
	}
}
#endif
