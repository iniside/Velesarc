// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "Display/ArcTagDisplayRegistry.h"
#include "GameplayTagsManager.h"

// ---------------------------------------------------------------------------
// Tag helpers — native tags registered on first use (safe to call repeatedly).
// Hierarchy: Test.TagDisp.Root  →  Test.TagDisp.Root.Mid  →  Test.TagDisp.Root.Mid.Leaf
// "Unrelated" is a sibling root with no entry in the registries under test.
// ---------------------------------------------------------------------------
namespace ArcTagDisplayRegistryTestHelpers
{
	static FGameplayTag TagRoot()
	{
		return UGameplayTagsManager::Get().AddNativeGameplayTag(TEXT("Test.TagDisp.Root"));
	}

	static FGameplayTag TagMid()
	{
		return UGameplayTagsManager::Get().AddNativeGameplayTag(TEXT("Test.TagDisp.Root.Mid"));
	}

	static FGameplayTag TagLeaf()
	{
		return UGameplayTagsManager::Get().AddNativeGameplayTag(TEXT("Test.TagDisp.Root.Mid.Leaf"));
	}

	static FGameplayTag TagUnrelated()
	{
		return UGameplayTagsManager::Get().AddNativeGameplayTag(TEXT("Test.TagDisp.Unrelated"));
	}

	static UArcTagDisplayRegistry* MakeRegistry()
	{
		return NewObject<UArcTagDisplayRegistry>(GetTransientPackage(), NAME_None, RF_Transient);
	}

	static FArcTagDisplayData MakeEntry(const FString& Name)
	{
		FArcTagDisplayData Data;
		Data.DisplayName = FText::FromString(Name);
		return Data;
	}
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST_CLASS(ArcTagDisplayRegistry_FindDisplay, "ArcCore.Display.TagDisplayRegistry")
{
	TEST_METHOD(ExactMatch_ReturnsEntryWithCorrectDisplayName)
	{
		UArcTagDisplayRegistry* Registry = ArcTagDisplayRegistryTestHelpers::MakeRegistry();
		FGameplayTag Leaf = ArcTagDisplayRegistryTestHelpers::TagLeaf();
		Registry->Entries.Add(Leaf, ArcTagDisplayRegistryTestHelpers::MakeEntry(TEXT("LeafEntry")));

		const FArcTagDisplayData* Result = Registry->FindDisplay(Leaf);

		ASSERT_THAT(IsNotNull(Result, TEXT("FindDisplay on exact-match tag must return non-null")));
		ASSERT_THAT(IsTrue(Result->DisplayName.ToString() == TEXT("LeafEntry"),
			TEXT("DisplayName must match what was stored in the entry")));
	}

	TEST_METHOD(ParentFallback_ReturnsAncestorEntry)
	{
		UArcTagDisplayRegistry* Registry = ArcTagDisplayRegistryTestHelpers::MakeRegistry();
		FGameplayTag Mid = ArcTagDisplayRegistryTestHelpers::TagMid();
		FGameplayTag Leaf = ArcTagDisplayRegistryTestHelpers::TagLeaf();
		// Only the mid-level tag has an entry; querying the leaf should walk up and find it.
		Registry->Entries.Add(Mid, ArcTagDisplayRegistryTestHelpers::MakeEntry(TEXT("MidEntry")));

		const FArcTagDisplayData* Result = Registry->FindDisplay(Leaf);

		ASSERT_THAT(IsNotNull(Result, TEXT("FindDisplay must walk up and find the ancestor entry for Leaf")));
		ASSERT_THAT(IsTrue(Result->DisplayName.ToString() == TEXT("MidEntry"),
			TEXT("Returned entry must be the Mid-level ancestor entry")));
	}

	TEST_METHOD(UnknownTag_ReturnsNullptr)
	{
		UArcTagDisplayRegistry* Registry = ArcTagDisplayRegistryTestHelpers::MakeRegistry();
		// Add one entry that is unrelated to the queried tag.
		FGameplayTag Root = ArcTagDisplayRegistryTestHelpers::TagRoot();
		FGameplayTag Unrelated = ArcTagDisplayRegistryTestHelpers::TagUnrelated();
		Registry->Entries.Add(Root, ArcTagDisplayRegistryTestHelpers::MakeEntry(TEXT("RootEntry")));

		const FArcTagDisplayData* Result = Registry->FindDisplay(Unrelated);

		ASSERT_THAT(IsNull(Result, TEXT("FindDisplay must return nullptr for a tag whose hierarchy has no matching entry")));
	}

	TEST_METHOD(EmptyRegistry_ReturnsNullptr)
	{
		UArcTagDisplayRegistry* Registry = ArcTagDisplayRegistryTestHelpers::MakeRegistry();
		FGameplayTag Leaf = ArcTagDisplayRegistryTestHelpers::TagLeaf();

		const FArcTagDisplayData* Result = Registry->FindDisplay(Leaf);

		ASSERT_THAT(IsNull(Result, TEXT("FindDisplay must return nullptr when the registry has no entries")));
	}
};
