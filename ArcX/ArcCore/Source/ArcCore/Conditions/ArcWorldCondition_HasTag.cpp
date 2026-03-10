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

#include "ArcCore/Conditions/ArcWorldCondition_HasTag.h"
#include "ArcCore/Conditions/ArcWorldConditionSchema.h"
#include "GameFramework/Actor.h"
#include "WorldConditionContext.h"
#include "GameplayTagAssetInterface.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcWorldCondition_HasTag)

bool FArcWorldCondition_HasTag::Initialize(const UWorldConditionSchema& Schema)
{
	const UArcWorldConditionSchema* ArcSchema = Cast<UArcWorldConditionSchema>(&Schema);
	if (!ArcSchema)
	{
		return false;
	}

	SourceActorRef = ArcSchema->GetSourceActorRef();
	bCanCacheResult = false; // Tags can change at runtime
	return true;
}

FWorldConditionResult FArcWorldCondition_HasTag::IsTrue(const FWorldConditionContext& Context) const
{
	FWorldConditionResult Result(EWorldConditionResultValue::IsFalse, bCanCacheResult);

	const AActor* Actor = Context.GetContextDataPtr<AActor>(SourceActorRef);
	if (!Actor)
	{
		return Result;
	}

	if (const IGameplayTagAssetInterface* TagInterface = Cast<IGameplayTagAssetInterface>(Actor))
	{
		FGameplayTagContainer OwnedTags;
		TagInterface->GetOwnedGameplayTags(OwnedTags);
		if (TagQuery.Matches(OwnedTags))
		{
			Result.Value = EWorldConditionResultValue::IsTrue;
		}
	}

	return Result;
}

#if WITH_EDITOR
FText FArcWorldCondition_HasTag::GetDescription() const
{
	return FText::FromString(TagQuery.GetDescription());
}
#endif
