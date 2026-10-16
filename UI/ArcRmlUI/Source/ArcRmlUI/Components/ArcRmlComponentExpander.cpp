// Copyright Lukasz Baran. All Rights Reserved.

#include "Components/ArcRmlComponentExpander.h"

#include "ArcRmlUILog.h"
#include "Interfaces/ArcRmlPaths.h"
#include "Interfaces/ArcRmlStrings.h"
#include "Misc/Paths.h"

#include "RmlUi/Core/ConstantDataExpression.h"
#include "RmlUi/Core/DataTypeRegister.h"
#include "RmlUi/Core/FileInterface.h"
#include "RmlUi/Core/StringUtilities.h"
#include "RmlUi/Core/Variant.h"

#if WITH_EDITOR
namespace ArcRmlComponentExpanderPrivate
{
	const TCHAR* const ImportTagName = TEXT("dc-import");
	const TCHAR* const ComponentTagName = TEXT("component");
	const TCHAR* const PropTagName = TEXT("prop");
	const TCHAR* const SlotTagName = TEXT("slot");

	const TCHAR* const HostStyleProperties[] = {
		TEXT("position"), TEXT("left"), TEXT("right"), TEXT("top"), TEXT("bottom"),
		TEXT("inset"), TEXT("width"), TEXT("height"), TEXT("z-index"), TEXT("transform")};

	const TCHAR* const ExpressionAttributes[] = {
		TEXT("data-if"), TEXT("data-for"), TEXT("data-visible"), TEXT("data-rml")};

	const TCHAR* const ExpressionAttributePrefixes[] = {
		TEXT("data-class-"), TEXT("data-style-"), TEXT("data-attr-"), TEXT("data-attrif-"), TEXT("data-event-")};

	bool IsNamed(FStringView View, const TCHAR* Name)
	{
		return View.Equals(Name, ESearchCase::IgnoreCase);
	}

	struct FAttribute
	{
		FStringView Name;
		FStringView RawValue;
		int32 NameStart = INDEX_NONE;
		int32 ValueSpanStart = INDEX_NONE;
		int32 ValueSpanEnd = INDEX_NONE;
		int32 End = INDEX_NONE;
		TCHAR Quote = 0;
		bool bHasValue = false;
	};

	struct FTag
	{
		FStringView Name;
		TArray<FAttribute, TInlineAllocator<16>> Attributes;
		bool bSelfClosing = false;

		const FAttribute* Find(const TCHAR* AttributeName) const
		{
			for (const FAttribute& Attribute : Attributes)
			{
				if (IsNamed(Attribute.Name, AttributeName))
				{
					return &Attribute;
				}
			}
			return nullptr;
		}
	};

	enum class ETokenType : uint8
	{
		Text,
		Data,
		Markup,
		StartTag,
		EndTag
	};

	struct FToken
	{
		ETokenType Type = ETokenType::Text;
		int32 Start = 0;
		int32 End = 0;
		FTag Tag;
		bool bMalformed = false;

		void Reset()
		{
			Type = ETokenType::Text;
			Start = 0;
			End = 0;
			Tag.Name.Reset();
			Tag.Attributes.Reset();
			Tag.bSelfClosing = false;
			bMalformed = false;
		}
	};

	struct FPropDeclaration
	{
		FString Name;
		FString Default;
		bool bHasDefault = false;
	};

	struct FPropValue
	{
		FString Text;
		bool bBound = false;
		bool bPath = false;
	};

	enum class EExpressionKind : uint8
	{
		Attribute,
		Container,
		Text
	};

	bool IsNameStart(TCHAR C)
	{
		return (C >= TEXT('a') && C <= TEXT('z')) || (C >= TEXT('A') && C <= TEXT('Z')) || C == TEXT('_');
	}

	bool IsNameChar(TCHAR C)
	{
		return IsNameStart(C) || (C >= TEXT('0') && C <= TEXT('9')) || C == TEXT('-') || C == TEXT('.') || C == TEXT(':');
	}

	bool IsIdentifierChar(TCHAR C)
	{
		return IsNameStart(C) || (C >= TEXT('0') && C <= TEXT('9'));
	}

	void AppendSpan(FString& Out, FStringView Text, int32 Start, int32 End)
	{
		Out.AppendChars(Text.GetData() + Start, End - Start);
	}

	bool StartsAt(FStringView Text, int32 Pos, const TCHAR* Prefix)
	{
		const int32 Length = FCString::Strlen(Prefix);
		return Pos + Length <= Text.Len() && FCString::Strncmp(Text.GetData() + Pos, Prefix, Length) == 0;
	}

	int32 SkipPast(FStringView Text, int32 From, const TCHAR* Marker)
	{
		const int32 Found = Text.Find(Marker, From, ESearchCase::CaseSensitive);
		return Found == INDEX_NONE ? Text.Len() : Found + FCString::Strlen(Marker);
	}

	int32 SkipDataBrackets(FStringView Text, int32 Pos)
	{
		bool bInString = false;
		for (int32 Index = Pos + 2; Index < Text.Len(); ++Index)
		{
			const TCHAR C = Text[Index];
			if (bInString && C == TEXT('\\'))
			{
				++Index;
			}
			else if (C == TEXT('\''))
			{
				bInString = !bInString;
			}
			else if (!bInString && C == TEXT('}') && Index + 1 < Text.Len() && Text[Index + 1] == TEXT('}'))
			{
				return Index + 2;
			}
		}
		return Text.Len();
	}

	bool IsBlank(FStringView Text, int32 Start, int32 End)
	{
		for (int32 Index = Start; Index < End; ++Index)
		{
			if (!FChar::IsWhitespace(Text[Index]))
			{
				return false;
			}
		}
		return true;
	}

	class FTokenizer
	{
	public:
		explicit FTokenizer(FStringView InText)
			: Text(InText)
		{
		}

		bool Next(FToken& Out)
		{
			const int32 Length = Text.Len();
			if (Pos >= Length)
			{
				return false;
			}

			Out.Reset();
			Out.Start = Pos;

			if (!RawCloseTag.IsEmpty())
			{
				const int32 Close = FindRawClose(Pos);
				RawCloseTag.Reset();
				const int32 End = Close == INDEX_NONE ? Length : Close;
				if (End > Pos)
				{
					return Finish(Out, ETokenType::Text, End);
				}
			}

			const TCHAR C = Text[Pos];
			if (C == TEXT('<'))
			{
				if (StartsAt(Text, Pos, TEXT("<!--")))
				{
					return Finish(Out, ETokenType::Markup, SkipPast(Text, Pos + 4, TEXT("-->")));
				}
				if (StartsAt(Text, Pos, TEXT("<![CDATA[")))
				{
					return Finish(Out, ETokenType::Markup, SkipPast(Text, Pos + 9, TEXT("]]>")));
				}
				if (StartsAt(Text, Pos, TEXT("<?")))
				{
					return Finish(Out, ETokenType::Markup, SkipPast(Text, Pos + 2, TEXT("?>")));
				}
				if (StartsAt(Text, Pos, TEXT("<!")))
				{
					return Finish(Out, ETokenType::Markup, SkipPast(Text, Pos + 2, TEXT(">")));
				}
				if (StartsAt(Text, Pos, TEXT("</")))
				{
					int32 NameEnd = Pos + 2;
					while (NameEnd < Length && IsNameChar(Text[NameEnd]))
					{
						++NameEnd;
					}
					Out.Tag.Name = Text.Mid(Pos + 2, NameEnd - Pos - 2);
					return Finish(Out, ETokenType::EndTag, SkipPast(Text, NameEnd, TEXT(">")));
				}
				if (Pos + 1 < Length && IsNameStart(Text[Pos + 1]))
				{
					return ParseTag(Out);
				}
			}
			else if (C == TEXT('{') && Pos + 1 < Length && Text[Pos + 1] == TEXT('{'))
			{
				return Finish(Out, ETokenType::Data, SkipDataBrackets(Text, Pos));
			}

			int32 End = Pos + 1;
			while (End < Length && Text[End] != TEXT('<') && !(Text[End] == TEXT('{') && End + 1 < Length && Text[End + 1] == TEXT('{')))
			{
				++End;
			}
			return Finish(Out, ETokenType::Text, End);
		}

	private:
		bool Finish(FToken& Out, ETokenType Type, int32 End)
		{
			Out.Type = Type;
			Out.End = End;
			Pos = End;
			return true;
		}

		int32 FindRawClose(int32 From) const
		{
			for (int32 Index = Text.Find(TEXT("</"), From, ESearchCase::CaseSensitive); Index != INDEX_NONE; Index = Text.Find(TEXT("</"), Index + 2, ESearchCase::CaseSensitive))
			{
				if (Text.Mid(Index + 2, RawCloseTag.Len()).Equals(RawCloseTag, ESearchCase::IgnoreCase))
				{
					return Index;
				}
			}
			return INDEX_NONE;
		}

