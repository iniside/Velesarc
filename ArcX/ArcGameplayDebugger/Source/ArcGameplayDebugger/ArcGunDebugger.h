#pragma once

class FArcGunDebugger
{
public:
	void Initialize();
	void Uninitialize();
	void Draw();

	bool bShow = false;

	bool bDrawTargeting = false;
	float TargetingDrawColor[3]  {0.f, 0.f, 0.f};
	float TargetingDrawSize = 1.f;

	bool bDrawCameraDirection = false;
	float CameraDirectionDrawColor[3]  {0.f, 0.f, 0.f};
	float CameraDirectionDrawSize = 1.f;
	float CameraDirectionDrawDistance = 1000.f;
	
	bool bDrawCameraPoint = false;
	float CameraPointDrawColor[3]  {0.f, 0.f, 0.f};
	float CameraPointDrawSize = 1.f;
	float CameraPointDrawOffset = 200.f;
};
