// Copyright Lukasz Baran. All Rights Reserved.

#include "Render/ArcRmlSlateElement.h"

#include "ArcRmlUILog.h"
#include "HAL/PlatformTime.h"
#include "Render/ArcRmlRdgReplay.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RenderingThread.h"

void FArcRmlSlateElement::SubmitFrame_RenderThread(FRHICommandListImmediate& RHICmdList, TUniquePtr<FArcRmlRenderFrame> Frame)
{
	check(IsInRenderingThread());
	if (!Frame.IsValid())
	{
		return;
	}

#if !UE_BUILD_SHIPPING
	const uint64 SubmitStartCycles = FPlatformTime::Cycles64();
	++Perf.Submitted;
#endif
	Resources.ApplyCreates(RHICmdList, *Frame);
	Frame->Geometries.Empty();
	Frame->Textures.Empty();

	if (CurrentFrame.IsValid())
	{
		ARCRMLUI_PERF(Perf.Superseded += bCurrentFrameReplayed ? 0 : 1);
		if (!bCurrentFrameReplayed && CurrentFrame->bSavesLayerTextures)
		{
			ARCRMLUI_PERF(++Perf.OffscreenReplays);
			FRDGBuilder GraphBuilder(RHICmdList, RDG_EVENT_NAME("ArcRmlSupersededFrame"));
			const FRDGTextureRef BaseLayer = ArcRmlRdg::CreateLayerTexture(GraphBuilder, CurrentFrame->ViewSize, TEXT("ArcRmlOffscreenBase"));
			AddClearRenderTargetPass(GraphBuilder, BaseLayer, FLinearColor::Transparent);
			FArcRmlRdgReplayer Replayer(GraphBuilder, Resources, *CurrentFrame);
			Replayer.Replay(BaseLayer);
			GraphBuilder.Execute();
		}
		Resources.ApplyReleases(*CurrentFrame);
	}

	CurrentFrame = MoveTemp(Frame);
	bCurrentFrameReplayed = false;
#if !UE_BUILD_SHIPPING
	Perf.Submit.Add(ArcRmlUIPerf::SinceMs(SubmitStartCycles));
	ReportPerf_RenderThread();
#endif
}

void FArcRmlSlateElement::SetDestination_RenderThread(const FIntRect& InDestination)
{
	check(IsInRenderingThread());
	Destination = InDestination;
}

void FArcRmlSlateElement::ReleaseResources_RenderThread()
{
	check(IsInRenderingThread());
	CurrentFrame.Reset();
	Resources.Reset();
}

#if !UE_BUILD_SHIPPING
void FArcRmlSlateElement::SetPerfLabel_RenderThread(uint32 InViewId, const FString& InDocumentPath)
{
	check(IsInRenderingThread());
	PerfViewId = InViewId;
	PerfDocumentPath = InDocumentPath;
}

void FArcRmlSlateElement::ReportPerf_RenderThread()
{
	double ElapsedSeconds = 0.0;
	if (!PerfClock.ShouldReport(ElapsedSeconds))
	{
		return;
	}

	if ((Perf.Submitted > 0 || Perf.Replay.Count > 0) && ArcRmlUIPerf::IsEnabled())
	{
		const double Draws = static_cast<double>(FMath::Max(1, Perf.Replay.Count));
		const ArcRmlRdg::FReplayStats& Stats = Perf.ReplayStats;
		UE_LOGF(LogArcRmlUI, Verbose,
			"ArcRmlUI perf: render view %u '%ls' %.1f s: submitted %d (superseded %d, offscreen replays %d), submit avg %.2f max %.2f ms, draws %d, "
			"replay avg %.2f max %.2f ms, per draw: passes %.1f textures %.1f blurs %.1f blur passes %.1f backdrop seeds %.1f backdrop copies %.1f",
			PerfViewId, *PerfDocumentPath, ElapsedSeconds, Perf.Submitted, Perf.Superseded, Perf.OffscreenReplays, Perf.Submit.GetAverageMs(), Perf.Submit.MaxMs,
			Perf.Replay.Count, Perf.Replay.GetAverageMs(), Perf.Replay.MaxMs, static_cast<double>(Stats.Passes) / Draws,
			static_cast<double>(Stats.Textures) / Draws, static_cast<double>(Stats.Blurs) / Draws, static_cast<double>(Stats.BlurPasses) / Draws,
			static_cast<double>(Perf.BackdropSeeds) / Draws, static_cast<double>(Perf.BackdropCopies) / Draws);
	}

	Perf = FPerf();
}
#endif

