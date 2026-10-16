// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UArcMassEntityConfigAsset;
class UArcMassEntityConfigTemplate;
enum class EArcMassConfigMergeMode : uint8;

class FArcMassEntityConfigHelpers
{
public:
	/**
	 * Open a modal asset picker filtered to UArcMassEntityConfigTemplate,
	 * with a live side-by-side merge preview (template / merged result / original)
	 * for each selected variant. Returns true if OK was pressed and a template
	 * was selected. OutTemplate may be nullptr if the user picked the empty row.
	 */
	static bool PickTemplateWithPreview(
		UArcMassEntityConfigTemplate*& OutTemplate,
		TArray<UArcMassEntityConfigAsset*> InSelectedVariants,
		EArcMassConfigMergeMode MergeMode);

	/**
	 * Open a modal preview showing what UpdateFromTemplate would produce
	 * for each selected variant, given their currently linked SourceTemplate.
	 * Returns true if OK was pressed.
	 */
	static bool PreviewUpdate(
		UArcMassEntityConfigTemplate* InTemplate,
		TArray<UArcMassEntityConfigAsset*> InSelectedVariants);
};
