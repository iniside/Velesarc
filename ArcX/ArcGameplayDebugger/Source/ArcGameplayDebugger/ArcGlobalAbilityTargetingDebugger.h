#pragma once

class FArcGlobalAbilityTargetingDebugger
{
public:
	void Initialize();
	void Uninitialize();
	void Draw();

	bool bShow = false;
	TArray<bool> bDrawDebug;
};
