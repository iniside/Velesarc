// Copyright Lukasz Baran. All Rights Reserved.

#include "MVVM/ArcRmlViewModelDeclarations.h"

#include "ArcRmlUILog.h"
#include "ArcRmlUIModule.h"
#include "ArcRmlUISettings.h"
#include "HAL/PlatformFileManager.h"
#include "Interfaces/ArcRmlPaths.h"
#include "Misc/FileHelper.h"

namespace ArcRmlViewModelDeclarationsPrivate
{
	const TCHAR* const HeadOpen = TEXT("<head");
	const TCHAR* const HeadClose = TEXT("</head");
	const TCHAR* const CommentOpen = TEXT("<!--");
	const TCHAR* const CommentClose = TEXT("-->");
	const TCHAR* const ViewModelOpen = TEXT("<viewmodel");
	const TCHAR* const MetaOpen = TEXT("<meta");

	bool IsTagNameEnd(const FString& Text, int32 Index)
	{
		if (Index >= Text.Len())
		{
			return true;
		}
		const TCHAR Character = Text[Index];
		return FChar::IsWhitespace(Character) || Character == TEXT('/') || Character == TEXT('>');
	}

	int32 FindTag(const FString& Text, const TCHAR* Tag, int32 From, int32 To)
	{
		const int32 TagLength = FCString::Strlen(Tag);
		int32 Index = Text.Find(Tag, ESearchCase::IgnoreCase, ESearchDir::FromStart, From);
		while (Index != INDEX_NONE && Index < To)
		{
			if (IsTagNameEnd(Text, Index + TagLength))
			{
				return Index;
			}
			Index = Text.Find(Tag, ESearchCase::IgnoreCase, ESearchDir::FromStart, Index + 1);
		}
		return INDEX_NONE;
	}

	FString DecodeEntities(const FString& Value)
	{
		FString Result = Value;
		Result.ReplaceInline(TEXT("&lt;"), TEXT("<"), ESearchCase::CaseSensitive);
		Result.ReplaceInline(TEXT("&gt;"), TEXT(">"), ESearchCase::CaseSensitive);
		Result.ReplaceInline(TEXT("&quot;"), TEXT("\""), ESearchCase::CaseSensitive);
		Result.ReplaceInline(TEXT("&apos;"), TEXT("'"), ESearchCase::CaseSensitive);
		Result.ReplaceInline(TEXT("&amp;"), TEXT("&"), ESearchCase::CaseSensitive);
		return Result;
	}

	void SkipWhitespace(const FString& Text, int32& Pos, int32 End)
	{
		while (Pos < End && FChar::IsWhitespace(Text[Pos]))
		{
			++Pos;
		}
	}

	int32 ParseAttributes(const FString& Text, int32 Pos, int32 End, TArray<TPair<FString, FString>>& OutAttributes)
	{
		while (Pos < End)
		{
			SkipWhitespace(Text, Pos, End);
			if (Pos >= End)
			{
				break;
			}

			const TCHAR Character = Text[Pos];
			if (Character == TEXT('>'))
			{
				return Pos + 1;
			}
			if (Character == TEXT('/'))
			{
				++Pos;
				continue;
			}

			const int32 NameStart = Pos;
			while (Pos < End && !FChar::IsWhitespace(Text[Pos]) && Text[Pos] != TEXT('=') && Text[Pos] != TEXT('>') && Text[Pos] != TEXT('/'))
			{
				++Pos;
			}
			const FString Name = Text.Mid(NameStart, Pos - NameStart);

			FString Value;
			SkipWhitespace(Text, Pos, End);
			if (Pos < End && Text[Pos] == TEXT('='))
			{
				++Pos;
				SkipWhitespace(Text, Pos, End);
				if (Pos < End && (Text[Pos] == TEXT('"') || Text[Pos] == TEXT('\'')))
				{
					const TCHAR Quote = Text[Pos];
					const int32 ValueStart = ++Pos;
					while (Pos < End && Text[Pos] != Quote)
					{
						++Pos;
					}
					Value = Text.Mid(ValueStart, Pos - ValueStart);
					++Pos;
				}
				else
				{
					const int32 ValueStart = Pos;
					while (Pos < End && !FChar::IsWhitespace(Text[Pos]) && Text[Pos] != TEXT('>'))
					{
						++Pos;
					}
					Value = Text.Mid(ValueStart, Pos - ValueStart);
					Value.RemoveFromEnd(TEXT("/"));
				}
			}

			if (!Name.IsEmpty())
			{
				OutAttributes.Emplace(Name, DecodeEntities(Value));
			}
		}
		return End;
	}

