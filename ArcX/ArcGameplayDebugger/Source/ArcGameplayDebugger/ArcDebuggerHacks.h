#pragma once

#include "GameplayEffect.h"
#include "GameplayEffectAggregator.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"

namespace DebugHack
{
	template<typename Tag, typename Tag::type M>
	struct PropertyAccessor {
		friend typename Tag::type get(Tag) {
			return M;
		}
	};

	// Define a tag for the specific member you want to access
	struct GameplayTagCountMap {
		typedef TMap<FGameplayTag, int32> FGameplayTagCountContainer::*type;
		friend type get(GameplayTagCountMap);
	};
	
	template struct PropertyAccessor<GameplayTagCountMap, &FGameplayTagCountContainer::GameplayTagCountMap>;
	inline const TMap<FGameplayTag, int32>& GetPrivateGameplayTagCountMap(const FGameplayTagCountContainer* Component) {
		return Component->*get(GameplayTagCountMap());
	}

	struct AttributeAggregatorMap {
		typedef TMap<FGameplayAttribute, FAggregatorRef> FActiveGameplayEffectsContainer::*type;
		friend type get(AttributeAggregatorMap);
	};
	
	template struct PropertyAccessor<AttributeAggregatorMap, &FActiveGameplayEffectsContainer::AttributeAggregatorMap>;
	inline const TMap<FGameplayAttribute, FAggregatorRef>& GetPrivateAggregaotMap(const FActiveGameplayEffectsContainer* Component) {
		return Component->*get(AttributeAggregatorMap());
	}
	
	struct AggregatorModChannels {
		typedef FAggregatorModChannelContainer FAggregator::*type;
		friend type get(AggregatorModChannels);
	};
	
	template struct PropertyAccessor<AggregatorModChannels, &FAggregator::ModChannels>;
	inline const FAggregatorModChannelContainer& GetPrivateAggregatorModChannels(const FAggregator* Component)
	{
		return Component->*get(AggregatorModChannels());
	}
	
}
