/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or –
 * as soon as they will be approved by the European Commission – later versions
 * of the EUPL (the "License");
 *
 * You may not use this work except in compliance with the License.
 * You may get a copy of the License at:
 *
 * https://eupl.eu/
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 */

#include "ArcItemsTests.h"

#include "CQTest.h"
#include "Commands/TestMacros.h"

#include "Items/ArcItemSpec.h"
#include "ArcItemTestsSettings.h"
#include "NativeGameplayTags.h"
#include "PIEIrisNetworkComponent.h"
#include "PropertyBag.h"
#include "ArcPersistence/ArcPersistenceSubsystem.h"
#include "Player/ArcCorePlayerController.h"
#include "Engine/Engine.h"
#include "Commands/ArcMoveItemBetweenStoresCommand.h"
#include "Commands/ArcMoveItemToSlotCommand.h"
#include "Commands/ArcRemoveItemFromSlotCommand.h"
#include "Components/ActorTestSpawner.h"
#include "Components/PIENetworkComponent.h"
#include "Core/ArcCoreAssetManager.h"
#include "GameFramework/GameModeBase.h"
#include "Items/ArcItemsHelpers.h"
#include "Items/ArcItemsStoreComponent.h"
#include "Items/ArcItemsStoreSubsystem.h"
#include "Items/Fragments/ArcItemFragment_AbilityEffectsToApply.h"
#include "Items/Fragments/ArcItemFragment_GrantedAbilities.h"
#include "Items/Fragments/ArcItemFragment_ItemStats.h"
#include "Items/Fragments/ArcItemFragment_SocketSlots.h"
#include "Items/Fragments/ArcItemFragment_Stacks.h"
#include "Logging/StructuredLog.h"
#include "Net/UnrealNetwork.h"
#include "QuickBar/ArcQuickBarComponent.h"

DEFINE_LOG_CATEGORY(LogArcTestLog);

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_TestItemSlot_01, "QuickSlotId.ArcCoreTest.01");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_TestItemSlot_02, "QuickSlotId.ArcCoreTest.02");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_TestItemSlot_03, "QuickSlotId.ArcCoreTest.03");

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_TestEffectSpec_01, "GameplayEffect.Test.EffectToApply.01");


UArcTest_ItemWithSlots::UArcTest_ItemWithSlots()
{
	FInstancedStruct Struct;
	Struct.InitializeAs(FArcItemFragment_SocketSlots::StaticStruct());
	FArcSocketSlot SocketSlot;
	SocketSlot.SlotId = TAG_TestItemSlot_01;
	//Struct.GetMutablePtr<FArcItemFragment_SocketSlots>()->Slots.Add(SocketSlot);
	FragmentSet.Add({Struct});
}

UAbilitySystemComponent* AArcItemsTestActor::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent3;
}

AArcItemsTestActor::AArcItemsTestActor()
{
	bReplicates = true;
	bAlwaysRelevant = true;

	ItemsStore = CreateDefaultSubobject<UArcItemsStoreComponent>(TEXT("ItemsStore"));
	ItemsStore->SetIsReplicated(true);
	ItemsStore->SetNetAddressable();
	

	ItemsStore2 = CreateDefaultSubobject<UArcTestItemsStoreComponent>(TEXT("ItemsStore3"));
	ItemsStore2->SetIsReplicated(true);
	ItemsStore2->SetNetAddressable();

	ItemsStoreNotReplicated = CreateDefaultSubobject<UArcTestItemsStoreComponent>(TEXT("ItemsStoreNotReplicated"));
	ItemsStoreNotReplicated->bUseSubsystemForItemStore = true;
	ItemsStoreNotReplicated->SetIsReplicated(false);

	QuickBarComponent = CreateDefaultSubobject<UArcQuickBarComponent>(TEXT("QuickBarComponent"));
	QuickBarComponent->SetIsReplicated(true);
	
	AbilitySystemComponent3 = CreateDefaultSubobject<UArcCoreAbilitySystemComponent>(TEXT("AbilitySystemComponent3"));
	AbilitySystemComponent3->SetIsReplicated(true);
}

void AArcItemsTestActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AArcItemsTestActor, ReplicatedInt);
}

TEST_CLASS(ArcItemsStoreSubsystemTest, "ArcCore")
{
	uint32 SomeNumber = 0;
	FActorTestSpawner Spawner;
	BEFORE_EACH() 
	{
		SomeNumber++;
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
	}
	
	TEST_METHOD(AddItemNew) 
	{
		const int32 Value = 42;
		ASSERT_THAT(IsTrue(Value == 42));
		
		const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Name = *FGuid::NewGuid().ToString();
		AArcItemsTestActor& Actor = Spawner.SpawnActor<AArcItemsTestActor>(SpawnParameters);
		
		FArcItemSpec Spec;
		Spec.SetItemDefinition(Settings->SimpleBaseItem).SetAmount(1).SetItemLevel(1);
		FArcItemId Id = Actor.ItemsStoreNotReplicated->AddItem(Spec, FArcItemId::InvalidId);
		
		const FArcItemData* ItemPtr = Actor.ItemsStoreNotReplicated->GetItemPtr(Id);
		ASSERT_THAT(IsNotNull(ItemPtr, TEXT("ItemPtr is null")));
		//ASSERT_THAT(AreEqual( ItemPtr->GetItemId(), Id ));
	}

	TEST_METHOD(AddItemNewCopyToOtherItemsStore) 
	{
		const int32 Value = 42;
		ASSERT_THAT(IsTrue(Value == 42));
		
		const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Name = *FGuid::NewGuid().ToString();
		AArcItemsTestActor& Actor = Spawner.SpawnActor<AArcItemsTestActor>(SpawnParameters);
		
		FArcItemSpec Spec;
		Spec.SetItemDefinition(Settings->SimpleBaseItem).SetAmount(1).SetItemLevel(1);
		FArcItemId Id = Actor.ItemsStoreNotReplicated->AddItem(Spec, FArcItemId::InvalidId);
		
		const FArcItemData* ItemPtr = Actor.ItemsStoreNotReplicated->GetItemPtr(Id);
		ASSERT_THAT(IsNotNull(ItemPtr, TEXT("ItemPtr is null")));

		FArcItemCopyContainerHelper Copy = Actor.ItemsStoreNotReplicated->GetItemCopyHelper(Id);
		Actor.ItemsStore->AddItemDataInternal(Copy);
		UArcItemsStoreSubsystem* ItemsSubsystem = Actor.GetGameInstance<UGameInstance>()->GetSubsystem<UArcItemsStoreSubsystem>();
		
		const FArcItemData* ItemPtr2 = Actor.ItemsStore->GetItemPtr(Id);
		ASSERT_THAT(IsNotNull(ItemPtr2, TEXT("ItemPtr is null")));
		
		//ASSERT_THAT(AreEqual( ItemPtr->GetItemId(), Id ));
	}
};

