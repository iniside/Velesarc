// Copyright Lukasz Baran. All Rights Reserved.

#include "MVVM/ArcRmlPreviewData.h"

#include "ArcRmlUILog.h"
#include "ArcRmlUIModule.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformFileManager.h"
#include "Interfaces/ArcRmlPaths.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonTypes.h"

namespace ArcRmlPreviewDataPrivate
{
	using FReader = TJsonReader<TCHAR>;

	constexpr int32 MaxDepth = 64;
	const TCHAR* const VariantsKey = TEXT("$variants");
	const TCHAR* const ColourKey = TEXT("$colour");

#if !UE_BUILD_SHIPPING
	TAutoConsoleVariable<int32> CVarPreviewHidden(
		TEXT("ArcRml.PreviewHidden"),
		0,
		TEXT("1 binds preview models with ArcResolved=false, as Shipping does. Applies to documents opened afterwards."));
#endif

	FString FormatReaderError(const FReader& Reader, const FString& Message)
	{
		return FString::Printf(TEXT("%s Line: %u Ch: %u"), *Message, Reader.GetLineNumber(), Reader.GetCharacterNumber());
	}

	bool ReadNext(FReader& Reader, EJsonNotation& OutNotation, FString& OutError)
	{
		if (!Reader.ReadNext(OutNotation) || OutNotation == EJsonNotation::Error)
		{
			OutError = Reader.GetErrorMessage().IsEmpty() ? FormatReaderError(Reader, TEXT("Unexpected end of input.")) : Reader.GetErrorMessage();
			return false;
		}
		return true;
	}

	bool IsIntegral(const FString& NumberText)
	{
		int32 Index = INDEX_NONE;
		return !NumberText.FindChar(TEXT('.'), Index) && !NumberText.FindChar(TEXT('e'), Index) && !NumberText.FindChar(TEXT('E'), Index);
	}

	bool ParseColour(const FString& Text, FColor& OutColour)
	{
		if (Text.Len() != 9 || Text[0] != TEXT('#'))
		{
			return false;
		}
		for (int32 Index = 1; Index < Text.Len(); ++Index)
		{
			if (!FChar::IsHexDigit(Text[Index]))
			{
				return false;
			}
		}
		OutColour = FColor::FromHex(Text);
		return true;
	}

	bool ParseValue(FReader& Reader, EJsonNotation Notation, int32 Depth, FArcRmlValue& Out, FString& OutError);

	bool ParseObject(FReader& Reader, int32 Depth, FArcRmlValue& Out, FString& OutError)
	{
		Out.Kind = EArcRmlValueKind::Struct;
		while (true)
		{
			EJsonNotation Notation = EJsonNotation::Error;
			if (!ReadNext(Reader, Notation, OutError))
			{
				return false;
			}
			if (Notation == EJsonNotation::ObjectEnd)
			{
				break;
			}

			const FString Name = Reader.GetIdentifier();
			if (Out.MemberNames.Contains(Name))
			{
				OutError = FormatReaderError(Reader, FString::Printf(TEXT("Duplicate member '%s'."), *Name));
				return false;
			}
			FArcRmlValue Child;
			if (!ParseValue(Reader, Notation, Depth + 1, Child, OutError))
			{
				return false;
			}
			Out.MemberNames.Add(Name);
			Out.Children.Add(MoveTemp(Child));
		}

		const int32 ColourIndex = Out.MemberNames.IndexOfByKey(ColourKey);
		if (ColourIndex == INDEX_NONE)
		{
			return true;
		}

		FColor Colour;
		const FArcRmlValue& ColourValue = Out.Children[ColourIndex];
		if (Out.MemberNames.Num() != 1 || ColourValue.Kind != EArcRmlValueKind::String || !ParseColour(ColourValue.String, Colour))
		{
			OutError = FormatReaderError(Reader, TEXT("'$colour' must be the only member and a \"#rrggbbaa\" string."));
			return false;
		}
		Out = FArcRmlValue();
		Out.Kind = EArcRmlValueKind::Colour;
		Out.Colour = Colour;
		return true;
	}

