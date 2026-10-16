// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTacticsNodePresentation.h"

#include "ArcTacticsEditorStyle.h"
#include "Decision/ArcTacticsConfig.h"
#include "Decision/ArcTacticsNode.h"
#include "Framework/Text/ITextDecorator.h"
#include "Framework/Text/RichTextMarkupProcessing.h"
#include "Framework/Text/SlateTextRun.h"
#include "Styling/AppStyle.h"
#include "UObject/UnrealType.h"

namespace ArcTacticsNodePresentationPrivate
{
	class FTextStyleDecorator final : public ITextDecorator
	{
	public:
		FTextStyleDecorator(FString InTag, const FTextBlockStyle& InTextStyle)
			: Tag(MoveTemp(InTag))
			, TextStyle(InTextStyle)
		{
		}

		virtual bool Supports(const FTextRunParseResults& RunInfo, const FString&) const override
		{
			return RunInfo.Name == Tag;
		}

		virtual TSharedRef<ISlateRun> Create(
			const TSharedRef<FTextLayout>&,
			const FTextRunParseResults& RunParseResult,
			const FString& OriginalText,
			const TSharedRef<FString>& ModelText,
			const ISlateStyle*) override
		{
			FRunInfo RunInfo(RunParseResult.Name);
			for (const TPair<FString, FTextRange>& Pair : RunParseResult.MetaData)
			{
				RunInfo.MetaData.Add(
					Pair.Key,
					OriginalText.Mid(Pair.Value.BeginIndex, Pair.Value.EndIndex - Pair.Value.BeginIndex));
			}
			FString Run = RunParseResult.ContentRange.IsEmpty()
				? OriginalText.Mid(RunParseResult.OriginalRange.BeginIndex, RunParseResult.OriginalRange.Len())
				: OriginalText.Mid(RunParseResult.ContentRange.BeginIndex, RunParseResult.ContentRange.Len());

			ModelText->Append(Run);
			return FSlateTextRun::Create(RunInfo, ModelText, TextStyle);
		}

	private:
		FString Tag;
		FTextBlockStyle TextStyle;
	};

	FSlateIcon GetDefaultIcon(const EArcTacticsNodeRole Role)
	{
		FName StyleName = TEXT("Icons.Help");
		switch (Role)
		{
		case EArcTacticsNodeRole::GlobalTask:
			StyleName = TEXT("Icons.Download");
			break;
		case EArcTacticsNodeRole::Action:
			StyleName = TEXT("Icons.Play");
			break;
		case EArcTacticsNodeRole::Step:
			StyleName = TEXT("Icons.ArrowRight");
			break;
		case EArcTacticsNodeRole::ActiveStep:
			StyleName = TEXT("Icons.Pinned");
			break;
		case EArcTacticsNodeRole::Consideration:
			StyleName = TEXT("Icons.Adjust");
			break;
		case EArcTacticsNodeRole::Selector:
			StyleName = TEXT("Icons.Filter");
			break;
		default:
			break;
		}
		return FSlateIcon(FAppStyle::GetAppStyleSetName(), StyleName);
	}

	FSlateColor GetDefaultIconColor(const EArcTacticsNodeRole Role)
	{
		switch (Role)
		{
		case EArcTacticsNodeRole::GlobalTask:
			return FLinearColor(FColor(79, 153, 214));
		case EArcTacticsNodeRole::Action:
			return FLinearColor(FColor(225, 150, 65));
		case EArcTacticsNodeRole::Step:
			return FLinearColor(FColor(214, 184, 92));
		case EArcTacticsNodeRole::ActiveStep:
			return FLinearColor(FColor(120, 196, 104));
		case EArcTacticsNodeRole::Consideration:
			return FLinearColor(FColor(171, 108, 211));
		case EArcTacticsNodeRole::Selector:
			return FLinearColor(FColor(74, 183, 172));
		default:
			return FSlateColor::UseSubduedForeground();
		}
	}

