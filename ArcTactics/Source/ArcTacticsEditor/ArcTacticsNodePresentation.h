// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Textures/SlateIcon.h"

class IPropertyBindingBindingCollectionOwner;
class ITextDecorator;
struct FArcTacticsNode;
struct FArcTacticsNodeDefinition;
enum class EArcTacticsPropertyRole : uint8;

struct ARCTACTICSEDITOR_API FArcTacticsNodePresentation
{
	FText RichText;
	FText PlainText;
	FSlateIcon Icon;
	FSlateColor IconColor;

	static FArcTacticsNodePresentation Build(
		const FArcTacticsNodeDefinition& Definition,
		const IPropertyBindingBindingCollectionOwner& BindingOwner);
	static FArcTacticsNodePresentation BuildContext();

	static FSlateIcon ParseIcon(FName IconName);
	static FText GetPropertyRoleLabel(EArcTacticsPropertyRole Role);
	static FText GetPropertyRoleTooltip(EArcTacticsPropertyRole Role);
	static FSlateColor GetPropertyRoleColor(EArcTacticsPropertyRole Role);
	static FText GetInputRequirementLabel(bool bRequired);
	static FText GetInputRequirementTooltip(bool bRequired);
	static FSlateColor GetInputRequirementColor(bool bRequired);
	static TSharedRef<ITextDecorator> CreateTextDecorator(FName Tag);
	static FString EscapeRichText(FStringView Text);
};
