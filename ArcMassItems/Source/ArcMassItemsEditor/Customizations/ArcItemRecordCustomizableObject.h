// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class IPropertyHandle;
class UCustomizableObject;

namespace ArcItemRecordCustomizableObject
{
	UCustomizableObject* Find(const TSharedPtr<IPropertyHandle>& Handle);
}
