// Copyright Lukasz Baran. All Rights Reserved.

#include "Thread/ArcRmlLayoutDump.h"

#if !UE_BUILD_SHIPPING

#include "ArcRmlUIPerf.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "Interfaces/ArcRmlFileInterface.h"
#include "Interfaces/ArcRmlPaths.h"
#include "Interfaces/ArcRmlStrings.h"
#include "Internationalization/Regex.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonWriter.h"
#include "Thread/ArcRmlUIThread.h"

#include "RmlUi/Core/ArcPerf.h"
#include "RmlUi/Core/Box.h"
#include "RmlUi/Core/Context.h"
#include "RmlUi/Core/Core.h"
#include "RmlUi/Core/Element.h"
#include "RmlUi/Core/ElementDocument.h"
#include "RmlUi/Core/ElementScroll.h"
#include "RmlUi/Core/ElementText.h"
#include "RmlUi/Core/FontEngineInterface.h"
#include "RmlUi/Core/FontMetrics.h"
#include "RmlUi/Core/Property.h"
#include "RmlUi/Core/StyleSheetSpecification.h"
#include "RmlUi/Core/SystemInterface.h"

namespace ArcRmlLayoutDumpPrivate
{
	const TCHAR* const VoidTags[] = {
		TEXT("area"), TEXT("base"), TEXT("br"), TEXT("col"), TEXT("embed"), TEXT("hr"), TEXT("img"), TEXT("input"),
		TEXT("link"), TEXT("meta"), TEXT("source"), TEXT("track"), TEXT("wbr")
	};

	FString ChildId(const FString& ParentId, int32 Index)
	{
		return FString::Printf(TEXT("%s.%d"), *ParentId, Index);
	}

	void AppendNumber(FString& Out, float Value)
	{
		if (FMath::IsFinite(Value))
		{
			Out.Appendf(TEXT("%.9g"), Value);
		}
		else
		{
			Out += TEXT("null");
		}
	}

	void AppendNumbers(FString& Out, const TCHAR* Name, std::initializer_list<float> Values)
	{
		Out.Appendf(TEXT(",\"%s\":["), Name);
		bool bFirst = true;
		for (const float Value : Values)
		{
			if (!bFirst)
			{
				Out += TEXT(",");
			}
			bFirst = false;
			AppendNumber(Out, Value);
		}
		Out += TEXT("]");
	}

	void AppendString(FString& Out, const TCHAR* Name, const FString& Value)
	{
		Out.Appendf(TEXT(",\"%s\":"), Name);
		AppendEscapeJsonString(Out, Value);
	}

	void AppendEdges(FString& Out, const TCHAR* Name, const Rml::Box& Box, Rml::BoxArea Area)
	{
		AppendNumbers(Out, Name, {
			Box.GetEdge(Area, Rml::BoxEdge::Top), Box.GetEdge(Area, Rml::BoxEdge::Right),
			Box.GetEdge(Area, Rml::BoxEdge::Bottom), Box.GetEdge(Area, Rml::BoxEdge::Left)});
	}

	FString PropertyString(Rml::Element& Element, Rml::PropertyId Id)
	{
		const Rml::Property* Property = Element.GetProperty(Id);
		return Property ? ArcRmlStrings::ToFString(Property->ToString()) : FString();
	}

	TArray<FString> LocalDeclarations(Rml::Element& Element)
	{
		TArray<FString> Declarations;
		for (const Rml::PropertyMap::value_type& Pair : Element.GetLocalStyleProperties())
		{
			Declarations.Add(FString::Printf(TEXT("%s: %s%s"), *ArcRmlStrings::ToFString(Rml::StyleSheetSpecification::GetPropertyName(Pair.first)),
				*ArcRmlStrings::ToFString(Pair.second.ToString()).Replace(TEXT("devpx"), TEXT("px")), Pair.second.important ? TEXT(" !important") : TEXT("")));
		}
		Declarations.Sort();
		return Declarations;
	}

