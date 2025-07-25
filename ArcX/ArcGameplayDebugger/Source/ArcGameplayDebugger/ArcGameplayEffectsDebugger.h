#pragma once
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"

class FArcGameplayEffectsDebugger
{
public:
	void Initialize();
	void Uninitialize();
	void Draw();

	bool bShow = false;
};

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
	
	struct ExplicitTagCountMap {
		typedef TMap<FGameplayTag, int32> FGameplayTagCountContainer::*type;
		friend type get(ExplicitTagCountMap);
	};

	template struct PropertyAccessor<ExplicitTagCountMap, &FGameplayTagCountContainer::ExplicitTagCountMap>;

	inline const TMap<FGameplayTag, int32>& GetPrivateExplicitTagCountMap(const FGameplayTagCountContainer* Component) {
		return Component->*get(ExplicitTagCountMap());
	}
	
}
