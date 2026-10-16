// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Optional.h"
#include "Templates/SharedPointer.h"

class FJsonObject;
struct FArcTacticsRecording;

enum class EArcTacticsRecordingQueryItems : uint8
{
	All,
	Valid,
	None
};

struct FArcTacticsRecordingJsonOptions
{
	TOptional<uint32> EntityIndex;
	TOptional<uint32> EntitySerial;
	TOptional<double> FromTime;
	TOptional<double> ToTime;
	EArcTacticsRecordingQueryItems QueryItems = EArcTacticsRecordingQueryItems::All;
};

struct FArcTacticsRecordingJsonResult
{
	FString Json;
	TSharedPtr<FJsonObject> Summary;
};

namespace ArcTacticsRecordingJson
{
	ARCGAMEPLAYDEBUGGER_API FArcTacticsRecordingJsonResult Write(const FArcTacticsRecording& Recording, const FArcTacticsRecordingJsonOptions& Options);
}
