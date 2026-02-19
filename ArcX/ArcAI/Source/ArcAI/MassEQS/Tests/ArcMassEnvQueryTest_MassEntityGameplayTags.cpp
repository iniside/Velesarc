// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassEnvQueryTest_MassEntityGameplayTags.h"

#include "ArcMassEQSTypes.h"
#include "MassEQSSubsystem.h"
#include "MassEQSUtils.h"
#include "MassExecutionContext.h"
#include "ArcMass/ArcMassGameplayTagContainerFragment.h"
#include "Items/EnvQueryItemType_MassEntityHandle.h"


UArcMassEnvQueryTest_MassEntityGameplayTags::UArcMassEnvQueryTest_MassEntityGameplayTags(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Cost = EEnvTestCost::Low;
	TestPurpose = EEnvTestPurpose::Type::Filter;
	FilterType = EEnvTestFilterType::Type::Match;

	ValidItemType = UEnvQueryItemType_MassEntityHandle::StaticClass();

	SetWorkOnFloatValues(false);

}


TUniquePtr<FMassEQSRequestData> UArcMassEnvQueryTest_MassEntityGameplayTags::GetRequestData(FEnvQueryInstance& QueryInstance) const
{
	return MakeUnique<FMassEQSRequestData_MassEntityGameplayTags>(HasTags, MustNotHaveTags);
}

bool UArcMassEnvQueryTest_MassEntityGameplayTags::TryAcquireResults(FEnvQueryInstance& QueryInstance) const
{
	check(MassEQSRequestHandler.MassEQSSubsystem)
	TUniquePtr<FMassEQSRequestData> RawRequestData = MassEQSRequestHandler.MassEQSSubsystem->TryAcquireResults(MassEQSRequestHandler.RequestHandle);
	FMassEnvQueryResultData_MassEntityGameplayTags* RequestData = FMassEQSUtils::TryAndEnsureCast<FMassEnvQueryResultData_MassEntityGameplayTags>(RawRequestData);
	if (!RequestData)
	{
		return false;
	}

	FEnvQueryInstance::ItemIterator It(this, QueryInstance);
	for (It.IgnoreTimeLimit(); It; ++It)
	{
		const FMassEnvQueryEntityInfo& EntityInfo = FMassEQSUtils::GetItemAsEntityInfo(QueryInstance, It.GetIndex());

		const bool bSuccess = RequestData->EntityHandles.Contains(EntityInfo.EntityHandle);
		It.SetScore(TestPurpose, FilterType, bSuccess, true);
	}

	return true;
}


UArcMassEnvQueryTestProcessor_MassEntityGameplayTags::UArcMassEnvQueryTestProcessor_MassEntityGameplayTags()
	: EntityQuery(*this)
{
	CorrespondingRequestClass = UArcMassEnvQueryTest_MassEntityGameplayTags::StaticClass();
}

void UArcMassEnvQueryTestProcessor_MassEntityGameplayTags::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ProcessorRequirements.AddSubsystemRequirement<UMassEQSSubsystem>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FArcMassGameplayTagContainerFragment>(EMassFragmentAccess::ReadWrite);
}

void UArcMassEnvQueryTestProcessor_MassEntityGameplayTags::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& ExecutionContext)
{
	const UWorld* World = GetWorld();
	check(World);

	UMassEQSSubsystem* MassEQSSubsystem = ExecutionContext.GetMutableSubsystem<UMassEQSSubsystem>();
	check(MassEQSSubsystem);

	// Check for any requests of this type from MassEQSSubsystem, complete one if found.
	TUniquePtr<FMassEQSRequestData> TestDataUniquePtr = MassEQSSubsystem->PopRequest(CachedRequestQueryIndex);
	FMassEQSRequestData_MassEntityGameplayTags* TestData = FMassEQSUtils::TryAndEnsureCast<FMassEQSRequestData_MassEntityGameplayTags>(TestDataUniquePtr);
	if (!TestData)
	{
		return;
	}
	if (TestDataUniquePtr->EntityHandles.IsEmpty())
	{
		//UE_LOG(LogMassEQS, Error, TEXT("Request: [%s] Acquired by UMassEnvQueryTestProcessor_MassEntityTags, but had no Entities to query."), *TestDataUniquePtr->RequestHandle.ToString());
		return;
	}

	
	const FGameplayTagContainer& HasTags = TestData->HasTags;
	const FGameplayTagContainer& MustNoHaveTags = TestData->MustNotHaveTags;
	
	TArray<FMassEntityHandle> ScoreMap = {};


	ensureMsgf(ExecutionContext.GetEntityCollection().IsEmpty(), TEXT("We don't expect any collections to be set at this point. The data is going to be overridden."));

	TArray<FMassArchetypeEntityCollection> EntityCollectionsToTest;
	UE::Mass::Utils::CreateEntityCollections(EntityManager, TestDataUniquePtr->EntityHandles, FMassArchetypeEntityCollection::NoDuplicates, EntityCollectionsToTest);

	EntityQuery.ForEachEntityChunkInCollections(EntityCollectionsToTest, ExecutionContext
		, [&ScoreMap, &HasTags, &MustNoHaveTags](FMassExecutionContext& Context)
		{
			const FMassEntityManager& EM = Context.GetEntityManagerChecked();
				
			for (FMassExecutionContext::FEntityIterator EntityIt = Context.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				const FMassEntityHandle EntityHandle = Context.GetEntity(EntityIt);
				FArcMassGameplayTagContainerFragment* GameplayTags = EM.GetFragmentDataPtr<FArcMassGameplayTagContainerFragment>(EntityHandle);
				
				if (!GameplayTags)
				{
					continue;
				}
				if (!GameplayTags->Tags.HasAll(HasTags))
				{
					continue;
				}
				if (!GameplayTags->Tags.HasAny(MustNoHaveTags))
				{
					continue;
				}
				
				ScoreMap.Add(EntityHandle);
			}
		});

	MassEQSSubsystem->SubmitResults(TestData->RequestHandle, MakeUnique<FMassEnvQueryResultData_MassEntityGameplayTags>(MoveTemp(ScoreMap)));
}
