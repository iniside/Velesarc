// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "ArcRmlUISettingsSubsystem.generated.h"

class UArcSettingsModel;

struct FArcRmlUISettingsDefaults
{
	float UIScale = 0.0f;
	float MaxUIAspectRatio = 0.0f;
};

UCLASS()
class ARCRMLUI_API UArcRmlUISettingsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static FArcRmlUISettingsDefaults GetDefaults();

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	void RegisterSettings(UArcSettingsModel& Model);
	static void ApplyUIScale(const UArcSettingsModel& Model);

	bool bOwnsApplicationScale = false;
};
