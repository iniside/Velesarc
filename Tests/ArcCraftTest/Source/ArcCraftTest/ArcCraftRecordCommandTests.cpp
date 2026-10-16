// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "ArcItemTestRecordFiles.h"
#include "Components/ActorTestSpawner.h"

#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"
#include "StructUtils/InstancedStruct.h"

#include "Mass/EntityBuilder.h"
#include "Mass/EntityFragments.h"
#include "Mass/EntityHandle.h"
#include "Mass/EntityManager.h"
#include "Mass/EntityView.h"
#include "Mass/Executor.h"
#include "Mass/ProcessingContext.h"
#include "MassEntitySubsystem.h"

#include "Items/ArcItemDefinitionData.h"
#include "Items/ArcItemSpec.h"
#include "Items/Fragments/ArcItemFragment_Tags.h"

#include "ArcCraft/Commands/ArcCraftAuth.h"
#include "ArcCraft/Commands/ArcCraftFromStationInputCommand.h"
#include "ArcCraft/Mass/ArcCraftDeliveryConfigFragment.h"
#include "ArcCraft/Mass/ArcCraftMassFragments.h"
#include "ArcCraft/Mass/ArcCraftTickProcessor.h"
#include "ArcCraft/Mass/ArcMassAvailableRecipesSharedFragment.h"
#include "ArcCraft/Recipe/ArcRecipeDefinitionData.h"
#include "ArcCraft/Recipe/ArcRecipeIngredient.h"
#include "ArcCraft/Shared/ArcCraftRecords.h"
#include "ArcCraft/Station/ArcCraftItemSource.h"
#include "ArcCraft/Station/ArcCraftStationTypes.h"

#include "Fragments/ArcMassItemStoreFragment.h"
#include "Items/ArcItemData.h"
#include "Operations/ArcMassItemOperations.h"

#include "ArcCraftTestRecordFiles.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_RecCmd_Ore, "Test.Material.RecCmdOre");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_RecCmd_Tier, "Test.RecCmd.Tier");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_RecCmd_Station, "Test.RecCmd.Station");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_RecCmd_Buckle, "Test.RecCmd.Buckle");

namespace ArcCraftRecordCommandTestsImpl
{
	FPrimaryAssetId MakeOreItemDef(FArcItemTestRecordFiles& ItemRecords)
	{
		FArcItemDefinitionData Def;

		FArcItemFragment_Tags TagsFragment;
		TagsFragment.AssetTags.AddTag(TAG_RecCmd_Ore);
		ArcItemTestRecords::AddFragment(Def, TagsFragment);
		return ItemRecords.Write(Def);
	}

	FArcItemSpec MakeStationSpec(const FPrimaryAssetId& Def, int32 Amount)
	{
		FArcItemSpec Spec;
		Spec.SetItemDefinition(Def);
		Spec.SetItemId(FArcItemId::Generate());
		Spec.Amount = static_cast<uint16>(Amount);
		return Spec;
	}

	FArcRecipeDefinitionData MakeOreRecipe(int32 PerCraftAmount, bool bTierTagged)
	{
		FArcRecipeDefinitionData Recipe;
		Recipe.CraftTime = 1.0f;
		if (bTierTagged)
		{
			Recipe.RecipeTags.AddTag(TAG_RecCmd_Tier);
		}

		FArcRecipeIngredient_Tags Ingredient;
		Ingredient.Amount = PerCraftAmount;
		Ingredient.bConsumeOnCraft = true;
		Ingredient.RequiredTags.AddTag(TAG_RecCmd_Ore);
		Recipe.Ingredients.Add(FInstancedStruct::Make(Ingredient));
		return Recipe;
	}

