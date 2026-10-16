// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Templates/SharedPointer.h"

struct FArcEffectContext;

struct ARCMASSABILITIES_API FArcEffectTagSource
{
	virtual ~FArcEffectTagSource() = default;

	virtual void AppendTags(const FArcEffectContext& Context, FGameplayTagContainer& OutSourceTags,
	                        FGameplayTagContainer& OutTargetTags) const = 0;
};

struct ARCMASSABILITIES_API FArcEffectTagSource_OwnedTags : public FArcEffectTagSource
{
	virtual void AppendTags(const FArcEffectContext& Context, FGameplayTagContainer& OutSourceTags,
	                        FGameplayTagContainer& OutTargetTags) const override;
};

struct ARCMASSABILITIES_API FArcEffectTagSource_Definition : public FArcEffectTagSource
{
	virtual void AppendTags(const FArcEffectContext& Context, FGameplayTagContainer& OutSourceTags,
	                        FGameplayTagContainer& OutTargetTags) const override;
};

struct ARCMASSABILITIES_API FArcEffectTagSource_Ability : public FArcEffectTagSource
{
	virtual void AppendTags(const FArcEffectContext& Context, FGameplayTagContainer& OutSourceTags,
	                        FGameplayTagContainer& OutTargetTags) const override;
};

struct ARCMASSABILITIES_API FArcEffectTagSource_Origin : public FArcEffectTagSource
{
	virtual void AppendTags(const FArcEffectContext& Context, FGameplayTagContainer& OutSourceTags,
	                        FGameplayTagContainer& OutTargetTags) const override;
};

namespace ArcEffectTagSources
{
	ARCMASSABILITIES_API void Register(const TSharedRef<const FArcEffectTagSource>& Source);

	ARCMASSABILITIES_API void Unregister(const TSharedRef<const FArcEffectTagSource>& Source);

	ARCMASSABILITIES_API void GatherTags(const FArcEffectContext& Context, FGameplayTagContainer& OutSourceTags,
	                                     FGameplayTagContainer& OutTargetTags);
}
