// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcEnvQueryItemType_EntitySmartObject.h"

#include "MassEntityFragments.h"
#include "MassEntitySubsystem.h"
#include "SmartObjectSubsystem.h"
#include "Engine/Engine.h"
#include "EnvironmentQuery/Contexts/EnvQueryContext_Querier.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_Point.h"

UArcEnvQueryItemType_EntitySmartObject::UArcEnvQueryItemType_EntitySmartObject()
{
	ValueSize = sizeof(FArcMassSmartObjectItem);
}

const FArcMassSmartObjectItem& UArcEnvQueryItemType_EntitySmartObject::GetValue(const uint8* RawData)
{
	return GetValueFromMemory<FArcMassSmartObjectItem>(RawData);
}

void UArcEnvQueryItemType_EntitySmartObject::SetValue(uint8* RawData, const FArcMassSmartObjectItem& Value)
{
	return SetValueInMemory<FArcMassSmartObjectItem>(RawData, Value);
}

FVector UArcEnvQueryItemType_EntitySmartObject::GetItemLocation(const uint8* RawData) const
{
	const FArcMassSmartObjectItem& EntityInfo = GetValue(RawData);	
	return EntityInfo.Location;
}

FRotator UArcEnvQueryItemType_EntitySmartObject::GetItemRotation(const uint8* RawData) const
{
	const FArcMassSmartObjectItem& EntityInfo = GetValue(RawData);	
	return EntityInfo.Location.Rotation();
}


UArcEnvQueryGenerator_EntitySmartObjects::UArcEnvQueryGenerator_EntitySmartObjects()
{
	ItemType = UArcEnvQueryItemType_EntitySmartObject::StaticClass();

	QueryBoxExtent.Set(2000,2000, 500);
	QueryOriginContext = UEnvQueryContext_Querier::StaticClass();
}

void UArcEnvQueryGenerator_EntitySmartObjects::GenerateItems(FEnvQueryInstance& QueryInstance) const
{
	UObject* QueryOwner = QueryInstance.Owner.Get();
	if (QueryOwner == nullptr)
	{	
		return;
	}
		
	UWorld* World = GEngine->GetWorldFromContextObject(QueryOwner, EGetWorldErrorMode::LogAndReturnNull);
	USmartObjectSubsystem* SmartObjectSubsystem = UWorld::GetSubsystem<USmartObjectSubsystem>(World);

	UMassEntitySubsystem* MassEntitySubsystem = UWorld::GetSubsystem<UMassEntitySubsystem>(World);
	FMassEntityManager& EntityManager = MassEntitySubsystem->GetMutableEntityManager();
	
	if (SmartObjectSubsystem == nullptr)
	{
		return;
	}

	TArray<FSmartObjectRequestResult> FoundSlots;
	TArray<FVector> OriginLocations;
	QueryInstance.PrepareContext(QueryOriginContext, OriginLocations);
	int32 NumberOfSuccessfulQueries = 0;

	for (const FVector& Origin : OriginLocations)
	{
		FBox QueryBox(Origin - QueryBoxExtent, Origin + QueryBoxExtent);
		FSmartObjectRequest Request(QueryBox, SmartObjectRequestFilter);

		// @todo note that with this approach, if there's more than one Origin being used for generation we can end up 
		// with duplicates in AllResults
		NumberOfSuccessfulQueries += SmartObjectSubsystem->FindSmartObjects(Request, FoundSlots, Cast<AActor>(QueryOwner)) ? 1 : 0;
	}

	if (NumberOfSuccessfulQueries > 1)
	{
		FoundSlots.Sort([](const FSmartObjectRequestResult& A, const FSmartObjectRequestResult& B)
		{
			return A.SlotHandle < B.SlotHandle;
		});
	}

	TArray<FArcMassSmartObjectItem> AllResults;
	AllResults.Reserve(FoundSlots.Num());

	FSmartObjectSlotHandle PreviousSlotHandle;

	for (const FSmartObjectRequestResult& SlotResult : FoundSlots)
	{
		if (NumberOfSuccessfulQueries > 1 && SlotResult.SlotHandle == PreviousSlotHandle)
		{
			// skip duplicates
			continue;
		}
		PreviousSlotHandle = SlotResult.SlotHandle;

		if (bOnlyClaimable == false || SmartObjectSubsystem->CanBeClaimed(SlotResult.SlotHandle))
		{
			FArcMassSmartObjectItem NewItem;
			NewItem.SmartObjectHandle = SlotResult.SmartObjectHandle;
			NewItem.SlotHandle = SlotResult.SlotHandle;
			
			const FTransform& SlotTransform = SmartObjectSubsystem->GetSlotTransformChecked(SlotResult.SlotHandle);
			NewItem.Location = SlotTransform.GetLocation();
			
			FConstStructView OwnerData = SmartObjectSubsystem->GetOwnerData(SlotResult.SmartObjectHandle);
			if (OwnerData.GetScriptStruct() == FMassEntityHandle::StaticStruct())
			{
				NewItem.EntityHandle = OwnerData.Get<FMassEntityHandle>();
				NewItem.OwningEntity = OwnerData.Get<FMassEntityHandle>();
				NewItem.CachedTransform = SlotTransform;
			}
			
			AllResults.Emplace(NewItem);
		}
	}

	QueryInstance.AddItemData<UArcEnvQueryItemType_EntitySmartObject>(AllResults);
}