/*
struct FArcItemDataInternalBagSerializerAnother
{
	static void Serialize(const FSerializeParam& Param, FInstancedPropertyBag& Out)
	{
		const FArcItemDataInternal* Ptr = reinterpret_cast<const FArcItemDataInternal*>(Param.Source);
		if (Ptr != nullptr)
		{
			FArcItemId Id = Ptr->ItemId;
			if (Id.IsValid())
			{
					
			}
		}
	}

	static void Deserialize()
	{
		
	}
};
*/
#if 0
TEST_CLASS(ArcItemsTests, "ArcCore") 
{
	uint32 SomeNumber = 0;
	FActorTestSpawner Spawner;
	BEFORE_EACH() 
	{
		SomeNumber++;
	}

	TEST_METHOD(SerializePropertyBag) 
	{
		const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Name = *FGuid::NewGuid().ToString();
		AArcItemsTestActor& Actor = Spawner.SpawnActor<AArcItemsTestActor>(SpawnParameters);

		FArcItemSpec Spec;
		Spec.SetItemDefinition(Settings->SimpleBaseItem).SetAmount(1)
			.SetItemLevel(1);
		FArcItemId Id = Actor.ItemsStore->AddItem(Spec, FArcItemId::InvalidId);
		Actor.Items->AddOwnedItem(Id);

		FArcItemDataInternal Data;
		Data.ItemId = FArcItemId::Generate();
		
		const FArcItemDataInternal* Item = &Data;
		FInstancedPropertyBag Bag;

		Arcx::FArcPersistenceSerializer* Test = &Arcx::FArcObjectDefaultPersistenceSerializer::Serializer;

		Arcx::FArcPersistenceSerializer* TestAnother = &Arcx::FArcObjectDefaultPersistenceSerializer::Serializer;
		
		Test->Serialize( {Item}, Bag);
		
		FInstancedPropertyBag InnerBag;
		InnerBag.AddProperty("TestFloat", EPropertyBagPropertyType::Float);
		InnerBag.SetValueFloat("TestFloat", 42.f);
		
		Bag.AddProperty("InnerBag", EPropertyBagPropertyType::Struct, FInstancedPropertyBag::StaticStruct());
		Bag.SetValueStruct("InnerBag", InnerBag);

		TValueOrError<FInstancedPropertyBag*, EPropertyBagResult> Val = Bag.GetValueStruct<FInstancedPropertyBag>("InnerBag");

		if (Val.HasError())
		{
			return;
		}

		TValueOrError<float, EPropertyBagResult> floatVal = Val.GetValue()->GetValueFloat("TestFloat");

		if (floatVal.HasError())
		{
			
		}
		//Test_PersistData<FArcItemDataInternal>(Data, &TestItemSomething::GetBag);
		//
		//Test_PersistDataSecond<FArcItemDataInternal, TestItemSomething>(Data);
	}
	
	TEST_METHOD(AddItemNew) 
	{
		const int32 Value = 42;
		ASSERT_THAT(IsTrue(Value == 42));
		
		//const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();
		//FActorSpawnParameters SpawnParameters;
		//SpawnParameters.Name = *FGuid::NewGuid().ToString();
		//AArcItemsTestActor& Actor = Spawner.SpawnActor<AArcItemsTestActor>(SpawnParameters);
		//
		//FArcItemSpec Spec;
		//Spec.SetItemDefinition(Settings->SimpleBaseItem).SetAmount(1)
		//	.SetItemLevel(1);
		//FArcItemId Id = Actor.ItemsStore->AddItem(Spec, FArcItemId::InvalidId);
		//Actor.Items->AddOwnedItem(Id);
		//
		//const FArcItemData* ItemPtr = Actor.ItemsStore->GetItemPtr(Id);
		//ASSERT_THAT(AreEqual( ItemPtr->GetItemId(), Id ));
		//
		//const FArcItemData* ItemPtrItems = Actor.Items->GetItem(Id);
		//ASSERT_THAT(AreEqual( ItemPtrItems->GetItemId(), Id ));
	}

	TEST_METHOD(AddItemToSlot) 
	{
		const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();
		FActorSpawnParameters SpawnParameters;
		AArcItemsTestActor& Actor = Spawner.SpawnActor<AArcItemsTestActor>(SpawnParameters);

		FArcItemSpec Spec;
		Spec.SetItemDefinition(Settings->ItemDefinitionId).SetAmount(1)
			.SetItemLevel(1);
		FArcItemId Id = Actor.ItemsStore->AddItem(Spec, FArcItemId::InvalidId);
		Actor.ItemsStore->AddItemToSlot(Id, TAG_TestItemSlot_01);

		const FArcItemData* ItemPtr = Actor.ItemsStore->GetItemFromSlot(TAG_TestItemSlot_01);
		ASSERT_THAT(AreEqual( ItemPtr->GetSlotId(), TAG_TestItemSlot_01 ));
	}

	TEST_METHOD(AttachItemToOtherItemSlot) 
	{
		const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();
		FActorSpawnParameters SpawnParameters;
		AArcItemsTestActor& Actor = Spawner.SpawnActor<AArcItemsTestActor>(SpawnParameters);

		FArcItemSpec OwnerSpec;
		OwnerSpec.SetItemDefinition(Settings->ItemWithAttachmentSlots).SetAmount(1)
			.SetItemLevel(1);
		FArcItemId OwnerItemId = Actor.ItemsStore->AddItem(OwnerSpec, FArcItemId::InvalidId);
		Actor.ItemsStore->AddItemToSlot(OwnerItemId, TAG_TestItemSlot_01);

		FArcItemSpec AttachedSpec;
		AttachedSpec.SetItemDefinition(Settings->SimpleBaseItem).SetAmount(1)
			.SetItemLevel(1);
		FArcItemId AttachedItemId = Actor.ItemsStore->AddItem(OwnerSpec, FArcItemId::InvalidId);

		Actor.ItemsStore->InternalAttachToItem(OwnerItemId, AttachedItemId, TAG_TestItemSlot_02);

		TArray<const FArcItemData*> AttachedItems = Actor.ItemsStore->GetAttachedItems(OwnerItemId);
		ASSERT_THAT(AreEqual( AttachedItems[0]->GetOwnerId(), OwnerItemId ));

		const FArcItemData* AttachedItemPtr = Actor.ItemsStore->GetItemAttachedTo(OwnerItemId, TAG_TestItemSlot_02);
		ASSERT_THAT(AreEqual( AttachedItemPtr->GetItemId(), AttachedItemId ));
	}

	TEST_METHOD(AttachItemToOtherItemSlot_RemoveOwnerFromSlot) 
	{
		const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();
		FActorSpawnParameters SpawnParameters;
		AArcItemsTestActor& Actor = Spawner.SpawnActor<AArcItemsTestActor>(SpawnParameters);

		FArcItemSpec OwnerSpec;
		OwnerSpec.SetItemDefinition(Settings->ItemWithAttachmentSlots).SetAmount(1)
			.SetItemLevel(1);
		FArcItemId OwnerItemId = Actor.ItemsStore->AddItem(OwnerSpec, FArcItemId::InvalidId);
		Actor.ItemsStore->AddItemToSlot(OwnerItemId, TAG_TestItemSlot_01);

		FArcItemSpec AttachedSpec;
		AttachedSpec.SetItemDefinition(Settings->SimpleBaseItem).SetAmount(1)
			.SetItemLevel(1);
		FArcItemId AttachedItemId = Actor.ItemsStore->AddItem(OwnerSpec, FArcItemId::InvalidId);

		Actor.ItemsStore->InternalAttachToItem(OwnerItemId, AttachedItemId, TAG_TestItemSlot_02);

		const FArcItemData* AttachedItemPtr = Actor.ItemsStore->GetItemAttachedTo(OwnerItemId, TAG_TestItemSlot_02);

		Actor.ItemsStore->RemoveItemFromSlot(OwnerItemId);
	}
	
	TEST_METHOD(AttachItemToOtherItemSlot_SlotIsMissing) 
	{
		const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();
		FActorSpawnParameters SpawnParameters;
		AArcItemsTestActor& Actor = Spawner.SpawnActor<AArcItemsTestActor>(SpawnParameters);

		FArcItemSpec OwnerSpec;
		OwnerSpec.SetItemDefinition(Settings->ItemWithAttachmentSlots).SetAmount(1)
			.SetItemLevel(1);
		FArcItemId OwnerItemId = Actor.ItemsStore->AddItem(OwnerSpec, FArcItemId::InvalidId);
		Actor.ItemsStore->AddItemToSlot(OwnerItemId, TAG_TestItemSlot_01);

		FArcItemSpec AttachedSpec;
		AttachedSpec.SetItemDefinition(Settings->SimpleBaseItem).SetAmount(1)
			.SetItemLevel(1);
		FArcItemId AttachedItemId = Actor.ItemsStore->AddItem(OwnerSpec, FArcItemId::InvalidId);

		Actor.ItemsStore->InternalAttachToItem(OwnerItemId, AttachedItemId, TAG_TestItemSlot_03);

		TArray<const FArcItemData*> AttachedItems = Actor.ItemsStore->GetAttachedItems(OwnerItemId);
		ASSERT_THAT(AreEqual( AttachedItems.Num(), 0 ));
	}

	TEST_METHOD(RemoveItemAttachedToSlot) 
	{
		const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();
		FActorSpawnParameters SpawnParameters;
		AArcItemsTestActor& Actor = Spawner.SpawnActor<AArcItemsTestActor>(SpawnParameters);

		FArcItemSpec OwnerSpec;
		OwnerSpec.SetItemDefinition(Settings->ItemWithAttachmentSlots).SetAmount(1)
			.SetItemLevel(1);
		FArcItemId OwnerItemId = Actor.ItemsStore->AddItem(OwnerSpec, FArcItemId::InvalidId);
		Actor.ItemsStore->AddItemToSlot(OwnerItemId, TAG_TestItemSlot_01);

		FArcItemSpec AttachedSpec;
		AttachedSpec.SetItemDefinition(Settings->SimpleBaseItem).SetAmount(1)
			.SetItemLevel(1);
		FArcItemId AttachedItemId = Actor.ItemsStore->AddItem(OwnerSpec, FArcItemId::InvalidId);

		Actor.ItemsStore->InternalAttachToItem(OwnerItemId, AttachedItemId, TAG_TestItemSlot_02);

		TArray<const FArcItemData*> AttachedItems = Actor.ItemsStore->GetAttachedItems(OwnerItemId);
		ASSERT_THAT(AreEqual( AttachedItems[0]->GetOwnerId(), OwnerItemId ));

		const FArcItemData* AttachedItemPtr = Actor.ItemsStore->GetItemAttachedTo(OwnerItemId, TAG_TestItemSlot_02);
		ASSERT_THAT(AreEqual( AttachedItemPtr->GetItemId(), AttachedItemId ));

		Actor.ItemsStore->DetachItemFrom(OwnerItemId, AttachedItemId);
		TArray<const FArcItemData*> ZeroAttachedItems = Actor.ItemsStore->GetAttachedItems(OwnerItemId);
		ASSERT_THAT(AreEqual( ZeroAttachedItems.Num(), 0 ));
	}
	
	TEST_METHOD(AddItemToSlot_GrantAbility) 
	{
		const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();
		FActorSpawnParameters SpawnParameters;
		AArcItemsTestActor& Actor = Spawner.SpawnActor<AArcItemsTestActor>(SpawnParameters);

		FArcItemSpec Spec;
		Spec.SetItemDefinition(Settings->ItemWithGrantedAbility).SetAmount(1)
			.SetItemLevel(1);
		FArcItemId Id = Actor.ItemsStore->AddItem(Spec, FArcItemId::InvalidId);
		Actor.ItemsStore->AddItemToSlot(Id, TAG_TestItemSlot_01);

		const FArcItemData* ItemPtr = Actor.ItemsStore->GetItemFromSlot(TAG_TestItemSlot_01);
		ASSERT_THAT(AreEqual( ItemPtr->GetSlotId(), TAG_TestItemSlot_01 ));

		const FArcItemInstance_GrantedAbilities* Instance = ArcItemsHelper::FindInstance<FArcItemInstance_GrantedAbilities>(ItemPtr);
		ASSERT_THAT(AreEqual( Instance->GetGrantedAbilities().Num(), 1 ));
	}
	
	TEST_METHOD(RemoveItemFromSlot_GrantAbility) 
	{
		const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();
		FActorSpawnParameters SpawnParameters;
		AArcItemsTestActor& Actor = Spawner.SpawnActor<AArcItemsTestActor>(SpawnParameters);

		FArcItemSpec Spec;
		Spec.SetItemDefinition(Settings->ItemWithGrantedAbility).SetAmount(1)
			.SetItemLevel(1);
		FArcItemId Id = Actor.ItemsStore->AddItem(Spec, FArcItemId::InvalidId);
		Actor.ItemsStore->AddItemToSlot(Id, TAG_TestItemSlot_01);

		const FArcItemData* ItemPtr = Actor.ItemsStore->GetItemFromSlot(TAG_TestItemSlot_01);
		ASSERT_THAT(AreEqual( ItemPtr->GetSlotId(), TAG_TestItemSlot_01 ));

		const FArcItemInstance_GrantedAbilities* Instance = ArcItemsHelper::FindInstance<FArcItemInstance_GrantedAbilities>(ItemPtr);
		ASSERT_THAT(AreEqual( Instance->GetGrantedAbilities().Num(), 1 ));

		Actor.ItemsStore->RemoveItemFromSlot(Id);

		ASSERT_THAT(AreEqual( Instance->GetGrantedAbilities().Num(), 0 ));
		ASSERT_THAT(AreEqual( ItemPtr->GetSlotId().IsValid(), false ));	
	}
	
	TEST_METHOD(AttachItemToOtherItemSlot_AttachOwnerToSlot_GrantAbility) 
	{
		const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();
		FActorSpawnParameters SpawnParameters;
		AArcItemsTestActor& Actor = Spawner.SpawnActor<AArcItemsTestActor>(SpawnParameters);

		FArcItemSpec OwnerSpec;
		OwnerSpec.SetItemDefinition(Settings->ItemWithAttachmentSlots).SetAmount(1)
			.SetItemLevel(1);
		FArcItemId OwnerItemId = Actor.ItemsStore->AddItem(OwnerSpec, FArcItemId::InvalidId);
		
		FArcItemSpec AttachedSpec;
		AttachedSpec.SetItemDefinition(Settings->ItemWithGrantedAbility).SetAmount(1)
			.SetItemLevel(1);
		FArcItemId AttachedItemId = Actor.ItemsStore->AddItem(AttachedSpec, FArcItemId::InvalidId);

		Actor.ItemsStore->InternalAttachToItem(OwnerItemId, AttachedItemId, TAG_TestItemSlot_02);
		FArcItemData* AttachItemData = Actor.ItemsStore->GetItemPtr(AttachedItemId);
		
		const FArcItemInstance_GrantedAbilities* Instance = ArcItemsHelper::FindInstance<FArcItemInstance_GrantedAbilities>(AttachItemData);
		ASSERT_THAT(AreEqual( Instance->GetGrantedAbilities().Num(), 0 ));

		Actor.ItemsStore->AddItemToSlot(OwnerItemId, TAG_TestItemSlot_01);
		
		ASSERT_THAT(AreEqual( Instance->GetGrantedAbilities().Num(), 1 ));
	}

	TEST_METHOD(AttachItemToOtherItemSlot_RemoveOwnerFromSlot_GrantAbility) 
	{
		const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();
		FActorSpawnParameters SpawnParameters;
		AArcItemsTestActor& Actor = Spawner.SpawnActor<AArcItemsTestActor>(SpawnParameters);

		FArcItemSpec OwnerSpec;
		OwnerSpec.SetItemDefinition(Settings->ItemWithAttachmentSlots).SetAmount(1)
			.SetItemLevel(1);
		FArcItemId OwnerItemId = Actor.ItemsStore->AddItem(OwnerSpec, FArcItemId::InvalidId);
		
		FArcItemSpec AttachedSpec;
		AttachedSpec.SetItemDefinition(Settings->ItemWithGrantedAbility).SetAmount(1)
			.SetItemLevel(1);
		FArcItemId AttachedItemId = Actor.ItemsStore->AddItem(AttachedSpec, FArcItemId::InvalidId);

		Actor.ItemsStore->InternalAttachToItem(OwnerItemId, AttachedItemId, TAG_TestItemSlot_02);
		FArcItemData* AttachItemData = Actor.ItemsStore->GetItemPtr(AttachedItemId);
		
		const FArcItemInstance_GrantedAbilities* Instance = ArcItemsHelper::FindInstance<FArcItemInstance_GrantedAbilities>(AttachItemData);
		ASSERT_THAT(AreEqual( Instance->GetGrantedAbilities().Num(), 0 ));

		Actor.ItemsStore->AddItemToSlot(OwnerItemId, TAG_TestItemSlot_01);
		
		ASSERT_THAT(AreEqual( Instance->GetGrantedAbilities().Num(), 1 ));

		Actor.ItemsStore->RemoveItemFromSlot(OwnerItemId);
		ASSERT_THAT(AreEqual( Instance->GetGrantedAbilities().Num(), 0 ));
	}
	
	TEST_METHOD(AttachItemToOtherItemSlot_OwnerIsOnSlot_GrantAbility) 
	{
		const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();
		FActorSpawnParameters SpawnParameters;
		AArcItemsTestActor& Actor = Spawner.SpawnActor<AArcItemsTestActor>(SpawnParameters);

		FArcItemSpec OwnerSpec;
		OwnerSpec.SetItemDefinition(Settings->ItemWithAttachmentSlots).SetAmount(1).SetItemLevel(1);
		FArcItemId OwnerItemId = Actor.ItemsStore->AddItem(OwnerSpec, FArcItemId::InvalidId);
		Actor.ItemsStore->AddItemToSlot(OwnerItemId, TAG_TestItemSlot_01);
		
		FArcItemSpec AttachedSpec;
		AttachedSpec.SetItemDefinition(Settings->ItemWithGrantedAbility).SetAmount(1).SetItemLevel(1);
		FArcItemId AttachedItemId = Actor.ItemsStore->AddItem(AttachedSpec, FArcItemId::InvalidId);

		Actor.ItemsStore->InternalAttachToItem(OwnerItemId, AttachedItemId, TAG_TestItemSlot_02);
		FArcItemData* AttachItemData = Actor.ItemsStore->GetItemPtr(AttachedItemId);
		
		const FArcItemInstance_GrantedAbilities* Instance = ArcItemsHelper::FindInstance<FArcItemInstance_GrantedAbilities>(AttachItemData);
		
		ASSERT_THAT(AreEqual( Instance->GetGrantedAbilities().Num(), 1 ));
	}
	
	TEST_METHOD(AddItem_WithEffectToApply) 
	{
		const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();
		FActorSpawnParameters SpawnParameters;
		AArcItemsTestActor& Actor = Spawner.SpawnActor<AArcItemsTestActor>(SpawnParameters);

		FArcItemSpec OwnerSpec;
		OwnerSpec.SetItemDefinition(Settings->ItemWithEffectToApply).SetAmount(1).SetItemLevel(1);
		FArcItemId OwnerItemId = Actor.ItemsStore->AddItem(OwnerSpec, FArcItemId::InvalidId);

		const FArcItemData* ItemPtr = Actor.ItemsStore->GetItemPtr(OwnerItemId);
		
		const FArcItemInstance_EffectToApply* Instance = ArcItemsHelper::FindInstance<FArcItemInstance_EffectToApply>(ItemPtr);
		TArray<const FArcEffectSpecItem*> Specs = Instance->GetEffectSpecHandles(TAG_TestEffectSpec_01);
		
		ASSERT_THAT(AreEqual( Specs.Num(), 1 ));
	}

	TEST_METHOD(AddItem_AttachItem_WithEffectToApply) 
	{
		const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();
		FActorSpawnParameters SpawnParameters;
		AArcItemsTestActor& Actor = Spawner.SpawnActor<AArcItemsTestActor>(SpawnParameters);

		FArcItemSpec OwnerSpec;
		OwnerSpec.SetItemDefinition(Settings->ItemWithAttachmentSlots).SetAmount(1).SetItemLevel(1);
		FArcItemId OwnerItemId = Actor.ItemsStore->AddItem(OwnerSpec, FArcItemId::InvalidId);

		const FArcItemData* ItemPtr = Actor.ItemsStore->GetItemPtr(OwnerItemId);

		FArcItemSpec AttachedSpec;
		AttachedSpec.SetItemDefinition(Settings->ItemWithEffectToApply).SetAmount(1).SetItemLevel(1);
		FArcItemId AttachedItemId = Actor.ItemsStore->AddItem(AttachedSpec, FArcItemId::InvalidId);

		Actor.ItemsStore->InternalAttachToItem(OwnerItemId, AttachedItemId, FGameplayTag::EmptyTag);
		
		const FArcItemInstance_EffectToApply* Instance = ArcItemsHelper::FindInstance<FArcItemInstance_EffectToApply>(ItemPtr);
		TArray<const FArcEffectSpecItem*> Specs = Instance->GetEffectSpecHandles(TAG_TestEffectSpec_01);
		
		ASSERT_THAT(AreEqual( Specs.Num(), 1 ));
	}

	TEST_METHOD(AddItem_WithEffectToApply_Duplicate) 
	{
		const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();
		FActorSpawnParameters SpawnParameters;
		AArcItemsTestActor& Actor = Spawner.SpawnActor<AArcItemsTestActor>(SpawnParameters);

		FArcItemSpec OwnerSpec;
		OwnerSpec.SetItemDefinition(Settings->ItemWithEffectToApply).SetAmount(1).SetItemLevel(1);
		FArcItemId OwnerItemId = Actor.ItemsStore->AddItem(OwnerSpec, FArcItemId::InvalidId);

		const FArcItemData* ItemPtr = Actor.ItemsStore->GetItemPtr(OwnerItemId);
		
		const FArcItemInstance_EffectToApply* Instance = ArcItemsHelper::FindInstance<FArcItemInstance_EffectToApply>(ItemPtr);
		TArray<const FArcEffectSpecItem*> Specs = Instance->GetEffectSpecHandles(TAG_TestEffectSpec_01);
		
		ASSERT_THAT(AreEqual( Specs.Num(), 1 ));

		const FArcItemDataInternal* InternalItem = Actor.ItemsStore->GetInternalItem(OwnerItemId);
		FArcItemCopyContainerHelper Copy = FArcItemCopyContainerHelper::New(*InternalItem);
		TArray<FArcItemId> CopyId = Actor.ItemsStore2->AddItemDataInternal(Actor.Items, Copy);

		const FArcItemData* CopyItemPtr = Actor.ItemsStore2->GetItemPtr(CopyId[0]);
		
		const FArcItemInstance_EffectToApply* CopyInstance = ArcItemsHelper::FindInstance<FArcItemInstance_EffectToApply>(CopyItemPtr);
		ASSERT_THAT(IsNotNull(CopyInstance));
	}

	TEST_METHOD(AttachItemToOther_DuplicateItem) 
	{
		const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();
		FActorSpawnParameters SpawnParameters;
		AArcItemsTestActor& Actor = Spawner.SpawnActor<AArcItemsTestActor>(SpawnParameters);

		FArcItemSpec OwnerSpec;
		OwnerSpec.SetItemDefinition(Settings->ItemWithAttachmentSlots).SetAmount(1)
			.SetItemLevel(1);
		FArcItemId OwnerItemId = Actor.ItemsStore->AddItem(OwnerSpec, FArcItemId::InvalidId);
		Actor.ItemsStore->AddItemToSlot(OwnerItemId, TAG_TestItemSlot_01);

		FArcItemSpec AttachedSpec;
		AttachedSpec.SetItemDefinition(Settings->SimpleBaseItem).SetAmount(1)
			.SetItemLevel(1);
		FArcItemId AttachedItemId = Actor.ItemsStore->AddItem(OwnerSpec, FArcItemId::InvalidId);

		Actor.ItemsStore->InternalAttachToItem(OwnerItemId, AttachedItemId, TAG_TestItemSlot_02);

		TArray<const FArcItemData*> AttachedItems = Actor.ItemsStore->GetAttachedItems(OwnerItemId);
		ASSERT_THAT(AreEqual( AttachedItems[0]->GetOwnerId(), OwnerItemId ));

		const FArcItemData* AttachedItemPtr = Actor.ItemsStore->GetItemAttachedTo(OwnerItemId, TAG_TestItemSlot_02);
		ASSERT_THAT(AreEqual( AttachedItemPtr->GetItemId(), AttachedItemId ));

		const FArcItemData* ItemPtr = Actor.ItemsStore->GetItemPtr(OwnerItemId);

		const FArcItemDataInternal* InternalItem = Actor.ItemsStore->GetInternalItem(OwnerItemId);
		FArcItemCopyContainerHelper Copy = FArcItemCopyContainerHelper::Duplicate(Actor.ItemsStore, *InternalItem);
		TArray<FArcItemId> CopyId = Actor.ItemsStore2->AddItemDataInternal(Actor.Items, Copy);
		
		//FArcItemId CopyId = Actor.ItemsStore2->AddCopyInternal(Actor.Items
		//	, ItemPtr);

		const FArcItemData* CopyItemPtr = Actor.ItemsStore2->GetItemPtr(CopyId[0]);

		int32 ItemNum = Actor.ItemsStore2->GetItemNum();
		
		ASSERT_THAT(AreEqual( ItemNum, 2 ));
		
		const FArcItemData* CopyAttachedItemPtr = Actor.ItemsStore2->GetItemAttachedTo(CopyId[0], TAG_TestItemSlot_02);

		ASSERT_THAT(IsNotNull(CopyAttachedItemPtr));
	}

	TEST_METHOD(AttachItemToOther_DuplicateItem_DestroyOriginal) 
	{
		const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();
		FActorSpawnParameters SpawnParameters;
		AArcItemsTestActor& Actor = Spawner.SpawnActor<AArcItemsTestActor>(SpawnParameters);

		FArcItemSpec OwnerSpec;
		OwnerSpec.SetItemDefinition(Settings->ItemWithAttachmentSlots).SetAmount(1)
			.SetItemLevel(1);
		FArcItemId OwnerItemId = Actor.ItemsStore->AddItem(OwnerSpec, FArcItemId::InvalidId);
		Actor.ItemsStore->AddItemToSlot(OwnerItemId, TAG_TestItemSlot_01);

		FArcItemSpec AttachedSpec;
		AttachedSpec.SetItemDefinition(Settings->SimpleBaseItem).SetAmount(1)
			.SetItemLevel(1);
		FArcItemId AttachedItemId = Actor.ItemsStore->AddItem(OwnerSpec, FArcItemId::InvalidId);

		Actor.ItemsStore->InternalAttachToItem(OwnerItemId, AttachedItemId, TAG_TestItemSlot_02);

		TArray<const FArcItemData*> AttachedItems = Actor.ItemsStore->GetAttachedItems(OwnerItemId);
		ASSERT_THAT(AreEqual( AttachedItems[0]->GetOwnerId(), OwnerItemId ));

		const FArcItemData* AttachedItemPtr = Actor.ItemsStore->GetItemAttachedTo(OwnerItemId, TAG_TestItemSlot_02);
		ASSERT_THAT(AreEqual( AttachedItemPtr->GetItemId(), AttachedItemId ));

		const FArcItemData* ItemPtr = Actor.ItemsStore->GetItemPtr(OwnerItemId);
		
		//FArcItemId CopyId = Actor.ItemsStore2->AddCopyInternal(Actor.Items, ItemPtr);
		const FArcItemDataInternal* InternalItem = Actor.ItemsStore->GetInternalItem(OwnerItemId);
		FArcItemCopyContainerHelper Copy = FArcItemCopyContainerHelper::New(*InternalItem);
		TArray<FArcItemId> CopyId = Actor.ItemsStore2->AddItemDataInternal(Actor.Items, Copy);
		
		const FArcItemData* CopyItemPtr = Actor.ItemsStore2->GetItemPtr(CopyId[0]);

		Actor.ItemsStore->DestroyItem(OwnerItemId);

		int32 ZeroItemNum = Actor.ItemsStore->GetItemNum();
		
		ASSERT_THAT(AreEqual( ZeroItemNum, 0 ));
		
		int32 ItemNum = Actor.ItemsStore2->GetItemNum();
		
		ASSERT_THAT(AreEqual( ItemNum, 2 ));
		
		const FArcItemData* CopyAttachedItemPtr = Actor.ItemsStore2->GetItemAttachedTo(CopyId[0], TAG_TestItemSlot_02);

		ASSERT_THAT(IsNotNull(CopyAttachedItemPtr));
	}

	TEST_METHOD(AttachItemToOtherItemSlot_GrantAbility_MoveToOtherStore) 
	{
		const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();
		FActorSpawnParameters SpawnParameters;
		AArcItemsTestActor& Actor = Spawner.SpawnActor<AArcItemsTestActor>(SpawnParameters);

		FArcItemSpec OwnerSpec;
		OwnerSpec.SetItemDefinition(Settings->ItemWithAttachmentSlots).SetAmount(1)
			.SetItemLevel(1);
		FArcItemId OwnerItemId = Actor.ItemsStore->AddItem(OwnerSpec, FArcItemId::InvalidId);
		
		FArcItemSpec AttachedSpec;
		AttachedSpec.SetItemDefinition(Settings->ItemWithGrantedAbility).SetAmount(1)
			.SetItemLevel(1);
		FArcItemId AttachedItemId = Actor.ItemsStore->AddItem(AttachedSpec, FArcItemId::InvalidId);

		Actor.ItemsStore->InternalAttachToItem(OwnerItemId, AttachedItemId, TAG_TestItemSlot_02);
		FArcItemData* AttachItemData = Actor.ItemsStore->GetItemPtr(AttachedItemId);
		
		const FArcItemInstance_GrantedAbilities* Instance = ArcItemsHelper::FindInstance<FArcItemInstance_GrantedAbilities>(AttachItemData);
		ASSERT_THAT(AreEqual( Instance->GetGrantedAbilities().Num(), 0 ));

		Actor.ItemsStore->AddItemToSlot(OwnerItemId, TAG_TestItemSlot_01);
		
		ASSERT_THAT(AreEqual( Instance->GetGrantedAbilities().Num(), 1 ));

		FGameplayAbilitySpecHandle AbilitySpecHandle = Instance->GetGrantedAbilities()[0];
		
		FArcItemData* ItemPtr = Actor.ItemsStore->GetItemPtr(OwnerItemId);
		//FArcItemId CopyId = Actor.ItemsStore2->AddCopyInternal(Actor.Items, ItemPtr);
		const FArcItemDataInternal* InternalItem = Actor.ItemsStore->GetInternalItem(OwnerItemId);
		FArcItemCopyContainerHelper Copy = FArcItemCopyContainerHelper::New(*InternalItem);
		TArray<FArcItemId> CopyId = Actor.ItemsStore2->AddItemDataInternal(Actor.Items, Copy);
		
		Actor.ItemsStore->DestroyItem(OwnerItemId);

		FGameplayAbilitySpec* Spec = Actor.AbilitySystemComponent3->FindAbilitySpecFromHandle(AbilitySpecHandle);
		ASSERT_THAT(IsNull(Spec));
	}

	TEST_METHOD(AddItemNew_IncreaseStacks) 
	{
		const int32 Value = 42;
		ASSERT_THAT(IsTrue(Value == 42));
		
		//const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();
		//FActorSpawnParameters SpawnParameters;
		//SpawnParameters.Name = *FGuid::NewGuid().ToString();
		//AArcItemsTestActor& Actor = Spawner.SpawnActor<AArcItemsTestActor>(SpawnParameters);
		//
		//FArcItemSpec Spec;
		//Spec.SetItemDefinition(Settings->ItemWithStacks).SetAmount(1)
		//	.SetItemLevel(1);
		//FArcItemId Id = Actor.ItemsStore->AddItem(Spec, FArcItemId::InvalidId);
		//Actor.Items->AddOwnedItem(Id);
		//
		//const FArcItemData* ItemPtr = Actor.ItemsStore->GetItemPtr(Id);
		//ASSERT_THAT(AreEqual( ItemPtr->GetItemId(), Id ));
		//
		//const FArcItemData* ItemPtrItems = Actor.Items->GetItem(Id);
		//ASSERT_THAT(AreEqual( ItemPtrItems->GetItemId(), Id ));
		//
		//FArcItemInstance_Stacks* Stacks = ArcItemsHelper::FindMutableInstance<FArcItemInstance_Stacks>(ItemPtr);
		//ASSERT_THAT(AreEqual( Stacks->GetStacks(), 1 ));
		//
		//Stacks->AddStacks(1);
		//
		//ASSERT_THAT(AreEqual( Stacks->GetStacks(), 2 ));
	}
	
	TEST_METHOD(ArcMoveItemBetweenStoresCommand_MoveOneStack) 
	{
		const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Name = *FGuid::NewGuid().ToString();
		AArcItemsTestActor& Actor = Spawner.SpawnActor<AArcItemsTestActor>(SpawnParameters);

		FArcItemSpec Spec;
		Spec.SetItemDefinition(Settings->ItemWithStacks).SetAmount(1)
			.SetItemLevel(1);
		FArcItemId Id = Actor.ItemsStore->AddItem(Spec, FArcItemId::InvalidId);
		Actor.Items->AddOwnedItem(Id);

		const FArcItemData* ItemPtr = Actor.ItemsStore->GetItemPtr(Id);
		ASSERT_THAT(AreEqual( ItemPtr->GetItemId(), Id ));

		const FArcItemData* ItemPtrItems = Actor.Items->GetItem(Id);
		ASSERT_THAT(AreEqual( ItemPtrItems->GetItemId(), Id ));

		FArcItemInstance_Stacks* Stacks = ArcItemsHelper::FindMutableInstance<FArcItemInstance_Stacks>(ItemPtr);
		ASSERT_THAT(AreEqual( Stacks->GetStacks(), 1 ));

		Stacks->AddStacks(1);
		
		ASSERT_THAT(AreEqual( Stacks->GetStacks(), 2 ));

		TSharedPtr<FArcMoveItemBetweenStoresCommand> Command = MakeShareable(
			new FArcMoveItemBetweenStoresCommand(Actor.ItemsStore, Actor.Items,
				Actor.ItemsStore2, Actor.Items2, Id, 1));

		Command->Execute();

		const FArcItemData* AferMoveItemPtr = Actor.ItemsStore->GetItemPtr(Id);
		ASSERT_THAT(AreEqual( AferMoveItemPtr->GetItemId(), Id ));

		FArcItemInstance_Stacks* AfterMoveStacks = ArcItemsHelper::FindMutableInstance<FArcItemInstance_Stacks>(AferMoveItemPtr);
		ASSERT_THAT(AreEqual( AfterMoveStacks->GetStacks(), 1 ));

		TArray<const FArcItemData*> MovedItems = Actor.ItemsStore2->GetItems();
		ASSERT_THAT(AreEqual( MovedItems.Num(), 1 ));
	}
	
	TEST_METHOD(ArcMoveItemBetweenStoresCommand_MoveAllStacks_NoStackingPolicy) 
	{
		const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Name = *FGuid::NewGuid().ToString();
		AArcItemsTestActor& Actor = Spawner.SpawnActor<AArcItemsTestActor>(SpawnParameters);

		FArcItemSpec Spec;
		Spec.SetItemDefinition(Settings->ItemWithStacksNoStackingPolicy).SetAmount(1)
			.SetItemLevel(1);
		FArcItemId Id = Actor.ItemsStore->AddItem(Spec, FArcItemId::InvalidId);
		Actor.Items->AddOwnedItem(Id);

		const FArcItemData* ItemPtr = Actor.ItemsStore->GetItemPtr(Id);
		ASSERT_THAT(AreEqual( ItemPtr->GetItemId(), Id ));

		const FArcItemData* ItemPtrItems = Actor.Items->GetItem(Id);
		ASSERT_THAT(AreEqual( ItemPtrItems->GetItemId(), Id ));

		FArcItemInstance_Stacks* Stacks = ArcItemsHelper::FindMutableInstance<FArcItemInstance_Stacks>(ItemPtr);
		ASSERT_THAT(AreEqual( Stacks->GetStacks(), 1 ));

		Stacks->AddStacks(1);
		
		ASSERT_THAT(AreEqual( Stacks->GetStacks(), 2 ));

		TSharedPtr<FArcMoveItemBetweenStoresCommand> Command = MakeShareable(
			new FArcMoveItemBetweenStoresCommand(Actor.ItemsStore, Actor.Items,
				Actor.ItemsStore2, Actor.Items2, Id, 2));

		Command->Execute();

		const FArcItemData* AferMoveItemPtr = Actor.ItemsStore->GetItemPtr(Id);
		ASSERT_THAT(IsNull( AferMoveItemPtr ));

		TArray<const FArcItemData*> MovedItems = Actor.ItemsStore2->GetItems();
		ASSERT_THAT(AreEqual( MovedItems.Num(), 1 ));
		
		FArcItemInstance_Stacks* AfterMoveStacks = ArcItemsHelper::FindMutableInstance<FArcItemInstance_Stacks>(MovedItems[0]);
        ASSERT_THAT(AreEqual( AfterMoveStacks->GetStacks(), 2 ));
	}

	TEST_METHOD(ArcMoveItemToSlotCommand_SameItemsComponent)
	{
		const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Name = *FGuid::NewGuid().ToString();
		AArcItemsTestActor& Actor = Spawner.SpawnActor<AArcItemsTestActor>(SpawnParameters);

		FArcItemSpec Spec;
		Spec.SetItemDefinition(Settings->ItemWithStacksNoStackingPolicy).SetAmount(1)
			.SetItemLevel(1);
		FArcItemId Id = Actor.ItemsStore->AddItem(Spec, FArcItemId::InvalidId);
		Actor.Items->AddOwnedItem(Id);
		
		TSharedPtr<FArcMoveItemToSlotCommand> Command = MakeShareable(
			new FArcMoveItemToSlotCommand(Actor.Items, Actor.Items,
				Id, TAG_TestItemSlot_01));
		Command->Execute();
		
		const FArcItemData* ItemData = Actor.Items->GetItem(Id);
		ASSERT_THAT(IsNotNull( ItemData ));
		const FArcItemData* ItemDataFromSlot = Actor.ItemsStore->GetItemFromSlot(TAG_TestItemSlot_01);
		ASSERT_THAT(IsNotNull( ItemDataFromSlot ));
	}
	
	TEST_METHOD(ArcMoveItemToSlotCommand_DifferentItemsComponent)
	{
		const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Name = *FGuid::NewGuid().ToString();
		AArcItemsTestActor& Actor = Spawner.SpawnActor<AArcItemsTestActor>(SpawnParameters);

		FArcItemSpec Spec;
		Spec.SetItemDefinition(Settings->ItemWithStacksNoStackingPolicy).SetAmount(1)
			.SetItemLevel(1);
		FArcItemId Id = Actor.ItemsStore->AddItem(Spec, FArcItemId::InvalidId);
		Actor.Items->AddOwnedItem(Id);
		
		TSharedPtr<FArcMoveItemToSlotCommand> Command = MakeShareable(
			new FArcMoveItemToSlotCommand(Actor.Items, Actor.ItemsSharedStore,
				Id, TAG_TestItemSlot_01));

		Command->Execute();
		
		const FArcItemData* ItemData = Actor.Items->GetItem(Id);
		ASSERT_THAT(IsNull( ItemData ));

		const FArcItemData* ItemData2 = Actor.ItemsSharedStore->GetItem(Id);
		ASSERT_THAT(IsNotNull( ItemData2 ));
		
		const FArcItemData* ItemDataFromSlot = Actor.ItemsStore->GetItemFromSlot(TAG_TestItemSlot_01);
		ASSERT_THAT(IsNotNull( ItemDataFromSlot ));
	}

	TEST_METHOD(ArcMoveItemCommand_DifferentItemsComponent)
	{
		const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Name = *FGuid::NewGuid().ToString();
		AArcItemsTestActor& Actor = Spawner.SpawnActor<AArcItemsTestActor>(SpawnParameters);

		FArcItemSpec Spec;
		Spec.SetItemDefinition(Settings->ItemWithStacksNoStackingPolicy).SetAmount(1)
			.SetItemLevel(1);
		FArcItemId Id = Actor.ItemsStore->AddItem(Spec, FArcItemId::InvalidId);
		Actor.Items->AddOwnedItem(Id);
		
		TSharedPtr<FArcMoveItemCommand> Command = MakeShareable(
			new FArcMoveItemCommand(Actor.Items, Actor.ItemsSharedStore, Id));

		Command->Execute();
		
		const FArcItemData* ItemData = Actor.Items->GetItem(Id);
		ASSERT_THAT(IsNull( ItemData ));

		const FArcItemData* ItemData2 = Actor.ItemsSharedStore->GetItem(Id);
		ASSERT_THAT(IsNotNull( ItemData2 ));
	}

	TEST_METHOD(ArcRemoveItemFromSlotCommand_SameItemsComponent)
	{
		const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Name = *FGuid::NewGuid().ToString();
		AArcItemsTestActor& Actor = Spawner.SpawnActor<AArcItemsTestActor>(SpawnParameters);

		FArcItemSpec Spec;
		Spec.SetItemDefinition(Settings->ItemWithStacksNoStackingPolicy).SetAmount(1)
			.SetItemLevel(1);
		FArcItemId Id = Actor.ItemsStore->AddItem(Spec, FArcItemId::InvalidId);
		Actor.Items->AddOwnedItem(Id);
		
		TSharedPtr<FArcMoveItemToSlotCommand> Command = MakeShareable(
			new FArcMoveItemToSlotCommand(Actor.Items, Actor.Items,
				Id, TAG_TestItemSlot_01));
		Command->Execute();
		
		const FArcItemData* ItemData = Actor.Items->GetItem(Id);
		ASSERT_THAT(IsNotNull( ItemData ));
		const FArcItemData* ItemDataFromSlot = Actor.ItemsStore->GetItemFromSlot(TAG_TestItemSlot_01);
		ASSERT_THAT(IsNotNull( ItemDataFromSlot ));
		
		TSharedPtr<FArcRemoveItemFromSlotCommand> RemoveCommand = MakeShareable(
			new FArcRemoveItemFromSlotCommand(Actor.Items, Actor.Items,TAG_TestItemSlot_01));
		
		RemoveCommand->Execute();
		
		const FArcItemData* ItemDataRevmoedFromSlot = Actor.ItemsStore->GetItemFromSlot(TAG_TestItemSlot_01);
		ASSERT_THAT(IsNull( ItemDataRevmoedFromSlot ));
	}
	
	TEST_METHOD(ArcRemoveItemFromSlotCommand_DifferentItemsComponent)
	{
		const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Name = *FGuid::NewGuid().ToString();
		AArcItemsTestActor& Actor = Spawner.SpawnActor<AArcItemsTestActor>(SpawnParameters);

		FArcItemSpec Spec;
		Spec.SetItemDefinition(Settings->ItemWithStacksNoStackingPolicy).SetAmount(1)
			.SetItemLevel(1);
		FArcItemId Id = Actor.ItemsStore->AddItem(Spec, FArcItemId::InvalidId);
		Actor.Items->AddOwnedItem(Id);
		
		TSharedPtr<FArcMoveItemToSlotCommand> Command = MakeShareable(
			new FArcMoveItemToSlotCommand(Actor.Items, Actor.ItemsSharedStore,
				Id, TAG_TestItemSlot_01));

		Command->Execute();
		
		const FArcItemData* ItemData = Actor.Items->GetItem(Id);
		ASSERT_THAT(IsNull( ItemData ));

		const FArcItemData* ItemData2 = Actor.ItemsSharedStore->GetItem(Id);
		ASSERT_THAT(IsNotNull( ItemData2 ));
		
		const FArcItemData* ItemDataFromSlot = Actor.ItemsStore->GetItemFromSlot(TAG_TestItemSlot_01);
		ASSERT_THAT(IsNotNull( ItemDataFromSlot ));
		
		TSharedPtr<FArcRemoveItemFromSlotCommand> RemoveCommand = MakeShareable(
			new FArcRemoveItemFromSlotCommand(Actor.ItemsSharedStore, Actor.Items,TAG_TestItemSlot_01));
		
		RemoveCommand->Execute();

		const FArcItemData* ItemDataRevmoedFromSlot = Actor.ItemsStore->GetItemFromSlot(TAG_TestItemSlot_01);
		ASSERT_THAT(IsNull( ItemDataRevmoedFromSlot ));
		
		const FArcItemData* ItemDataMovedBack = Actor.Items->GetItem(Id);
		ASSERT_THAT(IsNotNull( ItemDataMovedBack ));

		const FArcItemData* ItemDataRemoved = Actor.ItemsSharedStore->GetItem(Id);
		ASSERT_THAT(IsNull( ItemDataRemoved ));
	}
	
	TEST_METHOD(AddItem_WithOneStat) 
	{
		const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();
		FActorSpawnParameters SpawnParameters;
		AArcItemsTestActor& Actor = Spawner.SpawnActor<AArcItemsTestActor>(SpawnParameters);

		FArcItemSpec OwnerSpec;
		OwnerSpec.SetItemDefinition(Settings->ItemWithStatsA).SetAmount(1).SetItemLevel(1);
		FArcItemId OwnerItemId = Actor.ItemsStore->AddItem(OwnerSpec, FArcItemId::InvalidId);

		const FArcItemData* ItemPtr = Actor.ItemsStore->GetItemPtr(OwnerItemId);
		
		const FArcItemInstance_ItemStats* Instance = ArcItemsHelper::FindInstance<FArcItemInstance_ItemStats>(ItemPtr);
		float StatValue = Instance->GetStatValue(UArcCoreTestAttributeSet::GetTestAttributeAAttribute());
		
		ASSERT_THAT(IsNear( StatValue, 10.f, 0.1f ));
	}

	TEST_METHOD(AddItem_SpecWithFragment)
	{
		const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();
		FActorSpawnParameters SpawnParameters;
		AArcItemsTestActor& Actor = Spawner.SpawnActor<AArcItemsTestActor>(SpawnParameters);

		FArcItemFragment_ItemStats* ItemStats = new FArcItemFragment_ItemStats();
		ItemStats->DefaultStats.Add( { UArcCoreTestAttributeSet::GetTestAttributeAAttribute(), 10.f});
		ItemStats->DefaultStats.Add({ UArcCoreTestAttributeSet::GetTestAttributeBAttribute(), 15.f });

		FArcItemSpec OwnerSpec;
		OwnerSpec.SetItemDefinition(Settings->SimpleBaseItem).SetAmount(1).SetItemLevel(1)
			.AddInstanceData(ItemStats);

		FArcItemId OwnerItemId = Actor.ItemsStore->AddItem(OwnerSpec, FArcItemId::InvalidId);

		const FArcItemData* ItemPtr = Actor.ItemsStore->GetItemPtr(OwnerItemId);

		const FArcItemInstance_ItemStats* Instance = ArcItemsHelper::FindInstance<FArcItemInstance_ItemStats>(ItemPtr);
		float TestStat = Instance->GetStatValue(UArcCoreTestAttributeSet::GetTestAttributeAAttribute());

		ASSERT_THAT(IsNear(TestStat, 10.f, 0.1f));
	}


	TEST_METHOD(DeepCopyItemEntry)
	{
		const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Name = *FGuid::NewGuid().ToString();
		AArcItemsTestActor& Actor = Spawner.SpawnActor<AArcItemsTestActor>(SpawnParameters);

		FArcItemFragment_ItemStats* ItemStats = new FArcItemFragment_ItemStats();
		ItemStats->DefaultStats.Add( { UArcCoreTestAttributeSet::GetTestAttributeAAttribute(), 10.f});
		ItemStats->DefaultStats.Add({ UArcCoreTestAttributeSet::GetTestAttributeBAttribute(), 15.f });

		FArcItemSpec OwnerSpec;
		OwnerSpec.SetItemDefinition(Settings->SimpleBaseItem).SetAmount(1).SetItemLevel(1)
			.AddInstanceData(ItemStats);
		
		FArcItemId Id = Actor.ItemsStore->AddItem(OwnerSpec, FArcItemId::InvalidId);
		Actor.Items->AddOwnedItem(Id);

		const FArcItemData* ItemPtr = Actor.ItemsStore->GetItemPtr(Id);
		ASSERT_THAT(AreEqual( ItemPtr->GetItemId(), Id ));

		TSharedPtr<FArcItemData> Duplicate = FArcItemData::Duplicate(nullptr, nullptr, ItemPtr, true);
		const FArcItemData* DupPtr = Duplicate.Get();

		
		const FArcItemDataInternal* InternalItem = Actor.ItemsStore->GetInternalItem(Id);
		FArcItemCopyContainerHelper Copy = FArcItemCopyContainerHelper::New(*InternalItem);

		if (DupPtr)
		{
			if (ItemPtr)
			{
				
			}
		}
	}
};
#endif