		bool IsTagEnd(int32 Index) const
		{
			return Text[Index] == TEXT('>') || (Text[Index] == TEXT('/') && Index + 1 < Text.Len() && Text[Index + 1] == TEXT('>'));
		}

		bool ParseTag(FToken& Out)
		{
			const int32 Length = Text.Len();
			int32 Index = Pos + 1;
			while (Index < Length && IsNameChar(Text[Index]))
			{
				++Index;
			}
			Out.Tag.Name = Text.Mid(Pos + 1, Index - Pos - 1);

			while (Index < Length)
			{
				while (Index < Length && FChar::IsWhitespace(Text[Index]))
				{
					++Index;
				}
				if (Index >= Length)
				{
					break;
				}
				if (Text[Index] == TEXT('>'))
				{
					if (IsNamed(Out.Tag.Name, TEXT("style")) || IsNamed(Out.Tag.Name, TEXT("script")))
					{
						RawCloseTag = Out.Tag.Name;
					}
					return Finish(Out, ETokenType::StartTag, Index + 1);
				}
				if (IsTagEnd(Index))
				{
					Out.Tag.bSelfClosing = true;
					return Finish(Out, ETokenType::StartTag, Index + 2);
				}

				const int32 NameStart = Index;
				while (Index < Length && !FChar::IsWhitespace(Text[Index]) && Text[Index] != TEXT('=') && !IsTagEnd(Index))
				{
					++Index;
				}
				if (Index == NameStart)
				{
					++Index;
					continue;
				}

				FAttribute& Attribute = Out.Tag.Attributes.AddDefaulted_GetRef();
				Attribute.Name = Text.Mid(NameStart, Index - NameStart);
				Attribute.NameStart = NameStart;
				Attribute.End = Index;

				int32 ValueStart = Index;
				while (ValueStart < Length && FChar::IsWhitespace(Text[ValueStart]))
				{
					++ValueStart;
				}
				if (ValueStart >= Length || Text[ValueStart] != TEXT('='))
				{
					continue;
				}

				++ValueStart;
				while (ValueStart < Length && FChar::IsWhitespace(Text[ValueStart]))
				{
					++ValueStart;
				}
				Attribute.bHasValue = true;
				Attribute.ValueSpanStart = ValueStart;
				if (ValueStart < Length && (Text[ValueStart] == TEXT('"') || Text[ValueStart] == TEXT('\'')))
				{
					Attribute.Quote = Text[ValueStart];
					int32 Close = ValueStart + 1;
					while (Close < Length && Text[Close] != Attribute.Quote)
					{
						++Close;
					}
					if (Close >= Length)
					{
						break;
					}
					Attribute.RawValue = Text.Mid(ValueStart + 1, Close - ValueStart - 1);
					Index = Close + 1;
				}
				else
				{
					Index = ValueStart;
					while (Index < Length && !FChar::IsWhitespace(Text[Index]) && !IsTagEnd(Index))
					{
						++Index;
					}
					Attribute.RawValue = Text.Mid(ValueStart, Index - ValueStart);
				}
				Attribute.ValueSpanEnd = Index;
				Attribute.End = Index;
			}

			Out.bMalformed = true;
			return Finish(Out, ETokenType::Text, Length);
		}

		FStringView Text;
		int32 Pos = 0;
		FStringView RawCloseTag;
	};

	class FLineCounter
	{
	public:
		FLineCounter(FStringView InText, int32 FirstLine)
			: Text(InText)
			, Line(FirstLine)
		{
		}

		int32 At(int32 Target)
		{
			const int32 Clamped = FMath::Min(Target, Text.Len());
			for (; Offset < Clamped; ++Offset)
			{
				if (Text[Offset] == TEXT('\n'))
				{
					++Line;
				}
			}
			return Line;
		}

	private:
		FStringView Text;
		int32 Offset = 0;
		int32 Line = 1;
	};

	void ReportError(int32& ErrorCount, const FString& File, int32 Line, const FString& Message)
	{
		UE_LOGF(LogArcRmlUI, Error, "ArcRmlUI: %ls(%d): %ls", *File, Line, *Message);
		++ErrorCount;
	}

	FString DecodeXml(FStringView Value)
	{
		int32 Ampersand = INDEX_NONE;
		if (!Value.FindChar(TEXT('&'), Ampersand))
		{
			return FString(Value);
		}

		FString Result;
		Result.Reserve(Value.Len());
		for (int32 Index = 0; Index < Value.Len(); ++Index)
		{
			if (Value[Index] == TEXT('&'))
			{
				const int32 Semicolon = Value.Find(TEXT(";"), Index, ESearchCase::CaseSensitive);
				if (Semicolon != INDEX_NONE && Semicolon - Index <= 10)
				{
					const FStringView Entity = Value.Mid(Index + 1, Semicolon - Index - 1);
					uint32 Decoded = 0;
					if (Entity.Equals(TEXT("lt"), ESearchCase::IgnoreCase))
					{
						Decoded = TEXT('<');
					}
					else if (Entity.Equals(TEXT("gt"), ESearchCase::IgnoreCase))
					{
						Decoded = TEXT('>');
					}
					else if (Entity.Equals(TEXT("amp"), ESearchCase::IgnoreCase))
					{
						Decoded = TEXT('&');
					}
					else if (Entity.Equals(TEXT("quot"), ESearchCase::IgnoreCase))
					{
						Decoded = TEXT('"');
					}
					else if (Entity.Equals(TEXT("apos"), ESearchCase::IgnoreCase))
					{
						Decoded = TEXT('\'');
					}
					else if (Entity.StartsWith(TEXT("#x"), ESearchCase::IgnoreCase))
					{
						Decoded = static_cast<uint32>(FCString::Strtoi(*FString(Entity.RightChop(2)), nullptr, 16));
					}
					else if (Entity.StartsWith(TEXT('#')))
					{
						Decoded = static_cast<uint32>(FCString::Atoi(*FString(Entity.RightChop(1))));
					}

					if (Decoded != 0 && Decoded <= 0xFFFF)
					{
						Result.AppendChar(static_cast<TCHAR>(Decoded));
						Index = Semicolon;
						continue;
					}
				}
			}
			Result.AppendChar(Value[Index]);
		}
		return Result;
	}

	void AppendEncodedXml(FString& Out, FStringView Value)
	{
		for (const TCHAR C : Value)
		{
			switch (C)
			{
			case TEXT('&'):
				Out += TEXT("&amp;");
				break;
			case TEXT('<'):
				Out += TEXT("&lt;");
				break;
			case TEXT('>'):
				Out += TEXT("&gt;");
				break;
			case TEXT('"'):
				Out += TEXT("&quot;");
				break;
			default:
				Out.AppendChar(C);
				break;
			}
		}
	}

	FString KebabToCamel(FStringView Name)
	{
		FString Result;
		Result.Reserve(Name.Len());
		bool bUpper = false;
		for (const TCHAR C : Name)
		{
			if (C == TEXT('-'))
			{
				bUpper = true;
				continue;
			}
			Result.AppendChar(bUpper ? FChar::ToUpper(C) : C);
			bUpper = false;
		}
		return Result;
	}

	bool IsIdentifier(FStringView Name)
	{
		if (Name.IsEmpty() || !IsNameStart(Name[0]))
		{
			return false;
		}
		for (const TCHAR C : Name)
		{
			if (!IsIdentifierChar(C))
			{
				return false;
			}
		}
		return true;
	}

	bool IsPlainAddress(FStringView Expression)
	{
		int32 Index = 0;
		const int32 Length = Expression.Len();
		bool bExpectName = true;
		while (Index < Length)
		{
			if (bExpectName)
			{
				if (!IsNameStart(Expression[Index]))
				{
					return false;
				}
				while (Index < Length && IsIdentifierChar(Expression[Index]))
				{
					++Index;
				}
				bExpectName = false;
			}
			else if (Expression[Index] == TEXT('.'))
			{
				++Index;
				bExpectName = true;
			}
			else if (Expression[Index] == TEXT('['))
			{
				const int32 DigitsStart = ++Index;
				while (Index < Length && FChar::IsDigit(Expression[Index]))
				{
					++Index;
				}
				if (Index == DigitsStart || Index >= Length || Expression[Index] != TEXT(']'))
				{
					return false;
				}
				++Index;
			}
			else
			{
				return false;
			}
		}
		return Length > 0 && !bExpectName;
	}

	bool IsExpressionAttribute(FStringView Name)
	{
		for (const TCHAR* Attribute : ExpressionAttributes)
		{
			if (IsNamed(Name, Attribute))
			{
				return true;
			}
		}
		for (const TCHAR* Prefix : ExpressionAttributePrefixes)
		{
			if (Name.StartsWith(Prefix, ESearchCase::CaseSensitive))
			{
				return true;
			}
		}
		return false;
	}

