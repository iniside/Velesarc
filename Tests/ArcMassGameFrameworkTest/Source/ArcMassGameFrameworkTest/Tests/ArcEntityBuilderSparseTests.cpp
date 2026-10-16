// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "Components/ActorTestSpawner.h"

#include "Mass/EntityBuilder.h"
#include "Mass/EntityManager.h"
#include "MassEntitySubsystem.h"
#include "Mass/EntityView.h"

#include "ArcEntityBuilderTestTypes.h"

TEST_CLASS(ArcEntityBuilder_SparseAtCreation, "ArcMassGameFramework.EntityBuilder.SparseAtCreation")
{
	FActorTestSpawner Spawner;
	FMassEntityManager* EntityManager = nullptr;

	BEFORE_EACH()
	{
		FAutomationTestBase::bSuppressLogWarnings = true;
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
		UMassEntitySubsystem* MES = Spawner.GetWorld().GetSubsystem<UMassEntitySubsystem>();
		ASSERT_THAT(IsNotNull(MES));
		EntityManager = &MES->GetMutableEntityManager();
	}

	TEST_METHOD(SparseFragment_WithInitialValue_ReadableImmediatelyAfterCommit)
	{
		FArcEntityBuilderTestSparseFragment SparseInit;
		SparseInit.Health = 111;
		SparseInit.Speed = 7.5f;

		UE::Mass::FEntityBuilder Builder(*EntityManager);
		const FMassEntityHandle Entity = Builder
			.Add<FArcEntityBuilderTestRegularFragment>()
			.Add<FArcEntityBuilderTestSparseFragment>(SparseInit)
			.Commit();

		ASSERT_THAT(IsTrue(EntityManager->IsEntityValid(Entity)));

		const FMassEntityView View(*EntityManager, Entity);
		const FArcEntityBuilderTestSparseFragment* Sparse = View.GetSparseFragmentDataPtr<FArcEntityBuilderTestSparseFragment>();
		ASSERT_THAT(IsNotNull(Sparse));
		ASSERT_THAT(AreEqual(111, Sparse->Health));
		ASSERT_THAT(IsTrue(Sparse->Speed == 7.5f));
	}

	TEST_METHOD(MixedRegularSparseFragmentsAndTags_AllPresentAfterCommit)
	{
		FArcEntityBuilderTestSparseFragment SparseInit;
		SparseInit.Health = 42;
		SparseInit.Speed = 1.0f;

		UE::Mass::FEntityBuilder Builder(*EntityManager);
		const FMassEntityHandle Entity = Builder
			.Add<FArcEntityBuilderTestRegularFragment>()
			.Add<FArcEntityBuilderTestSparseFragment>(SparseInit)
			.Add<FArcEntityBuilderTestRegularTag>()
			.Add<FArcEntityBuilderTestSparseTag>()
			.Commit();

		const FMassEntityView View(*EntityManager, Entity);

		const FArcEntityBuilderTestRegularFragment* Regular = View.GetFragmentDataPtr<FArcEntityBuilderTestRegularFragment>();
		ASSERT_THAT(IsNotNull(Regular));

		const FArcEntityBuilderTestSparseFragment* Sparse = View.GetSparseFragmentDataPtr<FArcEntityBuilderTestSparseFragment>();
		ASSERT_THAT(IsNotNull(Sparse));
		ASSERT_THAT(AreEqual(42, Sparse->Health));

		ASSERT_THAT(IsTrue(EntityManager->DoesEntityHaveElement<FArcEntityBuilderTestRegularTag>(Entity)));
		ASSERT_THAT(IsTrue(EntityManager->DoesEntityHaveElement<FArcEntityBuilderTestSparseTag>(Entity)));
	}

	TEST_METHOD(CommitAndReprepare_DistinctSparseValuesAcrossEntities)
	{
		FArcEntityBuilderTestSparseFragment SparseInit1;
		SparseInit1.Health = 100;
		SparseInit1.Speed = 1.0f;

		FArcEntityBuilderTestSparseFragment SparseInit2;
		SparseInit2.Health = 200;
		SparseInit2.Speed = 2.0f;

		UE::Mass::FEntityBuilder Builder(*EntityManager);
		Builder.Add<FArcEntityBuilderTestRegularFragment>()
			.Add<FArcEntityBuilderTestSparseFragment>(SparseInit1);

		const FMassEntityHandle E1 = Builder.CommitAndReprepare();

		Builder.GetOrCreate<FArcEntityBuilderTestSparseFragment>(SparseInit2);
		const FMassEntityHandle E2 = Builder.Commit();

		const FMassEntityView V1(*EntityManager, E1);
		const FMassEntityView V2(*EntityManager, E2);

		const FArcEntityBuilderTestSparseFragment* S1 = V1.GetSparseFragmentDataPtr<FArcEntityBuilderTestSparseFragment>();
		const FArcEntityBuilderTestSparseFragment* S2 = V2.GetSparseFragmentDataPtr<FArcEntityBuilderTestSparseFragment>();
		ASSERT_THAT(IsNotNull(S1));
		ASSERT_THAT(IsNotNull(S2));
		ASSERT_THAT(AreEqual(100, S1->Health));
		ASSERT_THAT(AreEqual(200, S2->Health));
	}

	TEST_METHOD(SparseFragmentOnly_NoOtherRequirements_RoundTrips)
	{
		FArcEntityBuilderTestSparseFragment SparseInit;
		SparseInit.Health = 42;
		SparseInit.Speed = 0.0f;

		UE::Mass::FEntityBuilder Builder(*EntityManager);
		const FMassEntityHandle Entity = Builder
			.Add<FArcEntityBuilderTestSparseFragment>(SparseInit)
			.Commit();

		ASSERT_THAT(IsTrue(EntityManager->IsEntityValid(Entity)));

		const FMassEntityView View(*EntityManager, Entity);
		const FArcEntityBuilderTestSparseFragment* Sparse = View.GetSparseFragmentDataPtr<FArcEntityBuilderTestSparseFragment>();
		ASSERT_THAT(IsNotNull(Sparse));
		ASSERT_THAT(AreEqual(42, Sparse->Health));
	}
};
