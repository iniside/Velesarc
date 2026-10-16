// Copyright Lukasz Baran. All Rights Reserved.

/**
 * Tests for UArcUnifiedMeshVisualizationTrait::BuildTemplate mode-collapse logic.
 *
 * Fixture: FActorTestSpawner + InitializeGameSubsystems (provides UMassEntitySubsystem,
 * which BuildTemplate requires via UE::Mass::Utils::GetEntityManagerChecked).
 *
 * Mesh residency: static tests use /Engine/BasicShapes/Cube (always cooked in),
 * skinned tests use /Engine/EngineMeshes/SkeletalCube (cooked engine skeletal mesh).
 *
 * StaticMeshes / SkinnedMeshes / RenderingProps are protected UPROPERTYs — set via
 * FindFProperty reflection.
 *
 * Dedicated-server test: FActorTestSpawner worlds are NM_Standalone, not NM_DedicatedServer,
 * so the dedicated-server branch cannot be driven with this fixture. That test is replaced
 * with a GetInferredMode-only unit assertion that exercises the None/Static/Skinned/Composite
 * inference on soft paths without requiring residency or a build context.
 */

#include "CQTest.h"
#include "Components/ActorTestSpawner.h"

#include "Mass/EntityManager.h"
#include "MassEntitySubsystem.h"
#include "MassEntityTemplate.h"
#include "MassEntityTemplateRegistry.h"

#include "ArcMass/Visualization/ArcUnifiedMeshVisualizationTrait.h"
#include "ArcMass/Visualization/ArcMassEntityVisualization.h"         // FArcVisEntityTag, FArcVisConfigFragment, FArcVisComponentTransformFragment
#include "ArcMass/Visualization/ArcMassVisualizationConfigFragments.h" // FArcMassStaticMeshConfigFragment, FArcMassVisualizationMeshConfigFragment
#include "ArcMass/SkinnedMeshVisualization/ArcMassSkinnedMeshFragments.h"
#include "ArcMass/CompositeVisualization/ArcCompositeMeshVisualizationFragments.h"

#include "Engine/StaticMesh.h"
#include "Engine/SkinnedAsset.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"
#include "Containers/Array.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcUnifiedMeshVisTraitTest, Display, All);

namespace ArcUnifiedMeshVisTraitTests_Private
{
	/**
	 * Thin subclass of FMassEntityTemplateBuildContext that exposes SetTraitBeingProcessed
	 * so tests can drive BuildTemplate directly without going through BuildFromTraits.
	 */
	struct FTestBuildContext : public FMassEntityTemplateBuildContext
	{
		explicit FTestBuildContext(FMassEntityTemplateData& InTemplateData)
			: FMassEntityTemplateBuildContext(InTemplateData)
		{}

		void BeginTrait(const UMassEntityTraitBase* Trait)
		{
			SetTraitBeingProcessed(Trait);
		}
	};

	// Writes a USTRUCT-entry array into a protected TArray UPROPERTY via reflection.
	template<typename TEntry>
	static void SetEntryArray(UArcUnifiedMeshVisualizationTrait* Trait, const TCHAR* PropName, const TArray<TEntry>& Entries)
	{
		FArrayProperty* Prop = FindFProperty<FArrayProperty>(UArcUnifiedMeshVisualizationTrait::StaticClass(), PropName);
		check(Prop);
		FScriptArrayHelper Helper(Prop, Prop->ContainerPtrToValuePtr<void>(Trait));
		Helper.EmptyValues();
		for (const TEntry& Entry : Entries)
		{
			const int32 Idx = Helper.AddValue();
			Prop->Inner->CopySingleValue(Helper.GetRawPtr(Idx), &Entry);
		}
	}

	static void SetStaticMeshes(UArcUnifiedMeshVisualizationTrait* Trait, const TArray<FArcUnifiedStaticMeshEntry>& Entries)
	{
		SetEntryArray(Trait, TEXT("StaticMeshes"), Entries);
	}

	static void SetSkinnedMeshes(UArcUnifiedMeshVisualizationTrait* Trait, const TArray<FArcUnifiedSkinnedMeshEntry>& Entries)
	{
		SetEntryArray(Trait, TEXT("SkinnedMeshes"), Entries);
	}

