// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcRmlUITags.h"
#include "GameplayTagContainer.h"

struct ARCRMLUI_API FArcRmlViewModelDeclaration
{
	FString Name;
	FString ClassPath;
	FName Context;
	FString ResolverPath;
	FString PreviewPath;
	FString OnClose;
	TArray<TPair<FString, FString>> ResolverProperties;
	bool bOptional = false;

	bool IsPreview() const
	{
		return !PreviewPath.IsEmpty();
	}
};

enum class EArcRmlInputMode : uint8
{
	Game,
	Menu,
	All
};

struct ARCRMLUI_API FArcRmlDocumentMeta
{
	FGameplayTag Layer = ArcRmlUITags::UI_Layer_Game;
	EArcRmlInputMode InputMode = EArcRmlInputMode::Game;
	bool bHideLower = true;
	FString Frame;
};

struct ARCRMLUI_API FArcRmlDocumentHead
{
	TArray<FArcRmlViewModelDeclaration> Declarations;
	FArcRmlDocumentMeta Meta;
};

namespace ArcRmlViewModelDeclarations
{
	ARCRMLUI_API TOptional<FArcRmlDocumentHead> ParseDocumentHead(const FString& DocumentText, const FString& DocumentPath);
	ARCRMLUI_API TOptional<FArcRmlDocumentHead> ReadDocumentHead(const FString& DocumentPath);
	ARCRMLUI_API TOptional<FArcRmlDocumentHead> ReadFrameHead(const FString& TabPath, const FArcRmlDocumentHead& TabHead);
}
