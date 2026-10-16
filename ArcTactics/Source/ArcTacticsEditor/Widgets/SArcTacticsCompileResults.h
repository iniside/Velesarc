// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class FArcTacticsEditorViewModel;
class SVerticalBox;

class SArcTacticsCompileResults final : public SCompoundWidget
{
public:
	DECLARE_DELEGATE_OneParam(FOnDiagnosticClicked, FGuid);

	SLATE_BEGIN_ARGS(SArcTacticsCompileResults) {}
		SLATE_EVENT(FOnDiagnosticClicked, OnDiagnosticClicked)
	SLATE_END_ARGS()

	virtual ~SArcTacticsCompileResults() override;
	void Construct(
		const FArguments& InArgs,
		TSharedRef<FArcTacticsEditorViewModel> InViewModel);

private:
	void RebuildResults();
	FText GetArtifactStatusText() const;
	FText GetEmptyResultsText() const;
	FSlateColor GetArtifactStatusColor() const;
	FReply HandleDiagnosticClicked(FGuid NodeID) const;

	TWeakPtr<FArcTacticsEditorViewModel> ViewModel;
	TSharedPtr<SVerticalBox> ResultsBox;
	FOnDiagnosticClicked OnDiagnosticClicked;
};
