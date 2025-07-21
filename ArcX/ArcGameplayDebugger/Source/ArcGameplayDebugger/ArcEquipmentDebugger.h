#pragma once

class FArcEquipmentDebugger
{
public:
	void Initialize();
	void Uninitialize();
	void Draw();

	bool bShow = false;

	int32 SelectedItemStoreIndex = -1;
};
