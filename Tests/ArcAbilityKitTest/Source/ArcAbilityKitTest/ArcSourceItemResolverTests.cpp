// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "ArcAbilityKitTestTypes.h"
#include "Abilities/ArcAbilityDefinition.h"
#include "Abilities/ArcAbilityFunctions.h"
#include "Abilities/ArcAbilityStateTreeSubsystem.h"
#include "Components/ActorTestSpawner.h"
#include "Effects/ArcEffectContext.h"
#include "Fragments/ArcAbilityCollectionFragment.h"
#include "Fragments/ArcAbilityCustomData.h"
#include "Fragments/ArcAbilityInputFragment.h"
#include "Fragments/ArcOwnedTagsFragment.h"
#include "Items/ArcItemData.h"
#include "Mass/EntityBuilder.h"
#include "Mass/EntityManager.h"
#include "MassEntitySubsystem.h"
#include "NativeGameplayTags.h"
#include "Sources/ArcAbilitySource_Item.h"
#include "Sources/ArcSourceItemResolver.h"
#include "StructUtils/InstancedStruct.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_SourceItemResolverTest_Input, "Test.AbilityKit.SourceItemResolver.Input");

namespace ArcSourceItemResolverTestsPrivate
{
	FMassEntityHandle CreateAbilityEntity(FMassEntityManager& EntityManager)
	{
		UE::Mass::FEntityBuilder Builder(EntityManager);
		return Builder
			.Add<FArcAbilityCollectionFragment>()
			.Add<FArcAbilityInputFragment>()
			.Add<FArcOwnedTagsFragment>()
			.Commit();
	}

	FArcAbilitySource_Item MakeItemSource(FArcItemData& Item)
	{
		FArcAbilitySource_Item Source;
		Source.ItemDataHandle = FArcItemDataHandle(&Item);
		return Source;
	}

	FArcAbilityCustomData MakeItemCustomData(FArcItemData& Item)
	{
		FArcAbilityCustomData CustomData;
		CustomData.AddOrReplace<FArcAbilitySource_Item>(MakeItemSource(Item));
		return CustomData;
	}

	FArcAbilityCustomData MakeOtherCustomData()
	{
		FArcAbilityKitTest_OtherSource Other;
		Other.Marker = 7;
		FArcAbilityCustomData CustomData;
		CustomData.AddOrReplace<FArcAbilityKitTest_OtherSource>(MoveTemp(Other));
		return CustomData;
	}
}