	int32 LineAt(const FString& Text, int32 Index)
	{
		int32 Line = 1;
		for (int32 Pos = 0; Pos < Index; ++Pos)
		{
			if (Text[Pos] == TEXT('\n'))
			{
				++Line;
			}
		}
		return Line;
	}

	bool ApplyMeta(const TArray<TPair<FString, FString>>& Attributes, const FString& DocumentPath, int32 Line, TSet<FString>& SeenNames,
		FArcRmlDocumentMeta& Meta)
	{
		FString Name;
		FString Content;
		bool bHasContent = false;
		for (const TPair<FString, FString>& Attribute : Attributes)
		{
			if (Attribute.Key.Equals(TEXT("name"), ESearchCase::IgnoreCase))
			{
				Name = Attribute.Value;
			}
			else if (Attribute.Key.Equals(TEXT("content"), ESearchCase::IgnoreCase))
			{
				Content = Attribute.Value;
				bHasContent = true;
			}
			else
			{
				UE_LOGF(LogArcRmlUI, Error, "ArcRmlUI: %ls:%d: <meta> has unknown attribute '%ls'", *DocumentPath, Line, *Attribute.Key);
				return false;
			}
		}

		if (Name.IsEmpty() || !bHasContent)
		{
			UE_LOGF(LogArcRmlUI, Error, "ArcRmlUI: %ls:%d: <meta> needs 'name' and 'content'", *DocumentPath, Line);
			return false;
		}

		bool bAlreadySeen = false;
		SeenNames.Add(Name, &bAlreadySeen);
		if (bAlreadySeen)
		{
			UE_LOGF(LogArcRmlUI, Error, "ArcRmlUI: %ls:%d: <meta name='%ls'> is declared more than once", *DocumentPath, Line, *Name);
			return false;
		}

		if (Name == TEXT("layer"))
		{
			const FGameplayTag Layer = FGameplayTag::RequestGameplayTag(FName(*Content), false);
			if (!Layer.IsValid())
			{
				UE_LOGF(LogArcRmlUI, Error, "ArcRmlUI: %ls:%d: <meta name='layer'> has unknown gameplay tag '%ls'", *DocumentPath, Line, *Content);
				return false;
			}
			if (!GetDefault<UArcRmlUISettings>()->LayerOrder.Contains(Layer))
			{
				UE_LOGF(LogArcRmlUI, Error, "ArcRmlUI: %ls:%d: layer '%ls' is not in the ArcRmlUI settings LayerOrder", *DocumentPath, Line, *Content);
				return false;
			}
			Meta.Layer = Layer;
			return true;
		}

		if (Name == TEXT("input-mode"))
		{
			if (Content == TEXT("game"))
			{
				Meta.InputMode = EArcRmlInputMode::Game;
			}
			else if (Content == TEXT("menu"))
			{
				Meta.InputMode = EArcRmlInputMode::Menu;
			}
			else if (Content == TEXT("all"))
			{
				Meta.InputMode = EArcRmlInputMode::All;
			}
			else
			{
				UE_LOGF(LogArcRmlUI, Error, "ArcRmlUI: %ls:%d: <meta name='input-mode'> needs 'game', 'menu' or 'all', got '%ls'", *DocumentPath, Line, *Content);
				return false;
			}
			return true;
		}

		if (Name == TEXT("hide-lower"))
		{
			if (Content == TEXT("true"))
			{
				Meta.bHideLower = true;
			}
			else if (Content == TEXT("false"))
			{
				Meta.bHideLower = false;
			}
			else
			{
				UE_LOGF(LogArcRmlUI, Error, "ArcRmlUI: %ls:%d: <meta name='hide-lower'> needs 'true' or 'false', got '%ls'", *DocumentPath, Line, *Content);
				return false;
			}
			return true;
		}

		if (Name == TEXT("frame"))
		{
			const FString Frame = ArcRmlPaths::NormalizePackagePath(Content);
			if (Frame.IsEmpty())
			{
				UE_LOGF(LogArcRmlUI, Error, "ArcRmlUI: %ls:%d: <meta name='frame'> needs a document path", *DocumentPath, Line);
				return false;
			}
			Meta.Frame = Frame;
			return true;
		}

		UE_LOGF(LogArcRmlUI, Error, "ArcRmlUI: %ls:%d: unknown <meta name='%ls'>", *DocumentPath, Line, *Name);
		return false;
	}