	bool IsScrollbarEnabled(Rml::Element& Element, Rml::ElementScroll::Orientation Orientation)
	{
		Rml::ElementScroll* Scroll = Element.GetElementScroll();
		Rml::Element* Scrollbar = Scroll ? Scroll->GetScrollbar(Orientation) : nullptr;
		return Scrollbar && Scrollbar->IsVisible();
	}

	struct FDumpContext
	{
		TMap<const Rml::Element*, FString> Ids;
		FString Json;
		int32 Elements = 0;
	};

	void AssignIds(Rml::Element& Element, const FString& Id, FDumpContext& Dump)
	{
		Dump.Ids.Add(&Element, Id);
		const int32 NumDom = Element.GetNumChildren();
		const int32 NumAll = Element.GetNumChildren(true);
		for (int32 Index = 0; Index < NumAll; ++Index)
		{
			Rml::Element* Child = Element.GetChild(Index);
			if (!Child)
			{
				continue;
			}
			FString Id2;
			if (Index < NumDom)
			{
				Id2 = ChildId(Id, Index);
			}
			else if (Child->GetPseudoElementType() == Rml::PseudoElement::Before)
			{
				Id2 = Id + TEXT("::before");
			}
			else if (Child->GetPseudoElementType() == Rml::PseudoElement::After)
			{
				Id2 = Id + TEXT("::after");
			}
			else
			{
				Id2 = FString::Printf(TEXT("%s~%d"), *Id, Index - NumDom);
			}
			AssignIds(*Child, Id2, Dump);
		}
	}

	const TCHAR* GetKind(Rml::Element& Element, bool bText)
	{
		if (bText)
		{
			return TEXT("text");
		}
		if (Element.GetPseudoElementType() != Rml::PseudoElement::None)
		{
			return TEXT("pseudo");
		}
		if (Element.GetScrollbarPart() != Rml::ScrollbarPart::None)
		{
			return TEXT("scrollbar");
		}
		return TEXT("element");
	}

