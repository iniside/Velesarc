// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ISourceControlChangelist.h"

class FArcGitSourceControlChangelist : public ISourceControlChangelist
{
public:
	int32 Id = 0;

	FArcGitSourceControlChangelist() = default;
	explicit FArcGitSourceControlChangelist(int32 InId) : Id(InId) {}

	virtual bool CanDelete() const override { return Id != 0; }
	virtual FString GetIdentifier() const override { return FString::FromInt(Id); }
	virtual bool IsDefault() const override { return Id == 0; }
};

typedef TSharedRef<FArcGitSourceControlChangelist> FArcGitSourceControlChangelistRef;
