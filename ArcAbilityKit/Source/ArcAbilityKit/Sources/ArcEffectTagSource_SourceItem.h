// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Effects/ArcEffectTagSource.h"

struct ARCABILITYKIT_API FArcEffectTagSource_SourceItem : public FArcEffectTagSource
{
	virtual void AppendTags(const FArcEffectContext& Context, FGameplayTagContainer& OutSourceTags,
	                        FGameplayTagContainer& OutTargetTags) const override;
};