	void DumpElement(Rml::Element& Element, const FString* ParentId, bool bInDom, FDumpContext& Dump)
	{
		const FString& Id = Dump.Ids.FindChecked(&Element);
		Rml::ElementText* Text = rmlui_dynamic_cast<Rml::ElementText*>(&Element);
		const Rml::Box& Box = Element.GetBox();
		const Rml::Vector2f BorderOffset = Element.GetAbsoluteOffset(Rml::BoxArea::Border);
		const Rml::Vector2f BorderSize = Box.GetSize(Rml::BoxArea::Border);
		const Rml::Vector2f ContentSize = Box.GetSize(Rml::BoxArea::Content);
		const Rml::Vector2f RelativeOffset = Element.GetRelativeOffset(Rml::BoxArea::Border);

		FString& Out = Dump.Json;
		Out += Dump.Elements > 0 ? TEXT(",\n{") : TEXT("\n{");
		++Dump.Elements;
		Out += TEXT("\"id\":");
		AppendEscapeJsonString(Out, Id);
		Out += TEXT(",\"parent\":");
		if (ParentId)
		{
			AppendEscapeJsonString(Out, *ParentId);
		}
		else
		{
			Out += TEXT("null");
		}
		AppendString(Out, TEXT("kind"), GetKind(Element, Text != nullptr));
		Out += bInDom ? TEXT(",\"dom\":true") : TEXT(",\"dom\":false");
		AppendString(Out, TEXT("tag"), ArcRmlStrings::ToFString(Element.GetTagName()));
		AppendString(Out, TEXT("dom_id"), ArcRmlStrings::ToFString(Element.GetId()));
		AppendString(Out, TEXT("classes"), ArcRmlStrings::ToFString(Element.GetClassNames()));
		AppendString(Out, TEXT("inputs"), FString::Join(LocalDeclarations(Element), TEXT("; ")));
		AppendString(Out, TEXT("display"), PropertyString(Element, Rml::PropertyId::Display));
		AppendString(Out, TEXT("visibility"), PropertyString(Element, Rml::PropertyId::Visibility));
		AppendString(Out, TEXT("position"), PropertyString(Element, Rml::PropertyId::Position));
		Out += Element.IsVisible(true) ? TEXT(",\"visible\":true") : TEXT(",\"visible\":false");
		AppendNumbers(Out, TEXT("rect"), {BorderOffset.x, BorderOffset.y, BorderSize.x, BorderSize.y});
		AppendNumbers(Out, TEXT("content"), {ContentSize.x, ContentSize.y});
		AppendEdges(Out, TEXT("padding"), Box, Rml::BoxArea::Padding);
		AppendEdges(Out, TEXT("border"), Box, Rml::BoxArea::Border);
		AppendEdges(Out, TEXT("margin"), Box, Rml::BoxArea::Margin);

		const int32 NumBoxes = Element.GetNumBoxes();
		if (NumBoxes > 1)
		{
			Out += TEXT(",\"fragments\":[");
			for (int32 BoxIndex = 0; BoxIndex < NumBoxes; ++BoxIndex)
			{
				Rml::Vector2f FragmentOffset;
				const Rml::Box& Fragment = Element.GetBox(BoxIndex, FragmentOffset);
				const Rml::Vector2f FragmentSize = Fragment.GetSize(Rml::BoxArea::Border);
				Out += BoxIndex > 0 ? TEXT(",[") : TEXT("[");
				AppendNumber(Out, BorderOffset.x + FragmentOffset.x);
				Out += TEXT(",");
				AppendNumber(Out, BorderOffset.y + FragmentOffset.y);
				Out += TEXT(",");
				AppendNumber(Out, FragmentSize.x);
				Out += TEXT(",");
				AppendNumber(Out, FragmentSize.y);
				Out += TEXT("]");
			}
			Out += TEXT("]");
		}

		Out += TEXT(",\"offset_parent\":");
		const FString* OffsetParentId = Element.GetOffsetParent() ? Dump.Ids.Find(Element.GetOffsetParent()) : nullptr;
		if (OffsetParentId)
		{
			AppendEscapeJsonString(Out, *OffsetParentId);
		}
		else
		{
			Out += TEXT("null");
		}
		AppendNumbers(Out, TEXT("relative_offset"), {RelativeOffset.x, RelativeOffset.y});
		AppendNumbers(Out, TEXT("baseline"), {Element.GetBaseline()});
		AppendNumbers(Out, TEXT("scroll_size"), {Element.GetScrollWidth(), Element.GetScrollHeight()});
		AppendNumbers(Out, TEXT("scroll"), {Element.GetScrollLeft(), Element.GetScrollTop()});
		Out.Appendf(TEXT(",\"scrollbars\":[%s,%s]"),
			IsScrollbarEnabled(Element, Rml::ElementScroll::VERTICAL) ? TEXT("true") : TEXT("false"),
			IsScrollbarEnabled(Element, Rml::ElementScroll::HORIZONTAL) ? TEXT("true") : TEXT("false"));

		if (const Rml::OutOfFlowLayoutInputs* OutOfFlow = Rml::ArcPerf::GetOutOfFlowLayoutInputs(&Element))
		{
			Out += OutOfFlow->has_containing_block_area ? TEXT(",\"out_of_flow\":{\"has_area\":true") : TEXT(",\"out_of_flow\":{\"has_area\":false");
			AppendNumbers(Out, TEXT("static_position"), {OutOfFlow->static_position.x, OutOfFlow->static_position.y});
			AppendNumbers(Out, TEXT("absolute_containing_block"),
				{OutOfFlow->absolute_containing_block_size.x, OutOfFlow->absolute_containing_block_size.y});
			AppendNumbers(Out, TEXT("static_containing_block"), {OutOfFlow->static_containing_block_size.x, OutOfFlow->static_containing_block_size.y});
			AppendNumbers(Out, TEXT("static_alignment"), {OutOfFlow->static_alignment.x, OutOfFlow->static_alignment.y});
			if (OutOfFlow->has_containing_block_area)
			{
				AppendNumbers(Out, TEXT("area"), {OutOfFlow->containing_block_area_position.x, OutOfFlow->containing_block_area_position.y,
					OutOfFlow->containing_block_area_size.x, OutOfFlow->containing_block_area_size.y});
			}
			Out += TEXT("}");
		}

		if (Text)
		{
			AppendString(Out, TEXT("text"), ArcRmlStrings::ToFString(Text->GetText()));
			const Rml::FontFaceHandle FontFace = Element.GetFontFaceHandle();
			if (FontFace != 0)
			{
				const Rml::FontMetrics& Metrics = Rml::GetFontEngineInterface()->GetFontMetrics(FontFace);
				AppendNumbers(Out, TEXT("ascent"), {Metrics.ascent});
				AppendNumbers(Out, TEXT("descent"), {Metrics.descent});
			}
			const Rml::Vector2f ContentOffset = Element.GetAbsoluteOffset(Rml::BoxArea::Content);
			Out += TEXT(",\"lines\":[");
			bool bFirstLine = true;
			for (const Rml::ElementText::Line& Line : Text->GetLines())
			{
				Out += bFirstLine ? TEXT("{\"x\":") : TEXT(",{\"x\":");
				bFirstLine = false;
				AppendNumber(Out, ContentOffset.x + Line.position.x);
				Out += TEXT(",\"baseline\":");
				AppendNumber(Out, ContentOffset.y + Line.position.y);
				Out.Appendf(TEXT(",\"width\":%d"), Line.width);
				AppendString(Out, TEXT("text"), ArcRmlStrings::ToFString(Line.text));
				Out += TEXT("}");
			}
			Out += TEXT("]");
		}
		Out += TEXT("}");

		const int32 NumDom = Element.GetNumChildren();
		const int32 NumAll = Element.GetNumChildren(true);
		for (int32 Index = 0; Index < NumAll; ++Index)
		{
			if (Rml::Element* Child = Element.GetChild(Index))
			{
				DumpElement(*Child, &Id, bInDom && Index < NumDom, Dump);
			}
		}
	}