	FString HostPositionStyle(const FString& Style)
	{
		TArray<FString> Kept;
		int32 Depth = 0;
		TCHAR Quote = 0;
		int32 Start = 0;
		for (int32 Index = 0; Index <= Style.Len(); ++Index)
		{
			const TCHAR C = Index < Style.Len() ? Style[Index] : TEXT(';');
			if (Quote != 0)
			{
				if (C == Quote)
				{
					Quote = 0;
				}
				continue;
			}
			if (C == TEXT('"') || C == TEXT('\''))
			{
				Quote = C;
			}
			else if (C == TEXT('('))
			{
				++Depth;
			}
			else if (C == TEXT(')'))
			{
				--Depth;
			}
			else if (C == TEXT(';') && Depth <= 0)
			{
				const FString Declaration = Style.Mid(Start, Index - Start);
				Start = Index + 1;
				int32 Colon = INDEX_NONE;
				if (!Declaration.FindChar(TEXT(':'), Colon))
				{
					continue;
				}
				const FString Property = Declaration.Left(Colon).TrimStartAndEnd().ToLower();
				for (const TCHAR* HostProperty : HostStyleProperties)
				{
					if (Property == HostProperty)
					{
						Kept.Add(Property + TEXT(": ") + Declaration.Mid(Colon + 1).TrimStartAndEnd());
						break;
					}
				}
			}
		}
		return FString::Join(Kept, TEXT("; "));
	}

	FString ImportName(const FTag& Tag)
	{
		const FAttribute* NameAttribute = Tag.Find(TEXT("name"));
		return NameAttribute ? DecodeXml(NameAttribute->RawValue).TrimStartAndEnd() : FString();
	}

	class FPropSubstitution
	{
	public:
		FPropSubstitution(TConstArrayView<FPropDeclaration> InProps, const TMap<FString, FPropValue>& InValues, const FString& InFile, int32& InErrorCount)
			: Props(InProps)
			, Values(InValues)
			, File(InFile)
			, ErrorCount(InErrorCount)
		{
		}

		void Run(FStringView Body, int32 BodyLine, FString& Out)
		{
			FTokenizer Tokenizer(Body);
			FLineCounter Lines(Body, BodyLine);
			FToken Token;
			while (Tokenizer.Next(Token))
			{
				Line = Lines.At(Token.Start);
				const FStringView Span = Body.Mid(Token.Start, Token.End - Token.Start);
				if (Token.Type == ETokenType::Data && Span.Contains(TEXT("$")))
				{
					AppendEncodedXml(Out, SubstituteBrackets(DecodeXml(Span), EExpressionKind::Text));
					continue;
				}
				if (Token.Type != ETokenType::StartTag)
				{
					Out.AppendChars(Span.GetData(), Span.Len());
					continue;
				}

				int32 Cursor = Token.Start;
				for (const FAttribute& Attribute : Token.Tag.Attributes)
				{
					if (!Attribute.bHasValue || !Attribute.RawValue.Contains(TEXT("$")))
					{
						continue;
					}

					FString Replaced;
					if (IsExpressionAttribute(Attribute.Name))
					{
						const EExpressionKind Kind = IsNamed(Attribute.Name, TEXT("data-for")) ? EExpressionKind::Container : EExpressionKind::Attribute;
						Replaced = SubstituteExpression(DecodeXml(Attribute.RawValue), Kind);
					}
					else if (Attribute.RawValue.Contains(TEXT("{{")))
					{
						Replaced = SubstituteBrackets(DecodeXml(Attribute.RawValue), EExpressionKind::Attribute);
					}
					else
					{
						continue;
					}

					AppendSpan(Out, Body, Cursor, Attribute.ValueSpanStart);
					Out.AppendChar(TEXT('"'));
					AppendEncodedXml(Out, Replaced);
					Out.AppendChar(TEXT('"'));
					Cursor = Attribute.ValueSpanEnd;
				}
				AppendSpan(Out, Body, Cursor, Token.End);
			}
		}

	private:
		FString SubstituteBrackets(const FString& Value, EExpressionKind Kind)
		{
			const FStringView ValueView(Value);
			FString Result;
			int32 Index = 0;
			while (Index < Value.Len())
			{
				const int32 Open = Value.Find(TEXT("{{"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Index);
				if (Open == INDEX_NONE)
				{
					break;
				}
				const int32 End = SkipDataBrackets(ValueView, Open);
				const bool bClosed = End >= Open + 4 && Value[End - 1] == TEXT('}') && Value[End - 2] == TEXT('}');
				AppendSpan(Result, ValueView, Index, Open);
				if (!bClosed)
				{
					Index = Open;
					break;
				}
				Result += TEXT("{{");
				Result += SubstituteExpression(ValueView.Mid(Open + 2, End - Open - 4), Kind);
				Result += TEXT("}}");
				Index = End;
			}
			AppendSpan(Result, ValueView, Index, Value.Len());
			return Result;
		}

		FString SubstituteExpression(FStringView Expression, EExpressionKind Kind)
		{
			FString Result;
			Result.Reserve(Expression.Len());
			bool bInString = false;
			const int32 Length = Expression.Len();
			for (int32 Index = 0; Index < Length; ++Index)
			{
				const TCHAR C = Expression[Index];
				if (bInString)
				{
					Result.AppendChar(C);
					if (C == TEXT('\\') && Index + 1 < Length)
					{
						Result.AppendChar(Expression[++Index]);
					}
					else if (C == TEXT('\''))
					{
						bInString = false;
					}
					continue;
				}
				if (C == TEXT('\''))
				{
					bInString = true;
					Result.AppendChar(C);
					continue;
				}
				if (C == TEXT('$') && Index + 1 < Length && IsNameStart(Expression[Index + 1]))
				{
					int32 NameEnd = Index + 1;
					while (NameEnd < Length && IsIdentifierChar(Expression[NameEnd]))
					{
						++NameEnd;
					}
					const FString Name(Expression.Mid(Index + 1, NameEnd - Index - 1));
					const bool bMemberAccess = NameEnd < Length && (Expression[NameEnd] == TEXT('.') || Expression[NameEnd] == TEXT('['));
					const FString Replacement = Resolve(Name, Kind, bMemberAccess);
					Result += Replacement;
					if (bMemberAccess && Replacement == TEXT("''"))
					{
						while (NameEnd < Length && (IsIdentifierChar(Expression[NameEnd]) || Expression[NameEnd] == TEXT('.') || Expression[NameEnd] == TEXT('[') || Expression[NameEnd] == TEXT(']')))
						{
							++NameEnd;
						}
					}
					Index = NameEnd - 1;
					continue;
				}
				Result.AppendChar(C);
			}
			return Result;
		}

		FString Resolve(const FString& Name, EExpressionKind Kind, bool bMemberAccess)
		{
			const FPropDeclaration* Declaration = nullptr;
			for (const FPropDeclaration& Prop : Props)
			{
				if (Prop.Name == Name)
				{
					Declaration = &Prop;
					break;
				}
			}
			if (!Declaration)
			{
				ReportError(ErrorCount, File, Line, FString::Printf(TEXT("$%s is not a declared prop"), *Name));
				return TEXT("''");
			}

			if (const FPropValue* Value = Values.Find(Name))
			{
				if (Value->bBound && Value->bPath)
				{
					return Value->Text;
				}
				if (Kind == EExpressionKind::Container)
				{
					ReportError(ErrorCount, File, Line, FString::Printf(TEXT("$%s is a data-for container; pass it as a whole-attribute {{ path }}"), *Name));
					return ArcRmlComponents::EmptyArrayName;
				}
				if (bMemberAccess)
				{
					ReportError(ErrorCount, File, Line, FString::Printf(TEXT("$%s needs a member access, which requires a whole-attribute {{ path }} value"), *Name));
					return TEXT("''");
				}
				if (!Value->bBound)
				{
					return Literal(Name, Value->Text, Kind);
				}
				if (Kind == EExpressionKind::Text && Value->Text.Contains(TEXT("\\'")))
				{
					ReportError(ErrorCount, File, Line, FString::Printf(TEXT("$%s binds \"%s\", whose escaped quote a {{ }} text expression cannot hold"), *Name, *Value->Text));
					return TEXT("''");
				}
				return TEXT("(") + Value->Text + TEXT(")");
			}

			if (Declaration->bHasDefault && Declaration->Default.TrimStartAndEnd() == TEXT("[]"))
			{
				return ArcRmlComponents::EmptyArrayName;
			}
			if (Kind == EExpressionKind::Container)
			{
				if (Declaration->bHasDefault)
				{
					ReportError(ErrorCount, File, Line, FString::Printf(TEXT("$%s is a data-for container but its default is not []"), *Name));
				}
				return ArcRmlComponents::EmptyArrayName;
			}
			if (bMemberAccess)
			{
				ReportError(ErrorCount, File, Line, FString::Printf(TEXT("$%s needs a member access, which a missing value cannot provide"), *Name));
				return TEXT("''");
			}
			if (!Declaration->bHasDefault)
			{
				return TEXT("''");
			}
			return Literal(Name, Declaration->Default, Kind);
		}

		// Quoting follows DataExpression.cpp:571-592; text views toggle on every quote (XMLParseTools.cpp:129-130), so text cannot hold one.
		FString Literal(const FString& Name, const FString& Value, EExpressionKind Kind)
		{
			if (Kind == EExpressionKind::Text && Value.Contains(TEXT("'")))
			{
				ReportError(ErrorCount, File, Line, FString::Printf(TEXT("$%s value \"%s\" contains a single quote, which a {{ }} text expression cannot quote"), *Name, *Value));
				return TEXT("''");
			}

			FString Result = TEXT("'");
			for (const TCHAR C : Value)
			{
				if (C == TEXT('\\') || C == TEXT('\''))
				{
					Result.AppendChar(TEXT('\\'));
				}
				Result.AppendChar(C);
			}
			Result.AppendChar(TEXT('\''));
			return Result;
		}

		TConstArrayView<FPropDeclaration> Props;
		const TMap<FString, FPropValue>& Values;
		const FString& File;
		int32& ErrorCount;
		int32 Line = 1;
	};

