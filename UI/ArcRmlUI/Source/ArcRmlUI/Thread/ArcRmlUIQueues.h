// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Queue.h"
#include "HAL/CriticalSection.h"
#include "Interfaces/ArcRmlPaths.h"
#include "MVVM/ArcRmlModelTypes.h"

#include "RmlUi/Core/AssetTexture.h"

class FArcRmlSlateElement;
class FArcRmlViewChannel;
class FEvent;
struct FArcRmlCompileReport;

enum class EArcRmlCommandType : uint8
{
	AddView,
	RemoveView,
	LoadDocument,
	ClearDocumentCaches,
	Resize,
	SetViewPaused,
	SetRootClass,
	BindModel,
	UnbindModel,
	EmbedDocument,
	ClearEmbed,
	ApplyModelDelta,
	ApplyModelListOps,
	UpdateContentRoot,
	InstallAssetTexture,
#if WITH_EDITOR
	DumpExpandedDocument,
	CompileChanged,
	CompileAll,
#endif
#if !UE_BUILD_SHIPPING
	ClickElement,
	DumpLayout,
#endif
	Count
};

struct FArcRmlCommand
{
	EArcRmlCommandType Type = EArcRmlCommandType::AddView;
	uint32 ViewId = 0;
	FString Path;
	FIntPoint Size = FIntPoint::ZeroValue;
	float DpRatio = 1.0f;
	bool bPaused = false;
	FString RootClass;
	bool bRootClassSet = false;
	TSharedPtr<FArcRmlSlateElement, ESPMode::ThreadSafe> Element;
	TSharedPtr<FArcRmlViewChannel, ESPMode::ThreadSafe> Channel;
	FString ModelName;
	TArray<FArcRmlPathEntry> ModelPath;
	FArcRmlValue ModelValue;
	TArray<FArcRmlListOp> ModelListOps;
	FArcRmlContentRoot ContentRoot;
	bool bContentRootMounted = false;
	Rml::AssetTextureRequest AssetTexture;
	Rml::AssetTextureToken AssetTextureToken = 0;
#if WITH_EDITOR
	TArray<FString> Paths;
	FArcRmlCompileReport* CompileReport = nullptr;
	FEvent* CompileDone = nullptr;
#endif
#if !UE_BUILD_SHIPPING
	int32 ElementIndex = 0;
	double OpenSeconds = 0.0;
#endif
};

enum class EArcRmlInputType : uint8
{
	MouseMove,
	MouseDown,
	MouseUp,
	MouseWheel,
	MouseLeave,
	KeyDown,
	KeyUp,
	TextInput,
	ImeSetText,
	ImeSetSelection,
	ImeSetComposition
};

struct FArcRmlInputEvent
{
	EArcRmlInputType Type = EArcRmlInputType::MouseMove;
	uint32 ViewId = 0;
	FVector2f Position = FVector2f::ZeroVector;
	int32 Button = 0;
	float WheelDelta = 0.0f;
	int32 Key = 0;
	int32 Modifiers = 0;
	FString Text;
	int32 RangeStart = 0;
	int32 RangeEnd = 0;
	uint64 Generation = 0;
};

struct FArcRmlTextFieldShadow
{
	bool bActive = false;
	FString Text;
	int32 SelectionStart = 0;
	int32 SelectionEnd = 0;
	FIntRect Bounds;
	uint64 Generation = 0;
};

struct FArcRmlViewSnapshot
{
	TArray<FIntRect> InteractiveRects;
	FString Cursor;
	bool bHasFocusedElement = false;
	FArcRmlTextFieldShadow TextField;
};

struct FArcRmlViewCamera
{
	bool bValid = false;
	FVector ViewOrigin = FVector::ZeroVector;
	FMatrix TranslatedViewProjection = FMatrix::Identity;
	FIntRect ViewRect;
	FVector2D ContentOrigin = FVector2D::ZeroVector;

	bool operator==(const FArcRmlViewCamera& Other) const = default;
};

enum class EArcRmlOutboundType : uint8
{
	ClipboardCopy,
	ViewModelCall,
	RootTransitionEnd,
	AssetTextureRequest
};

struct FArcRmlOutboundEvent
{
	EArcRmlOutboundType Type = EArcRmlOutboundType::ClipboardCopy;
	FString Text;
	FArcRmlViewModelCall Call;
	uint32 RootClassSerial = 0;
	Rml::AssetTextureRequest AssetTexture;
};

class FArcRmlViewChannel
{
public:
	void PublishSnapshot(FArcRmlViewSnapshot&& InSnapshot);
	bool ReadSnapshot(FArcRmlViewSnapshot& OutSnapshot, uint64& InOutSerial) const;

	void PublishCamera(const FArcRmlViewCamera& InCamera);
	bool ReadCamera(FArcRmlViewCamera& OutCamera, uint64& InOutSerial) const;

	void PushOutbound(FArcRmlOutboundEvent&& Event);
	bool PopOutbound(FArcRmlOutboundEvent& OutEvent);

private:
	mutable FCriticalSection SnapshotLock;
	FArcRmlViewSnapshot Snapshot;
	uint64 SnapshotSerial = 0;
	mutable FCriticalSection CameraLock;
	FArcRmlViewCamera Camera;
	uint64 CameraSerial = 0;
	TQueue<FArcRmlOutboundEvent, EQueueMode::Spsc> Outbound;
};