FText UArcEnvQueryGenerator_EntitySmartObjects::GetDescriptionTitle() const
{
	FStringFormatNamedArguments Args;
	Args.Add("DescribeContext", UEnvQueryTypes::DescribeContext(QueryOriginContext).ToString());
	;
	return FText::FromString(FString::Format(TEXT("Smart Object slots around {DescribeContext}"), Args));
};

FText UArcEnvQueryGenerator_EntitySmartObjects::GetDescriptionDetails() const
{
	FFormatNamedArguments Args;

	FTextBuilder DescBuilder;

	if (!SmartObjectRequestFilter.UserTags.IsEmpty()
		|| !SmartObjectRequestFilter.ActivityRequirements.IsEmpty()
		|| !SmartObjectRequestFilter.BehaviorDefinitionClasses.IsEmpty()
		|| SmartObjectRequestFilter.Predicate)
	{
		DescBuilder.AppendLine(FText::FromString("Smart Object Filter:"));
		if (!SmartObjectRequestFilter.UserTags.IsEmpty())
		{
			DescBuilder.AppendLineFormat(FTextFormat::FromString("\tUser Tags: {0}"), FText::FromString(SmartObjectRequestFilter.UserTags.ToString()));
		}
		
		if (!SmartObjectRequestFilter.ActivityRequirements.IsEmpty())
		{
			DescBuilder.AppendLineFormat(FTextFormat::FromString("\tActivity Requirements: {0}")
				, FText::FromString(SmartObjectRequestFilter.ActivityRequirements.GetDescription()));
		}

		if (!SmartObjectRequestFilter.BehaviorDefinitionClasses.IsEmpty())
		{
			if (SmartObjectRequestFilter.BehaviorDefinitionClasses.Num() == 1)//SmartObjectRequestFilter.BehaviorDefinitionClass)
			{
				DescBuilder.AppendLineFormat(FTextFormat::FromString("\tBehavior Definition Class: {0}")
					, FText::FromString(GetNameSafe(SmartObjectRequestFilter.BehaviorDefinitionClasses[0])));
			}
			else
			{
				DescBuilder.AppendLine(FText::FromString("\tBehavior Definition Classes:"));
				for (int i = 0; i < SmartObjectRequestFilter.BehaviorDefinitionClasses.Num(); ++i)
				{
					const TSubclassOf<USmartObjectBehaviorDefinition>& BehaviorClass = SmartObjectRequestFilter.BehaviorDefinitionClasses[i];
					DescBuilder.AppendLineFormat(FTextFormat::FromString("\t\t[{0}]: {1}")
						, i, FText::FromString(GetNameSafe(BehaviorClass)));
				}
			}
		}
		
		if (SmartObjectRequestFilter.Predicate)
		{
			DescBuilder.AppendLine(FText::FromString("\tWith Predicate function"));
		}
	}

	Args.Add(TEXT("QUERYEXTENT"), FText::FromString(FString::Printf(TEXT("[%.1f, %.1f, %.1f]"), float(QueryBoxExtent.X), float(QueryBoxExtent.Y), float(QueryBoxExtent.Z))));
	Args.Add(TEXT("CLAIMABLE"), FText::FromString(bOnlyClaimable ? TEXT("Claimable") : TEXT("All")));
	
	DescBuilder.AppendLineFormat(FTextFormat::FromString("Query Extent: {QUERYEXTENT}\nSlots: {CLAIMABLE}"), Args);

	return DescBuilder.ToText();
}

