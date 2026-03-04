// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "Storage/ArcPersistenceKeyConvention.h"

TEST_CLASS(ArcKeyConvention_Parse, "ArcPersistence.KeyConvention.Parse")
{
    TEST_METHOD(WorldKey_ParsesCorrectly)
    {
        auto Parsed = UE::ArcPersistence::ParseKey(TEXT("world/ABC-123/actors/npc_01"));
        ASSERT_THAT(IsTrue(Parsed.bValid));
        ASSERT_THAT(AreEqual(UE::ArcPersistence::EKeyCategory::World, Parsed.Category));
        ASSERT_THAT(AreEqual(FString(TEXT("ABC-123")), Parsed.OwnerId));
        ASSERT_THAT(AreEqual(FString(TEXT("actors/npc_01")), Parsed.SubKey));
    }

    TEST_METHOD(PlayerKey_ParsesCorrectly)
    {
        auto Parsed = UE::ArcPersistence::ParseKey(TEXT("players/DEF-456/inventory"));
        ASSERT_THAT(IsTrue(Parsed.bValid));
        ASSERT_THAT(AreEqual(UE::ArcPersistence::EKeyCategory::Player, Parsed.Category));
        ASSERT_THAT(AreEqual(FString(TEXT("DEF-456")), Parsed.OwnerId));
        ASSERT_THAT(AreEqual(FString(TEXT("inventory")), Parsed.SubKey));
    }

    TEST_METHOD(WorldCellKey_ParsesCorrectly)
    {
        auto Parsed = UE::ArcPersistence::ParseKey(TEXT("world/GUID-1/cells/3_5"));
        ASSERT_THAT(IsTrue(Parsed.bValid));
        ASSERT_THAT(AreEqual(UE::ArcPersistence::EKeyCategory::World, Parsed.Category));
        ASSERT_THAT(AreEqual(FString(TEXT("GUID-1")), Parsed.OwnerId));
        ASSERT_THAT(AreEqual(FString(TEXT("cells/3_5")), Parsed.SubKey));
    }

    TEST_METHOD(UnknownPrefix_Invalid)
    {
        auto Parsed = UE::ArcPersistence::ParseKey(TEXT("other/something"));
        ASSERT_THAT(IsFalse(Parsed.bValid));
        ASSERT_THAT(AreEqual(UE::ArcPersistence::EKeyCategory::Unknown, Parsed.Category));
    }

    TEST_METHOD(EmptyKey_Invalid)
    {
        auto Parsed = UE::ArcPersistence::ParseKey(TEXT(""));
        ASSERT_THAT(IsFalse(Parsed.bValid));
    }

    TEST_METHOD(WorldWithNoSubKey_Invalid)
    {
        auto Parsed = UE::ArcPersistence::ParseKey(TEXT("world/ABC-123"));
        ASSERT_THAT(IsFalse(Parsed.bValid));
    }

    TEST_METHOD(PlayersWithNoSubKey_Invalid)
    {
        auto Parsed = UE::ArcPersistence::ParseKey(TEXT("players/ABC-123"));
        ASSERT_THAT(IsFalse(Parsed.bValid));
    }
};

TEST_CLASS(ArcKeyConvention_Validate, "ArcPersistence.KeyConvention.Validate")
{
    TEST_METHOD(ValidWorldKey_ReturnsTrue)
    {
        ASSERT_THAT(IsTrue(UE::ArcPersistence::ValidateKey(TEXT("world/id/sub"))));
    }

    TEST_METHOD(ValidPlayerKey_ReturnsTrue)
    {
        ASSERT_THAT(IsTrue(UE::ArcPersistence::ValidateKey(TEXT("players/id/domain"))));
    }

    TEST_METHOD(InvalidKey_ReturnsFalse)
    {
        ASSERT_THAT(IsFalse(UE::ArcPersistence::ValidateKey(TEXT("garbage"))));
    }
};

TEST_CLASS(ArcKeyConvention_Builders, "ArcPersistence.KeyConvention.Builders")
{
    TEST_METHOD(MakeWorldKey_FormatsCorrectly)
    {
        FString Key = UE::ArcPersistence::MakeWorldKey(TEXT("WORLD-1"), TEXT("actors/npc"));
        ASSERT_THAT(AreEqual(FString(TEXT("world/WORLD-1/actors/npc")), Key));
    }

    TEST_METHOD(MakePlayerKey_FormatsCorrectly)
    {
        FString Key = UE::ArcPersistence::MakePlayerKey(TEXT("PLAYER-1"), TEXT("inventory"));
        ASSERT_THAT(AreEqual(FString(TEXT("players/PLAYER-1/inventory")), Key));
    }

    TEST_METHOD(BuiltKeys_ParseBackCorrectly)
    {
        FString WorldKey = UE::ArcPersistence::MakeWorldKey(TEXT("W1"), TEXT("cells/0_0"));
        auto Parsed = UE::ArcPersistence::ParseKey(WorldKey);
        ASSERT_THAT(IsTrue(Parsed.bValid));
        ASSERT_THAT(AreEqual(UE::ArcPersistence::EKeyCategory::World, Parsed.Category));
        ASSERT_THAT(AreEqual(FString(TEXT("W1")), Parsed.OwnerId));
        ASSERT_THAT(AreEqual(FString(TEXT("cells/0_0")), Parsed.SubKey));

        FString PlayerKey = UE::ArcPersistence::MakePlayerKey(TEXT("P1"), TEXT("stats"));
        auto Parsed2 = UE::ArcPersistence::ParseKey(PlayerKey);
        ASSERT_THAT(IsTrue(Parsed2.bValid));
        ASSERT_THAT(AreEqual(UE::ArcPersistence::EKeyCategory::Player, Parsed2.Category));
        ASSERT_THAT(AreEqual(FString(TEXT("P1")), Parsed2.OwnerId));
        ASSERT_THAT(AreEqual(FString(TEXT("stats")), Parsed2.SubKey));
    }
};
