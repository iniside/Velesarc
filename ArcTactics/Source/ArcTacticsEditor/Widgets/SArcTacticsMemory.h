// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Decision/ArcTacticsConfig.h"
#include "GameplayTagContainer.h"
#include "Widgets/SCompoundWidget.h"

class FArcTacticsEditorViewModel;
class SVerticalBox;

class SArcTacticsMemory final : public SCompoundWidget
{
public:
	DECLARE_DELEGATE_TwoParams(FOnMemoryUserClicked, FGuid, FName);

	SLATE_BEGIN_ARGS(SArcTacticsMemory) {}
		SLATE_EVENT(FOnMemoryUserClicked, OnMemoryUserClicked)
	SLATE_END_ARGS()

	virtual ~SArcTacticsMemory() override;
	void Construct(
		const FArguments& InArgs,
		TSharedRef<FArcTacticsEditorViewModel> InViewModel);

private:
	struct FMemoryUser
	{
		FGuid NavigateID;
		FName PropertyName;
		FText Label;
		bool bForgetOnly = false;
	};

	struct FMemoryRow
	{
		FGameplayTag Key;
		TArray<FString> Types;
		TArray<FMemoryUser> Readers;
		TArray<FMemoryUser> Writers;
		bool bWritten = false;
	};

	void HandleModelChanged(EArcTacticsAuthoringChange Change);
	void RebuildRows();
	TSharedRef<SWidget> MakeUsersWidget(const TArray<FMemoryUser>& Users, FText EmptyText) const;
	FReply HandleUserClicked(FGuid NavigateID, FName PropertyName) const;

	TWeakPtr<FArcTacticsEditorViewModel> ViewModel;
	TSharedPtr<SVerticalBox> RowsBox;
	FOnMemoryUserClicked OnMemoryUserClicked;
};