	void CheckChildIds(int32& ErrorCount, const TArray<FString>& Ids, int32 SlotCount, const FString& ComponentName, const FString& File, int32 Line)
	{
		TSet<FString> Seen;
		for (const FString& Id : Ids)
		{
			bool bAlreadyInSet = false;
			Seen.Add(Id, &bAlreadyInSet);
			if (bAlreadyInSet)
			{
				ReportError(ErrorCount, File, Line, FString::Printf(TEXT("children of '%s' repeat the id '%s'"), *ComponentName, *Id));
			}
		}

		if (SlotCount > 1 && Seen.Num() > 0)
		{
			ReportError(ErrorCount, File, Line, FString::Printf(TEXT("children of '%s' carry ids but go into %d slots, which duplicates them"), *ComponentName, SlotCount));
		}
	}

	struct FOpenElement
	{
		FStringView Name;
		bool bModel = false;
		bool bRaw = false;
	};

	struct FTextRun
	{
		int32 Start = INDEX_NONE;
		bool bAfterTag = false;
		bool bHasData = false;
		bool bInModel = false;
		bool bInRaw = false;
	};

	Rml::String ToRmlUtf8(FStringView Value)
	{
		const FTCHARToUTF8 Converted(Value.GetData(), Value.Len());
		return Rml::String(reinterpret_cast<const char*>(Converted.Get()), static_cast<size_t>(Converted.Length()));
	}

	bool EvaluateConstantValue(FStringView RawValue, Rml::DataTypeRegister& TypeRegister, Rml::Variant& OutValue)
	{
		Rml::String Expression = ToRmlUtf8(RawValue);
		if (Expression.find('&') != Rml::String::npos)
		{
			Expression = Rml::StringUtilities::DecodeRml(Expression);
		}
		return Rml::EvaluateConstantDataExpression(Expression, TypeRegister, OutValue) == Rml::ConstantDataExpressionResult::Constant;
	}

	bool ScanDataBrackets(bool& bInBrackets, bool& bInString, char C, char Previous)
	{
		if (bInBrackets)
		{
			if (C == '\'')
			{
				bInString = !bInString;
			}
			if (!bInString)
			{
				if (C == '}' && Previous == '}')
				{
					bInBrackets = false;
				}
				else if ((C == '{' && Previous == '{') || (Previous == '}' && C != '}' && C != '\'') || (Previous == '/' && C == '>') || (Previous == '<' && C == '/'))
				{
					return false;
				}
			}
			return true;
		}
		if (C == '{' && Previous == '{')
		{
			bInBrackets = true;
		}
		else if (C == '}' && Previous == '}')
		{
			return false;
		}
		return true;
	}

	bool IsBlankRml(const Rml::String& Text)
	{
		for (const char C : Text)
		{
			if (!Rml::StringUtilities::IsWhitespace(C))
			{
				return false;
			}
		}
		return true;
	}

	bool HasValidDataBrackets(const Rml::String& Text)
	{
		bool bInBrackets = false;
		bool bInString = false;
		char Previous = 0;
		for (const char C : Text)
		{
			if (!ScanDataBrackets(bInBrackets, bInString, C, Previous))
			{
				return false;
			}
			Previous = C;
		}
		return true;
	}

	bool HasNoDataBrackets(const Rml::String& Text)
	{
		bool bInBrackets = false;
		bool bInString = false;
		char Previous = 0;
		for (const char C : Text)
		{
			if (!ScanDataBrackets(bInBrackets, bInString, C, Previous) || bInBrackets)
			{
				return false;
			}
			Previous = C;
		}
		return true;
	}

	bool FoldTextRun(FString& Out, int32 Start, Rml::DataTypeRegister& TypeRegister)
	{
		const Rml::String Raw = ToRmlUtf8(FStringView(Out).Mid(Start));
		if (!HasValidDataBrackets(Raw))
		{
			return false;
		}

		const Rml::String Decoded = Raw.find('&') == Rml::String::npos ? Raw : Rml::StringUtilities::DecodeRml(Raw);
		Rml::String Folded;
		Folded.reserve(Decoded.size());
		bool bHasExpression = false;
		bool bInBrackets = false;
		bool bInString = false;
		size_t BracketsBegin = 0;
		char Previous = 0;
		for (size_t Index = 0; Index < Decoded.size(); ++Index)
		{
			char C = Decoded[Index];
			const bool bWasInBrackets = bInBrackets;
			if (!ScanDataBrackets(bInBrackets, bInString, C, Previous))
			{
				return false;
			}
			if (!bWasInBrackets && bInBrackets)
			{
				BracketsBegin = Index;
			}
			else if (bWasInBrackets && !bInBrackets)
			{
				Rml::Variant Value;
				const Rml::String Expression = Decoded.substr(BracketsBegin + 1, Index - BracketsBegin - 2);
				if (Rml::EvaluateConstantDataExpression(Expression, TypeRegister, Value) != Rml::ConstantDataExpressionResult::Constant)
				{
					return false;
				}
				Folded += Value.Get<Rml::String>();
				bHasExpression = true;
				C = 0;
			}
			else if (!bInBrackets && Previous != 0)
			{
				Folded.push_back(Previous);
			}
			Previous = C;
		}
		if (bInBrackets || !bHasExpression)
		{
			return false;
		}
		if (Previous != 0)
		{
			Folded.push_back(Previous);
		}

		const Rml::String Encoded = Rml::StringUtilities::EncodeRml(Folded);
		if (IsBlankRml(Encoded) || !HasNoDataBrackets(Encoded))
		{
			return false;
		}
		Out.LeftInline(Start, EAllowShrinking::No);
		Out += ArcRmlStrings::ToFString(Encoded);
		return true;
	}

	bool HasAttributePrefix(const FTag& Tag, const TCHAR* Prefix)
	{
		for (const FAttribute& Attribute : Tag.Attributes)
		{
			if (Attribute.Name.StartsWith(Prefix, ESearchCase::CaseSensitive))
			{
				return true;
			}
		}
		return false;
	}

