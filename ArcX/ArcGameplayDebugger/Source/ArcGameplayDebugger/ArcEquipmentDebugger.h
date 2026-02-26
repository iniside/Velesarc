// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

class FArcEquipmentDebugger
{
public:
	void Initialize();
	void Uninitialize();
	void Draw();

	bool bShow = false;

private:
	void DrawOverview(class APlayerController* PC, class UArcEquipmentComponent* EquipmentComponent);
	void DrawSlotDetails(class APlayerController* PC, class UArcEquipmentComponent* EquipmentComponent, int32 SlotIdx);
	void DrawItemsStoreInfo(class UArcEquipmentComponent* EquipmentComponent);

	int32 SelectedSlotIdx = -1;
};