	FText GetPresentationText(
		const FArcTacticsNodeDefinition& Definition,
		const FArcTacticsNode& Node,
		const IPropertyBindingBindingCollectionOwner& BindingOwner,
		const bool bRichText)
	{
		if (!Definition.Name.IsNone())
		{
			const FString Name = Definition.Name.ToString();
			return FText::FromString(bRichText ? FArcTacticsNodePresentation::EscapeRichText(Name) : Name);
		}

		const FText Description = Node.GetDescription(Definition.ID, Definition.InstanceData, BindingOwner, bRichText);
		if (!Description.IsEmpty())
		{
			return Description;
		}

		const FText DisplayName = Node.GetDisplayName();
		return bRichText
			? FText::FromString(FArcTacticsNodePresentation::EscapeRichText(DisplayName.ToString()))
			: DisplayName;
	}
}

FArcTacticsNodePresentation FArcTacticsNodePresentation::Build(
	const FArcTacticsNodeDefinition& Definition,
	const IPropertyBindingBindingCollectionOwner& BindingOwner)
{
	FArcTacticsNodePresentation Result;
	const FArcTacticsNode* Node = Definition.Node.GetPtr<FArcTacticsNode>();
	if (!Node)
	{
		Result.RichText = NSLOCTEXT("ArcTactics", "MissingNodeRich", "<s>None</>");
		Result.PlainText = NSLOCTEXT("ArcTactics", "MissingNode", "None");
		Result.Icon = ArcTacticsNodePresentationPrivate::GetDefaultIcon(EArcTacticsNodeRole::Invalid);
		Result.IconColor = FSlateColor::UseSubduedForeground();
		return Result;
	}

	Result.RichText = ArcTacticsNodePresentationPrivate::GetPresentationText(Definition, *Node, BindingOwner, true);
	Result.PlainText = ArcTacticsNodePresentationPrivate::GetPresentationText(Definition, *Node, BindingOwner, false);
	const bool bUseRoleFallback = Node->GetIconName().IsNone();
	Result.Icon = bUseRoleFallback ? ArcTacticsNodePresentationPrivate::GetDefaultIcon(Node->GetRole()) : ParseIcon(Node->GetIconName());
	Result.IconColor = bUseRoleFallback ? ArcTacticsNodePresentationPrivate::GetDefaultIconColor(Node->GetRole()) : FSlateColor(FLinearColor(Node->GetIconColor()));
	return Result;
}

FArcTacticsNodePresentation FArcTacticsNodePresentation::BuildContext()
{
	const FText DisplayName = NSLOCTEXT("ArcTactics", "ContextDisplayName", "Context");
	TArray<FString> Outputs;
	for (TFieldIterator<FProperty> It(FArcTacticsContextData::StaticStruct()); It; ++It)
	{
		const FProperty* Property = *It;
		if (!Property->HasMetaData(TEXT("ArcTacticsOutput")))
		{
			continue;
		}
		const FStructProperty* StructProperty = CastField<FStructProperty>(Property);
		const FString Type = StructProperty
			? StructProperty->Struct->GetDisplayNameText().ToString()
			: Property->GetCPPType();
		Outputs.Add(FString::Printf(TEXT("%s : %s"), *Property->GetDisplayNameText().ToString(), *Type));
	}
	const FString OutputText = FString::Join(Outputs, TEXT(", "));

	FArcTacticsNodePresentation Result;
	Result.RichText = FText::FromString(FString::Printf(
		TEXT("%s  <s>%s</>"),
		*EscapeRichText(DisplayName.ToString()),
		*EscapeRichText(OutputText)));
	Result.PlainText = FText::FromString(FString::Printf(
		TEXT("%s  %s"),
		*DisplayName.ToString(),
		*OutputText));
	Result.Icon = ParseIcon(TEXT("AppStyle|Icons.Info"));
	Result.IconColor = FSlateColor::UseForeground();
	return Result;
}