	// Writes the trait-wide RenderingProps (protected UPROPERTY) via reflection.
	static void SetRenderingProps(UArcUnifiedMeshVisualizationTrait* Trait, const FArcMassVisRenderingProps& Props)
	{
		FStructProperty* Prop = FindFProperty<FStructProperty>(UArcUnifiedMeshVisualizationTrait::StaticClass(), TEXT("RenderingProps"));
		check(Prop);
		Prop->CopySingleValue(Prop->ContainerPtrToValuePtr<void>(Trait), &Props);
	}

	/**
	 * Drive BuildTemplate directly on a single trait, bypassing BuildFromTraits to avoid
	 * the editor UMassTraitRepository CDO-scan path that crashes on default-constructed traits.
	 * Caller owns the returned FMassEntityTemplateData lifetime.
	 */
	static FMassEntityTemplateData BuildTemplateData(UArcUnifiedMeshVisualizationTrait* Trait, const UWorld& World)
	{
		FMassEntityTemplateData TemplateData;
		FTestBuildContext BuildContext(TemplateData);
		BuildContext.BeginTrait(Trait);
		Trait->BuildTemplate(BuildContext, World);
		return TemplateData;
	}

	// Read a const-shared fragment value by type from FMassEntityTemplateData.
	template<typename T>
	static const T* FindConstSharedFragment(const FMassEntityTemplateData& Data)
	{
		const TArray<FConstSharedStruct>& ConstShared = Data.GetSharedFragmentValues().GetConstSharedFragments();
		for (const FConstSharedStruct& S : ConstShared)
		{
			if (S.GetScriptStruct() == T::StaticStruct())
			{
				return &S.Get<T>();
			}
		}
		return nullptr;
	}
} // namespace ArcUnifiedMeshVisTraitTests_Private

// -------------------------------------------------------------------------
// Test class
// -------------------------------------------------------------------------

