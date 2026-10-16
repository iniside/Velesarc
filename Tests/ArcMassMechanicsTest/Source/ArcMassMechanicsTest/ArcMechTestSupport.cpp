// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMechTestSupport.h"

#include "Effects/ArcEffectSpec.h"
#include "Fragments/ArcEffectStackFragment.h"
#include "Fragments/ArcOwnedTagsFragment.h"
#include "Logging/LogCategory.h"
#include "Logging/LogSuppressionInterface.h"
#include "Misc/AutomationTest.h"
#include "Mass/EntityManager.h"
#include "Mass/EntityView.h"
#include "Misc/Paths.h"

FString ArcMechTest::MakeTestAssetPath(const TCHAR* MountRoot, const TCHAR* Path)
{
	return FString::Printf(TEXT("/%s/%s.%s"), MountRoot, Path, *FPaths::GetBaseFilename(Path));
}

void ArcMechTest::ReportMissingTestAsset(const FString& ObjectPath, const UClass* ExpectedClass)
{
	const FString Message = FString::Printf(TEXT("Test asset %s is missing or is not a %s (docs/reference/mechanics-test-assets.md)"), *ObjectPath, *GetNameSafe(ExpectedClass));
	if (FAutomationTestBase* CurrentTest = FAutomationTestFramework::Get().GetCurrentTest())
	{
		CurrentTest->AddError(Message);
		return;
	}
	UE_LOGF(LogTemp, Error, "%ls", *Message);
}

int32 ArcMechTest::GetOwnedTagCount(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const FGameplayTag& Tag)
{
	const FArcOwnedTagsFragment* OwnedTags = FMassEntityView(EntityManager, Entity).GetSparseFragmentDataPtr<FArcOwnedTagsFragment>();
	return OwnedTags ? OwnedTags->GetTags().GetCount(Tag) : -1;
}

int32 ArcMechTest::CountActiveEffects(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UArcEffectDefinition* Definition)
{
	const FArcEffectStackFragment* Stack = FMassEntityView(EntityManager, Entity).GetFragmentDataPtr<FArcEffectStackFragment>();
	if (!Stack)
	{
		return -1;
	}
	int32 Count = 0;
	for (const FArcActiveEffect& Active : Stack->ActiveEffects)
	{
		const FArcEffectSpec* Spec = Active.Spec.GetPtr<FArcEffectSpec>();
		if (Spec && Spec->Definition == Definition)
		{
			++Count;
		}
	}
	return Count;
}

ArcMechTest::FVerboseLogScope::FVerboseLogScope(const TCHAR* CategoryName)
{
	// The mechanics log categories are not exported from their modules, so the test modules find them by name.
	Category = const_cast<FLogCategoryBase*>(FLogSuppressionInterface::Get().FindCategory(FName(CategoryName)));
	if (Category)
	{
		PreviousVerbosity = Category->GetVerbosity();
		Category->SetVerbosity(ELogVerbosity::Verbose);
	}
}

ArcMechTest::FVerboseLogScope::~FVerboseLogScope()
{
	if (Category)
	{
		Category->SetVerbosity(PreviousVerbosity);
	}
}