	bool ParseBool(const FString& Value)
	{
		return Value.IsEmpty() || Value.Equals(TEXT("true"), ESearchCase::IgnoreCase) || Value == TEXT("1");
	}

	bool BuildDeclaration(const TArray<TPair<FString, FString>>& Attributes, const FString& DocumentPath, FArcRmlViewModelDeclaration& Out)
	{
		TArray<TPair<FString, FString>> Remaining;
		bool bHasPreview = false;
		bool bHasOnClose = false;
		for (const TPair<FString, FString>& Attribute : Attributes)
		{
			if (Attribute.Key.Equals(TEXT("name"), ESearchCase::IgnoreCase))
			{
				Out.Name = Attribute.Value;
			}
			else if (Attribute.Key.Equals(TEXT("preview"), ESearchCase::IgnoreCase))
			{
				Out.PreviewPath = Attribute.Value.IsEmpty() ? FString() : ArcRmlPaths::JoinPackagePath(DocumentPath, Attribute.Value);
				bHasPreview = true;
			}
			else if (Attribute.Key.Equals(TEXT("class"), ESearchCase::IgnoreCase))
			{
				Out.ClassPath = Attribute.Value;
			}
			else if (Attribute.Key.Equals(TEXT("context"), ESearchCase::IgnoreCase))
			{
				Out.Context = FName(*Attribute.Value);
			}
			else if (Attribute.Key.Equals(TEXT("resolver"), ESearchCase::IgnoreCase))
			{
				Out.ResolverPath = Attribute.Value;
			}
			else if (Attribute.Key.Equals(TEXT("optional"), ESearchCase::IgnoreCase))
			{
				Out.bOptional = ParseBool(Attribute.Value);
			}
			else if (Attribute.Key.Equals(TEXT("on-close"), ESearchCase::IgnoreCase))
			{
				Out.OnClose = Attribute.Value;
				bHasOnClose = true;
			}
			else
			{
				Remaining.Add(Attribute);
			}
		}

		if (Out.Name.IsEmpty())
		{
			UE_LOGF(LogArcRmlUI, Error, "ArcRmlUI: '%ls' has a <viewmodel> without 'name'", *DocumentPath);
			return false;
		}

		const bool bHasContext = !Out.Context.IsNone();
		const bool bHasResolver = !Out.ResolverPath.IsEmpty();
		if (bHasPreview)
		{
			if (Out.PreviewPath.IsEmpty() || !Out.ClassPath.IsEmpty() || bHasContext || bHasResolver || bHasOnClose)
			{
				UE_LOGF(LogArcRmlUI, Error, "ArcRmlUI: <viewmodel name='%ls'> in '%ls' needs a non-empty 'preview' and no 'class', 'context', 'resolver' or 'on-close'",
					*Out.Name, *DocumentPath);
				return false;
			}
			for (const TPair<FString, FString>& Attribute : Remaining)
			{
				UE_LOGF(LogArcRmlUI, Warning, "ArcRmlUI: <viewmodel name='%ls'> in '%ls' ignores unknown attribute '%ls'", *Out.Name, *DocumentPath, *Attribute.Key);
			}
			return true;
		}

		if (Out.ClassPath.IsEmpty())
		{
			UE_LOGF(LogArcRmlUI, Error, "ArcRmlUI: <viewmodel name='%ls'> in '%ls' needs 'class' or 'preview'", *Out.Name, *DocumentPath);
			return false;
		}

		if (bHasContext == bHasResolver)
		{
			UE_LOGF(LogArcRmlUI, Error, "ArcRmlUI: <viewmodel name='%ls'> in '%ls' needs exactly one of 'context' or 'resolver'", *Out.Name, *DocumentPath);
			return false;
		}

		if (bHasOnClose && Out.OnClose.IsEmpty())
		{
			UE_LOGF(LogArcRmlUI, Error, "ArcRmlUI: <viewmodel name='%ls'> in '%ls' has an empty 'on-close'", *Out.Name, *DocumentPath);
			return false;
		}

		if (bHasResolver)
		{
			Out.ResolverProperties = MoveTemp(Remaining);
		}
		else
		{
			for (const TPair<FString, FString>& Attribute : Remaining)
			{
				UE_LOGF(LogArcRmlUI, Warning, "ArcRmlUI: <viewmodel name='%ls'> in '%ls' ignores unknown attribute '%ls'", *Out.Name, *DocumentPath, *Attribute.Key);
			}
		}
		return true;
	}
}