NETWORK_TEST_CLASS(ArcCoreNetworkTests, "ArcCore.Network")
{
	struct DerivedState : public FBasePIEIrisNetworkComponentState
	{
		AArcItemsTestActor* ReplicatedActor = nullptr;
		int32 IndependentNumber = 0;
	};
	
	int32 SharedNumber = 0;
	FArcItemId SharedItemId;
	FPIEIrisNetworkComponent<DerivedState> Network{ TestRunner, TestCommandBuilder, bInitializing };
	
	BEFORE_EACH()
	{
		FAutomationTestBase::bSuppressLogWarnings = true;
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogEOSSDK");
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogEOSFriends");
		
    	FIrisNetworkComponentBuilder<DerivedState>()
			.WithClients(1)
			.AsDedicatedServer()
    		.WithGameInstanceClass(UGameInstance::StaticClass())
    		.WithGameMode(AGameModeBase::StaticClass())
    		.Build(Network);
    }
	
	TEST_METHOD(AddItem_MoveToOtherItemStore)
	{
		//TestRunner->SetSuppressLogWarnings(ECQTestSuppressLogBehavior::True);
		Network.SpawnAndReplicate<AArcItemsTestActor, &DerivedState::ReplicatedActor>(true)
		.ThenServer([&](DerivedState& ServerState) {
			ASSERT_THAT(IsNotNull(ServerState.ReplicatedActor));
			const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();
		
			FArcItemSpec Spec;
			Spec.SetItemDefinition(Settings->SimpleBaseItem).SetAmount(1).SetItemLevel(1);
			FArcItemId Id = ServerState.ReplicatedActor->ItemsStore->AddItem(Spec, FArcItemId::InvalidId);
			SharedItemId = Id;
			ServerState.ReplicatedActor->ReplicatedInt = 42;
		})
		.UntilClients([&](DerivedState& ClientState) -> bool {
			int32 ItemNum = ClientState.ReplicatedActor->ItemsStore->GetItemNum();
			if (ItemNum != 0)
			{
				return true;
			}
			return false;
		})
		.ThenClients([&](DerivedState& ClientState) {
			ASSERT_THAT(IsNotNull(ClientState.ReplicatedActor));
	
			int32 ItemNum = ClientState.ReplicatedActor->ItemsStore->GetItemNum();
			int32 num = ClientState.ReplicatedActor->ReplicatedInt;
			ASSERT_THAT(AreEqual( num, 42 ));
		})
		.ThenServer([&](DerivedState& ServerState)
		{
			
			ServerState.ReplicatedActor->ItemsStore2->MoveItemFrom(SharedItemId, ServerState.ReplicatedActor->ItemsStore);
		});
	}

	TEST_METHOD(AddItem_AttatchItemToOwnerWithSlot_MoveToOtherItemsStore)
	{
		//TestRunner->SetSuppressLogWarnings(ECQTestSuppressLogBehavior::True);
		TSharedRef<FArcItemId> SharedOwnerId = MakeShareable(new FArcItemId);
		Network.SpawnAndReplicate<AArcItemsTestActor, &DerivedState::ReplicatedActor>(true)
		.ThenServer([&, SharedOwnerId](DerivedState& ServerState) {
			ASSERT_THAT(IsNotNull(ServerState.ReplicatedActor));
			const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();
		
			FArcItemSpec Spec;
			Spec.SetItemDefinition(Settings->SimpleBaseItem).SetAmount(1).SetItemLevel(1);
			FArcItemId Id = ServerState.ReplicatedActor->ItemsStore->AddItem(Spec, FArcItemId::InvalidId);
			SharedItemId = Id;
			*SharedOwnerId = Id;
			
			FArcItemId IdToAttach = ServerState.ReplicatedActor->ItemsStore->AddItem(Spec, FArcItemId::InvalidId);
			
			ServerState.ReplicatedActor->ItemsStore->InternalAttachToItem(Id, IdToAttach, FGameplayTag::EmptyTag);
		})
		.UntilClients([&](DerivedState& ClientState) -> bool {
			int32 ItemNum = ClientState.ReplicatedActor->ItemsStore->GetItemNum();
			if (ItemNum > 1)
			{
				return true;
			}
			return false;
		})
		.ThenClients([&, SharedOwnerId](DerivedState& ClientState) {
			ASSERT_THAT(IsNotNull(ClientState.ReplicatedActor));
	
			int32 ItemNum = ClientState.ReplicatedActor->ItemsStore->GetItemNum();
			ASSERT_THAT(AreEqual( ItemNum, 2 ));

			const FArcItemData* OwnerItemPtr = ClientState.ReplicatedActor->ItemsStore->GetItemPtr(SharedItemId);
			ASSERT_THAT(IsNotNull(OwnerItemPtr));

			TArray<const FArcItemData*> AttachedItems = OwnerItemPtr->GetItemsInSockets();
			ASSERT_THAT(AreEqual( AttachedItems.Num(), 1 ));
		}).ThenServer([&](DerivedState& ServerState)
		{
			
			ServerState.ReplicatedActor->ItemsStore2->MoveItemFrom(SharedItemId, ServerState.ReplicatedActor->ItemsStore);
		}).UntilClients([&](DerivedState& ClientState) -> bool {
			int32 ItemNum = ClientState.ReplicatedActor->ItemsStore2->GetItemNum();
			if (ItemNum > 1)
			{
				return true;
			}
			return false;
		}).ThenClients([&, SharedOwnerId](DerivedState& ClientState) {
			ASSERT_THAT(IsNotNull(ClientState.ReplicatedActor));
	
			int32 ItemNum = ClientState.ReplicatedActor->ItemsStore2->GetItemNum();
			ASSERT_THAT(AreEqual( ItemNum, 2 ));

			const FArcItemData* OwnerItemPtr = ClientState.ReplicatedActor->ItemsStore2->GetItemPtr(SharedItemId);
			ASSERT_THAT(IsNotNull(OwnerItemPtr));

			TArray<const FArcItemData*> AttachedItems = OwnerItemPtr->GetItemsInSockets();
			ASSERT_THAT(AreEqual( AttachedItems.Num(), 1 ));
		});
	}
#if 0
	TEST_METHOD(AddItem_ReplicateToClients)
	{
		//TestRunner->SetSuppressLogWarnings(ECQTestSuppressLogBehavior::True);
		Network.SpawnAndReplicate<AArcItemsTestActor, &DerivedState::ReplicatedActor>(true)
		.ThenServer([&](DerivedState& ServerState) {
			ASSERT_THAT(IsNotNull(ServerState.ReplicatedActor));
			const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();
		
			FArcItemSpec Spec;
			Spec.SetItemDefinition(Settings->SimpleBaseItem).SetAmount(1).SetItemLevel(1);
			FArcItemId Id = ServerState.ReplicatedActor->ItemsStore->AddItem(Spec, FArcItemId::InvalidId);
			ServerState.ReplicatedActor->Items->AddOwnedItem(Id);
			ServerState.ReplicatedActor->ReplicatedInt = 42;
		})
		.UntilClients([&](DerivedState& ClientState) -> bool {
			int32 ItemNum = ClientState.ReplicatedActor->ItemsStore->GetItemNum();
			if (ItemNum != 0)
			{
				return true;
			}
			return false;
		})
		.ThenClients([&](DerivedState& ClientState) {
			ASSERT_THAT(IsNotNull(ClientState.ReplicatedActor));
	
			int32 ItemNum = ClientState.ReplicatedActor->ItemsStore->GetItemNum();
			int32 num = ClientState.ReplicatedActor->ReplicatedInt;
			ASSERT_THAT(AreEqual( num, 42 ));
		});
	}

	TEST_METHOD(AddItem_SetOnSlot_ReplicateToClients)
	{
		//TestRunner->SetSuppressLogWarnings(ECQTestSuppressLogBehavior::True);
		Network.SpawnAndReplicate<AArcItemsTestActor, &DerivedState::ReplicatedActor>(true)
		.ThenServer([&](DerivedState& ServerState) {
			ASSERT_THAT(IsNotNull(ServerState.ReplicatedActor));
			const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();
		
			FArcItemSpec Spec;
			Spec.SetItemDefinition(Settings->SimpleBaseItem).SetAmount(1).SetItemLevel(1);
			FArcItemId Id = ServerState.ReplicatedActor->ItemsStore->AddItem(Spec, FArcItemId::InvalidId);
			ServerState.ReplicatedActor->Items->AddOwnedItem(Id);

			ServerState.ReplicatedActor->ItemsStore->AddItemToSlot(Id, TAG_TestItemSlot_01);
			ServerState.ReplicatedActor->ReplicatedInt = 42;
		})
		.UntilClients([&](DerivedState& ClientState) -> bool {
			int32 ItemNum = ClientState.ReplicatedActor->ItemsStore->GetItemNum();
			if (ItemNum != 0)
			{
				return true;
			}
			return false;
		})
		.ThenClients([&](DerivedState& ClientState) {
			ASSERT_THAT(IsNotNull(ClientState.ReplicatedActor));
	
			const FArcItemData* ItemOnSlot = ClientState.ReplicatedActor->ItemsStore->GetItemFromSlot(TAG_TestItemSlot_01);
			ASSERT_THAT(IsNotNull(ItemOnSlot));
		});
	}

	TEST_METHOD(AddItem_SetOnSlot_GrantAbility_ReplicateToClient)
	{
		//TestRunner->SetSuppressLogWarnings(ECQTestSuppressLogBehavior::True);
		Network.SpawnAndReplicate<AArcItemsTestActor, &DerivedState::ReplicatedActor>(true)
		.ThenServer([&](DerivedState& ServerState) {
			ASSERT_THAT(IsNotNull(ServerState.ReplicatedActor));
			const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();
		
			FArcItemSpec Spec;
			Spec.SetItemDefinition(Settings->ItemWithGrantedAbility).SetAmount(1).SetItemLevel(1);
			FArcItemId Id = ServerState.ReplicatedActor->ItemsStore->AddItem(Spec, FArcItemId::InvalidId);
			ServerState.ReplicatedActor->Items->AddOwnedItem(Id);

			ServerState.ReplicatedActor->ItemsStore->AddItemToSlot(Id, TAG_TestItemSlot_01);
			ServerState.ReplicatedActor->ReplicatedInt = 42;
		})
		.UntilClients([&](DerivedState& ClientState) -> bool {
			int32 ItemNum = ClientState.ReplicatedActor->ItemsStore->GetItemNum();
			if (ItemNum != 0)
			{
				return true;
			}
			return false;
		})
		.ThenClients([&](DerivedState& ClientState) {
			ASSERT_THAT(IsNotNull(ClientState.ReplicatedActor));
	
			const FArcItemData* ItemOnSlot = ClientState.ReplicatedActor->ItemsStore->GetItemFromSlot(TAG_TestItemSlot_01);
			ASSERT_THAT(IsNotNull(ItemOnSlot));
			
			const FArcItemInstance_GrantedAbilities* Instance = ArcItemsHelper::FindInstance<FArcItemInstance_GrantedAbilities>(ItemOnSlot);
			ASSERT_THAT(AreEqual( Instance->GetGrantedAbilities().Num(), 1 ));
		});
	}
	
	TEST_METHOD(AddItem_SetOnSlot_GrantAbility_ReplicateToClient_RemoveFromSlot)
	{
		//TestRunner->SetSuppressLogWarnings(ECQTestSuppressLogBehavior::True);
		Network.SpawnAndReplicate<AArcItemsTestActor, &DerivedState::ReplicatedActor>(true)
		.ThenServer([&](DerivedState& ServerState) {
			ASSERT_THAT(IsNotNull(ServerState.ReplicatedActor));
			const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();
		
			FArcItemSpec Spec;
			Spec.SetItemDefinition(Settings->ItemWithGrantedAbility).SetAmount(1).SetItemLevel(1);
			FArcItemId Id = ServerState.ReplicatedActor->ItemsStore->AddItem(Spec, FArcItemId::InvalidId);
			ServerState.ReplicatedActor->Items->AddOwnedItem(Id);

			ServerState.ReplicatedActor->ItemsStore->AddItemToSlot(Id, TAG_TestItemSlot_01);
			ServerState.ReplicatedActor->ReplicatedInt = 42;
		})
		.UntilClients([&](DerivedState& ClientState) -> bool {
			int32 ItemNum = ClientState.ReplicatedActor->ItemsStore->GetItemNum();
			if (ItemNum != 0)
			{
				return true;
			}
			return false;
		})
		.ThenClients([&](DerivedState& ClientState) {
			ASSERT_THAT(IsNotNull(ClientState.ReplicatedActor));
	
			const FArcItemData* ItemOnSlot = ClientState.ReplicatedActor->ItemsStore->GetItemFromSlot(TAG_TestItemSlot_01);
			ASSERT_THAT(IsNotNull(ItemOnSlot));
			
			const FArcItemInstance_GrantedAbilities* Instance = ArcItemsHelper::FindInstance<FArcItemInstance_GrantedAbilities>(ItemOnSlot);
			ASSERT_THAT(AreEqual( Instance->GetGrantedAbilities().Num(), 1 ));
		}).ThenServer([&](DerivedState& ServerState) {
			ASSERT_THAT(IsNotNull(ServerState.ReplicatedActor));
			const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();
		
			FArcItemSpec Spec;
			Spec.SetItemDefinition(Settings->ItemWithGrantedAbility).SetAmount(1).SetItemLevel(1);
			TArray<const FArcItemData*> Items = ServerState.ReplicatedActor->ItemsStore->GetAllItemsOnSlots();
			ServerState.ReplicatedActor->ItemsStore->RemoveItemFromSlot(Items[0]->GetItemId());
		}).UntilClients([&](DerivedState& ClientState) -> bool {
				TArray<const FArcItemData*> Items = ClientState.ReplicatedActor->ItemsStore->GetAllItemsOnSlots();
				if (Items.Num() == 0)
				{
					return true;
				}
				return false;
		}).ThenClients([&](DerivedState& ClientState) {
			ASSERT_THAT(IsNotNull(ClientState.ReplicatedActor));
	
			TArray<const FArcItemData*> Items = ClientState.ReplicatedActor->ItemsStore->GetItems();
			ASSERT_THAT(IsNotNull(Items[0]));
				
			const FArcItemInstance_GrantedAbilities* Instance = ArcItemsHelper::FindInstance<FArcItemInstance_GrantedAbilities>(Items[0]);
			ASSERT_THAT(AreEqual( Instance->GetGrantedAbilities().Num(), 0 ));
		});
	}

	TEST_METHOD(AddItem_AttatchItemToOwnerWithSlot_ReplicateToClients)
	{
		//TestRunner->SetSuppressLogWarnings(ECQTestSuppressLogBehavior::True);
		TSharedRef<FArcItemId> SharedOwnerId = MakeShareable(new FArcItemId);
		Network.SpawnAndReplicate<AArcItemsTestActor, &DerivedState::ReplicatedActor>(true)
		.ThenServer([&, SharedOwnerId](DerivedState& ServerState) {
			ASSERT_THAT(IsNotNull(ServerState.ReplicatedActor));
			const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();
		
			FArcItemSpec Spec;
			Spec.SetItemDefinition(Settings->SimpleBaseItem).SetAmount(1).SetItemLevel(1);
			FArcItemId Id = ServerState.ReplicatedActor->ItemsStore->AddItem(Spec, FArcItemId::InvalidId);
			ServerState.ReplicatedActor->Items->AddOwnedItem(Id);
			*SharedOwnerId = Id;
			
			FArcItemId IdToAttach = ServerState.ReplicatedActor->ItemsStore->AddItem(Spec, FArcItemId::InvalidId);
			ServerState.ReplicatedActor->Items->AddOwnedItem(Id);

			ServerState.ReplicatedActor->ItemsStore->InternalAttachToItem(Id, IdToAttach, FGameplayTag::EmptyTag);
		})
		.UntilClients([&](DerivedState& ClientState) -> bool {
			int32 ItemNum = ClientState.ReplicatedActor->ItemsStore->GetItemNum();
			if (ItemNum > 1)
			{
				return true;
			}
			return false;
		})
		.ThenClients([&, SharedOwnerId](DerivedState& ClientState) {
			ASSERT_THAT(IsNotNull(ClientState.ReplicatedActor));
	
			int32 ItemNum = ClientState.ReplicatedActor->ItemsStore->GetItemNum();
			ASSERT_THAT(AreEqual( ItemNum, 2 ));

			const FArcItemData* OwnerItemPtr = ClientState.ReplicatedActor->ItemsStore->GetItemPtr(SharedOwnerId.Get());
			ASSERT_THAT(IsNotNull(OwnerItemPtr));

			TArray<const FArcItemData*> AttachedItems = OwnerItemPtr->GetItemsInSockets();
			ASSERT_THAT(AreEqual( AttachedItems.Num(), 1 ));
		});
	}

	TEST_METHOD(AddItem_AttatchItemToOwnerWithSlot_AttachmentGrantAbility_ReplicateToClients)
	{
		//TestRunner->SetSuppressLogWarnings(ECQTestSuppressLogBehavior::True);
		TSharedRef<FArcItemId> SharedOwnerId = MakeShareable(new FArcItemId);
		TSharedRef<FArcItemId> SharedAttachedId = MakeShareable(new FArcItemId);
		Network.SpawnAndReplicate<AArcItemsTestActor, &DerivedState::ReplicatedActor>(true)
		.ThenServer([&, SharedOwnerId, SharedAttachedId](DerivedState& ServerState) {
			ASSERT_THAT(IsNotNull(ServerState.ReplicatedActor));
			const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();

			FArcItemSpec Spec;
			Spec.SetItemDefinition(Settings->ItemWithAttachmentSlots).SetAmount(1).SetItemLevel(1);
			FArcItemId Id = ServerState.ReplicatedActor->ItemsStore->AddItem(Spec, FArcItemId::InvalidId);
			ServerState.ReplicatedActor->Items->AddOwnedItem(Id);
			*SharedOwnerId = Id;
			ServerState.ReplicatedActor->ItemsStore->AddItemToSlot(Id, TAG_TestItemSlot_01);
			
			FArcItemSpec AttachSpec;
			AttachSpec.SetItemDefinition(Settings->ItemWithGrantedAbility).SetAmount(1).SetItemLevel(1);
			FArcItemId IdToAttach = ServerState.ReplicatedActor->ItemsStore->AddItem(AttachSpec, FArcItemId::InvalidId);
			*SharedAttachedId = IdToAttach;
			
			ServerState.ReplicatedActor->Items->AddOwnedItem(IdToAttach);

			ServerState.ReplicatedActor->ItemsStore->InternalAttachToItem(Id, IdToAttach, TAG_TestItemSlot_02);
		})
		.UntilClients([&](DerivedState& ClientState) -> bool {
			int32 ItemNum = ClientState.ReplicatedActor->ItemsStore->GetItemNum();
			if (ItemNum > 1)
			{
				return true;
			}
			return false;
		})
		.ThenClients([&, SharedOwnerId, SharedAttachedId](DerivedState& ClientState) {
			ASSERT_THAT(IsNotNull(ClientState.ReplicatedActor));
	
			int32 ItemNum = ClientState.ReplicatedActor->ItemsStore->GetItemNum();
			ASSERT_THAT(AreEqual( ItemNum, 2 ));

			const FArcItemData* OwnerItemPtr = ClientState.ReplicatedActor->ItemsStore->GetItemPtr(SharedOwnerId.Get());
			ASSERT_THAT(IsNotNull(OwnerItemPtr));

			TArray<const FArcItemData*> AttachedItems = OwnerItemPtr->GetItemsInSockets();
			ASSERT_THAT(AreEqual( AttachedItems.Num(), 1 ))
			
			const FArcItemData* AttachedItemPtr = ClientState.ReplicatedActor->ItemsStore->GetItemPtr(SharedAttachedId.Get());
			const FArcItemInstance_GrantedAbilities* Instance = ArcItemsHelper::FindInstance<FArcItemInstance_GrantedAbilities>(AttachedItemPtr);
			ASSERT_THAT(AreEqual( Instance->GetGrantedAbilities().Num(), 1 ));
		});
	}

	TEST_METHOD(AddItem_AttatchItemToOwner_AttachmentGrantAbility_AttachToSlot_ReplicateToClients)
	{
		//TestRunner->SetSuppressLogWarnings(ECQTestSuppressLogBehavior::True);
		TSharedRef<FArcItemId> SharedOwnerId = MakeShareable(new FArcItemId);
		TSharedRef<FArcItemId> SharedAttachedId = MakeShareable(new FArcItemId);
		Network.SpawnAndReplicate<AArcItemsTestActor, &DerivedState::ReplicatedActor>(true)
		.ThenServer([&, SharedOwnerId, SharedAttachedId](DerivedState& ServerState) {
			ASSERT_THAT(IsNotNull(ServerState.ReplicatedActor));
			const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();

			FArcItemSpec Spec;
			Spec.SetItemDefinition(Settings->ItemWithAttachmentSlots).SetAmount(1).SetItemLevel(1);
			FArcItemId Id = ServerState.ReplicatedActor->ItemsStore->AddItem(Spec, FArcItemId::InvalidId);
			ServerState.ReplicatedActor->Items->AddOwnedItem(Id);
			*SharedOwnerId = Id;
			
			FArcItemSpec AttachSpec;
			AttachSpec.SetItemDefinition(Settings->ItemWithGrantedAbility).SetAmount(1).SetItemLevel(1);
			FArcItemId IdToAttach = ServerState.ReplicatedActor->ItemsStore->AddItem(AttachSpec, FArcItemId::InvalidId);
			*SharedAttachedId = IdToAttach;
			
			ServerState.ReplicatedActor->Items->AddOwnedItem(IdToAttach);
			ServerState.ReplicatedActor->ItemsStore->InternalAttachToItem(Id, IdToAttach, TAG_TestItemSlot_02);

			ServerState.ReplicatedActor->ItemsStore->AddItemToSlot(Id, TAG_TestItemSlot_01);
		})
		.UntilClients([&](DerivedState& ClientState) -> bool {
			int32 ItemNum = ClientState.ReplicatedActor->ItemsStore->GetItemNum();
			if (ItemNum > 1)
			{
				return true;
			}
			return false;
		})
		.ThenClients([&, SharedOwnerId, SharedAttachedId](DerivedState& ClientState) {
			ASSERT_THAT(IsNotNull(ClientState.ReplicatedActor));
	
			int32 ItemNum = ClientState.ReplicatedActor->ItemsStore->GetItemNum();
			ASSERT_THAT(AreEqual( ItemNum, 2 ));

			const FArcItemData* OwnerItemPtr = ClientState.ReplicatedActor->ItemsStore->GetItemPtr(SharedOwnerId.Get());
			ASSERT_THAT(IsNotNull(OwnerItemPtr));

			TArray<const FArcItemData*> AttachedItems = OwnerItemPtr->GetItemsInSockets();
			ASSERT_THAT(AreEqual( AttachedItems.Num(), 1 ))
			
			const FArcItemData* AttachedItemPtr = ClientState.ReplicatedActor->ItemsStore->GetItemPtr(SharedAttachedId.Get());
			const FArcItemInstance_GrantedAbilities* Instance = ArcItemsHelper::FindInstance<FArcItemInstance_GrantedAbilities>(AttachedItemPtr);
			ASSERT_THAT(AreEqual( Instance->GetGrantedAbilities().Num(), 1 ));
		});
	}

	TEST_METHOD(AddItem_WithAttachments_AddToSlot_ReplicateToClient)
	{
		//TestRunner->SetSuppressLogWarnings(ECQTestSuppressLogBehavior::True);
		TSharedRef<FArcItemId> SharedOwnerId = MakeShareable(new FArcItemId);
		Network.SpawnAndReplicate<AArcItemsTestActor, &DerivedState::ReplicatedActor>(true)
		.ThenServer([&, SharedOwnerId](DerivedState& ServerState) {
			ASSERT_THAT(IsNotNull(ServerState.ReplicatedActor));
			const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();
		
			FArcItemSpec Spec;
			Spec.SetItemDefinition(Settings->ItemWithDefaultAttachment).SetAmount(1).SetItemLevel(1);
			FArcItemId Id = ServerState.ReplicatedActor->ItemsStore->AddItem(Spec, FArcItemId::InvalidId);
			ServerState.ReplicatedActor->Items->AddOwnedItem(Id);
			*SharedOwnerId = Id;
		})
		.UntilClients([&](DerivedState& ClientState) -> bool {
			int32 ItemNum = ClientState.ReplicatedActor->ItemsStore->GetItemNum();
			if (ItemNum > 1)
			{
				return true;
			}
			return false;
		})
		.ThenClients([&, SharedOwnerId](DerivedState& ClientState) {
			ASSERT_THAT(IsNotNull(ClientState.ReplicatedActor));
	
			int32 ItemNum = ClientState.ReplicatedActor->ItemsStore->GetItemNum();
			ASSERT_THAT(AreEqual( ItemNum, 2 ));

			const FArcItemData* OwnerItemPtr = ClientState.ReplicatedActor->ItemsStore->GetItemPtr(SharedOwnerId.Get());
			ASSERT_THAT(IsNotNull(OwnerItemPtr));

			TArray<const FArcItemData*> AttachedItems = OwnerItemPtr->GetItemsInSockets();
			ASSERT_THAT(AreEqual( AttachedItems.Num(), 1 ));
		});
	}

	TEST_METHOD(AddItem_AttachItem_WithEffectsToApply_ReplicateToClient)
	{
		TSharedRef<FArcItemId> SharedOwnerId = MakeShareable(new FArcItemId);
		Network.SpawnAndReplicate<AArcItemsTestActor, &DerivedState::ReplicatedActor>(true)
		.ThenServer([&, SharedOwnerId](DerivedState& ServerState) {
			ASSERT_THAT(IsNotNull(ServerState.ReplicatedActor));
			const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();
		
			FActorSpawnParameters SpawnParameters;

			FArcItemSpec OwnerSpec;
			OwnerSpec.SetItemDefinition(Settings->ItemWithAttachmentSlots).SetAmount(1).SetItemLevel(1);
			FArcItemId OwnerItemId = ServerState.ReplicatedActor->ItemsStore->AddItem(OwnerSpec, FArcItemId::InvalidId);
			*SharedOwnerId = OwnerItemId;
			const FArcItemData* ItemPtr = ServerState.ReplicatedActor->ItemsStore->GetItemPtr(OwnerItemId);

			FArcItemSpec AttachedSpec;
			AttachedSpec.SetItemDefinition(Settings->ItemWithEffectToApply).SetAmount(1).SetItemLevel(1);
			FArcItemId AttachedItemId = ServerState.ReplicatedActor->ItemsStore->AddItem(AttachedSpec, FArcItemId::InvalidId);

			ServerState.ReplicatedActor->ItemsStore->InternalAttachToItem(OwnerItemId, AttachedItemId, FGameplayTag::EmptyTag);
		
			const FArcItemInstance_EffectToApply* Instance = ArcItemsHelper::FindInstance<FArcItemInstance_EffectToApply>(ItemPtr);
			TArray<const FArcEffectSpecItem*> Specs = Instance->GetEffectSpecHandles(TAG_TestEffectSpec_01);
		
			ASSERT_THAT(AreEqual( Specs.Num(), 1 ));
		})
		.UntilClients([&](DerivedState& ClientState) -> bool {
			int32 ItemNum = ClientState.ReplicatedActor->ItemsStore->GetItemNum();
			if (ItemNum > 1)
			{
				return true;
			}
			return false;
		})
		.ThenClients([&, SharedOwnerId](DerivedState& ClientState) {
			ASSERT_THAT(IsNotNull(ClientState.ReplicatedActor));
			
			const FArcItemData* ItemPtr = ClientState.ReplicatedActor->ItemsStore->GetItemPtr(SharedOwnerId.Get());
			const FArcItemInstance_EffectToApply* Instance = ArcItemsHelper::FindInstance<FArcItemInstance_EffectToApply>(ItemPtr);
			TArray<const FArcEffectSpecItem*> Specs = Instance->GetEffectSpecHandles(TAG_TestEffectSpec_01);
		
			ASSERT_THAT(AreEqual( Specs.Num(), 1 ));
		});
	}
	
	TEST_METHOD(AddItem_WithStat_ReplicateToClients)
	{
		TSharedRef<FArcItemId> SharedOwnerId = MakeShareable(new FArcItemId);
		Network.SpawnAndReplicate<AArcItemsTestActor, &DerivedState::ReplicatedActor>(true)
		.ThenServer([&, SharedOwnerId](DerivedState& ServerState) {
			ASSERT_THAT(IsNotNull(ServerState.ReplicatedActor));
			const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();
		
			FArcItemSpec Spec;
			Spec.SetItemDefinition(Settings->ItemWithStatsA).SetAmount(1).SetItemLevel(1);
			FArcItemId Id = ServerState.ReplicatedActor->ItemsStore->AddItem(Spec, FArcItemId::InvalidId);

			*SharedOwnerId = Id;
			ServerState.ReplicatedActor->Items->AddOwnedItem(Id);
		})
		.UntilClients([&, SharedOwnerId](DerivedState& ClientState) -> bool {
			int32 ItemNum = ClientState.ReplicatedActor->ItemsStore->GetItemNum();
			if (ItemNum != 0)
			{
				return true;
			}
			return false;
		})
		.ThenClients([&, SharedOwnerId](DerivedState& ClientState) {
			ASSERT_THAT(IsNotNull(ClientState.ReplicatedActor));
	
			int32 ItemNum = ClientState.ReplicatedActor->ItemsStore->GetItemNum();
			const FArcItemData* ItemPtr = ClientState.ReplicatedActor->ItemsStore->GetItemPtr(*SharedOwnerId);
			const FArcItemInstance_ItemStats* Instance = ArcItemsHelper::FindInstance<FArcItemInstance_ItemStats>(ItemPtr);
			float StatValue = Instance->GetStatValue(UArcCoreTestAttributeSet::GetTestAttributeAAttribute());
			ASSERT_THAT(IsNear( StatValue, 10.f, 0.1f ));
		});
	}

	TEST_METHOD(AddItem_SpecWithFragment_ReplicateToClients)
	{
		TSharedRef<FArcItemId> SharedOwnerId = MakeShareable(new FArcItemId);
		Network.SpawnAndReplicate<AArcItemsTestActor, &DerivedState::ReplicatedActor>(true)
			.ThenServer([&, SharedOwnerId](DerivedState& ServerState) {
				ASSERT_THAT(IsNotNull(ServerState.ReplicatedActor));
				const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();

				FArcItemFragment_ItemStats* ItemStats = new FArcItemFragment_ItemStats();
				ItemStats->DefaultStats.Add({ UArcCoreTestAttributeSet::GetTestAttributeAAttribute(), 10.f });
				ItemStats->DefaultStats.Add({ UArcCoreTestAttributeSet::GetTestAttributeBAttribute(), 15.f });

				FArcItemSpec Spec;
				Spec.SetItemDefinition(Settings->SimpleBaseItem).SetAmount(1).SetItemLevel(1).AddInstanceData(ItemStats);
				FArcItemId Id = ServerState.ReplicatedActor->ItemsStore->AddItem(Spec, FArcItemId::InvalidId);

				*SharedOwnerId = Id;
				ServerState.ReplicatedActor->Items->AddOwnedItem(Id);
			})
			.UntilClients([&, SharedOwnerId](DerivedState& ClientState) -> bool {
				int32 ItemNum = ClientState.ReplicatedActor->ItemsStore->GetItemNum();
				if (ItemNum != 0)
				{
					return true;
				}
				return false;
			})
			.ThenClients([&, SharedOwnerId](DerivedState& ClientState) {
				ASSERT_THAT(IsNotNull(ClientState.ReplicatedActor));

				int32 ItemNum = ClientState.ReplicatedActor->ItemsStore->GetItemNum();
				const FArcItemData* ItemPtr = ClientState.ReplicatedActor->ItemsStore->GetItemPtr(*SharedOwnerId);
				const FArcItemFragment_ItemStats* ItemFragment = ArcItemsHelper::FindFragment<FArcItemFragment_ItemStats>(ItemPtr);
				const FArcItemInstance_ItemStats* Instance = ArcItemsHelper::FindInstance<FArcItemInstance_ItemStats>(ItemPtr);
				float StatValue = Instance->GetStatValue(UArcCoreTestAttributeSet::GetTestAttributeAAttribute());
				ASSERT_THAT(IsNotNull(ItemFragment));

				//ASSERT_THAT(AreEqual(StatValue, 10));
			});
	}
#endif
};

