#pragma once

class FArcGlobalAbilityTargetingDebugger
{
public:
	void Initialize();
	void Uninitialize();
	void Draw();

	bool bShow = false;
	struct DrawState
	{
		bool bShow = false;
		float Size = 50.f;
		float Color[3] = {1.f, 1.f, 1.f};

		TArray<bool> bDrawTargetingTask;
	};
	TArray<DrawState> DebugDraw;
};
