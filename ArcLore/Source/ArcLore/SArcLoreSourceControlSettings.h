// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class SArcLoreSourceControlSettings : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SArcLoreSourceControlSettings) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	/** Read-only discovered Lore repository info (root, identity, remote, branch). */
	FText GetStatusText() const;
};