	FString EscapeHtml(const FString& Value, bool bAttribute)
	{
		FString Result = Value.Replace(TEXT("&"), TEXT("&amp;")).Replace(TEXT("<"), TEXT("&lt;")).Replace(TEXT(">"), TEXT("&gt;"));
		return bAttribute ? Result.Replace(TEXT("\""), TEXT("&quot;")) : Result;
	}

	FString ToFileUrl(const FString& Path)
	{
		FString Full = FPaths::ConvertRelativePathToFull(Path);
		FPaths::NormalizeFilename(Full);
		return TEXT("file:///") + Full.Replace(TEXT(" "), TEXT("%20"));
	}

	FString MapGenericFamilies(const FString& Value, const TMap<FString, FString>& GenericFamilies)
	{
		TArray<FString> Families;
		Value.ParseIntoArray(Families, TEXT(","));
		for (FString& Family : Families)
		{
			Family.TrimStartAndEndInline();
			for (const TPair<FString, FString>& Generic : GenericFamilies)
			{
				if (Family.Equals(Generic.Key, ESearchCase::IgnoreCase))
				{
					Family = FString::Printf(TEXT("\"%s\""), *Generic.Value);
				}
			}
		}
		return FString::Join(Families, TEXT(", "));
	}

	FString ConvertCss(const FString& Css, float DpRatio, const TMap<FString, FString>& GenericFamilies)
	{
		FString DpConverted;
		{
			FRegexMatcher Matcher(FRegexPattern(TEXT("(?<![\\w#.\\-])(-?[0-9]*\\.?[0-9]+)(devpx|px|dp)\\b")), Css);
			int32 Last = 0;
			while (Matcher.FindNext())
			{
				DpConverted += Css.Mid(Last, Matcher.GetMatchBeginning() - Last);
				const FString Number = Matcher.GetCaptureGroup(1);
				if (Matcher.GetCaptureGroup(2) == TEXT("devpx"))
				{
					DpConverted += Number + TEXT("px");
				}
				else
				{
					DpConverted += FString::Printf(TEXT("%.9gpx"), FCString::Atof(*Number) * DpRatio);
				}
				Last = Matcher.GetMatchEnding();
			}
			DpConverted += Css.Mid(Last);
		}

		FString Result;
		FRegexMatcher Matcher(FRegexPattern(TEXT("font-family\\s*:([^;}]*)")), DpConverted);
		int32 Last = 0;
		while (Matcher.FindNext())
		{
			const int32 ValueBegin = Matcher.GetCaptureGroupBeginning(1);
			Result += DpConverted.Mid(Last, ValueBegin - Last);
			FString Value = Matcher.GetCaptureGroup(1);
			const bool bImportant = Value.Contains(TEXT("!important"));
			Value.ReplaceInline(TEXT("!important"), TEXT(""));
			Result += TEXT(" ") + MapGenericFamilies(Value, GenericFamilies) + (bImportant ? TEXT(" !important") : TEXT(""));
			Last = Matcher.GetCaptureGroupEnding(1);
		}
		Result += DpConverted.Mid(Last);
		return Result;
	}

