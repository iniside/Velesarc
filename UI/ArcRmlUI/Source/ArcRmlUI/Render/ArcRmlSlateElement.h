// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcRmlUIPerf.h"
#include "Render/ArcRmlRdgReplay.h"
#include "Render/ArcRmlRenderResources.h"
#include "Render/ArcRmlRenderTypes.h"
#include "Rendering/RenderingCommon.h"

class FRHICommandListImmediate;

class FArcRmlSlateElement final : public ICustomSlateElement
{
public:
	void SubmitFrame_RenderThread(FRHICommandListImmediate& RHICmdList, TUniquePtr<FArcRmlRenderFrame> Frame);
	void SetDestination_RenderThread(const FIntRect& InDestination);
	void ReleaseResources_RenderThread();
#if !UE_BUILD_SHIPPING
	void SetPerfLabel_RenderThread(uint32 InViewId, const FString& InDocumentPath);
#endif

	virtual void Draw_RenderThread(FRDGBuilder& GraphBuilder, const FDrawPassInputs& Inputs) override;

private:
	FArcRmlRenderResources Resources;
	TUniquePtr<FArcRmlRenderFrame> CurrentFrame;
	bool bCurrentFrameReplayed = false;
	FIntRect Destination;
	bool bLoggedNoBackdrop = false;

#if !UE_BUILD_SHIPPING
	struct FPerf
	{
		int32 Submitted = 0;
		int32 Superseded = 0;
		int32 OffscreenReplays = 0;
		ArcRmlUIPerf::FTiming Submit;
		ArcRmlUIPerf::FTiming Replay;
		int32 BackdropSeeds = 0;
		int32 BackdropCopies = 0;
		ArcRmlRdg::FReplayStats ReplayStats;
	};

	void ReportPerf_RenderThread();

	uint32 PerfViewId = 0;
	FString PerfDocumentPath;
	FPerf Perf;
	ArcRmlUIPerf::FReportClock PerfClock;
#endif
};
