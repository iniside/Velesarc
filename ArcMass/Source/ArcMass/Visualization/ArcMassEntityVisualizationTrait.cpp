// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassEntityVisualizationTrait.h"

#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Actor.h"
#include "MassActorSubsystem.h"
#include "MassCommonFragments.h"
#include "MassEntityTemplateRegistry.h"
#include "Mesh/MassEngineMeshFragments.h"
#include "ArcMassVisualizationConfigFragments.h"
#include "ArcMass/Physics/ArcMassPhysicsBody.h"
#include "ArcMass/Physics/ArcMassPhysicsBodyConfig.h"
#include "PhysicsEngine/BodySetup.h"
#include "Engine/StaticMesh.h"

#if WITH_EDITOR
#include "Editor.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcMassEntityVisualizationTrait)

// ---------------------------------------------------------------------------
// BuildTemplate
// ---------------------------------------------------------------------------

void UArcEntityVisualizationTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	BuildContext.RequireFragment<FTransformFragment>();
	BuildContext.RequireFragment<FMassActorFragment>();

	BuildContext.AddFragment<FArcVisRepresentationFragment>();
	BuildContext.AddTag<FArcVisEntityTag>();

	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);

	// Vis config (const shared)
	const FConstSharedStruct ConfigFragment = EntityManager.GetOrCreateConstSharedFragment(VisualizationConfig);
	BuildContext.AddConstSharedFragment(ConfigFragment);

	// Actor config (const shared, optional)
	if (ActorClass)
	{
		FArcVisActorConfigFragment ActorConfig;
		ActorConfig.ActorClass = ActorClass;
		BuildContext.AddConstSharedFragment(EntityManager.GetOrCreateConstSharedFragment(ActorConfig));
	}

	if (!bExtractionValid)
	{
		if (ActorClass)
		{
			UE_LOG(LogMass, Warning, TEXT("UArcEntityVisualizationTrait: ActorClass set but no extracted data — re-save the entity config asset. (%s)"),
				*ActorClass->GetName());
		}
		return;
	}

	// Static mesh (const shared)
	if (ExtractedMesh)
	{
		FArcMassStaticMeshConfigFragment StaticMeshConfigFrag(ExtractedMesh.Get());
		BuildContext.AddConstSharedFragment(EntityManager.GetOrCreateConstSharedFragment(StaticMeshConfigFrag));
	}

	// Component-relative transform (const shared)
	FArcVisComponentTransformFragment CompTransformFrag;
	CompTransformFrag.ComponentRelativeTransform = ExtractedComponentTransform;
	BuildContext.AddConstSharedFragment(EntityManager.GetOrCreateConstSharedFragment(CompTransformFrag));

	// Visualization mesh flags (const shared)
	FArcMassVisualizationMeshConfigFragment VisMeshConfigFrag;
	VisMeshConfigFrag.CastShadow = ExtractedCastShadow;
	BuildContext.AddConstSharedFragment(EntityManager.GetOrCreateConstSharedFragment(VisMeshConfigFrag));

	// Override materials (const shared, optional)
	// Config-level overrides take priority over extracted materials
	const TArray<TObjectPtr<UMaterialInterface>>& FinalMaterials =
		VisualizationConfig.MaterialOverrides.Num() > 0 ? VisualizationConfig.MaterialOverrides : ExtractedMaterials;
	if (FinalMaterials.Num() > 0)
	{
		FMassOverrideMaterialsFragment MatFrag;
		MatFrag.OverrideMaterials = FinalMaterials;
		BuildContext.AddConstSharedFragment(EntityManager.GetOrCreateConstSharedFragment(MatFrag));
	}

	// Physics body config (const shared, optional)
	if (ExtractedBodySetup)
	{
		FArcMassPhysicsBodyConfigFragment PhysicsConfig;
		PhysicsConfig.BodySetup = ExtractedBodySetup;
		PhysicsConfig.BodyTemplate = ExtractedBodyTemplate;
		PhysicsConfig.BodyType = EArcMassPhysicsBodyType::Static;
		BuildContext.AddConstSharedFragment(EntityManager.GetOrCreateConstSharedFragment(PhysicsConfig));

		BuildContext.AddFragment<FArcMassPhysicsBodyFragment>();
	}
}

// ---------------------------------------------------------------------------
// Serialize
// ---------------------------------------------------------------------------

void UArcEntityVisualizationTrait::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
}

// ---------------------------------------------------------------------------
// Editor — PostEditChangeProperty & ExtractFromActorClass
// ---------------------------------------------------------------------------

#if WITH_EDITOR

void UArcEntityVisualizationTrait::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	static const FName VisConfigName = GET_MEMBER_NAME_CHECKED(UArcEntityVisualizationTrait, VisualizationConfig);
	static const FName ActorClassName = GET_MEMBER_NAME_CHECKED(UArcEntityVisualizationTrait, ActorClass);

	if (PropertyChangedEvent.MemberProperty)
	{
		const FName PropName = PropertyChangedEvent.MemberProperty->GetFName();
		if (PropName == VisConfigName || PropName == ActorClassName)
		{
			ExtractFromActorClass();
		}
	}
}

