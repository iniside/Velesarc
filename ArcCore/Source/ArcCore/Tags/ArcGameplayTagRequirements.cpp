/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or –
 * as soon as they will be approved by the European Commission – later versions
 * of the EUPL (the "License");
 *
 * You may not use this work except in compliance with the License.
 * You may get a copy of the License at:
 *
 * https://eupl.eu/
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 */

#include "Tags/ArcGameplayTagRequirements.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcGameplayTagRequirements)

bool FArcGameplayTagRequirements::RequirementsMet(const FGameplayTagContainer& Container) const
{
	const bool bHasRequired = Container.HasAll(RequireTags);
	const bool bHasIgnored = Container.HasAny(IgnoreTags);
	const bool bMatchQuery = TagQuery.IsEmpty() || TagQuery.Matches(Container);

	return bHasRequired && !bHasIgnored && bMatchQuery;
}

bool FArcGameplayTagRequirements::IsEmpty() const
{
	return (RequireTags.Num() == 0 && IgnoreTags.Num() == 0 && TagQuery.IsEmpty());
}

FString FArcGameplayTagRequirements::ToString() const
{
	FString Str;

	if (RequireTags.Num() > 0)
	{
		Str += FString::Printf(TEXT("require: %s "), *RequireTags.ToStringSimple());
	}
	if (IgnoreTags.Num() > 0)
	{
		Str += FString::Printf(TEXT("ignore: %s "), *IgnoreTags.ToStringSimple());
	}
	if (!TagQuery.IsEmpty())
	{
		Str += TagQuery.GetDescription();
	}

	return Str;
}

bool FArcGameplayTagRequirements::operator==(const FArcGameplayTagRequirements& Other) const
{
	return RequireTags == Other.RequireTags && IgnoreTags == Other.IgnoreTags && TagQuery == Other.TagQuery;
}

bool FArcGameplayTagRequirements::operator!=(const FArcGameplayTagRequirements& Other) const
{
	return !(*this == Other);
}

FGameplayTagQuery FArcGameplayTagRequirements::ConvertTagFieldsToTagQuery() const
{
	const bool bHasRequireTags = !RequireTags.IsEmpty();
	const bool bHasIgnoreTags = !IgnoreTags.IsEmpty();

	if (!bHasIgnoreTags && !bHasRequireTags)
	{
		return FGameplayTagQuery{};
	}

	// FGameplayTagContainer::RequirementsMet is HasAll(RequireTags) && !HasAny(IgnoreTags)
	FGameplayTagQueryExpression RequiredTagsQueryExpression = FGameplayTagQueryExpression().AllTagsMatch().AddTags(RequireTags);
	FGameplayTagQueryExpression IgnoreTagsQueryExpression = FGameplayTagQueryExpression().NoTagsMatch().AddTags(IgnoreTags);

	FGameplayTagQueryExpression RootQueryExpression;
	if (bHasRequireTags && bHasIgnoreTags)
	{
		RootQueryExpression = FGameplayTagQueryExpression().AllExprMatch().AddExpr(RequiredTagsQueryExpression).AddExpr(IgnoreTagsQueryExpression);
	}
	else if (bHasRequireTags)
	{
		RootQueryExpression = RequiredTagsQueryExpression;
	}
	else // bHasIgnoreTags
	{
		RootQueryExpression = IgnoreTagsQueryExpression;
	}

	return FGameplayTagQuery::BuildQuery(RootQueryExpression);
}
