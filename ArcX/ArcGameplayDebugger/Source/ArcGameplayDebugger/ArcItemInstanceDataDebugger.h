#pragma once

struct FArcItemData;
struct FArcItemInstance;

class FArcItemInstanceDataDebugger
{
public:
	static void DrawGrantedAbilities(const FArcItemData* InItemData, const FArcItemInstance* InInstance);
	static void DrawAbilityEffectsToApply(const FArcItemData* InItemData, const FArcItemInstance* InInstance);
};

class FArcItemInstanceDebugger
{
public:
	static void DrawGrantedAbilities(const FArcItemData* InItemData, const FArcItemInstance* InInstance);
	static void DrawAbilityEffectsToApply(const FArcItemData* InItemData, const FArcItemInstance* InInstance);
};