	uint16 ReadU16(const TArray<uint8>& Data, int64 Offset)
	{
		return Offset >= 0 && Offset + 2 <= Data.Num() ? static_cast<uint16>((Data[Offset] << 8) | Data[Offset + 1]) : 0;
	}

	uint32 ReadU32(const TArray<uint8>& Data, int64 Offset)
	{
		return (static_cast<uint32>(ReadU16(Data, Offset)) << 16) | ReadU16(Data, Offset + 2);
	}

	bool ReadFontFace(const TArray<uint8>& Data, FString& OutFamily, int32& OutWeight, bool& bOutItalic)
	{
		int64 NameTable = -1;
		int64 Os2Table = -1;
		const int32 NumTables = ReadU16(Data, 4);
		for (int32 TableIndex = 0; TableIndex < NumTables; ++TableIndex)
		{
			const int64 Record = 12 + 16 * TableIndex;
			const uint32 Tag = ReadU32(Data, Record);
			if (Tag == 0x6E616D65)
			{
				NameTable = ReadU32(Data, Record + 8);
			}
			else if (Tag == 0x4F532F32)
			{
				Os2Table = ReadU32(Data, Record + 8);
			}
		}

		OutWeight = 400;
		bOutItalic = false;
		if (Os2Table >= 0)
		{
			const uint16 WeightClass = ReadU16(Data, Os2Table + 4);
			OutWeight = WeightClass != 0 ? WeightClass : 400;
			bOutItalic = (ReadU16(Data, Os2Table + 62) & 1) != 0;
		}
		if (NameTable < 0)
		{
			return false;
		}

		const int32 Count = ReadU16(Data, NameTable + 2);
		const int64 StringBase = NameTable + ReadU16(Data, NameTable + 4);
		int32 BestRank = MAX_int32;
		for (int32 RecordIndex = 0; RecordIndex < Count; ++RecordIndex)
		{
			const int64 Record = NameTable + 6 + 12 * RecordIndex;
			const uint16 Platform = ReadU16(Data, Record);
			const uint16 Language = ReadU16(Data, Record + 4);
			const uint16 NameId = ReadU16(Data, Record + 6);
			const int64 Length = ReadU16(Data, Record + 8);
			const int64 Offset = StringBase + ReadU16(Data, Record + 10);
			if ((NameId != 16 && NameId != 1) || Platform > 3 || Offset + Length > Data.Num())
			{
				continue;
			}
			const int32 Rank = (NameId == 16 ? 0 : 10) + (Platform == 3 && Language == 0x409 ? 0 : (Platform == 3 ? 1 : 2));
			if (Rank >= BestRank)
			{
				continue;
			}

			FString Name;
			if (Platform == 1)
			{
				for (int64 Index = 0; Index < Length; ++Index)
				{
					Name.AppendChar(static_cast<TCHAR>(Data[Offset + Index]));
				}
			}
			else
			{
				for (int64 Index = 0; Index + 1 < Length; Index += 2)
				{
					Name.AppendChar(static_cast<TCHAR>(ReadU16(Data, Offset + Index)));
				}
			}
			if (!Name.IsEmpty())
			{
				OutFamily = Name;
				BestRank = Rank;
			}
		}
		return BestRank != MAX_int32;
	}

