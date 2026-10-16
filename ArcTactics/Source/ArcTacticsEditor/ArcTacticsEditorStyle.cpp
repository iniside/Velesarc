// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTacticsEditorStyle.h"

#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/StyleColors.h"

FArcTacticsEditorStyle::FArcTacticsEditorStyle()
	: FSlateStyleSet(GetArcTacticsStyleSetName())
{
	SetParentStyleName(FAppStyle::GetAppStyleSetName());

	const FTextBlockStyle& NormalText = FAppStyle::GetWidgetStyle<FTextBlockStyle>(TEXT("NormalText"));
	const FSlateFontInfo NormalFont = FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont"));
	const FSlateFontInfo BoldFont = FAppStyle::GetFontStyle(TEXT("PropertyWindow.BoldFont"));

	const FTextBlockStyle NodeNormal = FTextBlockStyle(NormalText)
		.SetFont(NormalFont)
		.SetFontSize(10);
	Set(TEXT("ArcTactics.Node.Normal"), NodeNormal);
	Set(TEXT("ArcTactics.Node.Bold"), FTextBlockStyle(NormalText)
		.SetFont(BoldFont)
		.SetFontSize(10));
	Set(TEXT("ArcTactics.Node.Subdued"), FTextBlockStyle(NormalText)
		.SetColorAndOpacity(FSlateColor::UseSubduedForeground())
		.SetFont(NormalFont)
		.SetFontSize(10));
	Set(TEXT("ArcTactics.Role.Label"), FTextBlockStyle(NormalText)
		.SetFont(BoldFont)
		.SetFontSize(8));

	FEditableTextBoxStyle EditableText = FAppStyle::GetWidgetStyle<FEditableTextBoxStyle>(TEXT("NormalEditableTextBox"));
	EditableText.SetTextStyle(NodeNormal);
	Set(TEXT("ArcTactics.Node.TitleInlineEditableText"), FInlineEditableTextBlockStyle()
		.SetTextStyle(NodeNormal)
		.SetEditableTextBoxStyle(EditableText));

	Set(TEXT("ArcTactics.Node.Row"), new FSlateRoundedBoxBrush(FStyleColors::Recessed, 5.0f));
	Set(TEXT("ArcTactics.Node.Row.Hovered"), new FSlateRoundedBoxBrush(FStyleColors::Hover, 5.0f));
	Set(TEXT("ArcTactics.Node.Row.Selected"), new FSlateRoundedBoxBrush(
		FStyleColors::Select.GetSpecifiedColor().CopyWithNewOpacity(0.32f),
		5.0f,
		FStyleColors::Select,
		1.0f));
	Set(TEXT("ArcTactics.Node.Row.Focused"), new FSlateRoundedBoxBrush(
		FStyleColors::Recessed,
		5.0f,
		FStyleColors::Primary,
		2.0f));
	Set(TEXT("ArcTactics.Node.Row.DropTarget"), new FSlateRoundedBoxBrush(
		FStyleColors::Recessed,
		5.0f,
		FStyleColors::AccentGreen,
		2.0f));
	Set(TEXT("ArcTactics.Node.Row.Error"), new FSlateRoundedBoxBrush(
		FStyleColors::Recessed,
		5.0f,
		FStyleColors::Error,
		2.0f));
	Set(TEXT("ArcTactics.Role.Input"), new FSlateRoundedBoxBrush(
		FLinearColor(FColor(79, 153, 214)).CopyWithNewOpacity(0.18f), 3.0f));
	Set(TEXT("ArcTactics.Role.Parameter"), new FSlateRoundedBoxBrush(
		FLinearColor(FColor(225, 150, 65)).CopyWithNewOpacity(0.18f), 3.0f));
	Set(TEXT("ArcTactics.Role.Output"), new FSlateRoundedBoxBrush(
		FLinearColor(FColor(74, 183, 172)).CopyWithNewOpacity(0.18f), 3.0f));
	Set(TEXT("ArcTactics.Requirement.Required"), new FSlateRoundedBoxBrush(
		FLinearColor(FColor(224, 88, 80)).CopyWithNewOpacity(0.18f), 3.0f));
	Set(TEXT("ArcTactics.Requirement.Optional"), new FSlateRoundedBoxBrush(
		FLinearColor(FColor(150, 150, 150)).CopyWithNewOpacity(0.18f), 3.0f));
}

void FArcTacticsEditorStyle::Register()
{
	FSlateStyleRegistry::RegisterSlateStyle(Get());
}

void FArcTacticsEditorStyle::Unregister()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(Get());
}

FArcTacticsEditorStyle& FArcTacticsEditorStyle::Get()
{
	static FArcTacticsEditorStyle Instance;
	return Instance;
}

FName FArcTacticsEditorStyle::GetArcTacticsStyleSetName()
{
	static const FName StyleSetName(TEXT("ArcTacticsEditorStyle"));
	return StyleSetName;
}