TEST_CLASS(ArcUnifiedMeshVisTrait_BuildTemplate, "ArcMassPlacedEntity.UnifiedMeshVisTrait")
{
	FActorTestSpawner Spawner;
	FMassEntityManager* EntityManager = nullptr;

	BEFORE_EACH()
	{
		FAutomationTestBase::bSuppressLogWarnings = true;
		FAutomationTestBase::bSuppressLogErrors = true;
		FAutomationTestBase::SuppressedLogCategories.AddUnique(TEXT("LogUObjectGlobals"));
		FAutomationTestBase::SuppressedLogCategories.AddUnique(TEXT("LogRHI"));
		FAutomationTestBase::SuppressedLogCategories.AddUnique(TEXT("LogMass"));

		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();

		UMassEntitySubsystem* MES = Spawner.GetWorld().GetSubsystem<UMassEntitySubsystem>();
		ASSERT_THAT(IsNotNull(MES));
		EntityManager = &MES->GetMutableEntityManager();
	}

	// -------------------------------------------------------------------------
	// Test 1: Single static entry → static-ISM fragment set emitted.
	//   FArcVisEntityTag present, FArcCompositeMeshVisEntityTag absent,
	//   FArcMassStaticMeshConfigFragment const-shared present.
	// -------------------------------------------------------------------------
	TEST_METHOD(BuildTemplate_SingleStatic_EmitsStaticSet)
	{
		UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
		ASSERT_THAT(IsNotNull(Cube));

		UArcUnifiedMeshVisualizationTrait* Trait = NewObject<UArcUnifiedMeshVisualizationTrait>(&Spawner.GetWorld());

		FArcUnifiedStaticMeshEntry Entry;
		Entry.StaticMesh = TSoftObjectPtr<UStaticMesh>(Cube);
		ArcUnifiedMeshVisTraitTests_Private::SetStaticMeshes(Trait, {Entry});

		FMassEntityTemplateData Data = ArcUnifiedMeshVisTraitTests_Private::BuildTemplateData(Trait, Spawner.GetWorld());

		// Static set: FArcVisEntityTag must be present.
		ASSERT_THAT(IsTrue(Data.HasTag<FArcVisEntityTag>()));

		// Composite tag must NOT be present.
		ASSERT_THAT(IsFalse(Data.HasTag<FArcCompositeMeshVisEntityTag>()));

		// FArcMassStaticMeshConfigFragment const-shared must be present.
		const FArcMassStaticMeshConfigFragment* StaticConfig =
			ArcUnifiedMeshVisTraitTests_Private::FindConstSharedFragment<FArcMassStaticMeshConfigFragment>(Data);
		ASSERT_THAT(IsNotNull(StaticConfig));
	}

	// -------------------------------------------------------------------------
	// Test 2: Single skinned entry → skinned fragment set emitted.
	//   FArcMassSkinnedMeshFragment present, FArcMassStaticMeshConfigFragment absent.
	//   Also asserts the trait-wide RenderingProps (bVisibleInRayTracing=false +
	//   MinDrawDistance=123) reach the FArcMassVisualizationMeshConfigFragment.
	// -------------------------------------------------------------------------
	TEST_METHOD(BuildTemplate_SingleSkinned_EmitsSkinnedSet)
	{
		USkinnedAsset* SkeletalCube = LoadObject<USkinnedAsset>(nullptr, TEXT("/Engine/EngineMeshes/SkeletalCube.SkeletalCube"));
		if (SkeletalCube == nullptr)
		{
			UE_LOGF(LogArcUnifiedMeshVisTraitTest, Warning,"BuildTemplate_SingleSkinned_EmitsSkinnedSet: /Engine/EngineMeshes/SkeletalCube not available in this cook; test skipped.");
			return;
		}

		UArcUnifiedMeshVisualizationTrait* Trait = NewObject<UArcUnifiedMeshVisualizationTrait>(&Spawner.GetWorld());

		FArcUnifiedSkinnedMeshEntry Entry;
		Entry.SkinnedAsset = TSoftObjectPtr<USkinnedAsset>(SkeletalCube);
		ArcUnifiedMeshVisTraitTests_Private::SetSkinnedMeshes(Trait, {Entry});

		FArcMassVisRenderingProps Props;
		Props.bVisibleInRayTracing = false;
		Props.MinDrawDistance = 123.f;
		ArcUnifiedMeshVisTraitTests_Private::SetRenderingProps(Trait, Props);

		FMassEntityTemplateData Data = ArcUnifiedMeshVisTraitTests_Private::BuildTemplateData(Trait, Spawner.GetWorld());

		// Static config must NOT be present (skinned path uses FArcVisEntityTag too, but no static config).
		const FArcMassStaticMeshConfigFragment* StaticConfig =
			ArcUnifiedMeshVisTraitTests_Private::FindConstSharedFragment<FArcMassStaticMeshConfigFragment>(Data);
		ASSERT_THAT(IsNull(StaticConfig));

		// FArcMassSkinnedMeshFragment const-shared must be present.
		const FArcMassSkinnedMeshFragment* SkinnedFrag =
			ArcUnifiedMeshVisTraitTests_Private::FindConstSharedFragment<FArcMassSkinnedMeshFragment>(Data);
		ASSERT_THAT(IsNotNull(SkinnedFrag));

		// RenderingProps → FArcMassVisualizationMeshConfigFragment conversion check.
		const FArcMassVisualizationMeshConfigFragment* VisMeshConfig =
			ArcUnifiedMeshVisTraitTests_Private::FindConstSharedFragment<FArcMassVisualizationMeshConfigFragment>(Data);
		ASSERT_THAT(IsNotNull(VisMeshConfig));
		ASSERT_THAT(IsFalse(VisMeshConfig->VisibleInRayTracing));
		ASSERT_THAT(IsTrue(VisMeshConfig->MinDrawDistance == 123.f));
	}

	// -------------------------------------------------------------------------
	// Test 3: Static + skinned entries → composite fragment set emitted.
	//   FArcCompositeMeshVisEntityTag + FArcCompositeMeshVisInstanceLinkFragment present;
	//   FArcCompositeMeshConfigFragment.Entries.Num()==2 with statics ordered first.
	//
	//   If SkeletalCube is unavailable, falls back to two static entries (still composite).
	// -------------------------------------------------------------------------
	TEST_METHOD(BuildTemplate_Mixed_EmitsCompositeSet)
	{
		UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
		ASSERT_THAT(IsNotNull(Cube));

		UArcUnifiedMeshVisualizationTrait* Trait = NewObject<UArcUnifiedMeshVisualizationTrait>(&Spawner.GetWorld());

		FArcUnifiedStaticMeshEntry StaticEntry;
		StaticEntry.StaticMesh = TSoftObjectPtr<UStaticMesh>(Cube);

		USkinnedAsset* SkeletalCube = LoadObject<USkinnedAsset>(nullptr, TEXT("/Engine/EngineMeshes/SkeletalCube.SkeletalCube"));
		if (SkeletalCube != nullptr)
		{
			// True static+skinned composite.
			ArcUnifiedMeshVisTraitTests_Private::SetStaticMeshes(Trait, {StaticEntry});

			FArcUnifiedSkinnedMeshEntry SkinnedEntry;
			SkinnedEntry.SkinnedAsset = TSoftObjectPtr<USkinnedAsset>(SkeletalCube);
			ArcUnifiedMeshVisTraitTests_Private::SetSkinnedMeshes(Trait, {SkinnedEntry});
		}
		else
		{
			// Fallback: two static entries → still Composite mode.
			UE_LOGF(LogArcUnifiedMeshVisTraitTest, Warning,"BuildTemplate_Mixed_EmitsCompositeSet: SkeletalCube not available; using 2 static entries for composite path.");
			ArcUnifiedMeshVisTraitTests_Private::SetStaticMeshes(Trait, {StaticEntry, StaticEntry});
		}

		FMassEntityTemplateData Data = ArcUnifiedMeshVisTraitTests_Private::BuildTemplateData(Trait, Spawner.GetWorld());

		// Composite tag must be present.
		ASSERT_THAT(IsTrue(Data.HasTag<FArcCompositeMeshVisEntityTag>()));

		// Single-mesh static tag must NOT be present.
		ASSERT_THAT(IsFalse(Data.HasTag<FArcVisEntityTag>()));

		// FArcCompositeMeshVisInstanceLinkFragment must be present.
		ASSERT_THAT(IsTrue(Data.HasFragment<FArcCompositeMeshVisInstanceLinkFragment>()));

		// FArcCompositeMeshConfigFragment const-shared with 2 entries.
		const FArcCompositeMeshConfigFragment* CompositeConfig =
			ArcUnifiedMeshVisTraitTests_Private::FindConstSharedFragment<FArcCompositeMeshConfigFragment>(Data);
		ASSERT_THAT(IsNotNull(CompositeConfig));
		ASSERT_THAT(AreEqual(2, CompositeConfig->Entries.Num()));

		if (SkeletalCube != nullptr)
		{
			// Statics first, then skinned — fixed emission ordering.
			ASSERT_THAT(IsFalse(CompositeConfig->Entries[0].IsSkinned()));
			ASSERT_THAT(IsTrue(CompositeConfig->Entries[1].IsSkinned()));
		}
		else
		{
			ASSERT_THAT(IsFalse(CompositeConfig->Entries[0].IsSkinned()));
			ASSERT_THAT(IsFalse(CompositeConfig->Entries[1].IsSkinned()));
		}
	}

	// -------------------------------------------------------------------------
	// Test 4 (replaces DedicatedServer test — see file comment):
	//   GetInferredMode unit assertions on soft-path authored entries.
	//   No mesh residency required; no world/build-context required.
	//
	//   Covers: None (no entries), Static (1 static), Skinned (1 skinned),
	//           Composite (2 static), Composite (1 static + 1 skinned via soft paths).
	// -------------------------------------------------------------------------
	TEST_METHOD(GetInferredMode_SoftPaths_Correct)
	{
		// Synthetic soft paths — no actual asset, just non-null FSoftObjectPaths.
		const FSoftObjectPath FakeMeshPath(TEXT("/Game/Test/FakeMesh.FakeMesh"));
		const FSoftObjectPath FakeSkinnedPath(TEXT("/Game/Test/FakeSkinned.FakeSkinned"));

		UArcUnifiedMeshVisualizationTrait* Trait = NewObject<UArcUnifiedMeshVisualizationTrait>(&Spawner.GetWorld());

		FArcUnifiedStaticMeshEntry StaticEntry;
		StaticEntry.StaticMesh = TSoftObjectPtr<UStaticMesh>(FakeMeshPath);
		FArcUnifiedSkinnedMeshEntry SkinnedEntry;
		SkinnedEntry.SkinnedAsset = TSoftObjectPtr<USkinnedAsset>(FakeSkinnedPath);

		// --- None: no entries ---
		ArcUnifiedMeshVisTraitTests_Private::SetStaticMeshes(Trait, {});
		ArcUnifiedMeshVisTraitTests_Private::SetSkinnedMeshes(Trait, {});
		ASSERT_THAT(IsTrue(Trait->GetInferredMode() == EArcUnifiedMeshVisMode::None));

		// --- None: entries exist but their soft paths are null ---
		ArcUnifiedMeshVisTraitTests_Private::SetStaticMeshes(Trait, {FArcUnifiedStaticMeshEntry()});
		ArcUnifiedMeshVisTraitTests_Private::SetSkinnedMeshes(Trait, {FArcUnifiedSkinnedMeshEntry()});
		ASSERT_THAT(IsTrue(Trait->GetInferredMode() == EArcUnifiedMeshVisMode::None));

		// --- Static: 1 static entry ---
		ArcUnifiedMeshVisTraitTests_Private::SetStaticMeshes(Trait, {StaticEntry});
		ArcUnifiedMeshVisTraitTests_Private::SetSkinnedMeshes(Trait, {});
		ASSERT_THAT(IsTrue(Trait->GetInferredMode() == EArcUnifiedMeshVisMode::Static));

		// --- Skinned: 1 skinned entry ---
		ArcUnifiedMeshVisTraitTests_Private::SetStaticMeshes(Trait, {});
		ArcUnifiedMeshVisTraitTests_Private::SetSkinnedMeshes(Trait, {SkinnedEntry});
		ASSERT_THAT(IsTrue(Trait->GetInferredMode() == EArcUnifiedMeshVisMode::Skinned));

		// --- Composite: 2 static entries ---
		ArcUnifiedMeshVisTraitTests_Private::SetStaticMeshes(Trait, {StaticEntry, StaticEntry});
		ArcUnifiedMeshVisTraitTests_Private::SetSkinnedMeshes(Trait, {});
		ASSERT_THAT(IsTrue(Trait->GetInferredMode() == EArcUnifiedMeshVisMode::Composite));

		// --- Composite: 1 static + 1 skinned (mixed) ---
		ArcUnifiedMeshVisTraitTests_Private::SetStaticMeshes(Trait, {StaticEntry});
		ArcUnifiedMeshVisTraitTests_Private::SetSkinnedMeshes(Trait, {SkinnedEntry});
		ASSERT_THAT(IsTrue(Trait->GetInferredMode() == EArcUnifiedMeshVisMode::Composite));
	}

	// -------------------------------------------------------------------------
	// Test 5: Entries whose soft path is unset are ignored by the mode inference.
	//   StaticMeshes = [empty entry, cube entry] → 1 authored static → Static mode.
	// -------------------------------------------------------------------------
	TEST_METHOD(BuildTemplate_EmptyEntries_Ignored)
	{
		UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
		ASSERT_THAT(IsNotNull(Cube));

		UArcUnifiedMeshVisualizationTrait* Trait = NewObject<UArcUnifiedMeshVisualizationTrait>(&Spawner.GetWorld());

		FArcUnifiedStaticMeshEntry EmptyEntry; // null soft path — not authored
		FArcUnifiedStaticMeshEntry ValidEntry;
		ValidEntry.StaticMesh = TSoftObjectPtr<UStaticMesh>(Cube);
		ArcUnifiedMeshVisTraitTests_Private::SetStaticMeshes(Trait, {EmptyEntry, ValidEntry});

		// Empty entry excluded → 1 authored static → Static mode.
		ASSERT_THAT(IsTrue(Trait->GetInferredMode() == EArcUnifiedMeshVisMode::Static));

		FMassEntityTemplateData Data = ArcUnifiedMeshVisTraitTests_Private::BuildTemplateData(Trait, Spawner.GetWorld());

		// Static set: FArcVisEntityTag present, composite tag absent.
		ASSERT_THAT(IsTrue(Data.HasTag<FArcVisEntityTag>()));
		ASSERT_THAT(IsFalse(Data.HasTag<FArcCompositeMeshVisEntityTag>()));

		// FArcMassStaticMeshConfigFragment const-shared present.
		const FArcMassStaticMeshConfigFragment* StaticConfig =
			ArcUnifiedMeshVisTraitTests_Private::FindConstSharedFragment<FArcMassStaticMeshConfigFragment>(Data);
		ASSERT_THAT(IsNotNull(StaticConfig));
	}
};
