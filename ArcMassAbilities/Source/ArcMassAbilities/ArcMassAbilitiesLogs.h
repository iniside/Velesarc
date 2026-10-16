// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Logging/LogMacros.h"

DECLARE_LOG_CATEGORY_EXTERN(LogArcMassAbilities, Log, All)

// Engine VisualLogger gates on UE_DEBUG_RECORDING_ENABLED in ue5-main
// (Engine/Source/Runtime/Engine/Public/VisualLogger/VisualLogger.h:65).
// ENABLE_VISUAL_LOG is a legacy 5.4/5.5 guard and is NOT defined here.
#if UE_DEBUG_RECORDING_ENABLED
#include "VisualLogger/VisualLogger.h"
#include "MassEntitySubsystem.h"

#define ARCMA_VLOG(World, Verbosity, Format, ...) \
    UE_VLOG(UWorld::GetSubsystem<UMassEntitySubsystem>(World), \
            LogArcMassAbilities, Verbosity, Format, ##__VA_ARGS__)

#define ARCMA_VLOG_LOCATION(World, Location, Radius, Color, Format, ...) \
    UE_VLOG_LOCATION(UWorld::GetSubsystem<UMassEntitySubsystem>(World), \
                     LogArcMassAbilities, Log, Location, Radius, Color, Format, ##__VA_ARGS__)

#define ARCMA_VLOG_SEGMENT(World, Start, End, Color, Format, ...) \
    UE_VLOG_SEGMENT(UWorld::GetSubsystem<UMassEntitySubsystem>(World), \
                    LogArcMassAbilities, Log, Start, End, Color, Format, ##__VA_ARGS__)

#define ARCMA_VLOG_SPHERE(World, Location, Radius, Color, Format, ...) \
    UE_VLOG_SPHERE(UWorld::GetSubsystem<UMassEntitySubsystem>(World), \
                   LogArcMassAbilities, Log, Location, Radius, Color, Format, ##__VA_ARGS__)
#else
#define ARCMA_VLOG(World, Verbosity, Format, ...)                          (void)0
#define ARCMA_VLOG_LOCATION(World, Location, Radius, Color, Format, ...)   (void)0
#define ARCMA_VLOG_SEGMENT(World, Start, End, Color, Format, ...)          (void)0
#define ARCMA_VLOG_SPHERE(World, Location, Radius, Color, Format, ...)     (void)0
#endif