UArcEnvQueryContext_EntitySmartObjectLocations::UArcEnvQueryContext_EntitySmartObjectLocations()
{
}

void UArcEnvQueryContext_EntitySmartObjectLocations::ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const
{
	UObject* QueryOwner = QueryInstance.Owner.Get();
	if (QueryOwner == nullptr)
	{	
		return;
	}
	
	AActor* ActorOwner = Cast<AActor>(QueryOwner);
	if (ActorOwner == nullptr)
	{
		return;
	}
	
	UWorld* World = GEngine->GetWorldFromContextObject(QueryOwner, EGetWorldErrorMode::LogAndReturnNull);
	USmartObjectSubsystem* SmartObjectSubsystem = UWorld::GetSubsystem<USmartObjectSubsystem>(World);

	UMassEntitySubsystem* MassEntitySubsystem = UWorld::GetSubsystem<UMassEntitySubsystem>(World);
	FMassEntityManager& EntityManager = MassEntitySubsystem->GetMutableEntityManager();
	
	if (SmartObjectSubsystem == nullptr)
	{
		return;
	}

	TArray<FSmartObjectRequestResult> FoundSlots;
	TArray<FVector> OriginLocations;
	OriginLocations.Add(ActorOwner->GetActorLocation());
	
	int32 NumberOfSuccessfulQueries = 0;

	for (const FVector& Origin : OriginLocations)
	{
		FBox QueryBox(Origin - QueryBoxExtent, Origin + QueryBoxExtent);
		FSmartObjectRequest Request(QueryBox, SmartObjectRequestFilter);

		// @todo note that with this approach, if there's more than one Origin being used for generation we can end up 
		// with duplicates in AllResults
		NumberOfSuccessfulQueries += SmartObjectSubsystem->FindSmartObjects(Request, FoundSlots, Cast<AActor>(QueryOwner)) ? 1 : 0;
	}

	if (NumberOfSuccessfulQueries > 1)
	{
		FoundSlots.Sort([](const FSmartObjectRequestResult& A, const FSmartObjectRequestResult& B)
		{
			return A.SlotHandle < B.SlotHandle;
		});
	}

	TArray<FVector> AllResults;
	AllResults.Reserve(FoundSlots.Num());

	TSet<FSmartObjectHandle> FoundSmartObjects;
	for (const FSmartObjectRequestResult& SlotResult : FoundSlots)
	{
		if (FoundSmartObjects.Contains(SlotResult.SmartObjectHandle))
		{
			continue;
		}
		
		FConstStructView OwnerData = SmartObjectSubsystem->GetOwnerData(SlotResult.SmartObjectHandle);
		if (OwnerData.GetScriptStruct() == FMassEntityHandle::StaticStruct())
		{
			FMassEntityHandle EntityHandle = OwnerData.Get<FMassEntityHandle>();
			if (FTransformFragment* TransformFragment = EntityManager.GetFragmentDataPtr<FTransformFragment>(EntityHandle))
			{
				FoundSmartObjects.Add(SlotResult.SmartObjectHandle);
				AllResults.Add(TransformFragment->GetTransform().GetLocation());
			}
		}
		
	}
	
	UEnvQueryItemType_Point::SetContextHelper(ContextData, AllResults);
}
