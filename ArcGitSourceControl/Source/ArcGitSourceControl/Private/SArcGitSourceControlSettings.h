// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

enum class ECheckBoxState : uint8;
namespace ETextCommit { enum Type : int; }

class SArcGitSourceControlSettings : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SArcGitSourceControlSettings) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	/** Git binary path callbacks */
	FText GetBinaryPathText() const;
	void OnBinaryPathTextCommitted(const FText& InText, ETextCommit::Type InCommitType);

	/** Auto-push checkbox callbacks */
	ECheckBoxState GetAutoPushCheckState() const;
	void OnAutoPushCheckStateChanged(ECheckBoxState NewState);

	/** Read-only repository info */
	FText GetStatusText() const;
};