FSlateIcon FArcTacticsNodePresentation::ParseIcon(const FName IconName)
{
	FString Remaining = IconName.ToString();
	FName Names[4] = { NAME_None, NAME_None, NAME_None, NAME_None };
	int32 NameIndex = 0;
	while (!Remaining.IsEmpty() && NameIndex < UE_ARRAY_COUNT(Names))
	{
		FString Left;
		FString Right;
		if (!Remaining.Split(TEXT("|"), &Left, &Right))
		{
			Left = Remaining;
			Right.Reset();
		}
		Names[NameIndex++] = FName(*Left);
		Remaining = MoveTemp(Right);
	}
	if (Names[0] == TEXT("AppStyle"))
	{
		Names[0] = FAppStyle::GetAppStyleSetName();
	}
	return FSlateIcon(Names[0], Names[1], Names[2], Names[3]);
}

FText FArcTacticsNodePresentation::GetPropertyRoleLabel(const EArcTacticsPropertyRole Role)
{
	switch (Role)
	{
	case EArcTacticsPropertyRole::Input:
		return NSLOCTEXT("ArcTactics", "InputRole", "Input");
	case EArcTacticsPropertyRole::Parameter:
		return NSLOCTEXT("ArcTactics", "ParameterRole", "Parameter");
	case EArcTacticsPropertyRole::Output:
		return NSLOCTEXT("ArcTactics", "OutputRole", "Output");
	default:
		return FText::GetEmpty();
	}
}

FText FArcTacticsNodePresentation::GetPropertyRoleTooltip(const EArcTacticsPropertyRole Role)
{
	switch (Role)
	{
	case EArcTacticsPropertyRole::Input:
		return NSLOCTEXT("ArcTactics", "InputRoleTooltip", "Reads a value from an accessible source.");
	case EArcTacticsPropertyRole::Parameter:
		return NSLOCTEXT("ArcTactics", "ParameterRoleTooltip", "Uses its authored default until a binding supplies a value.");
	case EArcTacticsPropertyRole::Output:
		return NSLOCTEXT("ArcTactics", "OutputRoleTooltip", "Writes this value into a writable parameter bag in this scope.");
	default:
		return FText::GetEmpty();
	}
}

FSlateColor FArcTacticsNodePresentation::GetPropertyRoleColor(const EArcTacticsPropertyRole Role)
{
	switch (Role)
	{
	case EArcTacticsPropertyRole::Input:
		return FLinearColor(FColor(79, 153, 214));
	case EArcTacticsPropertyRole::Parameter:
		return FLinearColor(FColor(225, 150, 65));
	case EArcTacticsPropertyRole::Output:
		return FLinearColor(FColor(74, 183, 172));
	default:
		return FSlateColor::UseSubduedForeground();
	}
}

FText FArcTacticsNodePresentation::GetInputRequirementLabel(const bool bRequired)
{
	return bRequired
		? NSLOCTEXT("ArcTactics", "RequiredInput", "Required")
		: NSLOCTEXT("ArcTactics", "OptionalInput", "Optional");
}

FText FArcTacticsNodePresentation::GetInputRequirementTooltip(const bool bRequired)
{
	return bRequired
		? NSLOCTEXT("ArcTactics", "RequiredInputTooltip", "Must be bound; the config does not compile without a binding.")
		: NSLOCTEXT("ArcTactics", "OptionalInputTooltip", "May stay unbound.");
}

FSlateColor FArcTacticsNodePresentation::GetInputRequirementColor(const bool bRequired)
{
	return bRequired
		? FSlateColor(FLinearColor(FColor(224, 88, 80)))
		: FSlateColor(FLinearColor(FColor(150, 150, 150)));
}

TSharedRef<ITextDecorator> FArcTacticsNodePresentation::CreateTextDecorator(const FName Tag)
{
	FName StyleName = TEXT("ArcTactics.Node.Normal");
	if (Tag == TEXT("b"))
	{
		StyleName = TEXT("ArcTactics.Node.Bold");
	}
	else if (Tag == TEXT("s"))
	{
		StyleName = TEXT("ArcTactics.Node.Subdued");
	}
	return MakeShared<ArcTacticsNodePresentationPrivate::FTextStyleDecorator>(
		Tag.IsNone() ? FString() : Tag.ToString(),
		FArcTacticsEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>(StyleName));
}

FString FArcTacticsNodePresentation::EscapeRichText(const FStringView Text)
{
	FString Result(Text);
	FDefaultRichTextMarkupWriter::EscapeText(Result);
	return Result;
}