	bool HasAttributeNamed(const FTag& Tag, FStringView Name)
	{
		for (const FAttribute& Attribute : Tag.Attributes)
		{
			if (Attribute.Name.Equals(Name, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}
		return false;
	}

	bool HasClassName(FStringView Classes, FStringView ClassName)
	{
		int32 Index = 0;
		while (Index < Classes.Len())
		{
			while (Index < Classes.Len() && FChar::IsWhitespace(Classes[Index]))
			{
				++Index;
			}
			const int32 Start = Index;
			while (Index < Classes.Len() && !FChar::IsWhitespace(Classes[Index]))
			{
				++Index;
			}
			if (Index > Start && Classes.Mid(Start, Index - Start).Equals(ClassName, ESearchCase::CaseSensitive))
			{
				return true;
			}
		}
		return false;
	}

	bool IsFoldableStyleValue(const FString& Value)
	{
		if (Value.TrimStartAndEnd().IsEmpty() || Value.Contains(TEXT("/*")))
		{
			return false;
		}
		for (const TCHAR C : Value)
		{
			if (C == TEXT(';') || C == TEXT('{') || C == TEXT('}') || C == TEXT('"') || C == TEXT('\'') || C == TEXT('!'))
			{
				return false;
			}
		}
		return true;
	}

	void AppendStartTag(FString& Out, FStringView Text, const FToken& Token, const FAttribute* Removed, bool bFoldBindings, Rml::DataTypeRegister& TypeRegister)
	{
		const FTag& Tag = Token.Tag;
		TArray<const FAttribute*, TInlineAllocator<16>> Dropped;
		if (Removed)
		{
			Dropped.Add(Removed);
		}

		const FAttribute* StyleAttribute = Tag.Find(TEXT("style"));
		const FAttribute* ClassAttribute = Tag.Find(TEXT("class"));
		FString AddedAttributes;
		FString AddedStyle;
		FString AddedClasses;
		if (bFoldBindings)
		{
			const bool bStyleFoldable = !HasAttributeNamed(Tag, TEXT("data-attr-style")) && !HasAttributeNamed(Tag, TEXT("data-attrif-style"))
				&& (!StyleAttribute || (StyleAttribute->Quote != 0 && !StyleAttribute->RawValue.Contains(TEXT("!"))));
			const bool bClassFoldable = !HasAttributeNamed(Tag, TEXT("data-attr-class")) && !HasAttributeNamed(Tag, TEXT("data-attrif-class"))
				&& (!ClassAttribute || ClassAttribute->Quote != 0);
			const FString Classes = ClassAttribute ? DecodeXml(ClassAttribute->RawValue) : FString();

			for (const FAttribute& Attribute : Tag.Attributes)
			{
				if (!Attribute.bHasValue)
				{
					continue;
				}

				const FStringView Name = Attribute.Name;
				if (Name.StartsWith(TEXT("data-attr-"), ESearchCase::CaseSensitive))
				{
					const FStringView Target = Name.RightChop(10);
					const FString ConditionalName = FString(TEXT("data-attrif-")) + FString(Target);
					if (Target.IsEmpty() || Target.StartsWith(TEXT("data-"), ESearchCase::IgnoreCase) || HasAttributeNamed(Tag, Target)
						|| HasAttributeNamed(Tag, ConditionalName)
						|| (IsNamed(Target, TEXT("class")) && HasAttributePrefix(Tag, TEXT("data-class-")))
						|| (IsNamed(Target, TEXT("style")) && HasAttributePrefix(Tag, TEXT("data-style-"))))
					{
						continue;
					}
					Rml::Variant Value;
					if (!EvaluateConstantValue(Attribute.RawValue, TypeRegister, Value))
					{
						continue;
					}
					AddedAttributes.AppendChar(TEXT(' '));
					AddedAttributes.Append(Target);
					AddedAttributes += TEXT("=\"");
					AddedAttributes += ArcRmlStrings::ToFString(Rml::StringUtilities::EncodeRml(Value.Get<Rml::String>()));
					AddedAttributes.AppendChar(TEXT('"'));
					Dropped.Add(&Attribute);
				}
				else if (Name.StartsWith(TEXT("data-style-"), ESearchCase::CaseSensitive))
				{
					const FStringView Property = Name.RightChop(11);
					Rml::Variant Value;
					if (Property.IsEmpty() || !bStyleFoldable || !EvaluateConstantValue(Attribute.RawValue, TypeRegister, Value))
					{
						continue;
					}
					const FString PropertyValue = ArcRmlStrings::ToFString(Value.Get<Rml::String>());
					if (!IsFoldableStyleValue(PropertyValue))
					{
						continue;
					}
					AddedStyle += AddedStyle.IsEmpty() ? TEXT("") : TEXT("; ");
					AddedStyle.Append(Property);
					AddedStyle += TEXT(": ");
					AddedStyle += PropertyValue;
					Dropped.Add(&Attribute);
				}
				else if (Name.StartsWith(TEXT("data-class-"), ESearchCase::CaseSensitive))
				{
					const FStringView ClassName = Name.RightChop(11);
					Rml::Variant Value;
					if (ClassName.IsEmpty() || !bClassFoldable || !EvaluateConstantValue(Attribute.RawValue, TypeRegister, Value))
					{
						continue;
					}
					const bool bPresent = HasClassName(Classes, ClassName) || HasClassName(AddedClasses, ClassName);
					if (Value.Get<bool>())
					{
						if (!bPresent)
						{
							AddedClasses.AppendChar(TEXT(' '));
							AddedClasses.Append(ClassName);
						}
					}
					else if (bPresent)
					{
						continue;
					}
					Dropped.Add(&Attribute);
				}
			}
		}

		if (Dropped.IsEmpty())
		{
			AppendSpan(Out, Text, Token.Start, Token.End);
			return;
		}

		int32 Cursor = Token.Start;
		for (const FAttribute& Attribute : Tag.Attributes)
		{
			if (Dropped.Contains(&Attribute))
			{
				int32 CutStart = Attribute.NameStart;
				while (CutStart > Cursor && FChar::IsWhitespace(Text[CutStart - 1]))
				{
					--CutStart;
				}
				AppendSpan(Out, Text, Cursor, CutStart);
				Cursor = Attribute.End;
				continue;
			}

			const bool bAppendStyle = &Attribute == StyleAttribute && !AddedStyle.IsEmpty();
			const bool bAppendClasses = &Attribute == ClassAttribute && !AddedClasses.IsEmpty();
			if (bAppendStyle || bAppendClasses)
			{
				const int32 CloseQuote = Attribute.ValueSpanEnd - 1;
				AppendSpan(Out, Text, Cursor, CloseQuote);
				if (bAppendStyle)
				{
					const FString Existing = FString(Attribute.RawValue).TrimEnd();
					Out += Existing.IsEmpty() ? TEXT("") : (Existing.EndsWith(TEXT(";")) ? TEXT(" ") : TEXT("; "));
					AppendEncodedXml(Out, AddedStyle);
				}
				else
				{
					AppendEncodedXml(Out, Attribute.RawValue.TrimStartAndEnd().IsEmpty() ? FStringView(AddedClasses).RightChop(1) : FStringView(AddedClasses));
				}
				Cursor = CloseQuote;
			}
		}

		const int32 TagEnd = Token.End - (Tag.bSelfClosing ? 2 : 1);
		AppendSpan(Out, Text, Cursor, TagEnd);
		if (!StyleAttribute && !AddedStyle.IsEmpty())
		{
			Out += TEXT(" style=\"");
			AppendEncodedXml(Out, AddedStyle);
			Out.AppendChar(TEXT('"'));
		}
		if (!ClassAttribute && !AddedClasses.IsEmpty())
		{
			Out += TEXT(" class=\"");
			AppendEncodedXml(Out, FStringView(AddedClasses).RightChop(1));
			Out.AppendChar(TEXT('"'));
		}
		Out += AddedAttributes;
		AppendSpan(Out, Text, TagEnd, Token.End);
	}

	void CloseTextRun(FTextRun& Run, FString& Out, bool bTagFollows, Rml::DataTypeRegister& TypeRegister)
	{
		if (Run.Start != INDEX_NONE && bTagFollows && Run.bAfterTag && Run.bHasData && Run.bInModel && !Run.bInRaw)
		{
			FoldTextRun(Out, Run.Start, TypeRegister);
		}
		Run = FTextRun();
	}
}

struct FArcRmlComponentExpander::FComponent
{
	FString Path;
	TArray<ArcRmlComponentExpanderPrivate::FPropDeclaration> Props;
	TArray<FString> ImportNames;
	FString Body;
	int32 BodyLine = 1;
	FString StyleSheetPath;
	bool bBodyHasProps = false;

	bool HasProp(const FString& Name) const
	{
		for (const ArcRmlComponentExpanderPrivate::FPropDeclaration& Prop : Props)
		{
			if (Prop.Name == Name)
			{
				return true;
			}
		}
		return false;
	}
};

struct FArcRmlComponentExpander::FCursor
{
	FCursor(FStringView InText, int32 FirstLine)
		: Text(InText)
		, Tokenizer(InText)
		, Lines(InText, FirstLine)
	{
	}

	FStringView Text;
	ArcRmlComponentExpanderPrivate::FTokenizer Tokenizer;
	ArcRmlComponentExpanderPrivate::FLineCounter Lines;
	ArcRmlComponentExpanderPrivate::FToken Token;
};

struct FArcRmlComponentExpander::FExpandState
{
	TArray<FString> UsedComponents;
	TArray<FString> Stack;
	int32 HeadInsert = INDEX_NONE;
	int32 RootInsert = INDEX_NONE;
};

struct FArcRmlComponentExpander::FSlot
{
	const FString* Children = nullptr;
	const TArray<FString>* Ids = nullptr;
	int32 Count = 0;
};

struct FArcRmlComponentExpander::FScope
{
	const FString* File = nullptr;
	bool bInModel = false;
	bool bDocument = false;
	FSlot* Slot = nullptr;
	TArray<FString>* Ids = nullptr;
};

#endif

void ArcRmlComponents::RegisterTransforms(Rml::TransformFuncRegister& Register)
{
	Register.Register("arcicon", [](const Rml::VariantList& Arguments) -> Rml::Variant
	{
		Rml::String Value;
		if (Arguments.size() != 1 || !Arguments[0].GetInto(Value) || Value.empty())
		{
			return Rml::Variant(Rml::String());
		}
		if (Value.rfind("asset:", 0) == 0)
		{
			return Rml::Variant(Value);
		}
		if (Value.rfind("img:", 0) == 0)
		{
			const Rml::String File = Value.substr(4);
			const size_t Slash = File.find_last_of("/\\");
			return Rml::Variant(Rml::String(ArcRmlComponents::IconDirectory) + (Slash == Rml::String::npos ? File : File.substr(Slash + 1)));
		}
		return Rml::Variant(Rml::String(ArcRmlComponents::IconDirectory) + Value + ".svg");
	});

	Register.Register("arcisimg", [](const Rml::VariantList& Arguments) -> Rml::Variant
	{
		Rml::String Value;
		return Rml::Variant(Arguments.size() == 1 && Arguments[0].GetInto(Value) && (Value.rfind("img:", 0) == 0 || Value.rfind("asset:", 0) == 0));
	});

	Register.Register("arcclamp", [](const Rml::VariantList& Arguments) -> Rml::Variant
	{
		double Value = 0.0;
		double Low = 0.0;
		double High = 0.0;
		if (Arguments.size() != 3 || !Arguments[0].GetInto(Value) || !Arguments[1].GetInto(Low) || !Arguments[2].GetInto(High))
		{
			return Rml::Variant();
		}
		return Rml::Variant(FMath::Clamp(Value, Low, High));
	});

	Register.Register("arcmin", [](const Rml::VariantList& Arguments) -> Rml::Variant
	{
		double Value = 0.0;
		double Other = 0.0;
		if (Arguments.size() != 2 || !Arguments[0].GetInto(Value) || !Arguments[1].GetInto(Other))
		{
			return Rml::Variant();
		}
		return Rml::Variant(FMath::Min(Value, Other));
	});

	Register.Register("arcmax", [](const Rml::VariantList& Arguments) -> Rml::Variant
	{
		double Value = 0.0;
		double Other = 0.0;
		if (Arguments.size() != 2 || !Arguments[0].GetInto(Value) || !Arguments[1].GetInto(Other))
		{
			return Rml::Variant();
		}
		return Rml::Variant(FMath::Max(Value, Other));
	});
}

#if WITH_EDITOR
FArcRmlComponentExpander::FArcRmlComponentExpander(Rml::FileInterface& InFileInterface, const FArcRmlContentRoots& InRoots)
	: FileInterface(InFileInterface)
	, Roots(InRoots)
	, TypeRegister(MakeUnique<Rml::DataTypeRegister>())
{
	ArcRmlComponents::RegisterTransforms(*TypeRegister->GetTransformFuncRegister());
}

FArcRmlComponentExpander::~FArcRmlComponentExpander() = default;

bool FArcRmlComponentExpander::ReadFile(const FString& Path, FString& OutText)
{
	Rml::String Data;
	if (!FileInterface.LoadFile(ArcRmlStrings::ToRmlString(Path), Data))
	{
		return false;
	}
	OutText = ArcRmlStrings::ToFString(Data);
	if (OutText.Len() > 0 && OutText[0] == static_cast<TCHAR>(0xFEFF))
	{
		OutText.RemoveAt(0, 1);
	}
	return true;
}

bool FArcRmlComponentExpander::FileExists(const FString& Path)
{
	const Rml::FileHandle Handle = FileInterface.Open(ArcRmlStrings::ToRmlString(Path));
	if (!Handle)
	{
		return false;
	}
	FileInterface.Close(Handle);
	return true;
}

bool FArcRmlComponentExpander::ExpandDocument(const FString& DocumentPath, FString& OutText)
{
	using namespace ArcRmlComponentExpanderPrivate;

	ErrorCount = 0;
	FString Source;
	if (!ReadFile(DocumentPath, Source))
	{
		return false;
	}

	OutText.Reset(Source.Len());
	FExpandState State;
	FScope Scope;
	Scope.File = &DocumentPath;
	Scope.bDocument = true;
	Expand(Source, 1, Scope, State, OutText);

	FString Links;
	for (const FString& Name : State.UsedComponents)
	{
		const TUniquePtr<FComponent>* Component = Components.Find(Name);
		if (Component && !(*Component)->StyleSheetPath.IsEmpty())
		{
			Links += FString::Printf(TEXT("<link type=\"text/rcss\" href=\"%s\"/>"), *(*Component)->StyleSheetPath);
		}
	}

	if (Links.IsEmpty())
	{
		return ErrorCount == 0;
	}
	if (State.HeadInsert != INDEX_NONE)
	{
		OutText.InsertAt(State.HeadInsert, Links);
	}
	else if (State.RootInsert != INDEX_NONE)
	{
		OutText.InsertAt(State.RootInsert, TEXT("<head>") + Links + TEXT("</head>"));
	}
	else
	{
		ReportError(ErrorCount, DocumentPath, 1, TEXT("the document has no <rml> root to receive the component style sheets"));
	}
	return ErrorCount == 0;
}

const FArcRmlComponentExpander::FComponent* FArcRmlComponentExpander::FindComponent(const FString& Name, const FString& File, int32 Line)
{
	using namespace ArcRmlComponentExpanderPrivate;

	if (const TUniquePtr<FComponent>* Found = Components.Find(Name))
	{
		return Found->Get();
	}

	if (!IsIdentifier(Name))
	{
		ReportError(ErrorCount, File, Line, FString::Printf(TEXT("'%s' is not a component name"), *Name));
		return nullptr;
	}

	const FString RelativePath = FString(ArcRmlPaths::ComponentDirectory) + Name + TEXT(".rml");
	TArray<FString> Candidates;
	for (const FArcRmlContentRoot& Root : Roots.Roots)
	{
		FString Candidate = ArcRmlPaths::MakePackagePath(Root, RelativePath);
		if (FileExists(Candidate))
		{
			Candidates.Add(MoveTemp(Candidate));
		}
	}
	if (Candidates.IsEmpty())
	{
		ReportError(ErrorCount, File, Line, FString::Printf(TEXT("unknown component '%s': no content root has %s"), *Name, *RelativePath));
		return nullptr;
	}
	if (Candidates.Num() > 1)
	{
		ReportError(ErrorCount, File, Line, FString::Printf(TEXT("component '%s' is defined in more than one content root: %s"), *Name, *FString::Join(Candidates, TEXT(", "))));
		return nullptr;
	}

	const FString& Path = Candidates[0];
	FString Text;
	if (!ReadFile(Path, Text))
	{
		ReportError(ErrorCount, File, Line, FString::Printf(TEXT("unknown component '%s': %s cannot be read"), *Name, *Path));
		return nullptr;
	}

	TUniquePtr<FComponent> Component = LoadComponent(Path, Text);
	if (!Component)
	{
		return nullptr;
	}
	const FString StyleSheetPath = FPaths::ChangeExtension(Path, TEXT("rcss"));
	if (FileExists(StyleSheetPath))
	{
		Component->StyleSheetPath = StyleSheetPath;
	}

	const FComponent* Result = Component.Get();
	Components.Add(Name, MoveTemp(Component));
	return Result;
}

TUniquePtr<FArcRmlComponentExpander::FComponent> FArcRmlComponentExpander::LoadComponent(const FString& Path, const FString& Text)
{
	using namespace ArcRmlComponentExpanderPrivate;

	TUniquePtr<FComponent> Component = MakeUnique<FComponent>();
	Component->Path = Path;

	const FStringView TextView(Text);
	FTokenizer Tokenizer(TextView);
	FLineCounter Lines(TextView, 1);
	FToken Token;
	TArray<FString> OpenImports;
	bool bInRoot = false;
	int32 BodyStart = INDEX_NONE;
	int32 BodyEnd = INDEX_NONE;
	while (Tokenizer.Next(Token))
	{
		const int32 Line = Lines.At(Token.Start);
		if (Token.bMalformed)
		{
			ReportError(ErrorCount, Path, Line, TEXT("unterminated tag"));
			return nullptr;
		}

		const bool bSkippable = Token.Type == ETokenType::Markup || (Token.Type == ETokenType::Text && IsBlank(TextView, Token.Start, Token.End));
		if (!bInRoot)
		{
			if (bSkippable)
			{
				continue;
			}
			if (Token.Type == ETokenType::StartTag && IsNamed(Token.Tag.Name, ComponentTagName) && !Token.Tag.bSelfClosing)
			{
				bInRoot = true;
				continue;
			}
			ReportError(ErrorCount, Path, Line, TEXT("the root element must be <component>"));
			return nullptr;
		}

		if (BodyStart == INDEX_NONE)
		{
			if (bSkippable)
			{
				continue;
			}
			if (Token.Type == ETokenType::StartTag && IsNamed(Token.Tag.Name, PropTagName))
			{
				const FAttribute* NameAttribute = Token.Tag.Find(TEXT("name"));
				const FString PropName = NameAttribute ? DecodeXml(NameAttribute->RawValue) : FString();
				if (!Token.Tag.bSelfClosing)
				{
					ReportError(ErrorCount, Path, Line, TEXT("<prop> must be self-closing"));
				}
				else if (!IsIdentifier(PropName) || PropName == TEXT("name"))
				{
					ReportError(ErrorCount, Path, Line, FString::Printf(TEXT("'%s' is not a valid prop name"), *PropName));
				}
				else if (Component->HasProp(PropName))
				{
					ReportError(ErrorCount, Path, Line, FString::Printf(TEXT("prop '%s' is declared twice"), *PropName));
				}
				else
				{
					FPropDeclaration& Prop = Component->Props.AddDefaulted_GetRef();
					Prop.Name = PropName;
					if (const FAttribute* DefaultAttribute = Token.Tag.Find(TEXT("default")))
					{
						Prop.bHasDefault = DefaultAttribute->bHasValue;
						Prop.Default = DecodeXml(DefaultAttribute->RawValue);
					}
				}
				continue;
			}
			BodyStart = Token.Start;
			Component->BodyLine = Line;
		}

		if (Token.Type == ETokenType::EndTag && IsNamed(Token.Tag.Name, ComponentTagName))
		{
			BodyEnd = Token.Start;
		}
		if (BodyEnd != INDEX_NONE)
		{
			continue;
		}
		if (Token.Type == ETokenType::StartTag && IsNamed(Token.Tag.Name, ImportTagName))
		{
			if (Token.Tag.bSelfClosing)
			{
				Component->ImportNames.Add(ImportName(Token.Tag));
			}
			else
			{
				OpenImports.Add(ImportName(Token.Tag));
			}
		}
		else if (Token.Type == ETokenType::EndTag && IsNamed(Token.Tag.Name, ImportTagName) && OpenImports.Num() > 0)
		{
			Component->ImportNames.Add(OpenImports.Pop(EAllowShrinking::No));
		}
	}

	if (!bInRoot || BodyEnd == INDEX_NONE)
	{
		ReportError(ErrorCount, Path, 1, TEXT("missing <component> root or its closing tag"));
		return nullptr;
	}

	Component->Body = Text.Mid(BodyStart, BodyEnd - BodyStart);
	Component->bBodyHasProps = Component->Body.Contains(TEXT("$"), ESearchCase::CaseSensitive);
	return Component;
}

bool FArcRmlComponentExpander::EvaluateConstantCondition(FStringView RawValue, bool& bOutValue)
{
	using namespace ArcRmlComponentExpanderPrivate;

	Rml::Variant Value;
	if (!EvaluateConstantValue(RawValue, *TypeRegister, Value))
	{
		return false;
	}
	bOutValue = Value.Get<bool>();
	return true;
}

void FArcRmlComponentExpander::Expand(FStringView Text, int32 FirstLine, const FScope& Scope, FExpandState& State, FString& Out)
{
	FCursor Cursor(Text, FirstLine);
	ExpandTokens(Cursor, Scope, State, Out, false);
}

bool FArcRmlComponentExpander::ExpandTokens(FCursor& Cursor, const FScope& Scope, FExpandState& State, FString& Out, bool bUntilImportClose)
{
	using namespace ArcRmlComponentExpanderPrivate;

	const FStringView Text = Cursor.Text;
	FToken& Token = Cursor.Token;
	TArray<FOpenElement, TInlineAllocator<32>> OpenElements;
	int32 ModelDepth = 0;
	int32 RawDepth = 0;
	FTextRun Run;
	bool bAfterTag = false;
	while (Cursor.Tokenizer.Next(Token))
	{
		if (Token.bMalformed)
		{
			ReportError(ErrorCount, *Scope.File, Cursor.Lines.At(Token.Start), TEXT("unterminated tag"));
			AppendSpan(Out, Text, Token.Start, Text.Len());
			return false;
		}

		const bool bInModel = Scope.bInModel || ModelDepth > 0;
		if (Token.Type == ETokenType::Text || Token.Type == ETokenType::Data)
		{
			if (Run.Start == INDEX_NONE)
			{
				Run.Start = Out.Len();
				Run.bAfterTag = bAfterTag;
				Run.bInModel = bInModel;
				Run.bInRaw = RawDepth > 0;
			}
			Run.bHasData |= Token.Type == ETokenType::Data;
			AppendSpan(Out, Text, Token.Start, Token.End);
			bAfterTag = false;
			continue;
		}

		if (Token.Type == ETokenType::Markup)
		{
			Run.bAfterTag = false;
			AppendSpan(Out, Text, Token.Start, Token.End);
			bAfterTag = false;
			continue;
		}

		if (Token.Type == ETokenType::EndTag)
		{
			const bool bImportClose = bUntilImportClose && IsNamed(Token.Tag.Name, ImportTagName);
			const bool bSlotClose = Scope.Slot && IsNamed(Token.Tag.Name, SlotTagName);
			bAfterTag = !bImportClose && !bSlotClose;
			CloseTextRun(Run, Out, bAfterTag, *TypeRegister);
			if (bImportClose)
			{
				return true;
			}
			if (bSlotClose)
			{
				continue;
			}
			AppendSpan(Out, Text, Token.Start, Token.End);
			for (int32 Index = OpenElements.Num() - 1; Index >= 0; --Index)
			{
				if (OpenElements[Index].Name.Equals(Token.Tag.Name, ESearchCase::IgnoreCase))
				{
					for (int32 Closed = Index; Closed < OpenElements.Num(); ++Closed)
					{
						ModelDepth -= OpenElements[Closed].bModel ? 1 : 0;
						RawDepth -= OpenElements[Closed].bRaw ? 1 : 0;
					}
					OpenElements.SetNum(Index, EAllowShrinking::No);
					break;
				}
			}
			continue;
		}

		if (Scope.Slot && IsNamed(Token.Tag.Name, SlotTagName))
		{
			CloseTextRun(Run, Out, false, *TypeRegister);
			bAfterTag = false;
			Out += *Scope.Slot->Children;
			++Scope.Slot->Count;
			if (Scope.Ids)
			{
				Scope.Ids->Append(*Scope.Slot->Ids);
			}
			continue;
		}

		if (IsNamed(Token.Tag.Name, ImportTagName))
		{
			CloseTextRun(Run, Out, false, *TypeRegister);
			bAfterTag = false;
			ExpandImport(Cursor, Scope, bInModel, State, Out);
			continue;
		}

		const FAttribute* Condition = bInModel ? Token.Tag.Find(TEXT("data-if")) : nullptr;
		bool bConditionValue = false;
		const bool bConstantCondition = Condition && Condition->bHasValue && EvaluateConstantCondition(Condition->RawValue, bConditionValue);
		const bool bDead = bConstantCondition && !bConditionValue;
		bAfterTag = !bDead;
		CloseTextRun(Run, Out, bAfterTag, *TypeRegister);
		if (bDead)
		{
			if (!Token.Tag.bSelfClosing)
			{
				SkipElement(Cursor, Token.Tag.Name, true, *Scope.File, State);
			}
			continue;
		}

		const bool bFoldBindings = bInModel && RawDepth == 0 && (!Condition || bConstantCondition) && !Token.Tag.Find(TEXT("data-for")) && !Token.Tag.Find(TEXT("data-model"));
		AppendStartTag(Out, Text, Token, bConstantCondition ? Condition : nullptr, bFoldBindings, *TypeRegister);

		if (Scope.Ids)
		{
			if (const FAttribute* IdAttribute = Token.Tag.Find(TEXT("id")))
			{
				Scope.Ids->Add(DecodeXml(IdAttribute->RawValue));
			}
		}
		if (Scope.bDocument)
		{
			if (State.HeadInsert == INDEX_NONE && IsNamed(Token.Tag.Name, TEXT("head")) && !Token.Tag.bSelfClosing)
			{
				State.HeadInsert = Out.Len();
			}
			if (State.RootInsert == INDEX_NONE && IsNamed(Token.Tag.Name, TEXT("rml")))
			{
				State.RootInsert = Out.Len();
			}
		}
		if (!Token.Tag.bSelfClosing)
		{
			FOpenElement& Element = OpenElements.AddDefaulted_GetRef();
			Element.Name = Token.Tag.Name;
			Element.bModel = Token.Tag.Find(TEXT("data-model")) != nullptr;
			Element.bRaw = IsNamed(Token.Tag.Name, TEXT("svg")) || IsNamed(Token.Tag.Name, TEXT("textarea"));
			ModelDepth += Element.bModel ? 1 : 0;
			RawDepth += Element.bRaw ? 1 : 0;
		}
	}
	return false;
}

void FArcRmlComponentExpander::SkipElement(FCursor& Cursor, FStringView Name, bool bMarkUsed, const FString& File, FExpandState& State)
{
	using namespace ArcRmlComponentExpanderPrivate;

	FToken& Token = Cursor.Token;
	TArray<FStringView, TInlineAllocator<32>> OpenElements;
	OpenElements.Add(Name);
	TArray<TPair<FString, int32>, TInlineAllocator<8>> OpenImports;
	while (Cursor.Tokenizer.Next(Token))
	{
		if (Token.bMalformed)
		{
			ReportError(ErrorCount, File, Cursor.Lines.At(Token.Start), TEXT("unterminated tag"));
			return;
		}

		if (Token.Type == ETokenType::StartTag && !Token.Tag.bSelfClosing)
		{
			OpenElements.Add(Token.Tag.Name);
		}
		if (bMarkUsed && Token.Type == ETokenType::StartTag && IsNamed(Token.Tag.Name, ImportTagName))
		{
			const int32 Line = Cursor.Lines.At(Token.Start);
			if (Token.Tag.bSelfClosing)
			{
				MarkUsed(ImportName(Token.Tag), File, Line, State);
			}
			else
			{
				OpenImports.Emplace(ImportName(Token.Tag), Line);
			}
		}
		if (Token.Type != ETokenType::EndTag)
		{
			continue;
		}

		if (bMarkUsed && IsNamed(Token.Tag.Name, ImportTagName) && OpenImports.Num() > 0)
		{
			const TPair<FString, int32> Import = OpenImports.Pop(EAllowShrinking::No);
			MarkUsed(Import.Key, File, Import.Value, State);
		}
		for (int32 Index = OpenElements.Num() - 1; Index >= 0; --Index)
		{
			if (OpenElements[Index].Equals(Token.Tag.Name, ESearchCase::IgnoreCase))
			{
				OpenElements.SetNum(Index, EAllowShrinking::No);
				break;
			}
		}
		if (OpenElements.IsEmpty())
		{
			return;
		}
	}
}

void FArcRmlComponentExpander::MarkUsed(const FString& Name, const FString& File, int32 Line, FExpandState& State)
{
	if (Name.IsEmpty() || State.UsedComponents.Contains(Name) || State.Stack.Contains(Name))
	{
		return;
	}

	const FComponent* Component = FindComponent(Name, File, Line);
	if (!Component)
	{
		return;
	}

	State.Stack.Push(Name);
	for (const FString& Imported : Component->ImportNames)
	{
		MarkUsed(Imported, Component->Path, Component->BodyLine, State);
	}
	State.Stack.Pop();
	State.UsedComponents.AddUnique(Name);
}

void FArcRmlComponentExpander::ExpandImport(FCursor& Cursor, const FScope& Scope, bool bInModel, FExpandState& State, FString& Out)
{
	using namespace ArcRmlComponentExpanderPrivate;

	const FTag& Tag = Cursor.Token.Tag;
	const FString& File = *Scope.File;
	const int32 Line = Cursor.Lines.At(Cursor.Token.Start);
	const int32 ChildrenStart = Cursor.Token.End;
	const bool bHasChildren = !Tag.bSelfClosing;
	const FString Name = ImportName(Tag);
	if (Name.IsEmpty())
	{
		ReportError(ErrorCount, File, Line, TEXT("<dc-import> has no name"));
		if (bHasChildren)
		{
			SkipElement(Cursor, Tag.Name, false, File, State);
		}
		return;
	}

	bool bDead = false;
	if (bInModel)
	{
		const FAttribute* Condition = Tag.Find(TEXT("data-if"));
		bool bConditionValue = false;
		bDead = Condition && Condition->bHasValue && EvaluateConstantCondition(Condition->RawValue, bConditionValue) && !bConditionValue;
	}
	if (bDead)
	{
		if (bHasChildren)
		{
			SkipElement(Cursor, Tag.Name, true, File, State);
		}
		MarkUsed(Name, File, Line, State);
		return;
	}

	const FComponent* Component = FindComponent(Name, File, Line);

	FString HostStyle;
	FString HostAttributes;
	bool bHasModel = bInModel;
	TMap<FString, FPropValue> Values;
	for (const FAttribute& Attribute : Tag.Attributes)
	{
		const FStringView AttributeName = Attribute.Name;
		if (IsNamed(AttributeName, TEXT("name")) || IsNamed(AttributeName, TEXT("sc-name")) || IsNamed(AttributeName, TEXT("data-dc-tpl")))
		{
			continue;
		}
		if (IsNamed(AttributeName, TEXT("style")))
		{
			HostStyle = HostPositionStyle(DecodeXml(Attribute.RawValue));
			continue;
		}
		if (IsNamed(AttributeName, TEXT("data-for")) || IsNamed(AttributeName, TEXT("data-if")) || IsNamed(AttributeName, TEXT("data-model")) ||
			IsNamed(AttributeName, TEXT("data-virtual")))
		{
			bool bConditionValue = false;
			if (bInModel && IsNamed(AttributeName, TEXT("data-if")) && Attribute.bHasValue && EvaluateConstantCondition(Attribute.RawValue, bConditionValue))
			{
				continue;
			}
			const TCHAR Quote = Attribute.Quote != 0 ? Attribute.Quote : TEXT('"');
			HostAttributes.AppendChar(TEXT(' '));
			HostAttributes.Append(AttributeName);
			HostAttributes.AppendChar(TEXT('='));
			HostAttributes.AppendChar(Quote);
			HostAttributes.Append(Attribute.RawValue);
			HostAttributes.AppendChar(Quote);
			bHasModel |= IsNamed(AttributeName, TEXT("data-model"));
			continue;
		}
		if (AttributeName.StartsWith(TEXT("style-"), ESearchCase::IgnoreCase) || AttributeName.StartsWith(TEXT("hint-"), ESearchCase::IgnoreCase) || !Component)
		{
			continue;
		}

		const FString PropName = KebabToCamel(AttributeName);
		if (!Component->HasProp(PropName))
		{
			ReportError(ErrorCount, File, Line, FString::Printf(TEXT("'%s' has no prop '%s'"), *Name, *PropName));
			continue;
		}

		const FString Decoded = DecodeXml(Attribute.RawValue);
		const FString Trimmed = Decoded.TrimStartAndEnd();
		FPropValue Value;
		if (Trimmed.StartsWith(TEXT("{{")) && SkipDataBrackets(Trimmed, 0) == Trimmed.Len() && Trimmed.EndsWith(TEXT("}}")) && Trimmed.Len() >= 4)
		{
			Value.bBound = true;
			Value.Text = Trimmed.Mid(2, Trimmed.Len() - 4).TrimStartAndEnd();
			Value.bPath = IsPlainAddress(Value.Text);
			if (Value.Text.IsEmpty())
			{
				ReportError(ErrorCount, File, Line, FString::Printf(TEXT("prop '%s' binds an empty expression"), *PropName));
				continue;
			}
		}
		else if (Decoded.Contains(TEXT("{{")))
		{
			ReportError(ErrorCount, File, Line, FString::Printf(TEXT("prop '%s' mixes text and {{ }}; bind the whole value"), *PropName));
			continue;
		}
		else
		{
			Value.Text = Decoded;
		}
		Values.Add(PropName, MoveTemp(Value));
	}

	Out += TEXT("<div class=\"sc-host\" data-sc-name=\"");
	AppendEncodedXml(Out, Name);
	Out.AppendChar(TEXT('"'));
	if (!HostStyle.IsEmpty())
	{
		Out += TEXT(" style=\"");
		AppendEncodedXml(Out, HostStyle);
		Out.AppendChar(TEXT('"'));
	}
	Out += HostAttributes;
	Out.AppendChar(TEXT('>'));

	bool bExpandable = Component != nullptr;
	if (bExpandable && State.Stack.Contains(Name))
	{
		ReportError(ErrorCount, File, Line, FString::Printf(TEXT("circular import: %s -> %s"), *FString::Join(State.Stack, TEXT(" -> ")), *Name));
		bExpandable = false;
	}
	else if (bExpandable && !bHasModel)
	{
		ReportError(ErrorCount, File, Line, FString::Printf(TEXT("component '%s' is used outside any data-model"), *Name));
		bExpandable = false;
	}
	if (!bExpandable)
	{
		if (bHasChildren)
		{
			SkipElement(Cursor, Tag.Name, false, File, State);
		}
		Out += TEXT("</div>");
		return;
	}

	FString Children;
	TArray<FString> ChildIds;
	const int32 ChildrenLine = Cursor.Lines.At(ChildrenStart);
	if (bHasChildren)
	{
		FScope ChildScope;
		ChildScope.File = Scope.File;
		ChildScope.bInModel = bHasModel;
		ChildScope.Slot = Scope.Slot;
		ChildScope.Ids = &ChildIds;
		if (!ExpandTokens(Cursor, ChildScope, State, Children, true))
		{
			ReportError(ErrorCount, File, Line, TEXT("<dc-import> is not closed"));
		}
	}

	FString Substituted;
	FStringView Body = Component->Body;
	if (Component->bBodyHasProps)
	{
		Substituted.Reserve(Component->Body.Len());
		FPropSubstitution Substitution(Component->Props, Values, Component->Path, ErrorCount);
		Substitution.Run(Component->Body, Component->BodyLine, Substituted);
		Body = Substituted;
	}

	FSlot Slot;
	Slot.Children = &Children;
	Slot.Ids = &ChildIds;
	FScope BodyScope;
	BodyScope.File = &Component->Path;
	BodyScope.bInModel = true;
	BodyScope.Slot = &Slot;
	BodyScope.Ids = Scope.Ids;

	State.Stack.Push(Name);
	Expand(Body, Component->BodyLine, BodyScope, State, Out);
	State.Stack.Pop();

	CheckChildIds(ErrorCount, ChildIds, Slot.Count, Name, File, ChildrenLine);

	State.UsedComponents.AddUnique(Name);
	Out += TEXT("</div>");
}
#endif
