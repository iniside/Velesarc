// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Queue.h"
#include "Containers/Ticker.h"
#include "ArcRmlUIPerf.h"
#include "HAL/Runnable.h"
#include "Interfaces/ArcRmlPaths.h"
#include "Thread/ArcRmlUIQueues.h"

#include <atomic>

class FArcRmlAssetTextureRegistry;
class FArcRmlAsyncJobs;
class FArcRmlDocumentCompiler;
class FArcRmlFileInterface;
class FArcRmlSlateElement;
class FArcRmlSystemInterface;
class FArcRmlTextInputHandler;
class FEvent;
class FRunnableThread;
class IImageWrapperModule;
class IPlatformFile;
enum class EArcRmlCompiledKind : uint8;
struct FArcRmlCompiledFileData;
struct FArcRmlCompileReport;

struct FArcRmlUIThreadDependencies
{
	IImageWrapperModule* ImageWrapperModule = nullptr;
	IPlatformFile* PlatformFile = nullptr;
	FArcRmlContentRoots ContentRoots;
	TMap<FString, FString> GenericFontFamilies;
	FString FallbackFontFamily;
};

class FArcRmlUIThread final : public FRunnable
{
public:
	explicit FArcRmlUIThread(FArcRmlUIThreadDependencies&& InDependencies);
	virtual ~FArcRmlUIThread() override;

	void Start();
	void Shutdown();

	uint32 AddView(const TSharedRef<FArcRmlSlateElement, ESPMode::ThreadSafe>& Element, const TSharedRef<FArcRmlViewChannel, ESPMode::ThreadSafe>& Channel);
	void RemoveView(uint32 ViewId);
	void LoadDocument(uint32 ViewId, const FString& Path, FIntPoint Size, float DpRatio, double OpenSeconds);
	void ResizeView(uint32 ViewId, FIntPoint Size, float DpRatio);
	void SetViewPaused(uint32 ViewId, bool bPaused);
	void SetRootClass(uint32 ViewId, const FString& Class, bool bSet);
	void ClearDocumentCaches();
	void UpdateContentRoot(const FArcRmlContentRoot& Root, bool bMounted);
#if WITH_EDITOR
	void DumpExpandedDocument(const FString& Path);
	void CompileChanged(TArray<FString>&& ChangedPackagePaths);
	bool CompileAllAndWait(TConstArrayView<FString> ExcludedPrefixes, FArcRmlCompileReport& OutReport);
#endif
#if !UE_BUILD_SHIPPING
	void ClickElement(uint32 ViewId, const FString& Selector, int32 Index);
	void DumpLayout(uint32 ViewId, const FString& OutDir);
#endif
	void BindModel(uint32 ViewId, const FString& ModelName, FArcRmlValue&& Snapshot);
	void UnbindModel(uint32 ViewId, const FString& ModelName);
	void EmbedDocument(uint32 ViewId, const FString& Path);
	void ClearEmbed(uint32 ViewId);
	void ApplyModelDelta(uint32 ViewId, const FString& ModelName, TArray<FArcRmlPathEntry>&& Path, FArcRmlValue&& Value);
	void ApplyModelListOps(uint32 ViewId, const FString& ModelName, TArray<FArcRmlPathEntry>&& Path, TArray<FArcRmlListOp>&& Ops);
	void RequestAssetTexture(uint32 ViewId, const FString& AssetPath, const Rml::AssetTextureRequest& TextureRequest);
	void InstallAssetTexture(uint32 ViewId, const Rml::AssetTextureRequest& TextureRequest, FIntPoint Size, Rml::AssetTextureToken Token);
	void PostInput(FArcRmlInputEvent&& Event);
	void Wake();

	virtual uint32 Run() override;
	virtual void Stop() override;

private:
	struct FViewState;

	void EnqueueCommand(FArcRmlCommand&& Command);
	bool InitializeRml();
	void ShutdownRml();
	int32 LoadFonts(const FArcRmlContentRoot& Root);
	void Pump();
	void ProcessCommands();
	void ProcessInput();
	void InstallAsyncResults();
	void UpdateViews();
	void RenderView(FViewState& View, double Now);
	void PublishSnapshot(FViewState& View);
	void DestroyView(FViewState& View);
	void ClearCaches();
	void LoadDocumentForView(FViewState& View, const FString& Path);
	bool LoadCompiledFile(const FString& SourcePath, EArcRmlCompiledKind Kind, FArcRmlCompiledFileData& OutData, double& OutReadMs);
	void EmbedDocumentInView(FViewState& View, const FString& Path);
	void ClearEmbedInView(FViewState& View);
#if !UE_BUILD_SHIPPING
	void ClickElementInView(FViewState& View, const FString& Selector, int32 Index);
	void WriteLayoutDump(FViewState& View);
#endif
	FViewState* FindView(uint32 ViewId);
	void ActivateView(FViewState* View);
	double ComputeWaitSeconds() const;
	bool TickInline(float DeltaTime);
#if !UE_BUILD_SHIPPING
	void ReportPerf();
	void DumpPerf();
	void ReportLayoutVerify();
#endif

	FArcRmlUIThreadDependencies Dependencies;
	TUniquePtr<FArcRmlSystemInterface> SystemInterface;
	TUniquePtr<FArcRmlFileInterface> FileInterface;
#if WITH_EDITOR
	TUniquePtr<FArcRmlDocumentCompiler> Compiler;
#endif
	TUniquePtr<FArcRmlTextInputHandler> TextInputHandler;
	TUniquePtr<FArcRmlAsyncJobs> AsyncJobs;
	TSharedPtr<FArcRmlAssetTextureRegistry, ESPMode::ThreadSafe> AssetTextures;
	TMap<uint32, TUniquePtr<FViewState>> Views;
	FViewState* ActiveView = nullptr;
	TArray<TSharedPtr<FArcRmlSlateElement, ESPMode::ThreadSafe>> ElementsAtShutdown;

	TQueue<FArcRmlCommand, EQueueMode::Spsc> Commands;
	TQueue<FArcRmlInputEvent, EQueueMode::Spsc> Inputs;
	FEvent* WakeEvent = nullptr;
	FEvent* StoppedEvent = nullptr;
	FRunnableThread* Thread = nullptr;
	FTSTicker::FDelegateHandle InlineTickerHandle;
	std::atomic<bool> bStopRequested{false};
	bool bInline = false;
	bool bStarted = false;
	bool bRmlInitialized = false;
	uint32 NextViewId = 1;

#if !UE_BUILD_SHIPPING
	struct FPumpPerf
	{
		int32 Pumps = 0;
		double PumpMs = 0.0;
		double CommandsMs = 0.0;
		double InputMs = 0.0;
		double AsyncMs = 0.0;
		double UpdateMs = 0.0;
		double WaitMs = 0.0;
		int32 Inputs = 0;
		int32 AsyncInstalls = 0;
		bool bActivity = false;
		int32 CommandCounts[static_cast<int32>(EArcRmlCommandType::Count)] = {};
	};

	struct FAsyncInstallPerf;

	FPumpPerf PumpPerf;
	TUniquePtr<FAsyncInstallPerf> AsyncInstallPerf;
	ArcRmlUIPerf::FReportClock PerfClock;
	FViewState* LayoutVerifyView = nullptr;
#endif
};