	bool ParseArray(FReader& Reader, int32 Depth, FArcRmlValue& Out, FString& OutError)
	{
		Out.Kind = EArcRmlValueKind::Array;
		while (true)
		{
			EJsonNotation Notation = EJsonNotation::Error;
			if (!ReadNext(Reader, Notation, OutError))
			{
				return false;
			}
			if (Notation == EJsonNotation::ArrayEnd)
			{
				return true;
			}
			FArcRmlValue& Child = Out.Children.AddDefaulted_GetRef();
			if (!ParseValue(Reader, Notation, Depth + 1, Child, OutError))
			{
				return false;
			}
		}
	}

	bool ParseValue(FReader& Reader, EJsonNotation Notation, int32 Depth, FArcRmlValue& Out, FString& OutError)
	{
		if (Depth > MaxDepth)
		{
			OutError = FormatReaderError(Reader, FString::Printf(TEXT("Nesting is deeper than %d."), MaxDepth));
			return false;
		}

		switch (Notation)
		{
		case EJsonNotation::ObjectStart:
			return ParseObject(Reader, Depth, Out, OutError);
		case EJsonNotation::ArrayStart:
			return ParseArray(Reader, Depth, Out, OutError);
		case EJsonNotation::Boolean:
			Out.Kind = EArcRmlValueKind::Bool;
			Out.bBool = Reader.GetValueAsBoolean();
			return true;
		case EJsonNotation::String:
			Out.Kind = EArcRmlValueKind::String;
			Out.String = Reader.GetValueAsString();
			return true;
		case EJsonNotation::Number:
		{
			const FString& NumberText = Reader.GetValueAsNumberString();
			if (IsIntegral(NumberText))
			{
				Out.Kind = EArcRmlValueKind::Int;
				Out.Int = FCString::Atoi64(*NumberText);
			}
			else
			{
				Out.Kind = EArcRmlValueKind::Float;
				Out.Float = Reader.GetValueAsNumber();
			}
			return true;
		}
		case EJsonNotation::Null:
			OutError = FormatReaderError(Reader, TEXT("null has no model value."));
			return false;
		default:
			OutError = FormatReaderError(Reader, TEXT("Unexpected token."));
			return false;
		}
	}

	bool ParseDocument(const FString& Text, FArcRmlValue& OutRoot, FString& OutError)
	{
		const TSharedRef<FReader> Reader = TJsonReaderFactory<TCHAR>::Create(Text);
		EJsonNotation Notation = EJsonNotation::Error;
		if (!ReadNext(*Reader, Notation, OutError))
		{
			return false;
		}
		if (Notation != EJsonNotation::ObjectStart)
		{
			OutError = FormatReaderError(*Reader, TEXT("The root must be an object."));
			return false;
		}
		if (!ParseValue(*Reader, Notation, 0, OutRoot, OutError))
		{
			return false;
		}
		if (OutRoot.Kind != EArcRmlValueKind::Struct)
		{
			OutError = FormatReaderError(*Reader, TEXT("The root must not be a '$colour' object."));
			return false;
		}

		EJsonNotation Trailing = EJsonNotation::Error;
		if (Reader->ReadNext(Trailing) && Trailing == EJsonNotation::Error)
		{
			OutError = Reader->GetErrorMessage();
			return false;
		}
		return true;
	}

	bool ReadRoot(const FString& PreviewPath, const FString& ModelName, FArcRmlValue& OutRoot)
	{
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		FString ResolvedPath;
		FString Text;
		if (!ArcRmlPaths::ToFilename(FArcRmlUIModule::Get().GetContentRoots(), PreviewPath, ResolvedPath) || !FFileHelper::LoadFileToString(Text, &PlatformFile, *ResolvedPath))
		{
			UE_LOGF(LogArcRmlUI, Error, "ArcRmlUI: preview model '%ls' cannot read '%ls'", *ModelName, *PreviewPath);
			return false;
		}

		FString ParseError;
		if (!ParseDocument(Text, OutRoot, ParseError))
		{
			UE_LOGF(LogArcRmlUI, Error, "ArcRmlUI: preview model '%ls' cannot parse '%ls': %ls", *ModelName, *PreviewPath, *ParseError);
			return false;
		}
		return true;
	}

	const FArcRmlValue* FindVariants(const FArcRmlValue& Root)
	{
		const int32 VariantsIndex = Root.MemberNames.IndexOfByKey(VariantsKey);
		if (VariantsIndex == INDEX_NONE || Root.Children[VariantsIndex].Kind != EArcRmlValueKind::Struct)
		{
			return nullptr;
		}
		return &Root.Children[VariantsIndex];
	}