TEST_CLASS(ArcSourceItemResolver_Ability, "ArcAbilityKit.Sources.FindSourceItem.Ability")
{
	FActorTestSpawner Spawner;
	FMassEntityManager* EntityManager = nullptr;
	UArcAbilityDefinition* Definition = nullptr;

	BEFORE_EACH()
	{
		FAutomationTestBase::bSuppressLogWarnings = true;
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogUObjectGlobals");
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogRHI");
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();

		UMassEntitySubsystem* EntitySubsystem = Spawner.GetWorld().GetSubsystem<UMassEntitySubsystem>();
		check(EntitySubsystem);
		EntityManager = &EntitySubsystem->GetMutableEntityManager();
		Definition = NewObject<UArcAbilityDefinition>();
	}

	TEST_METHOD(ItemSource_ReturnsThatItem)
	{
		using namespace ArcSourceItemResolverTestsPrivate;
		FArcItemData Item;
		const FMassEntityHandle Entity = CreateAbilityEntity(*EntityManager);
		const FArcAbilityHandle Handle = ArcAbilities::GrantAbility(*EntityManager, Entity, Definition, TAG_SourceItemResolverTest_Input,
			FMassEntityHandle(), MakeItemCustomData(Item));

		ASSERT_THAT(IsTrue(ArcAbilityKit::FindSourceItem(*EntityManager, Entity, Handle) == &Item));
	}

	TEST_METHOD(ItemSourceAfterOtherSource_ReturnsItem)
	{
		using namespace ArcSourceItemResolverTestsPrivate;
		FArcItemData Item;
		FArcAbilityCustomData CustomData = MakeOtherCustomData();
		CustomData.AddOrReplace<FArcAbilitySource_Item>(MakeItemSource(Item));

		const FMassEntityHandle Entity = CreateAbilityEntity(*EntityManager);
		const FArcAbilityHandle Handle = ArcAbilities::GrantAbility(*EntityManager, Entity, Definition, TAG_SourceItemResolverTest_Input,
			FMassEntityHandle(), MoveTemp(CustomData));

		ASSERT_THAT(IsTrue(ArcAbilityKit::FindSourceItem(*EntityManager, Entity, Handle) == &Item));
	}

	TEST_METHOD(TwoAbilities_EachResolvesItsOwnItem)
	{
		using namespace ArcSourceItemResolverTestsPrivate;
		FArcItemData ItemA;
		FArcItemData ItemB;
		const FMassEntityHandle Entity = CreateAbilityEntity(*EntityManager);
		const FArcAbilityHandle HandleA = ArcAbilities::GrantAbility(*EntityManager, Entity, Definition, TAG_SourceItemResolverTest_Input,
			FMassEntityHandle(), MakeItemCustomData(ItemA));
		const FArcAbilityHandle HandleB = ArcAbilities::GrantAbility(*EntityManager, Entity, Definition, TAG_SourceItemResolverTest_Input,
			FMassEntityHandle(), MakeItemCustomData(ItemB));

		ASSERT_THAT(IsTrue(ArcAbilityKit::FindSourceItem(*EntityManager, Entity, HandleA) == &ItemA));
		ASSERT_THAT(IsTrue(ArcAbilityKit::FindSourceItem(*EntityManager, Entity, HandleB) == &ItemB));
	}

	TEST_METHOD(NoCustomData_ReturnsNull)
	{
		using namespace ArcSourceItemResolverTestsPrivate;
		const FMassEntityHandle Entity = CreateAbilityEntity(*EntityManager);
		const FArcAbilityHandle Handle = ArcAbilities::GrantAbility(*EntityManager, Entity, Definition, TAG_SourceItemResolverTest_Input);

		ASSERT_THAT(IsTrue(Handle.IsValid()));
		ASSERT_THAT(IsNull(ArcAbilityKit::FindSourceItem(*EntityManager, Entity, Handle)));
	}

	TEST_METHOD(OtherSourceOnly_ReturnsNull)
	{
		using namespace ArcSourceItemResolverTestsPrivate;
		const FMassEntityHandle Entity = CreateAbilityEntity(*EntityManager);
		const FArcAbilityHandle Handle = ArcAbilities::GrantAbility(*EntityManager, Entity, Definition, TAG_SourceItemResolverTest_Input,
			FMassEntityHandle(), MakeOtherCustomData());

		ASSERT_THAT(IsTrue(Handle.IsValid()));
		ASSERT_THAT(IsNull(ArcAbilityKit::FindSourceItem(*EntityManager, Entity, Handle)));
	}

	TEST_METHOD(ItemSourceWithEmptyHandle_ReturnsNull)
	{
		using namespace ArcSourceItemResolverTestsPrivate;
		FArcAbilityCustomData CustomData;
		CustomData.AddOrReplace<FArcAbilitySource_Item>(FArcAbilitySource_Item());

		const FMassEntityHandle Entity = CreateAbilityEntity(*EntityManager);
		const FArcAbilityHandle Handle = ArcAbilities::GrantAbility(*EntityManager, Entity, Definition, TAG_SourceItemResolverTest_Input,
			FMassEntityHandle(), MoveTemp(CustomData));

		ASSERT_THAT(IsNull(ArcAbilityKit::FindSourceItem(*EntityManager, Entity, Handle)));
	}

	TEST_METHOD(InvalidHandle_ReturnsNull)
	{
		using namespace ArcSourceItemResolverTestsPrivate;
		FArcItemData Item;
		const FMassEntityHandle Entity = CreateAbilityEntity(*EntityManager);
		ArcAbilities::GrantAbility(*EntityManager, Entity, Definition, TAG_SourceItemResolverTest_Input, FMassEntityHandle(), MakeItemCustomData(Item));

		ASSERT_THAT(IsNull(ArcAbilityKit::FindSourceItem(*EntityManager, Entity, FArcAbilityHandle())));
	}

	TEST_METHOD(RemovedAbility_ReturnsNull)
	{
		using namespace ArcSourceItemResolverTestsPrivate;
		UArcAbilityStateTreeSubsystem* AbilitySubsystem = Spawner.GetWorld().GetSubsystem<UArcAbilityStateTreeSubsystem>();
		ASSERT_THAT(IsNotNull(AbilitySubsystem));

		FArcItemData Item;
		const FMassEntityHandle Entity = CreateAbilityEntity(*EntityManager);
		const FArcAbilityHandle Handle = ArcAbilities::GrantAbility(*EntityManager, Entity, Definition, TAG_SourceItemResolverTest_Input,
			FMassEntityHandle(), MakeItemCustomData(Item));
		ASSERT_THAT(IsTrue(ArcAbilityKit::FindSourceItem(*EntityManager, Entity, Handle) == &Item));

		ArcAbilities::RemoveAbility(*EntityManager, Entity, Handle, *AbilitySubsystem);

		ASSERT_THAT(IsNull(ArcAbilityKit::FindSourceItem(*EntityManager, Entity, Handle)));
	}

	TEST_METHOD(HandleOnEntityWithoutThatAbility_ReturnsNull)
	{
		using namespace ArcSourceItemResolverTestsPrivate;
		FArcItemData Item;
		const FMassEntityHandle Owner = CreateAbilityEntity(*EntityManager);
		const FMassEntityHandle Other = CreateAbilityEntity(*EntityManager);
		const FArcAbilityHandle Handle = ArcAbilities::GrantAbility(*EntityManager, Owner, Definition, TAG_SourceItemResolverTest_Input,
			FMassEntityHandle(), MakeItemCustomData(Item));

		ASSERT_THAT(IsNull(ArcAbilityKit::FindSourceItem(*EntityManager, Other, Handle)));
	}
};