void FArcRmlSlateElement::Draw_RenderThread(FRDGBuilder& GraphBuilder, const FDrawPassInputs& Inputs)
{
	if (!CurrentFrame.IsValid() || Inputs.OutputTexture == nullptr || Destination.Area() <= 0)
	{
		return;
	}

	const FIntPoint ViewSize = CurrentFrame->ViewSize;
	if (ViewSize.X <= 0 || ViewSize.Y <= 0)
	{
		return;
	}

	const FIntPoint Origin = Destination.Min + FIntPoint(FMath::RoundToInt(Inputs.ElementsOffset.X), FMath::RoundToInt(Inputs.ElementsOffset.Y));
	FIntRect Target(Origin, Origin + ViewSize.ComponentMin(Destination.Size()));
	Target.Clip(FIntRect(FIntPoint::ZeroValue, Inputs.OutputTexture->Desc.Extent));
	if (Target.Area() <= 0)
	{
		return;
	}

	const bool bSeedBackdrop = Inputs.SceneViewportTexture == nullptr || Inputs.SceneViewportTexture == Inputs.OutputTexture;
	if (!bSeedBackdrop && !bLoggedNoBackdrop)
	{
		bLoggedNoBackdrop = true;
		UE_LOGF(LogArcRmlUI, Log, "ArcRmlUI: HDR/composite-UI output is active; drawing with a transparent base layer and no backdrop sampling.");
	}

#if !UE_BUILD_SHIPPING
	const uint64 ReplayStartCycles = FPlatformTime::Cycles64();
	ArcRmlRdg::FReplayStats DrawStats;
	DrawStats.Textures += 1;
	DrawStats.Passes += 1;
#endif
	const FRDGTextureRef BaseLayer = ArcRmlRdg::CreateLayerTexture(GraphBuilder, ViewSize, TEXT("ArcRmlBaseLayer"));
	AddClearRenderTargetPass(GraphBuilder, BaseLayer, FLinearColor::Transparent);

	const FIntRect LocalTarget(Target.Min - Origin, Target.Max - Origin);
	if (bSeedBackdrop)
	{
		// OutputTexture is the elements texture holding the scene plus Slate drawn so far (SlateRHIRenderingPolicy.cpp:1815), so seeding the base layer from it lets backdrop-filter sample what is behind the view.
		ArcRmlRdg::FCopyPassDesc SeedDesc;
		SeedDesc.Source = Inputs.OutputTexture;
		SeedDesc.SourceRegion = Target;
		SeedDesc.Bias = FVector2f(static_cast<float>(Origin.X), static_cast<float>(Origin.Y));
		if (!EnumHasAnyFlags(Inputs.OutputTexture->Desc.Flags, ETextureCreateFlags::ShaderResource))
		{
			const FRDGTextureDesc CopyDesc = FRDGTextureDesc::Create2D(Target.Size(), Inputs.OutputTexture->Desc.Format, FClearValueBinding::Transparent,
				ETextureCreateFlags::ShaderResource);
			const FRDGTextureRef BackdropCopy = GraphBuilder.CreateTexture(CopyDesc, TEXT("ArcRmlBackdropCopy"));
			AddCopyTexturePass(GraphBuilder, Inputs.OutputTexture, BackdropCopy, Target.Min, FIntPoint::ZeroValue, Target.Size());
#if !UE_BUILD_SHIPPING
			++Perf.BackdropCopies;
			DrawStats.Textures += 1;
			DrawStats.Passes += 1;
#endif
			SeedDesc.Source = BackdropCopy;
			SeedDesc.SourceRegion = FIntRect(FIntPoint::ZeroValue, Target.Size());
			SeedDesc.Bias = FVector2f(static_cast<float>(Origin.X - Target.Min.X), static_cast<float>(Origin.Y - Target.Min.Y));
		}
		SeedDesc.Destination = BaseLayer;
		SeedDesc.Viewport = FIntRect(FIntPoint::ZeroValue, ViewSize);
		SeedDesc.Scissor = LocalTarget;
		[[maybe_unused]] const bool bSeeded = ArcRmlRdg::AddCopyPass(GraphBuilder, SeedDesc);
#if !UE_BUILD_SHIPPING
		Perf.BackdropSeeds += bSeeded ? 1 : 0;
		DrawStats.Passes += bSeeded ? 1 : 0;
#endif
	}

	FArcRmlRdgReplayer Replayer(GraphBuilder, Resources, *CurrentFrame);
	Replayer.Replay(BaseLayer);
	bCurrentFrameReplayed = true;

	ArcRmlRdg::FCopyPassDesc WriteBackDesc;
	WriteBackDesc.Source = BaseLayer;
	WriteBackDesc.Destination = Inputs.OutputTexture;
	WriteBackDesc.SourceRegion = LocalTarget;
	WriteBackDesc.Viewport = FIntRect(FIntPoint::ZeroValue, Inputs.OutputTexture->Desc.Extent);
	WriteBackDesc.Scissor = Target;
	WriteBackDesc.Bias = FVector2f(static_cast<float>(-Origin.X), static_cast<float>(-Origin.Y));
	WriteBackDesc.BlendMode = bSeedBackdrop ? EArcRmlBlendMode::Replace : EArcRmlBlendMode::Blend;
	[[maybe_unused]] const bool bWroteBack = ArcRmlRdg::AddCopyPass(GraphBuilder, WriteBackDesc);
#if !UE_BUILD_SHIPPING
	DrawStats.Passes += bWroteBack ? 1 : 0;
	DrawStats.Append(Replayer.GetStats());
	Perf.ReplayStats.Append(DrawStats);
	Perf.Replay.Add(ArcRmlUIPerf::SinceMs(ReplayStartCycles));
	ReportPerf_RenderThread();
#endif
}
