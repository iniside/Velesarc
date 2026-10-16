// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "Components/ActorTestSpawner.h"

#include "NativeGameplayTags.h"
#include "StructUtils/InstancedStruct.h"

#include "Mass/EntityManager.h"
#include "MassEntitySubsystem.h"
#include "Mass/EntityHandle.h"

#include "ArcItemTestRecordFiles.h"
#include "Items/ArcItemDefinitionData.h"
#include "Items/ArcItemSpec.h"
#include "Fragments/ArcMassItemStoreFragment.h"
#include "Operations/ArcMassItemOperations.h"

#include "ArcBuildIngredient.h"
#include "ArcBuildRequirement.h"
#include "ArcBuildResourceConsumption.h"
#include "ArcBuildingDefinition.h"
#include "Commands/ArcPlaceBuildingCommand.h"
#include "ArcNamedPrimaryAssetId.h"

#include "ArcBuilderTestHelpers.h"

// ---------------------------------------------------------------------------
// Unique tag prefix: TAG_PlaceCmd_* — place-building command gate tests
// ---------------------------------------------------------------------------

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_PlaceCmd_Wood, "Test.Material.PlaceCmdWood");

namespace ArcPlaceBuildingCommandTestHelpers
{
	using namespace ArcBuilderTest::TestHelpers;

	/** Unique per-test-run primary asset type so AddDynamicAsset never collides with a
	 *  disk-registered ArcBuilding type. */
	const FPrimaryAssetType& TestBuildingType()
	{
		static const FPrimaryAssetType Type(TEXT("ArcTest.PlaceBuildingCommand"));
		return Type;
	}
}