TOptional<FArcRmlDocumentHead> ArcRmlViewModelDeclarations::ParseDocumentHead(const FString& DocumentText, const FString& DocumentPath)
{
	using namespace ArcRmlViewModelDeclarationsPrivate;

	FArcRmlDocumentHead Head;
	bool bValid = true;
	int32 HeadLine = 1;
	TSet<FString> SeenMetaNames;

	const int32 HeadStart = FindTag(DocumentText, HeadOpen, 0, DocumentText.Len());
	if (HeadStart != INDEX_NONE)
	{
		HeadLine = LineAt(DocumentText, HeadStart);
		int32 HeadEnd = FindTag(DocumentText, HeadClose, HeadStart, DocumentText.Len());
		if (HeadEnd == INDEX_NONE)
		{
			HeadEnd = DocumentText.Len();
		}

		int32 Pos = HeadStart + FCString::Strlen(HeadOpen);
		while (Pos < HeadEnd)
		{
			const int32 Comment = DocumentText.Find(CommentOpen, ESearchCase::CaseSensitive, ESearchDir::FromStart, Pos);
			const int32 ViewModelTag = FindTag(DocumentText, ViewModelOpen, Pos, HeadEnd);
			const int32 MetaTag = FindTag(DocumentText, MetaOpen, Pos, HeadEnd);
			if (ViewModelTag == INDEX_NONE && MetaTag == INDEX_NONE)
			{
				break;
			}

			const bool bIsMeta = ViewModelTag == INDEX_NONE || (MetaTag != INDEX_NONE && MetaTag < ViewModelTag);
			const int32 Tag = bIsMeta ? MetaTag : ViewModelTag;

			if (Comment != INDEX_NONE && Comment < Tag)
			{
				const int32 CommentEnd = DocumentText.Find(CommentClose, ESearchCase::CaseSensitive, ESearchDir::FromStart, Comment + FCString::Strlen(CommentOpen));
				if (CommentEnd == INDEX_NONE)
				{
					break;
				}
				Pos = CommentEnd + FCString::Strlen(CommentClose);
				continue;
			}

			TArray<TPair<FString, FString>> Attributes;
			Pos = ParseAttributes(DocumentText, Tag + FCString::Strlen(bIsMeta ? MetaOpen : ViewModelOpen), HeadEnd, Attributes);

			if (bIsMeta)
			{
				if (!ApplyMeta(Attributes, DocumentPath, LineAt(DocumentText, Tag), SeenMetaNames, Head.Meta))
				{
					bValid = false;
				}
				continue;
			}

			FArcRmlViewModelDeclaration Declaration;
			if (!BuildDeclaration(Attributes, DocumentPath, Declaration))
			{
				continue;
			}

			const bool bDuplicate = Head.Declarations.ContainsByPredicate([&Declaration](const FArcRmlViewModelDeclaration& Existing) -> bool
			{
				return Existing.Name == Declaration.Name;
			});
			if (bDuplicate)
			{
				UE_LOGF(LogArcRmlUI, Error, "ArcRmlUI: '%ls' declares view model '%ls' more than once", *DocumentPath, *Declaration.Name);
				continue;
			}
			Head.Declarations.Add(MoveTemp(Declaration));
		}
	}

	if (SeenMetaNames.Contains(TEXT("frame")) && SeenMetaNames.Num() > 1)
	{
		UE_LOGF(LogArcRmlUI, Error, "ArcRmlUI: %ls:%d: a document with <meta name='frame'> takes no other <meta>", *DocumentPath, HeadLine);
		bValid = false;
	}

	if (!SeenMetaNames.Contains(TEXT("layer")) && !SeenMetaNames.Contains(TEXT("frame")) && !GetDefault<UArcRmlUISettings>()->LayerOrder.Contains(Head.Meta.Layer))
	{
		UE_LOGF(LogArcRmlUI, Error, "ArcRmlUI: %ls:%d: default layer '%ls' is not in the ArcRmlUI settings LayerOrder", *DocumentPath, HeadLine,
			*Head.Meta.Layer.ToString());
		bValid = false;
	}

	if (!bValid)
	{
		return TOptional<FArcRmlDocumentHead>();
	}
	return Head;
}