	FString BuildFontFaces(const FArcRmlUIThreadDependencies& Dependencies)
	{
		TArray<FString> FontPaths;
		for (const FArcRmlContentRoot& Root : Dependencies.ContentRoots.Roots)
		{
			ArcRmlPaths::FindFontFiles(*Dependencies.PlatformFile, Root, FontPaths);
		}

		FString Result;
		for (const FString& FontPath : FontPaths)
		{
			FString FontFile;
			if (!ArcRmlPaths::ToFilename(Dependencies.ContentRoots, FontPath, FontFile))
			{
				continue;
			}
			TArray<uint8> Data;
			FString Family;
			int32 Weight = 400;
			bool bItalic = false;
			if (!FFileHelper::LoadFileToArray(Data, *FontFile) || !ReadFontFace(Data, Family, Weight, bItalic))
			{
				continue;
			}
			Result += FString::Printf(TEXT("@font-face { font-family: \"%s\"; font-weight: %d; font-style: %s; src: url(\"%s\"); }\n"),
				*Family, Weight, bItalic ? TEXT("italic") : TEXT("normal"), *ToFileUrl(FontFile));
		}
		return Result;
	}

	FString BuildStyleSheets(const FString& DocumentPath, TConstArrayView<FString> StyleSheetLinks, const FArcRmlFileInterface& FileInterface, float DpRatio,
		const TMap<FString, FString>& GenericFamilies)
	{
		const Rml::String RmlDocumentPath = ArcRmlStrings::ToRmlString(DocumentPath);

		FString Result;
		for (const FString& Link : StyleSheetLinks)
		{
			Rml::String Joined;
			Rml::GetSystemInterface()->JoinPath(Joined, RmlDocumentPath, ArcRmlStrings::ToRmlString(Link));
			const FString Resolved = FileInterface.ResolvePath(ArcRmlStrings::ToFString(Joined));
			FString Css;
			if (Resolved.IsEmpty() || !FFileHelper::LoadFileToString(Css, *Resolved))
			{
				Result += FString::Printf(TEXT("<!-- unresolved stylesheet %s -->\n"), *EscapeHtml(Link, false));
				continue;
			}
			Result += FString::Printf(TEXT("<style data-arc-source=\"%s\">\n%s\n</style>\n"), *EscapeHtml(ToFileUrl(Resolved), true),
				*ConvertCss(Css, DpRatio, GenericFamilies));
		}
		return Result;
	}

	struct FHtmlContext
	{
		Rml::String DocumentPath;
		const FArcRmlFileInterface* FileInterface = nullptr;
		const TMap<FString, FString>* GenericFamilies = nullptr;
		float DpRatio = 1.0f;
		FString Html;
	};

	FString BuildInlineStyle(Rml::Element& Element, const FHtmlContext& Context)
	{
		TArray<FString> Declarations;
		for (const Rml::PropertyMap::value_type& Pair : Element.GetLocalStyleProperties())
		{
			const FString Name = ArcRmlStrings::ToFString(Rml::StyleSheetSpecification::GetPropertyName(Pair.first));
			if (Name == TEXT("transform") || Name == TEXT("perspective"))
			{
				continue;
			}
			FString Value = ArcRmlStrings::ToFString(Pair.second.ToString());
			if (Name == TEXT("font-family"))
			{
				Value = MapGenericFamilies(Value, *Context.GenericFamilies);
			}
			Declarations.Add(FString::Printf(TEXT("%s: %s%s"), *Name, *Value, Pair.second.important ? TEXT(" !important") : TEXT("")));
		}
		Declarations.Sort();
		return ConvertCss(FString::Join(Declarations, TEXT("; ")), Context.DpRatio, TMap<FString, FString>());
	}