	FPrimaryAssetId MakeUnindexedRecipeId()
	{
		return FPrimaryAssetId(FArcRecipeDefinitionData::JsonPrimaryAssetType(), FName(*FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	}

	FMassEntityHandle BuildStation(
		FMassEntityManager& EM,
		const FGameplayTagContainer& OfferedRecipeTags,
		const TArray<FPrimaryAssetId>& PinnedRecipes,
		bool bAutoTick)
	{
		TArray<FInstancedStruct> Fragments;
		Fragments.Add(FInstancedStruct::Make(FTransformFragment()));
		Fragments.Add(FInstancedStruct::Make(FArcCraftInputFragment()));
		Fragments.Add(FInstancedStruct::Make(FArcCraftQueueFragment()));
		Fragments.Add(FInstancedStruct::Make(FArcCraftOutputFragment()));

		FMassEntityHandle Entity = EM.CreateEntity(Fragments);
		EM.AddTagToEntity(Entity, FArcCraftStationTag::StaticStruct());
		if (bAutoTick)
		{
			EM.AddTagToEntity(Entity, FArcCraftAutoTickTag::StaticStruct());
		}

		FArcCraftStationConfigFragment Config;
		Config.OfferedRecipeTags = OfferedRecipeTags;
		Config.ConcurrentCraftSlots = 2;
		EM.AddConstSharedFragmentToEntity(Entity, EM.GetOrCreateConstSharedFragment(Config));

		FArcMassAvailableRecipesSharedFragment Available;
		Available.Recipes = PinnedRecipes;
		EM.AddConstSharedFragmentToEntity(Entity, EM.GetOrCreateConstSharedFragment(Available));
		return Entity;
	}

	FMassEntityHandle BuildInstigator(FMassEntityManager& EM)
	{
		UE::Mass::FEntityBuilder Builder(EM);
		return Builder
			.Add<FTransformFragment>()
			.Add<FArcMassItemStoreFragment>()
			.Commit();
	}

	FMassEntityHandle BuildDepositStation(FMassEntityManager& EM, const FPrimaryAssetId& PinnedRecipe)
	{
		const FMassEntityHandle Entity = BuildStation(EM, FGameplayTagContainer(), { PinnedRecipe }, false);
		FArcCraftDeliveryConfigFragment Delivery;
		Delivery.IngredientSource.InitializeAs<FArcCraftItemSource_EntityStore>();
		EM.AddConstSharedFragmentToEntity(Entity, EM.GetOrCreateConstSharedFragment(Delivery));
		return Entity;
	}

	FArcItemId AddOre(FMassEntityManager& EM, FMassEntityHandle Owner, const FPrimaryAssetId& Def, int32 Stacks)
	{
		return ArcMassItems::AddItem(EM, Owner, FArcMassItemStoreFragment::StaticStruct(), MakeStationSpec(Def, Stacks));
	}

	int32 HeldStacks(FMassEntityManager& EM, FMassEntityHandle Owner, const FArcItemId& ItemId)
	{
		const FArcItemData* Data = ArcMassItems::FindItemById(EM, Owner, FArcMassItemStoreFragment::StaticStruct(), ItemId);
		return Data != nullptr ? static_cast<int32>(Data->GetStacks()) : 0;
	}

	TArray<FArcCraftInputSourceGroup> MakeBaseStoreSources(FMassEntityHandle Owner, const FArcItemId& ItemId, int32 Stacks)
	{
		FArcCraftInputSourceGroup Group;
		Group.OwnerEntity = Owner;
		Group.StoreType = FArcMassItemStoreFragment::StaticStruct();
		FArcCraftInputItem& Row = Group.Items.AddDefaulted_GetRef();
		Row.ItemId = ItemId;
		Row.Stacks = Stacks;
		return { Group };
	}
}

TEST_CLASS(ArcCraftRecordCommands, "ArcCraft.RecordCommands")
{
	FArcItemTestRecordFiles ItemRecords;
	FArcCraftTestRecordFiles Records;
	FActorTestSpawner Spawner;
	FMassEntityManager* EM = nullptr;
	UArcCraftTickProcessor* Processor = nullptr;

	BEFORE_EACH()
	{
		FAutomationTestBase::bSuppressLogWarnings = true;
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogUObjectGlobals");

		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();

		UMassEntitySubsystem* MES = Spawner.GetWorld().GetSubsystem<UMassEntitySubsystem>();
		check(MES);
		EM = &MES->GetMutableEntityManager();

		TSharedRef<FMassEntityManager> SharedEM = EM->AsShared();
		Processor = NewObject<UArcCraftTickProcessor>();
		Processor->CallInitialize(EM->GetOwner(), SharedEM);
	}

	void RunProcessor(float DeltaSeconds)
	{
		UE::Mass::FProcessingContext ProcessingContext(*EM, DeltaSeconds, /*bFlushCommandBuffer=*/true);
		UE::Mass::Executor::Run(*Processor, ProcessingContext);
	}

	TEST_METHOD(FromStationInput_Execute_PinnedIndexedRecipe_QueuesEntryWithRecipeId)
	{
		using namespace ArcCraftRecordCommandTestsImpl;

		const FPrimaryAssetId Def = MakeOreItemDef(ItemRecords);
		ASSERT_THAT(IsTrue(Def.IsValid(), TEXT("Item record must be written and indexed")));
		const FPrimaryAssetId RecipeId = Records.Write(MakeOreRecipe(1, false));
		ASSERT_THAT(IsTrue(RecipeId.IsValid(), TEXT("Recipe record must be written and indexed")));

		const FMassEntityHandle Station = BuildStation(*EM, FGameplayTagContainer(), { RecipeId }, false);
		const FMassEntityHandle Instigator = BuildInstigator(*EM);
		EM->GetFragmentDataPtr<FArcCraftInputFragment>(Station)->InputItems.Add(MakeStationSpec(Def, 3));

		FArcCraftFromStationInputCommand Command(FArcNamedPrimaryAssetId(RecipeId), Station, Instigator, 2);
		ASSERT_THAT(IsTrue(Command.Execute(&Spawner.GetWorld()), TEXT("Execute must accept an indexed, pinned recipe id")));
		RunProcessor(0.0f);

		const FArcCraftQueueFragment* Queue = EM->GetFragmentDataPtr<FArcCraftQueueFragment>(Station);
		ASSERT_THAT(IsNotNull(Queue));
		ASSERT_THAT(AreEqual(1, Queue->Entries.Num(), TEXT("One queue entry must be appended")));
		ASSERT_THAT(IsTrue(Queue->Entries[0].RecipeId == RecipeId, TEXT("Queue entry must carry the command's record id")));
		ASSERT_THAT(AreEqual(2, Queue->Entries[0].Amount, TEXT("Queue entry amount must be the requested amount the input funds")));
	}

	TEST_METHOD(FromStationInput_Execute_UnresolvedRecipeId_FailsWithoutStateChange)
	{
		using namespace ArcCraftRecordCommandTestsImpl;

		const FPrimaryAssetId Def = MakeOreItemDef(ItemRecords);
		ASSERT_THAT(IsTrue(Def.IsValid(), TEXT("Item record must be written and indexed")));
		const FPrimaryAssetId MissingId = MakeUnindexedRecipeId();

		const FMassEntityHandle Station = BuildStation(*EM, FGameplayTagContainer(), { MissingId }, false);
		const FMassEntityHandle Instigator = BuildInstigator(*EM);
		EM->GetFragmentDataPtr<FArcCraftInputFragment>(Station)->InputItems.Add(MakeStationSpec(Def, 3));

		FArcCraftFromStationInputCommand Command(FArcNamedPrimaryAssetId(MissingId), Station, Instigator, 2);
		ASSERT_THAT(IsFalse(Command.Execute(&Spawner.GetWorld()), TEXT("Execute must fail for a record id that is not indexed")));
		RunProcessor(0.0f);

		const FArcCraftQueueFragment* Queue = EM->GetFragmentDataPtr<FArcCraftQueueFragment>(Station);
		const FArcCraftInputFragment* Input = EM->GetFragmentDataPtr<FArcCraftInputFragment>(Station);
		ASSERT_THAT(IsNotNull(Queue));
		ASSERT_THAT(IsNotNull(Input));
		ASSERT_THAT(AreEqual(0, Queue->Entries.Num(), TEXT("No queue entry may be appended")));
		ASSERT_THAT(AreEqual(1, Input->InputItems.Num(), TEXT("Station input must be untouched")));
		ASSERT_THAT(AreEqual(static_cast<uint16>(3), Input->InputItems[0].Amount, TEXT("Station input amount must be untouched")));
	}

	TEST_METHOD(FromStationInput_Execute_TierOfferedNotPinned_QueuesEntry)
	{
		using namespace ArcCraftRecordCommandTestsImpl;

		const FPrimaryAssetId Def = MakeOreItemDef(ItemRecords);
		ASSERT_THAT(IsTrue(Def.IsValid(), TEXT("Item record must be written and indexed")));
		const FPrimaryAssetId RecipeId = Records.Write(MakeOreRecipe(1, true));
		ASSERT_THAT(IsTrue(RecipeId.IsValid(), TEXT("Recipe record must be written and indexed")));

		FGameplayTagContainer Offered;
		Offered.AddTag(TAG_RecCmd_Tier);
		const FMassEntityHandle Station = BuildStation(*EM, Offered, {}, false);
		const FMassEntityHandle Instigator = BuildInstigator(*EM);
		EM->GetFragmentDataPtr<FArcCraftInputFragment>(Station)->InputItems.Add(MakeStationSpec(Def, 1));

		FArcCraftFromStationInputCommand Command(FArcNamedPrimaryAssetId(RecipeId), Station, Instigator, 1);
		ASSERT_THAT(IsTrue(Command.Execute(&Spawner.GetWorld()), TEXT("Execute must accept a recipe whose tag overlaps the offered tier")));
		RunProcessor(0.0f);

		const FArcCraftQueueFragment* Queue = EM->GetFragmentDataPtr<FArcCraftQueueFragment>(Station);
		ASSERT_THAT(IsNotNull(Queue));
		ASSERT_THAT(AreEqual(1, Queue->Entries.Num(), TEXT("One queue entry must be appended")));
	}

	TEST_METHOD(FromStationInput_Execute_NotPinnedTierNotOffered_Refused)
	{
		using namespace ArcCraftRecordCommandTestsImpl;

		const FPrimaryAssetId Def = MakeOreItemDef(ItemRecords);
		ASSERT_THAT(IsTrue(Def.IsValid(), TEXT("Item record must be written and indexed")));
		const FPrimaryAssetId RecipeId = Records.Write(MakeOreRecipe(1, true));
		ASSERT_THAT(IsTrue(RecipeId.IsValid(), TEXT("Recipe record must be written and indexed")));
		ASSERT_THAT(IsNotNull(ArcCraftRecords::Find<FArcRecipeDefinitionData>(RecipeId), TEXT("Recipe id must resolve so the refusal is the pin/tier gate")));

		const FMassEntityHandle Station = BuildStation(*EM, FGameplayTagContainer(), {}, false);
		const FMassEntityHandle Instigator = BuildInstigator(*EM);
		EM->GetFragmentDataPtr<FArcCraftInputFragment>(Station)->InputItems.Add(MakeStationSpec(Def, 1));

		FArcCraftFromStationInputCommand Command(FArcNamedPrimaryAssetId(RecipeId), Station, Instigator, 1);
		ASSERT_THAT(IsFalse(Command.Execute(&Spawner.GetWorld()), TEXT("Execute must refuse an unpinned recipe the station does not offer")));
		RunProcessor(0.0f);

		const FArcCraftQueueFragment* Queue = EM->GetFragmentDataPtr<FArcCraftQueueFragment>(Station);
		ASSERT_THAT(IsNotNull(Queue));
		ASSERT_THAT(AreEqual(0, Queue->Entries.Num(), TEXT("No queue entry may be appended")));
	}

	TEST_METHOD(FromStationInput_Execute_Sources_MovedThenQueued)
	{
		using namespace ArcCraftRecordCommandTestsImpl;

		const FPrimaryAssetId Def = MakeOreItemDef(ItemRecords);
		ASSERT_THAT(IsTrue(Def.IsValid(), TEXT("Item record must be written and indexed")));
		const FPrimaryAssetId RecipeId = Records.Write(MakeOreRecipe(1, false));
		ASSERT_THAT(IsTrue(RecipeId.IsValid(), TEXT("Recipe record must be written and indexed")));

		const FMassEntityHandle Station = BuildDepositStation(*EM, RecipeId);
		const FMassEntityHandle Instigator = BuildInstigator(*EM);
		const FArcItemId Ore = AddOre(*EM, Instigator, Def, 3);
		ASSERT_THAT(IsTrue(Ore.IsValid()));

		FArcCraftFromStationInputCommand Command(FArcNamedPrimaryAssetId(RecipeId), Station, Instigator, 2, MakeBaseStoreSources(Instigator, Ore, 2));
		ASSERT_THAT(IsTrue(Command.Execute(&Spawner.GetWorld()), TEXT("A valid grouped command is accepted")));

		const FArcCraftInputFragment* Input = EM->GetFragmentDataPtr<FArcCraftInputFragment>(Station);
		ASSERT_THAT(AreEqual(0, Input->InputItems.Num(), TEXT("Execute only validates; the move happens in the deferred command")));
		ASSERT_THAT(AreEqual(3, HeldStacks(*EM, Instigator, Ore)));

		RunProcessor(0.0f);

		ASSERT_THAT(AreEqual(1, Input->InputItems.Num(), TEXT("The sent ore reaches the station input")));
		ASSERT_THAT(IsTrue(Input->InputItems[0].ItemId == Ore));
		ASSERT_THAT(AreEqual(static_cast<uint16>(2), Input->InputItems[0].Amount));
		ASSERT_THAT(AreEqual(1, HeldStacks(*EM, Instigator, Ore), TEXT("2 of 3 stacks left the player store")));

		const FArcCraftQueueFragment* Queue = EM->GetFragmentDataPtr<FArcCraftQueueFragment>(Station);
		ASSERT_THAT(AreEqual(1, Queue->Entries.Num(), TEXT("The craft is queued against the moved input")));
		ASSERT_THAT(AreEqual(2, Queue->Entries[0].Amount, TEXT("Two crafts are funded by the two moved ore")));
	}

	TEST_METHOD(FromStationInput_Execute_SourcesAskMoreThanHeld_FailsNothingMoved)
	{
		using namespace ArcCraftRecordCommandTestsImpl;

		const FPrimaryAssetId Def = MakeOreItemDef(ItemRecords);
		ASSERT_THAT(IsTrue(Def.IsValid(), TEXT("Item record must be written and indexed")));
		const FPrimaryAssetId RecipeId = Records.Write(MakeOreRecipe(1, false));
		ASSERT_THAT(IsTrue(RecipeId.IsValid(), TEXT("Recipe record must be written and indexed")));

		const FMassEntityHandle Station = BuildDepositStation(*EM, RecipeId);
		const FMassEntityHandle Instigator = BuildInstigator(*EM);
		const FArcItemId Ore = AddOre(*EM, Instigator, Def, 1);

		FArcCraftFromStationInputCommand Command(FArcNamedPrimaryAssetId(RecipeId), Station, Instigator, 1, MakeBaseStoreSources(Instigator, Ore, 2));
		ASSERT_THAT(IsFalse(Command.Execute(&Spawner.GetWorld()), TEXT("Execute reports a bad source group to the confirm")));
		RunProcessor(0.0f);

		ASSERT_THAT(AreEqual(0, EM->GetFragmentDataPtr<FArcCraftInputFragment>(Station)->InputItems.Num(), TEXT("Nothing is moved")));
		ASSERT_THAT(AreEqual(1, HeldStacks(*EM, Instigator, Ore), TEXT("The player keeps the item")));
		ASSERT_THAT(AreEqual(0, EM->GetFragmentDataPtr<FArcCraftQueueFragment>(Station)->Entries.Num(), TEXT("Nothing is queued")));
	}

	TEST_METHOD(FromStationInput_Execute_ItemGoneBeforeDeferredMove_NothingMovedNothingQueued)
	{
		using namespace ArcCraftRecordCommandTestsImpl;

		const FPrimaryAssetId Def = MakeOreItemDef(ItemRecords);
		ASSERT_THAT(IsTrue(Def.IsValid(), TEXT("Item record must be written and indexed")));
		const FPrimaryAssetId RecipeId = Records.Write(MakeOreRecipe(1, false));
		ASSERT_THAT(IsTrue(RecipeId.IsValid(), TEXT("Recipe record must be written and indexed")));

		const FMassEntityHandle Station = BuildDepositStation(*EM, RecipeId);
		const FMassEntityHandle Instigator = BuildInstigator(*EM);
		const FArcItemId Ore = AddOre(*EM, Instigator, Def, 2);
		const FArcItemId OtherOre = AddOre(*EM, Instigator, Def, 3);

		TArray<FArcCraftInputSourceGroup> Sources = MakeBaseStoreSources(Instigator, Ore, 2);
		FArcCraftInputItem& OtherRow = Sources[0].Items.AddDefaulted_GetRef();
		OtherRow.ItemId = OtherOre;
		OtherRow.Stacks = 1;

		FArcCraftFromStationInputCommand Command(FArcNamedPrimaryAssetId(RecipeId), Station, Instigator, 1, Sources);
		ASSERT_THAT(IsTrue(Command.Execute(&Spawner.GetWorld()), TEXT("The sources are valid when Execute runs")));

		ASSERT_THAT(IsTrue(ArcMassItems::RemoveItem(*EM, Instigator, FArcMassItemStoreFragment::StaticStruct(), Ore, 1),
			TEXT("The player drops one ore before the deferred command runs")));
		RunProcessor(0.0f);

		ASSERT_THAT(AreEqual(0, EM->GetFragmentDataPtr<FArcCraftInputFragment>(Station)->InputItems.Num(),
			TEXT("The deferred re-validation rejects the whole command: not even the still-valid row moves")));
		ASSERT_THAT(AreEqual(1, HeldStacks(*EM, Instigator, Ore)));
		ASSERT_THAT(AreEqual(3, HeldStacks(*EM, Instigator, OtherOre)));
		ASSERT_THAT(AreEqual(0, EM->GetFragmentDataPtr<FArcCraftQueueFragment>(Station)->Entries.Num(), TEXT("Nothing is queued")));
	}

	TEST_METHOD(FromStationInput_Execute_SourcesMovedButNotFundable_ItemsStayNothingQueued)
	{
		using namespace ArcCraftRecordCommandTestsImpl;

		const FPrimaryAssetId Def = MakeOreItemDef(ItemRecords);
		ASSERT_THAT(IsTrue(Def.IsValid(), TEXT("Item record must be written and indexed")));
		FArcRecipeDefinitionData Recipe = MakeOreRecipe(1, false);
		FArcRecipeIngredient_Tags Buckle;
		Buckle.Amount = 1;
		Buckle.RequiredTags.AddTag(TAG_RecCmd_Buckle);
		Recipe.Ingredients.Add(FInstancedStruct::Make(Buckle));
		const FPrimaryAssetId RecipeId = Records.Write(Recipe);
		ASSERT_THAT(IsTrue(RecipeId.IsValid(), TEXT("Recipe record must be written and indexed")));

		const FMassEntityHandle Station = BuildDepositStation(*EM, RecipeId);
		const FMassEntityHandle Instigator = BuildInstigator(*EM);
		const FArcItemId Ore = AddOre(*EM, Instigator, Def, 2);

		FArcCraftFromStationInputCommand Command(FArcNamedPrimaryAssetId(RecipeId), Station, Instigator, 1, MakeBaseStoreSources(Instigator, Ore, 2));
		ASSERT_THAT(IsTrue(Command.Execute(&Spawner.GetWorld())));
		RunProcessor(0.0f);

		const FArcCraftInputFragment* Input = EM->GetFragmentDataPtr<FArcCraftInputFragment>(Station);
		ASSERT_THAT(AreEqual(1, Input->InputItems.Num(), TEXT("The ore stays in the input for the player to withdraw")));
		ASSERT_THAT(AreEqual(static_cast<uint16>(2), Input->InputItems[0].Amount));
		ASSERT_THAT(AreEqual(0, HeldStacks(*EM, Instigator, Ore)));
		ASSERT_THAT(AreEqual(0, EM->GetFragmentDataPtr<FArcCraftQueueFragment>(Station)->Entries.Num(),
			TEXT("A recipe missing its required buckle queues nothing")));
	}

	TEST_METHOD(AuthPin_PinnedRecipeFailingTierGate_IsAuthorised)
	{
		using namespace ArcCraftRecordCommandTestsImpl;

		const FPrimaryAssetId RecipeId = MakeUnindexedRecipeId();
		const FArcRecipeDefinitionData Recipe = MakeOreRecipe(1, true);
		const FMassEntityHandle Station = BuildStation(*EM, FGameplayTagContainer(), { RecipeId }, false);

		FText Reason;
		ASSERT_THAT(IsTrue(ArcCraftAuth::CanUseRecipeAtStation(*EM, Station, RecipeId, Recipe, &Reason),
			TEXT("A recipe id listed in the station's pinned recipes must pass although the tier gate fails")));
	}

	TEST_METHOD(AuthPin_OtherRecipePinned_TierGateFails_IsRefused)
	{
		using namespace ArcCraftRecordCommandTestsImpl;

		const FPrimaryAssetId RecipeId = MakeUnindexedRecipeId();
		const FPrimaryAssetId OtherId = MakeUnindexedRecipeId();
		const FArcRecipeDefinitionData Recipe = MakeOreRecipe(1, true);
		const FMassEntityHandle Station = BuildStation(*EM, FGameplayTagContainer(), { OtherId }, false);

		FText Reason;
		ASSERT_THAT(IsFalse(ArcCraftAuth::CanUseRecipeAtStation(*EM, Station, RecipeId, Recipe, &Reason),
			TEXT("An unpinned recipe failing the tier gate must be refused even when another recipe is pinned")));
		ASSERT_THAT(IsFalse(Reason.IsEmpty(), TEXT("A refusal must carry a reason")));
	}

	TEST_METHOD(AuthPin_PinnedRecipeFailingTypeGate_IsRefused)
	{
		using namespace ArcCraftRecordCommandTestsImpl;

		const FPrimaryAssetId RecipeId = MakeUnindexedRecipeId();
		FArcRecipeDefinitionData Recipe = MakeOreRecipe(1, false);
		Recipe.RequiredStationTags.AddTag(TAG_RecCmd_Station);
		const FMassEntityHandle Station = BuildStation(*EM, FGameplayTagContainer(), { RecipeId }, false);

		FText Reason;
		ASSERT_THAT(IsFalse(ArcCraftAuth::CanUseRecipeAtStation(*EM, Station, RecipeId, Recipe, &Reason),
			TEXT("Pinning must not bypass the station-type gate")));
	}

	TEST_METHOD(Tick_PendingEntryWithUnresolvedRecipeId_IsSkippedAndResolvedSiblingPromoted)
	{
		using namespace ArcCraftRecordCommandTestsImpl;

		const FPrimaryAssetId ResolvedId = Records.Write(FArcRecipeDefinitionData());
		ASSERT_THAT(IsTrue(ResolvedId.IsValid(), TEXT("Recipe record must be written and indexed")));

		const FMassEntityHandle Station = BuildStation(*EM, FGameplayTagContainer(), {}, true);

		FArcCraftQueueEntry Unresolved;
		Unresolved.EntryId = FGuid::NewGuid();
		Unresolved.RecipeId = MakeUnindexedRecipeId();
		Unresolved.Priority = 0;
		FArcCraftQueueEntry Resolved;
		Resolved.EntryId = FGuid::NewGuid();
		Resolved.RecipeId = ResolvedId;
		Resolved.Priority = 1;
		FArcCraftQueueFragment* Queue = EM->GetFragmentDataPtr<FArcCraftQueueFragment>(Station);
		ASSERT_THAT(IsNotNull(Queue));
		Queue->Entries.Add(Unresolved);
		Queue->Entries.Add(Resolved);

		RunProcessor(0.0f);

		ASSERT_THAT(AreEqual(2, Queue->Entries.Num(), TEXT("Both entries must remain queued")));
		ASSERT_THAT(IsTrue(Queue->Entries[0].State == EArcCraftQueueEntryState::Pending, TEXT("Entry with an unresolved recipe id must stay Pending")));
		ASSERT_THAT(IsTrue(Queue->Entries[1].State == EArcCraftQueueEntryState::Active, TEXT("Entry with an indexed recipe id must be promoted")));
	}

	TEST_METHOD(Tick_ActiveEntryWithUnresolvedRecipeId_IsNotAdvanced)
	{
		using namespace ArcCraftRecordCommandTestsImpl;

		FArcRecipeDefinitionData ResolvedRecipe;
		ResolvedRecipe.CraftTime = 10.0f;
		const FPrimaryAssetId ResolvedId = Records.Write(ResolvedRecipe);
		ASSERT_THAT(IsTrue(ResolvedId.IsValid(), TEXT("Recipe record must be written and indexed")));

		const FMassEntityHandle Station = BuildStation(*EM, FGameplayTagContainer(), {}, true);

		FArcCraftQueueEntry Unresolved;
		Unresolved.EntryId = FGuid::NewGuid();
		Unresolved.RecipeId = MakeUnindexedRecipeId();
		Unresolved.State = EArcCraftQueueEntryState::Active;
		FArcCraftQueueEntry Resolved;
		Resolved.EntryId = FGuid::NewGuid();
		Resolved.RecipeId = ResolvedId;
		Resolved.State = EArcCraftQueueEntryState::Active;
		FArcCraftQueueFragment* Queue = EM->GetFragmentDataPtr<FArcCraftQueueFragment>(Station);
		ASSERT_THAT(IsNotNull(Queue));
		Queue->Entries.Add(Unresolved);
		Queue->Entries.Add(Resolved);

		RunProcessor(5.0f);

		ASSERT_THAT(AreEqual(2, Queue->Entries.Num(), TEXT("Both entries must stay queued")));
		ASSERT_THAT(IsNear(5.0f, Queue->Entries[1].ElapsedTickTime, 0.0001f, TEXT("The resolved Active sibling must accumulate craft time, proving the station was iterated")));
		ASSERT_THAT(IsNear(0.0f, Queue->Entries[0].ElapsedTickTime, 0.0001f, TEXT("An entry whose recipe id does not resolve must not accumulate craft time")));
		ASSERT_THAT(IsTrue(Queue->Entries[0].State == EArcCraftQueueEntryState::Active, TEXT("The unresolved entry must stay Active")));
	}
};