TEST_CLASS(ArcPlaceBuildingCommandGate, "ArcBuilder.Commands.PlaceBuilding")
{
	FActorTestSpawner Spawner;
	FMassEntityManager* EM = nullptr;
	FArcItemTestRecordFiles Records;

	// Primary-asset ids registered via AddDynamicAsset this test; cleaned up in AFTER_EACH.
	TArray<FPrimaryAssetId> DynamicIds;

	BEFORE_EACH()
	{
		FAutomationTestBase::bSuppressLogWarnings = true;
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogUObjectGlobals");

		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();

		UMassEntitySubsystem* MES = Spawner.GetWorld().GetSubsystem<UMassEntitySubsystem>();
		check(MES);
		EM = &MES->GetMutableEntityManager();
	}

	AFTER_EACH()
	{
		using namespace ArcBuilderTest::TestHelpers;
		for (const FPrimaryAssetId& Id : DynamicIds)
		{
			UnregisterDynamicAsset(Id);
		}
		DynamicIds.Reset();
	}

	// =========================================================================
	// InactiveInstigator_ReturnsFalse — a never-created / unset instigator handle
	// fails the very first gate (EntityManager.IsEntityActive), before any
	// definition resolution is attempted.
	// =========================================================================
	TEST_METHOD(InactiveInstigator_ReturnsFalse)
	{
		FArcPlaceBuildingCommand Cmd(
			FArcNamedPrimaryAssetId(FPrimaryAssetId(FPrimaryAssetType(TEXT("ArcTest.Whatever")), TEXT("DoesNotMatter"))),
			FTransform::Identity,
			FMassEntityHandle()); // default-constructed — never created, not active

		const bool bResult = Cmd.Execute(&Spawner.GetWorld());
		ASSERT_THAT(IsFalse(bResult, TEXT("An inactive instigator entity must fail Execute before any other gate runs")));
	}

	// =========================================================================
	// UnresolvableDefinition_ReturnsFalse — a valid, active instigator but a
	// building definition id that resolves to nothing (never registered).
	// =========================================================================
	TEST_METHOD(UnresolvableDefinition_ReturnsFalse)
	{
		using namespace ArcBuilderTest::TestHelpers;

		FMassEntityHandle Instigator = CreateBareEntity(*EM);

		FArcPlaceBuildingCommand Cmd(
			FArcNamedPrimaryAssetId(FPrimaryAssetId(FPrimaryAssetType(TEXT("ArcTest.Whatever")), TEXT("NeverRegistered"))),
			FTransform::Identity,
			Instigator);

		const bool bResult = Cmd.Execute(&Spawner.GetWorld());
		ASSERT_THAT(IsFalse(bResult, TEXT("An unresolvable building definition id must fail Execute")));
	}

	// =========================================================================
	// FailedRequirement_ReturnsFalse_StoreUntouched — a registered, free-placement
	// (no ingredients) building definition whose sole BuildRequirement refuses
	// placement. Execute must fail at the requirement gate, before resource
	// consumption runs — the requirement fixture is reached (its call counter
	// proves it), and the instigator's item store is left untouched.
	// =========================================================================
	TEST_METHOD(FailedRequirement_ReturnsFalse_StoreUntouched)
	{
		using namespace ArcBuilderTest::TestHelpers;
		using namespace ArcPlaceBuildingCommandTestHelpers;

		UArcBuildingDefinition* BuildingDef = MakeTransientBuildingDef(TEXT("PlaceCmd3_Building"));
		BuildingDef->ResourceConsumption = TInstancedStruct<FArcBuildResourceConsumption>::Make<FArcBuildResourceConsumption_MassStore>();

		TInstancedStruct<FArcBuildRequirement> FailingReq;
		FailingReq.InitializeAs<FArcBuildRequirement_TestControlled>();
		FailingReq.GetMutablePtr<FArcBuildRequirement_TestControlled>()->bResult = false;
		BuildingDef->BuildRequirements.Add(MoveTemp(FailingReq));

		const FPrimaryAssetId RegisteredId = RegisterBuildingDefDynamic(BuildingDef, TestBuildingType());
		ASSERT_THAT(IsTrue(RegisteredId.IsValid(), TEXT("Dynamic asset registration must succeed")));
		DynamicIds.Add(RegisteredId);

		const FPrimaryAssetId WoodDef = MakeTaggedItemDef(Records, TAG_PlaceCmd_Wood);
		ASSERT_THAT(IsTrue(WoodDef.IsValid(), TEXT("Item record must be written and indexed")));
		FMassEntityHandle Instigator = CreateStoreEntity(*EM);
		ArcMassItems::AddItem(*EM, Instigator, FArcMassItemStoreFragment::StaticStruct(), MakeSpec(WoodDef, 5));

		FArcPlaceBuildingCommand Cmd(
			FArcNamedPrimaryAssetId(RegisteredId),
			FTransform::Identity,
			Instigator);

		const bool bResult = Cmd.Execute(&Spawner.GetWorld());
		ASSERT_THAT(IsFalse(bResult, TEXT("A failing BuildRequirement must fail Execute")));

		const FArcMassItemStoreFragment* Store = EM->GetSparseElementDataForEntity(
			FArcMassItemStoreFragment::StaticStruct(), Instigator).GetPtr<FArcMassItemStoreFragment>();
		ASSERT_THAT(IsNotNull(Store));
		ASSERT_THAT(AreEqual(1, Store->ReplicatedItems.Items.Num(),
			TEXT("Store must be untouched — requirement gate fails before resource consumption runs")));
	}

	// =========================================================================
	// FailedConsumption_ReturnsFalse — requirements pass, but the instigator's
	// store cannot fund the building's ingredients.
	// =========================================================================
	TEST_METHOD(FailedConsumption_ReturnsFalse)
	{
		using namespace ArcBuilderTest::TestHelpers;
		using namespace ArcPlaceBuildingCommandTestHelpers;

		UArcBuildingDefinition* BuildingDef = MakeTransientBuildingDef(TEXT("PlaceCmd4_Building"));

		FArcBuildIngredient_Tags Ingredient;
		Ingredient.Amount = 3;
		Ingredient.RequiredTags.AddTag(TAG_PlaceCmd_Wood);
		BuildingDef->Ingredients.Add(FInstancedStruct::Make(Ingredient));
		BuildingDef->ResourceConsumption = TInstancedStruct<FArcBuildResourceConsumption>::Make<FArcBuildResourceConsumption_MassStore>();

		const FPrimaryAssetId RegisteredId = RegisterBuildingDefDynamic(BuildingDef, TestBuildingType());
		ASSERT_THAT(IsTrue(RegisteredId.IsValid(), TEXT("Dynamic asset registration must succeed")));
		DynamicIds.Add(RegisteredId);

		// Instigator has an empty store — nothing funds the 3-stack wood ingredient.
		FMassEntityHandle Instigator = CreateStoreEntity(*EM);

		FArcPlaceBuildingCommand Cmd(
			FArcNamedPrimaryAssetId(RegisteredId),
			FTransform::Identity,
			Instigator);

		const bool bResult = Cmd.Execute(&Spawner.GetWorld());
		ASSERT_THAT(IsFalse(bResult, TEXT("Unfunded ingredients must fail Execute at the consumption gate")));

		const FArcMassItemStoreFragment* Store = EM->GetSparseElementDataForEntity(
			FArcMassItemStoreFragment::StaticStruct(), Instigator).GetPtr<FArcMassItemStoreFragment>();
		ASSERT_THAT(IsNotNull(Store));
		ASSERT_THAT(AreEqual(0, Store->ReplicatedItems.Items.Num(), TEXT("Store must remain empty — nothing to consume")));
	}
};