TEST_CLASS(ArcSourceItemResolver_EffectContext, "ArcAbilityKit.Sources.FindSourceItem.EffectContext")
{
	TEST_METHOD(ItemSourceData_ReturnsThatItem)
	{
		FArcItemData Item;
		FArcEffectContext Context;
		Context.SourceData = FInstancedStruct::Make(ArcSourceItemResolverTestsPrivate::MakeItemSource(Item));

		ASSERT_THAT(IsTrue(ArcAbilityKit::FindSourceItem(Context) == &Item));
	}

	TEST_METHOD(ItemSourceDataWithEmptyHandle_ReturnsNull)
	{
		FArcEffectContext Context;
		Context.SourceData = FInstancedStruct::Make(FArcAbilitySource_Item());

		ASSERT_THAT(IsNull(ArcAbilityKit::FindSourceItem(Context)));
	}

	TEST_METHOD(OtherPayload_ReturnsNull)
	{
		FArcAbilityKitTest_OtherSource Other;
		Other.Marker = 3;
		FArcEffectContext Context;
		Context.SourceData = FInstancedStruct::Make(Other);

		ASSERT_THAT(IsNull(ArcAbilityKit::FindSourceItem(Context)));
	}

	TEST_METHOD(EmptySourceData_ReturnsNull)
	{
		const FArcEffectContext Context;

		ASSERT_THAT(IsNull(ArcAbilityKit::FindSourceItem(Context)));
	}
};