void UArcEntityVisualizationTrait::ExtractFromActorClass()
{
	// Skip CDOs
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return;
	}

	// Clear previous extraction
	ExtractedMesh = nullptr;
	ExtractedComponentTransform = FTransform::Identity;
	ExtractedCastShadow = false;
	ExtractedMaterials.Empty();
	ExtractedBodySetup = nullptr;
	ExtractedBodyTemplate = FBodyInstance();
	bExtractionValid = false;

	if (!ActorClass)
	{
		return;
	}

	UWorld* EditorWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!EditorWorld)
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.bNoFail = true;
	SpawnParams.ObjectFlags |= RF_Transient;
	AActor* Exemplar = EditorWorld->SpawnActor<AActor>(ActorClass, FTransform::Identity, SpawnParams);

	if (!Exemplar)
	{
		UE_LOG(LogMass, Error, TEXT("UArcEntityVisualizationTrait: Failed to spawn exemplar for %s"),
			*ActorClass->GetName());
		return;
	}

	// --- Mesh extraction ---
	UStaticMeshComponent* MeshComp = Exemplar->FindComponentByClass<UStaticMeshComponent>();
	if (MeshComp && MeshComp->GetStaticMesh())
	{
		ExtractedMesh = MeshComp->GetStaticMesh();
		ExtractedComponentTransform = MeshComp->GetRelativeTransform();
		ExtractedCastShadow = MeshComp->CastShadow;

		for (int32 MatIdx = 0; MatIdx < MeshComp->GetNumMaterials(); ++MatIdx)
		{
			UMaterialInterface* Mat = MeshComp->GetMaterial(MatIdx);
			if (Mat)
			{
				ExtractedMaterials.Add(Mat);
			}
		}
	}
	else
	{
		UE_LOG(LogMass, Warning, TEXT("UArcEntityVisualizationTrait: No UStaticMeshComponent with mesh found on exemplar %s"),
			*ActorClass->GetName());
	}

	// --- Collision extraction ---
	{
		UBodySetup* MergedSetup = NewObject<UBodySetup>(this, NAME_None, RF_NoFlags);
		MergedSetup->CollisionTraceFlag = CTF_UseSimpleAsComplex;
		bool bFoundCollision = false;

		TInlineComponentArray<UPrimitiveComponent*> PrimComponents;
		Exemplar->GetComponents<UPrimitiveComponent>(PrimComponents);

		for (UPrimitiveComponent* PrimComp : PrimComponents)
		{
			if (PrimComp->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
			{
				continue;
			}

			UBodySetup* SourceSetup = PrimComp->GetBodySetup();
			if (!SourceSetup)
			{
				continue;
			}

			const FTransform CompTransform = PrimComp->GetRelativeTransform();
			const FKAggregateGeom& SourceGeom = SourceSetup->AggGeom;

			for (const FKBoxElem& Box : SourceGeom.BoxElems)
			{
				FKBoxElem NewBox = Box;
				FTransform ShapeTransform = NewBox.GetTransform() * CompTransform;
				NewBox.SetTransform(ShapeTransform);
				MergedSetup->AggGeom.BoxElems.Add(NewBox);
				bFoundCollision = true;
			}

			for (const FKSphereElem& Sphere : SourceGeom.SphereElems)
			{
				FKSphereElem NewSphere = Sphere;
				FTransform ShapeTransform = NewSphere.GetTransform() * CompTransform;
				NewSphere.SetTransform(ShapeTransform);
				MergedSetup->AggGeom.SphereElems.Add(NewSphere);
				bFoundCollision = true;
			}

			for (const FKSphylElem& Sphyl : SourceGeom.SphylElems)
			{
				FKSphylElem NewSphyl = Sphyl;
				FTransform ShapeTransform = NewSphyl.GetTransform() * CompTransform;
				NewSphyl.SetTransform(ShapeTransform);
				MergedSetup->AggGeom.SphylElems.Add(NewSphyl);
				bFoundCollision = true;
			}

			for (const FKConvexElem& Convex : SourceGeom.ConvexElems)
			{
				FKConvexElem NewConvex = Convex;
				NewConvex.SetTransform(Convex.GetTransform() * CompTransform);
				MergedSetup->AggGeom.ConvexElems.Add(NewConvex);
				bFoundCollision = true;
			}
		}

		if (bFoundCollision)
		{
			MergedSetup->CreatePhysicsMeshes();

			UPrimitiveComponent* RootPrim = Cast<UPrimitiveComponent>(Exemplar->GetRootComponent());
			if (RootPrim)
			{
				UE::ArcMass::Physics::CopyBodyInstanceConfig(ExtractedBodyTemplate, RootPrim->BodyInstance);
			}

			ExtractedBodySetup = MergedSetup;
		}
	}

	bExtractionValid = (ExtractedMesh != nullptr);
	Exemplar->Destroy();

	if (bExtractionValid)
	{
		UE_LOG(LogMass, Log, TEXT("UArcEntityVisualizationTrait: Extracted mesh '%s' and %s collision from %s"),
			*ExtractedMesh->GetName(),
			ExtractedBodySetup ? TEXT("with") : TEXT("no"),
			*ActorClass->GetName());
	}
}

#endif // WITH_EDITOR
