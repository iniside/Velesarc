// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class SWidget;
class UObject;

namespace ArcRmlPreviewFields
{
	TSharedRef<SWidget> MakeFieldList(UObject& ViewModel);
}
