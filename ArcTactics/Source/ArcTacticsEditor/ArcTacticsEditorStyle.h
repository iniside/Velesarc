// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Styling/SlateStyle.h"

class FArcTacticsEditorStyle final : public FSlateStyleSet
{
public:
	static void Register();
	static void Unregister();
	static FArcTacticsEditorStyle& Get();
	static FName GetArcTacticsStyleSetName();

private:
	FArcTacticsEditorStyle();
};
