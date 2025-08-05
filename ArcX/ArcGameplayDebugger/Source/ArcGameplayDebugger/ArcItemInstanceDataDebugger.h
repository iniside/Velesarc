#pragma once

struct FGameplayEffectSpecHandle;
struct FGameplayEffectSpec;
class UGameplayEffect;
struct FGameplayAbilitySpec;
class UArcCoreAbilitySystemComponent;
struct FArcItemData;
struct FArcItemInstance;

class FArcItemInstanceDebugger
{
public:
	static void DrawGrantedAbilities(const FArcItemData* InItemData, const FArcItemInstance* InInstance);
	static void DrawAbilityEffectsToApply(const FArcItemData* InItemData, const FArcItemInstance* InInstance);
	static void DrawGrantedPassiveAbilities(const FArcItemData* InItemData, const FArcItemInstance* InInstance);
	static void DrawGrantedEffects(const FArcItemData* InItemData, const FArcItemInstance* InInstance);
	static void DrawItemStats(const FArcItemData* InItemData, const FArcItemInstance* Instance);
	
	static void DrawGameplayEffectSpec(const FGameplayEffectSpecHandle* Spec);
	static void DrawGameplayAbilitySpec(UArcCoreAbilitySystemComponent* InASC, const FGameplayAbilitySpec* Spec);
};