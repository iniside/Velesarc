#pragma once
#include "AttributeSet.h"
#include "GameplayEffect.h"
#include "GameplayEffectAggregator.h"

class FArcAttributesDebugger
{
public:
	void Initialize();
	void Uninitialize();
	void Draw();

	bool bShow = false;
};

namespace AttrHacks
{
	template<typename Tag, typename Tag::type M>
	struct PropertyAccessor {
		friend typename Tag::type get(Tag) {
			return M;
		}
	};

	// Define a tag for the specific member you want to access
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
	inline const FAggregatorModChannelContainer& GetPrivateAggregatorModChannels(const FAggregator* Component) {
		return Component->*get(AggregatorModChannels());
	}
}
