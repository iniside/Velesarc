// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Logging/LogMacros.h"

/**
 * Diagnostic logging for componentless entity visualization: holder registries, render-state
 * helpers, and scene-proxy lifecycle. Default verbosity captures per-holder lifecycle events
 * (proxy create/destroy, holder destroy, teardown sweeps) and bookkeeping-desync warnings;
 * switch to Verbose (`Log LogArcMassVisDiag Verbose`) for per-instance add/remove/swap tracing.
 */
ARCMASSRENDER_API DECLARE_LOG_CATEGORY_EXTERN(LogArcMassVisDiag, Log, All);
