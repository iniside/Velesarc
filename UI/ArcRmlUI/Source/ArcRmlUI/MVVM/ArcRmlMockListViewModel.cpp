// Copyright Lukasz Baran. All Rights Reserved.

#include "MVVM/ArcRmlMockListViewModel.h"

#include "ArcRmlUILog.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "MVVMGameSubsystem.h"
#include "Types/MVVMViewModelCollection.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcRmlMockListViewModel)

void UArcRmlMockListViewModel::Fill(int32 Count)
{
	Rows.Reset();
	Rows.SetRowType(FArcRmlMockListRow::StaticStruct());
	for (int32 Index = 0; Index < Count; ++Index)
	{
		AddRow(Random.RandRange(0, 999));
	}
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Rows);
}

bool UArcRmlMockListViewModel::Edit(const FString& Operation, int32 Count)
{
	if (Operation == TEXT("add"))
	{
		TArray<FString> Order = Rows.GetKeys();
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const FString Key = AddRow(Random.RandRange(0, 999));
			Order.Insert(Key, Random.RandRange(0, Order.Num()));
		}
		Rows.SetOrder(Order);
	}
	else if (Operation == TEXT("remove"))
	{
		TArray<FString> Removed;
		for (const int32 Index : PickIndices(Count))
		{
			Removed.Add(Rows.GetKeys()[Index]);
		}
		for (const FString& Key : Removed)
		{
			Rows.Remove(Key);
		}
	}
	else if (Operation == TEXT("update"))
	{
		for (const int32 Index : PickIndices(Count))
		{
			const FString Key = Rows.GetKeys()[Index];
			FArcRmlMockListRow Row = *Rows.FindAs<FArcRmlMockListRow>(Key);
			Row.Value += 1000;
			Row.Name = FString::Printf(TEXT("Updated %d"), Row.Id);
			Rows.Set(Key, Row);
		}
	}
	else if (Operation == TEXT("shuffle"))
	{
		const TArray<FString>& Keys = Rows.GetKeys();
		TBitArray<> IsMoved(false, Keys.Num());
		TArray<FString> Moved;
		for (const int32 Index : PickIndices(Count))
		{
			IsMoved[Index] = true;
			Moved.Add(Keys[Index]);
		}
		TArray<FString> Order;
		Order.Reserve(Keys.Num());
		for (int32 Index = 0; Index < Keys.Num(); ++Index)
		{
			if (!IsMoved[Index])
			{
				Order.Add(Keys[Index]);
			}
		}
		for (const FString& Key : Moved)
		{
			Order.Insert(Key, Random.RandRange(0, Order.Num()));
		}
		Rows.SetOrder(Order);
	}
	else
	{
		return false;
	}

	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Rows);
	return true;
}

FString UArcRmlMockListViewModel::AddRow(int32 Value)
{
	FArcRmlMockListRow Row;
	Row.Id = NextId++;
	Row.Name = FString::Printf(TEXT("Row %d"), Row.Id);
	Row.Value = Value;
	const FString Key = LexToString(Row.Id);
	Rows.Set(Key, Row);
	return Key;
}

TArray<int32> UArcRmlMockListViewModel::PickIndices(int32 Count)
{
	TArray<int32> Indices;
	Indices.Reserve(Rows.Num());
	for (int32 Index = 0; Index < Rows.Num(); ++Index)
	{
		Indices.Add(Index);
	}
	const int32 Picked = FMath::Clamp(Count, 0, Indices.Num());
	for (int32 Index = 0; Index < Picked; ++Index)
	{
		Indices.Swap(Index, Random.RandRange(Index, Indices.Num() - 1));
	}
	Indices.SetNum(Picked);
	return Indices;
}

#if !UE_BUILD_SHIPPING
namespace ArcRmlMockListViewModelPrivate
{
	const FName ContextName(TEXT("MockList"));

	UMVVMViewModelCollectionObject* FindCollection(UWorld* World)
	{
		UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		UMVVMGameSubsystem* Subsystem = GameInstance ? GameInstance->GetSubsystem<UMVVMGameSubsystem>() : nullptr;
		return Subsystem ? Subsystem->GetViewModelCollection() : nullptr;
	}

	FMVVMViewModelContext MakeContext()
	{
		FMVVMViewModelContext Context;
		Context.ContextClass = UArcRmlMockListViewModel::StaticClass();
		Context.ContextName = ContextName;
		return Context;
	}

	void HandleMockListCommand(const TArray<FString>& Args, UWorld* World)
	{
		UMVVMViewModelCollectionObject* Collection = FindCollection(World);
		if (!Collection)
		{
			UE_LOGF(LogArcRmlUI, Warning, "ArcRml.MockList: no view model collection in this world");
			return;
		}

		const int32 Count = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 2000;
		UArcRmlMockListViewModel* ViewModel = Cast<UArcRmlMockListViewModel>(Collection->FindViewModelInstance(MakeContext()));
		const bool bCreated = ViewModel == nullptr;
		if (bCreated)
		{
			ViewModel = NewObject<UArcRmlMockListViewModel>(Collection);
		}
		ViewModel->Fill(FMath::Max(0, Count));
		if (bCreated)
		{
			Collection->AddViewModelInstance(MakeContext(), ViewModel);
		}
		UE_LOGF(LogArcRmlUI, Log, "ArcRml.MockList: %d rows under context '%ls'", Count, *ContextName.ToString());
	}

	void HandleMockListEditCommand(const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() < 2)
		{
			UE_LOGF(LogArcRmlUI, Warning, "Usage: ArcRml.MockList.Edit <add|remove|update|shuffle> <N>");
			return;
		}

		UMVVMViewModelCollectionObject* Collection = FindCollection(World);
		UArcRmlMockListViewModel* ViewModel = Collection ? Cast<UArcRmlMockListViewModel>(Collection->FindViewModelInstance(MakeContext())) : nullptr;
		if (!ViewModel)
		{
			UE_LOGF(LogArcRmlUI, Warning, "ArcRml.MockList.Edit: run ArcRml.MockList first");
			return;
		}

		if (!ViewModel->Edit(Args[0].ToLower(), FMath::Max(0, FCString::Atoi(*Args[1]))))
		{
			UE_LOGF(LogArcRmlUI, Warning, "Usage: ArcRml.MockList.Edit <add|remove|update|shuffle> <N>");
		}
	}

	FAutoConsoleCommandWithWorldAndArgs MockListCommand(
		TEXT("ArcRml.MockList"),
		TEXT("Registers a mock struct list view model under context 'MockList' and fills it: ArcRml.MockList <Count>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HandleMockListCommand));

	FAutoConsoleCommandWithWorldAndArgs MockListEditCommand(
		TEXT("ArcRml.MockList.Edit"),
		TEXT("Edits N random rows of the mock list: ArcRml.MockList.Edit <add|remove|update|shuffle> <N>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HandleMockListEditCommand));
}
#endif