	bool ApplyVariant(FArcRmlValue& Root, const FString& Variant, const FString& ModelName, const FString& PreviewPath)
	{
		FArcRmlValue Variants;
		const int32 VariantsIndex = Root.MemberNames.IndexOfByKey(VariantsKey);
		if (VariantsIndex != INDEX_NONE)
		{
			Variants = MoveTemp(Root.Children[VariantsIndex]);
			Root.Children.RemoveAt(VariantsIndex);
			Root.MemberNames.RemoveAt(VariantsIndex);
			if (Variants.Kind != EArcRmlValueKind::Struct)
			{
				UE_LOGF(LogArcRmlUI, Error, "ArcRmlUI: preview model '%ls' in '%ls' has a '$variants' that is not an object", *ModelName, *PreviewPath);
				return false;
			}
		}

		if (Variant.IsEmpty())
		{
			return true;
		}

		const int32 SelectedIndex = Variants.MemberNames.IndexOfByKey(Variant);
		if (SelectedIndex == INDEX_NONE || Variants.Children[SelectedIndex].Kind != EArcRmlValueKind::Struct)
		{
			UE_LOGF(LogArcRmlUI, Error, "ArcRmlUI: preview model '%ls' in '%ls' has no object variant '%ls'; it binds the base data", *ModelName, *PreviewPath,
				*Variant);
			return true;
		}

		const FArcRmlValue& Overrides = Variants.Children[SelectedIndex];
		for (int32 Index = 0; Index < Overrides.MemberNames.Num(); ++Index)
		{
			const int32 Existing = Root.MemberNames.IndexOfByKey(Overrides.MemberNames[Index]);
			if (Existing != INDEX_NONE)
			{
				Root.Children[Existing] = Overrides.Children[Index];
			}
			else
			{
				Root.MemberNames.Add(Overrides.MemberNames[Index]);
				Root.Children.Add(Overrides.Children[Index]);
			}
		}
		return true;
	}
}

bool ArcRmlPreviewData::ShowsPreviewData()
{
#if UE_BUILD_SHIPPING
	return false;
#else
	return ArcRmlPreviewDataPrivate::CVarPreviewHidden.GetValueOnGameThread() == 0;
#endif
}

bool ArcRmlPreviewData::HasVariant(const FString& PreviewPath, const FString& Variant)
{
	check(IsInGameThread());

	FArcRmlValue Root;
	if (!ArcRmlPreviewDataPrivate::ReadRoot(PreviewPath, PreviewPath, Root))
	{
		return false;
	}

	const FArcRmlValue* Variants = ArcRmlPreviewDataPrivate::FindVariants(Root);
	if (!Variants)
	{
		return false;
	}

	const int32 SelectedIndex = Variants->MemberNames.IndexOfByKey(Variant);
	return SelectedIndex != INDEX_NONE && Variants->Children[SelectedIndex].Kind == EArcRmlValueKind::Struct;
}

TArray<FString> ArcRmlPreviewData::GetVariantNames(const FString& PreviewPath)
{
	check(IsInGameThread());

	TArray<FString> Names;
	FArcRmlValue Root;
	if (!ArcRmlPreviewDataPrivate::ReadRoot(PreviewPath, PreviewPath, Root))
	{
		return Names;
	}

	const FArcRmlValue* Variants = ArcRmlPreviewDataPrivate::FindVariants(Root);
	if (!Variants)
	{
		return Names;
	}

	for (int32 Index = 0; Index < Variants->MemberNames.Num(); ++Index)
	{
		if (Variants->Children[Index].Kind == EArcRmlValueKind::Struct)
		{
			Names.Add(Variants->MemberNames[Index]);
		}
	}
	return Names;
}

bool ArcRmlPreviewData::LoadModel(const FString& PreviewPath, const FString& Variant, const FString& ModelName, FArcRmlValue& OutSnapshot)
{
	check(IsInGameThread());

	OutSnapshot = FArcRmlValue();
	OutSnapshot.Kind = EArcRmlValueKind::Struct;

	FArcRmlValue Root;
	if (!ArcRmlPreviewDataPrivate::ReadRoot(PreviewPath, ModelName, Root))
	{
		return false;
	}

	if (!ArcRmlPreviewDataPrivate::ApplyVariant(Root, Variant, ModelName, PreviewPath))
	{
		return false;
	}

	OutSnapshot = MoveTemp(Root);
	return true;
}