	void AppendHtml(Rml::Element& Element, const FString& Id, FHtmlContext& Context)
	{
		if (Rml::ElementText* Text = rmlui_dynamic_cast<Rml::ElementText*>(&Element))
		{
			Context.Html += FString::Printf(TEXT("<!--arc:%s-->"), *Id) + EscapeHtml(ArcRmlStrings::ToFString(Text->GetText()), false);
			return;
		}

		const FString Tag = ArcRmlStrings::ToFString(Element.GetTagName());
		FString& Html = Context.Html;
		Html.Appendf(TEXT("<%s data-arc-id=\"%s\""), *Tag, *Id);
		if (!Element.GetId().empty())
		{
			Html.Appendf(TEXT(" id=\"%s\""), *EscapeHtml(ArcRmlStrings::ToFString(Element.GetId()), true));
		}
		const Rml::String Classes = Element.GetClassNames();
		if (!Classes.empty())
		{
			Html.Appendf(TEXT(" class=\"%s\""), *EscapeHtml(ArcRmlStrings::ToFString(Classes), true));
		}
		const FString Style = BuildInlineStyle(Element, Context);
		if (!Style.IsEmpty())
		{
			Html.Appendf(TEXT(" style=\"%s\""), *EscapeHtml(Style, true));
		}

		const bool bReplaced = Tag == TEXT("img") || Tag == TEXT("svg");
		TArray<FString> Attributes;
		for (const Rml::ElementAttributes::value_type& Attribute : Element.GetAttributes())
		{
			const FString Name = ArcRmlStrings::ToFString(Attribute.first);
			if (Name == TEXT("id") || Name == TEXT("class") || Name == TEXT("style") || Name == TEXT("data-arc-id") || Name == TEXT("rmlui-inner-rml"))
			{
				continue;
			}
			FString Value = ArcRmlStrings::ToFString(Attribute.second.Get<Rml::String>());
			if (Name == TEXT("src") && !Value.IsEmpty())
			{
				Rml::String Joined;
				Rml::GetSystemInterface()->JoinPath(Joined, Context.DocumentPath, ArcRmlStrings::ToRmlString(Value));
				const FString Resolved = Context.FileInterface->ResolvePath(ArcRmlStrings::ToFString(Joined));
				if (!Resolved.IsEmpty())
				{
					Value = ToFileUrl(Resolved);
				}
			}
			else if (bReplaced && (Name == TEXT("width") || Name == TEXT("height")) && Value.IsNumeric())
			{
				Value = FString::Printf(TEXT("%.9g"), FCString::Atof(*Value) * Context.DpRatio);
			}
			Attributes.Add(FString::Printf(TEXT(" %s=\"%s\""), *Name, *EscapeHtml(Value, true)));
		}
		if (Tag == TEXT("img") && !Element.HasAttribute("width") && !Element.HasAttribute("height"))
		{
			Rml::Vector2f Intrinsic;
			float Ratio = 0.f;
			if (Element.GetIntrinsicDimensions(Intrinsic, Ratio))
			{
				Attributes.Add(FString::Printf(TEXT(" width=\"%.9g\""), Intrinsic.x));
				Attributes.Add(FString::Printf(TEXT(" height=\"%.9g\""), Intrinsic.y));
			}
		}
		Attributes.Sort();
		for (const FString& Attribute : Attributes)
		{
			Html += Attribute;
		}
		Html += TEXT(">");

		const int32 NumDom = Element.GetNumChildren();
		for (int32 Index = 0; Index < NumDom; ++Index)
		{
			if (Rml::Element* Child = Element.GetChild(Index))
			{
				AppendHtml(*Child, ChildId(Id, Index), Context);
			}
		}

		bool bVoid = false;
		for (const TCHAR* VoidTag : VoidTags)
		{
			bVoid |= Tag == VoidTag;
		}
		if (!bVoid || NumDom > 0)
		{
			Html.Appendf(TEXT("</%s>"), *Tag);
		}
	}
}

