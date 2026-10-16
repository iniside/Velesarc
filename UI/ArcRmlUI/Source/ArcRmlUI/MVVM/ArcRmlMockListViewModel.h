// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcRmlStructList.h"
#include "MVVMViewModelBase.h"

#include "ArcRmlMockListViewModel.generated.h"

USTRUCT()
struct FArcRmlMockListRow
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Id = 0;

	UPROPERTY()
	FString Name;

	UPROPERTY()
	int32 Value = 0;
};

UCLASS()
class ARCRMLUI_API UArcRmlMockListViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	void Fill(int32 Count);
	bool Edit(const FString& Operation, int32 Count);

private:
	FString AddRow(int32 Value);
	TArray<int32> PickIndices(int32 Count);

	UPROPERTY(FieldNotify, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	FArcRmlStructList Rows;

	int32 NextId = 0;
	FRandomStream Random{1};
};