TOptional<FArcRmlDocumentHead> ArcRmlViewModelDeclarations::ReadDocumentHead(const FString& DocumentPath)
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	FString ResolvedPath;
	FString DocumentText;
	if (!ArcRmlPaths::ToFilename(FArcRmlUIModule::Get().GetContentRoots(), DocumentPath, ResolvedPath) || !FFileHelper::LoadFileToString(DocumentText, &PlatformFile, *ResolvedPath))
	{
		UE_LOGF(LogArcRmlUI, Error, "ArcRmlUI: cannot read '%ls'", *DocumentPath);
		return TOptional<FArcRmlDocumentHead>();
	}
	return ParseDocumentHead(DocumentText, DocumentPath);
}

TOptional<FArcRmlDocumentHead> ArcRmlViewModelDeclarations::ReadFrameHead(const FString& TabPath, const FArcRmlDocumentHead& TabHead)
{
	const FString& FramePath = TabHead.Meta.Frame;
	TOptional<FArcRmlDocumentHead> FrameHead = ReadDocumentHead(FramePath);
	if (!FrameHead.IsSet())
	{
		UE_LOGF(LogArcRmlUI, Error, "ArcRmlUI: cannot open '%ls': its frame '%ls' has no usable head", *TabPath, *FramePath);
		return TOptional<FArcRmlDocumentHead>();
	}
	if (!FrameHead->Meta.Frame.IsEmpty())
	{
		UE_LOGF(LogArcRmlUI, Error, "ArcRmlUI: cannot open '%ls': its frame '%ls' declares a frame itself", *TabPath, *FramePath);
		return TOptional<FArcRmlDocumentHead>();
	}
	for (const FArcRmlViewModelDeclaration& Declaration : TabHead.Declarations)
	{
		const bool bClash = FrameHead->Declarations.ContainsByPredicate([&Declaration](const FArcRmlViewModelDeclaration& FrameDeclaration) -> bool
		{
			return FrameDeclaration.Name == Declaration.Name;
		});
		if (bClash)
		{
			UE_LOGF(LogArcRmlUI, Error, "ArcRmlUI: cannot open '%ls': view model '%ls' is also declared by its frame '%ls'", *TabPath, *Declaration.Name, *FramePath);
			return TOptional<FArcRmlDocumentHead>();
		}
	}
	return FrameHead;
}