bool ArcRmlLayoutDump::Write(Rml::ElementDocument& Document, const FString& DocumentPath, TConstArrayView<FString> StyleSheetLinks, const FArcRmlFileInterface& FileInterface,
	const FArcRmlUIThreadDependencies& Dependencies, const FString& OutDir, FResult& OutResult)
{
	using namespace ArcRmlLayoutDumpPrivate;

	Rml::Context* RmlContext = Document.GetContext();
	if (!RmlContext)
	{
		return false;
	}
	const Rml::Vector2i Size = RmlContext->GetDimensions();
	const float DpRatio = RmlContext->GetDensityIndependentPixelRatio();
	const FString RootId = TEXT("0");

	FDumpContext Dump;
	AssignIds(Document, RootId, Dump);
	Dump.Json = TEXT("{\"document\":");
	AppendEscapeJsonString(Dump.Json, DocumentPath);
	Dump.Json += TEXT(",\"format\":2");
	Dump.Json += TEXT(",\"units\":\"RmlUi layout px in device pixels, absolute to the context origin; html and Chrome use the same device px (px and dp multiplied by dp_ratio, devpx unscaled, img and svg size attributes multiplied by dp_ratio)\"");
	Dump.Json.Appendf(TEXT(",\"context\":{\"width\":%d,\"height\":%d,\"dp_ratio\":"), Size.x, Size.y);
	AppendNumber(Dump.Json, DpRatio);
	Dump.Json += ArcRmlUIPerf::IsLayoutVerifyEnabled() ? TEXT(",\"layout_verify\":1") : TEXT(",\"layout_verify\":0");
	double ClockSeconds = 0.0;
	if (ArcRmlUIPerf::GetClockOverride(ClockSeconds))
	{
		Dump.Json.Appendf(TEXT(",\"clock\":%.17g"), ClockSeconds);
	}
	else
	{
		Dump.Json += TEXT(",\"clock\":null");
	}
	Dump.Json += TEXT("},\"elements\":[");
	DumpElement(Document, nullptr, true, Dump);
	Dump.Json += TEXT("\n]}\n");

	FHtmlContext Html;
	Html.DocumentPath = ArcRmlStrings::ToRmlString(DocumentPath);
	Html.FileInterface = &FileInterface;
	Html.GenericFamilies = &Dependencies.GenericFontFamilies;
	Html.DpRatio = DpRatio;
	AppendHtml(Document, RootId, Html);

	const FString ResolvedDocument = FileInterface.ResolvePath(DocumentPath);
	const FString BaseUrl = ToFileUrl(ResolvedDocument.IsEmpty() ? FString() : FPaths::GetPath(ResolvedDocument)) + TEXT("/");
	FString Page = TEXT("<!DOCTYPE html>\n<html><head><meta charset=\"utf-8\">\n");
	Page += FString::Printf(TEXT("<title>%s</title>\n<base href=\"%s\">\n"), *EscapeHtml(DocumentPath, false), *EscapeHtml(BaseUrl, true));
	Page += FString::Printf(TEXT("<meta name=\"arc-context\" content=\"%d,%d\">\n"), Size.x, Size.y);
	Page += TEXT("<style data-arc-source=\"fonts\">\n") + BuildFontFaces(Dependencies) + TEXT("</style>\n");
	Page += FString::Printf(TEXT("<style data-arc-source=\"rmlui-defaults\">\n")
		TEXT("html { margin: 0; padding: 0; width: %dpx; height: %dpx; overflow: hidden; font-size: 12px; font-family: \"%s\"; }\n")
		TEXT(":where(body, body *:not(svg, svg *, img)) { all: unset; min-width: 0; min-height: 0; }\n")
		TEXT("</style>\n"), Size.x, Size.y, *Dependencies.FallbackFontFamily);
	Page += BuildStyleSheets(DocumentPath, StyleSheetLinks, FileInterface, DpRatio, Dependencies.GenericFontFamilies);
	Page += TEXT("<style data-arc-source=\"freeze\">\n")
		TEXT("*, *::before, *::after { animation: none !important; transition: none !important; transform: none !important; translate: none !important; rotate: none !important; scale: none !important; }\n")
		TEXT("</style>\n</head>\n");
	Page += Html.Html;
	Page += TEXT("\n</html>\n");

	const FString Name = FPaths::GetBaseFilename(DocumentPath);
	OutResult.LayoutPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(OutDir, Name + TEXT(".layout.json")));
	OutResult.HtmlPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(OutDir, Name + TEXT(".html")));
	OutResult.Elements = Dump.Elements;
	return FFileHelper::SaveStringToFile(Dump.Json, *OutResult.LayoutPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)
		&& FFileHelper::SaveStringToFile(Page, *OutResult.HtmlPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

#endif
