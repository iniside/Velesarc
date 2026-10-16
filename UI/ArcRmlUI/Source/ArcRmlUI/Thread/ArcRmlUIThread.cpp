// Copyright Lukasz Baran. All Rights Reserved.

#include "Thread/ArcRmlUIThread.h"

#include "ArcRmlUILog.h"
#include "Async/Async.h"
#include "Compile/ArcRmlCompiledFile.h"
#include "Compile/ArcRmlDocumentCompiler.h"
#include "Components/ArcRmlComponentExpander.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "HAL/Event.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "HAL/RunnableThread.h"
#include "Host/ArcRmlAssetTextureRegistry.h"
#include "Host/ArcRmlTextInput.h"
#include "Interfaces/ArcRmlFileInterface.h"
#include "Interfaces/ArcRmlStrings.h"
#include "Interfaces/ArcRmlSystemInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Thread/ArcRmlAsyncJobs.h"
#include "Thread/ArcRmlLayoutDump.h"
#include "MVVM/ArcRmlActionRouter.h"
#include "MVVM/ArcRmlModelStore.h"
#include "Render/ArcRmlRecordingRenderInterface.h"
#include "Render/ArcRmlSlateElement.h"
#include "RenderingThread.h"
#include "SceneView.h"

#include "RmlUi/Core/ArcPerf.h"
#include "RmlUi/Core/AsyncJobInterface.h"
#include "RmlUi/Core/CompiledDocument.h"
#include "RmlUi/Core/ComputedValues.h"
#include "RmlUi/Core/Context.h"
#include "RmlUi/Core/Core.h"
#include "RmlUi/Core/Element.h"
#include "RmlUi/Core/ElementDocument.h"
#include "RmlUi/Core/Elements/ElementFormControl.h"
#include "RmlUi/Core/Event.h"
#include "RmlUi/Core/EventListener.h"
#include "RmlUi/Core/Factory.h"
#include "RmlUi/Core/Input.h"
#include "RmlUi/Core/StringUtilities.h"
#include "RmlUi/Core/StyleSheet.h"
#include "RmlUi/Core/StyleSheetBinary.h"
#include "RmlUi/Core/StyleSheetContainer.h"
#include "RmlUi/Core/TextInputContext.h"
#include "RmlUi/Core/TextureFileDecoder.h"
#include "RmlUi/Core/WorldAnchors.h"
#include "RmlUi/SVG/SVGFileReader.h"

#include <limits>

namespace ArcRmlUIThreadPrivate
{
	TAutoConsoleVariable<float> CVarMaxFrameRate(
		TEXT("ArcRml.MaxFrameRate"),
		120.0f,
		TEXT("Upper bound on how often the ArcRmlUI thread re-renders a view that is animating."),
		ECVF_Default);

	double GetMinFrameInterval()
	{
		const float MaxFrameRate = CVarMaxFrameRate.GetValueOnAnyThread();
		return MaxFrameRate > 0.0f ? 1.0 / static_cast<double>(MaxFrameRate) : 0.0;
	}

	bool ProjectWorldPoint(const FArcRmlViewCamera& Camera, const Rml::WorldPoint& WorldPoint, Rml::Vector2f& OutContextPoint)
	{
		if (!Camera.bValid)
		{
			return false;
		}

		const FVector TranslatedPoint = FVector(WorldPoint.x, WorldPoint.y, WorldPoint.z) - Camera.ViewOrigin;
		FVector2D ScreenPoint;
		if (!FSceneView::ProjectWorldToScreen(TranslatedPoint, Camera.ViewRect, Camera.TranslatedViewProjection, ScreenPoint))
		{
			return false;
		}

		const FVector2D ContextPoint = ScreenPoint - Camera.ContentOrigin;
		OutContextPoint = Rml::Vector2f(static_cast<float>(ContextPoint.X), static_cast<float>(ContextPoint.Y));
		return true;
	}

	bool HasEventAttribute(const Rml::Element& Element)
	{
		for (const Rml::Dictionary::value_type& Attribute : Element.GetAttributes())
		{
			const Rml::String& Name = Attribute.first;
			if (Name.rfind("on", 0) == 0 || Name.rfind("data-event-", 0) == 0)
			{
				return true;
			}
			if (Name == "data-action" && !Attribute.second.Get<Rml::String>().empty())
			{
				return true;
			}
		}
		return false;
	}

	bool IsInteractive(const Rml::Element& Element)
	{
		const Rml::String& Tag = Element.GetTagName();
		if (Tag == "input" || Tag == "textarea" || Tag == "select" || Tag == "button")
		{
			return true;
		}
		if (Element.GetComputedValues().tab_index() == Rml::Style::TabIndex::Auto || Element.GetComputedValues().drag() != Rml::Style::Drag::None)
		{
			return true;
		}
		return HasEventAttribute(Element);
	}

	void CollectInteractiveRects(Rml::Element* Root, TArray<FIntRect>& OutRects)
	{
		TArray<Rml::Element*> Stack;
		Stack.Add(Root);
		while (Stack.Num() > 0)
		{
			Rml::Element* Element = Stack.Pop(EAllowShrinking::No);
			if (Element == nullptr || !Element->IsVisible())
			{
				continue;
			}

			if (Element->GetComputedValues().pointer_events() != Rml::Style::PointerEvents::None && IsInteractive(*Element))
			{
				const Rml::Vector2f Offset = Element->GetAbsoluteOffset(Rml::BoxArea::Border);
				const Rml::Vector2f Size = Element->GetBox().GetSize(Rml::BoxArea::Border);
				const FIntRect Rect(
					FMath::FloorToInt(Offset.x), FMath::FloorToInt(Offset.y),
					FMath::CeilToInt(Offset.x + Size.x), FMath::CeilToInt(Offset.y + Size.y));
				if (Rect.Area() > 0)
				{
					OutRects.Add(Rect);
				}
			}

			const int32 NumChildren = Element->GetNumChildren();
			for (int32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex)
			{
				Stack.Add(Element->GetChild(ChildIndex));
			}
		}
	}

	int32 FindEmbedContainer(Rml::Element* Root, Rml::Element*& OutContainer)
	{
		OutContainer = nullptr;
		int32 Count = 0;
		TArray<Rml::Element*> Stack;
		Stack.Add(Root);
		while (Stack.Num() > 0)
		{
			Rml::Element* Element = Stack.Pop(EAllowShrinking::No);
			if (Element == nullptr)
			{
				continue;
			}
			if (Element->HasAttribute(ArcRmlEmbedAttributes::Container))
			{
				OutContainer = Element;
				++Count;
			}
			const int32 NumChildren = Element->GetNumChildren();
			for (int32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex)
			{
				Stack.Add(Element->GetChild(ChildIndex));
			}
		}
		return Count;
	}

#if !UE_BUILD_SHIPPING
	int32 CountElements(Rml::Element* Root)
	{
		int32 Count = 0;
		TArray<Rml::Element*> Stack;
		Stack.Add(Root);
		while (Stack.Num() > 0)
		{
			Rml::Element* Element = Stack.Pop(EAllowShrinking::No);
			if (Element == nullptr)
			{
				continue;
			}
			++Count;
			const int32 NumChildren = Element->GetNumChildren();
			for (int32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex)
			{
				Stack.Add(Element->GetChild(ChildIndex));
			}
		}
		return Count;
	}

	const TCHAR* GetCommandTypeName(EArcRmlCommandType Type)
	{
		switch (Type)
		{
		case EArcRmlCommandType::AddView:
			return TEXT("AddView");
		case EArcRmlCommandType::RemoveView:
			return TEXT("RemoveView");
		case EArcRmlCommandType::LoadDocument:
			return TEXT("LoadDocument");
		case EArcRmlCommandType::ClearDocumentCaches:
			return TEXT("ClearDocumentCaches");
		case EArcRmlCommandType::Resize:
			return TEXT("Resize");
		case EArcRmlCommandType::SetViewPaused:
			return TEXT("SetViewPaused");
		case EArcRmlCommandType::SetRootClass:
			return TEXT("SetRootClass");
		case EArcRmlCommandType::BindModel:
			return TEXT("BindModel");
		case EArcRmlCommandType::UnbindModel:
			return TEXT("UnbindModel");
		case EArcRmlCommandType::EmbedDocument:
			return TEXT("EmbedDocument");
		case EArcRmlCommandType::ClearEmbed:
			return TEXT("ClearEmbed");
		case EArcRmlCommandType::ApplyModelDelta:
			return TEXT("ApplyModelDelta");
		case EArcRmlCommandType::ApplyModelListOps:
			return TEXT("ApplyModelListOps");
		case EArcRmlCommandType::UpdateContentRoot:
			return TEXT("UpdateContentRoot");
		case EArcRmlCommandType::InstallAssetTexture:
			return TEXT("InstallAssetTexture");
#if WITH_EDITOR
		case EArcRmlCommandType::DumpExpandedDocument:
			return TEXT("DumpExpandedDocument");
		case EArcRmlCommandType::CompileChanged:
			return TEXT("CompileChanged");
		case EArcRmlCommandType::CompileAll:
			return TEXT("CompileAll");
#endif
		case EArcRmlCommandType::ClickElement:
			return TEXT("ClickElement");
		case EArcRmlCommandType::DumpLayout:
			return TEXT("DumpLayout");
		case EArcRmlCommandType::Count:
			break;
		}
		return TEXT("Unknown");
	}

	FString FormatDelay(double DelaySeconds)
	{
		return DelaySeconds < std::numeric_limits<double>::max() ? FString::Printf(TEXT("%.3f s"), DelaySeconds) : FString(TEXT("none"));
	}

	struct FViewPerf
	{
		int32 Frames = 0;
		int32 DirtyFrames = 0;
		int32 TimedFrames = 0;
		ArcRmlUIPerf::FTiming Update;
		ArcRmlUIPerf::FTiming Render;
		ArcRmlUIPerf::FTiming Publish;
		int32 Commands = 0;
		int32 Draws = 0;
		int32 LayerPushes = 0;
		int32 Composites = 0;
		int32 FilterCreates = 0;
		int32 ShaderCreates = 0;
		int32 GeometryCreates = 0;
		int32 GeometryReleases = 0;
		int32 GeometryVertices = 0;
		int32 TextureCreates = 0;
		int32 TextureReleases = 0;
		int32 ModelDeltas = 0;
		int32 ModelListOps = 0;
		int32 Inputs = 0;
		double LastNextUpdateDelay = 0.0;
		bool bPausedChanged = false;
	};

	struct FLoadPerf
	{
		bool bPending = false;
		double OpenSeconds = 0.0;
		double ReadMs = 0.0;
		int32 ReadBytes = 0;
		double LoadMs = 0.0;
		int32 LoadedElements = 0;
	};

	void AccumulateFrame(FViewPerf& Perf, const FArcRmlRenderFrame& Frame)
	{
		Perf.Commands += Frame.Commands.Num();
		for (const FArcRmlDrawCommand& Command : Frame.Commands)
		{
			switch (Command.Op)
			{
			case EArcRmlDrawOp::RenderGeometry:
			case EArcRmlDrawOp::RenderShader:
				++Perf.Draws;
				break;
			case EArcRmlDrawOp::PushLayer:
				++Perf.LayerPushes;
				break;
			case EArcRmlDrawOp::CompositeLayers:
				++Perf.Composites;
				break;
			default:
				break;
			}
		}

		for (const FArcRmlResourceOp& Op : Frame.ResourceOps)
		{
			switch (Op.Type)
			{
			case EArcRmlResourceOpType::CreateGeometry:
				++Perf.GeometryCreates;
				break;
			case EArcRmlResourceOpType::ReleaseGeometry:
				++Perf.GeometryReleases;
				break;
			case EArcRmlResourceOpType::CreateTexture:
			case EArcRmlResourceOpType::CreateAssetTexture:
				++Perf.TextureCreates;
				break;
			case EArcRmlResourceOpType::ReleaseTexture:
				++Perf.TextureReleases;
				break;
			case EArcRmlResourceOpType::CreateFilter:
				++Perf.FilterCreates;
				break;
			case EArcRmlResourceOpType::CreateShader:
				++Perf.ShaderCreates;
				break;
			default:
				break;
			}
		}

		for (const FArcRmlGeometryPayload& Geometry : Frame.Geometries)
		{
			Perf.GeometryVertices += Geometry.Vertices.Num();
		}
	}

	constexpr int32 NumRmlBuckets = static_cast<int32>(Rml::ArcPerf::Bucket::Count);
	constexpr int32 NumRmlCounters = static_cast<int32>(Rml::ArcPerf::Counter::Count);
	constexpr int32 NumRmlElementLists = static_cast<int32>(Rml::ArcPerf::ElementList::Count);
	constexpr int32 NumRmlNamedLists = static_cast<int32>(Rml::ArcPerf::NamedList::Count);
	constexpr int32 MaxDetailsPerEntry = 6;
	constexpr int32 MaxAnimationsPerLine = 100;

	using FRmlElementMap = Rml::UnorderedMap<const Rml::Element*, Rml::ArcPerf::ElementStat>;
	using FRmlNamedMap = Rml::UnorderedMap<Rml::String, Rml::ArcPerf::NamedStat>;
	using FRmlDetailMap = Rml::UnorderedMap<Rml::String, int>;

	struct FRmlBreakdown
	{
		int32 Frames = 0;
		double TotalMs = 0.0;
		double MaxFrameMs = 0.0;
		double BucketTotalMs[NumRmlBuckets] = {};
		double BucketMaxMs[NumRmlBuckets] = {};
		int64 BucketCalls[NumRmlBuckets] = {};
		int64 CounterTotal[NumRmlCounters] = {};
		int64 CounterMax[NumRmlCounters] = {};

		void Add(const Rml::ArcPerf::FrameStats& Frame)
		{
			++Frames;
			double FrameMs = 0.0;
			for (int32 Index = 0; Index < NumRmlBuckets; ++Index)
			{
				const double Ms = Frame.bucket_ms[Index];
				FrameMs += Ms;
				BucketTotalMs[Index] += Ms;
				BucketMaxMs[Index] = FMath::Max(BucketMaxMs[Index], Ms);
				BucketCalls[Index] += Frame.bucket_calls[Index];
			}
			TotalMs += FrameMs;
			MaxFrameMs = FMath::Max(MaxFrameMs, FrameMs);
			for (int32 Index = 0; Index < NumRmlCounters; ++Index)
			{
				CounterTotal[Index] += Frame.counters[Index];
				CounterMax[Index] = FMath::Max(CounterMax[Index], static_cast<int64>(Frame.counters[Index]));
			}
		}
	};

	FString FormatBreakdown(const FRmlBreakdown& Breakdown)
	{
		const double Frames = static_cast<double>(FMath::Max(1, Breakdown.Frames));
		TArray<int32> Order;
		int64 Scopes = 0;
		for (int32 Index = 0; Index < NumRmlBuckets; ++Index)
		{
			Scopes += Breakdown.BucketCalls[Index];
			if (Breakdown.BucketTotalMs[Index] > 0.0 || Breakdown.BucketCalls[Index] > 0)
			{
				Order.Add(Index);
			}
		}
		Order.Sort([&Breakdown](const int32 A, const int32 B)
		{
			return Breakdown.BucketTotalMs[A] > Breakdown.BucketTotalMs[B];
		});

		FString Result = FString::Printf(TEXT("frames %d, total %.3f ms (avg %.3f, max %.3f), instrumented scopes %lld (%.1f per frame)"),
			Breakdown.Frames, Breakdown.TotalMs, Breakdown.TotalMs / Frames, Breakdown.MaxFrameMs, Scopes, static_cast<double>(Scopes) / Frames);
		for (const int32 Index : Order)
		{
			Result += FString::Printf(TEXT(" | %s %.3f ms (avg %.3f, max %.3f, calls %lld)"),
				UTF8_TO_TCHAR(Rml::ArcPerf::GetBucketName(static_cast<Rml::ArcPerf::Bucket>(Index))),
				Breakdown.BucketTotalMs[Index], Breakdown.BucketTotalMs[Index] / Frames, Breakdown.BucketMaxMs[Index], Breakdown.BucketCalls[Index]);
		}
		return Result;
	}

	FString FormatCounts(const FRmlBreakdown& Breakdown)
	{
		const double Frames = static_cast<double>(FMath::Max(1, Breakdown.Frames));
		TArray<FString> Parts;
		for (int32 Index = 0; Index < NumRmlCounters; ++Index)
		{
			if (Breakdown.CounterTotal[Index] == 0)
			{
				continue;
			}
			const FString Name = UTF8_TO_TCHAR(Rml::ArcPerf::GetCounterName(static_cast<Rml::ArcPerf::Counter>(Index)));
			if (Breakdown.Frames > 1)
			{
				Parts.Add(FString::Printf(TEXT("%s %.1f (max %lld)"), *Name, static_cast<double>(Breakdown.CounterTotal[Index]) / Frames, Breakdown.CounterMax[Index]));
			}
			else
			{
				Parts.Add(FString::Printf(TEXT("%s %lld"), *Name, Breakdown.CounterTotal[Index]));
			}
		}
		return Parts.Num() > 0 ? FString::Join(Parts, TEXT(", ")) : FString(TEXT("none"));
	}

	FString DescribeElement(const Rml::Element* Element)
	{
		return ArcRmlStrings::ToFString(Rml::ArcPerf::DescribeElement(Element, 2));
	}

	FString FormatDetails(const FRmlDetailMap& Details)
	{
		TArray<TPair<int32, FString>> Sorted;
		for (const FRmlDetailMap::value_type& Detail : Details)
		{
			Sorted.Emplace(Detail.second, ArcRmlStrings::ToFString(Detail.first));
		}
		Sorted.Sort([](const TPair<int32, FString>& A, const TPair<int32, FString>& B)
		{
			return A.Key > B.Key;
		});

		TArray<FString> Parts;
		for (int32 Index = 0; Index < Sorted.Num() && Index < MaxDetailsPerEntry; ++Index)
		{
			Parts.Add(FString::Printf(TEXT("%s x%d"), *Sorted[Index].Value, Sorted[Index].Key));
		}
		if (Sorted.Num() > MaxDetailsPerEntry)
		{
			Parts.Add(FString::Printf(TEXT("+%d more"), Sorted.Num() - MaxDetailsPerEntry));
		}
		return FString::Join(Parts, TEXT(", "));
	}

	FString FormatElementList(const FRmlElementMap& Map, int32 TopN)
	{
		TArray<const Rml::ArcPerf::ElementStat*> Stats;
		double TotalMs = 0.0;
		int64 TotalCount = 0;
		for (const FRmlElementMap::value_type& Pair : Map)
		{
			Stats.Add(&Pair.second);
			TotalMs += Pair.second.ms;
			TotalCount += Pair.second.count;
		}
		Stats.Sort([](const Rml::ArcPerf::ElementStat& A, const Rml::ArcPerf::ElementStat& B)
		{
			return A.ms != B.ms ? A.ms > B.ms : A.count > B.count;
		});

		FString Result = FString::Printf(TEXT("%d elements, %.3f ms, %lld hits; top %d:"), Stats.Num(), TotalMs, TotalCount, FMath::Min(TopN, Stats.Num()));
		for (int32 Index = 0; Index < Stats.Num() && Index < TopN; ++Index)
		{
			const Rml::ArcPerf::ElementStat& Stat = *Stats[Index];
			Result += FString::Printf(TEXT(" [%d] %.3f ms x%d '%s'"), Index + 1, Stat.ms, Stat.count, *DescribeElement(Stat.element.get()));
			if (!Stat.details.empty())
			{
				Result += FString::Printf(TEXT(" {%s}"), *FormatDetails(Stat.details));
			}
			Result += TEXT(";");
		}
		return Result;
	}

	FString FormatNamedList(const FRmlNamedMap& Map, int32 TopN)
	{
		TArray<TPair<FString, Rml::ArcPerf::NamedStat>> Stats;
		double TotalMs = 0.0;
		int64 TotalCount = 0;
		for (const FRmlNamedMap::value_type& Pair : Map)
		{
			Stats.Emplace(ArcRmlStrings::ToFString(Pair.first), Pair.second);
			TotalMs += Pair.second.ms;
			TotalCount += Pair.second.count;
		}
		Stats.Sort([](const TPair<FString, Rml::ArcPerf::NamedStat>& A, const TPair<FString, Rml::ArcPerf::NamedStat>& B)
		{
			return A.Value.ms != B.Value.ms ? A.Value.ms > B.Value.ms : A.Value.count > B.Value.count;
		});

		FString Result = FString::Printf(TEXT("%d names, %.3f ms, %lld hits; top %d:"), Stats.Num(), TotalMs, TotalCount, FMath::Min(TopN, Stats.Num()));
		for (int32 Index = 0; Index < Stats.Num() && Index < TopN; ++Index)
		{
			const TPair<FString, Rml::ArcPerf::NamedStat>& Stat = Stats[Index];
			if (Stat.Value.ms > 0.0)
			{
				Result += FString::Printf(TEXT(" '%s' %.3f ms x%d;"), *Stat.Key, Stat.Value.ms, Stat.Value.count);
			}
			else
			{
				Result += FString::Printf(TEXT(" '%s' x%d;"), *Stat.Key, Stat.Value.count);
			}
		}
		return Result;
	}

	FString FormatAnimations(Rml::Element* Root, int32 MaxEntries)
	{
		Rml::Vector<Rml::ArcPerf::AnimationInfo> Animations;
		Rml::ArcPerf::CollectAnimations(Root, Animations);

		TMap<FString, int32> States;
		for (const Rml::ArcPerf::AnimationInfo& Animation : Animations)
		{
			++States.FindOrAdd(ArcRmlStrings::ToFString(Animation.state));
		}
		TArray<FString> StateParts;
		for (const TPair<FString, int32>& State : States)
		{
			StateParts.Add(FString::Printf(TEXT("%s %d"), *State.Key, State.Value));
		}

		FString Result = FString::Printf(TEXT("%d (%s)"), static_cast<int32>(Animations.size()), *FString::Join(StateParts, TEXT(", ")));
		const int32 NumListed = FMath::Min(MaxEntries, static_cast<int32>(Animations.size()));
		for (int32 Index = 0; Index < NumListed; ++Index)
		{
			const Rml::ArcPerf::AnimationInfo& Animation = Animations[Index];
			const FString Iterations = Animation.num_iterations < 0 ? FString(TEXT("infinite")) : FString::FromInt(Animation.num_iterations);
			Result += FString::Printf(TEXT(" [%d] '%s' %s %s %s %.3f s iteration %d/%s%s;"), Index + 1, *DescribeElement(Animation.element.get()),
				*ArcRmlStrings::ToFString(Animation.property), *ArcRmlStrings::ToFString(Animation.origin), *ArcRmlStrings::ToFString(Animation.state),
				Animation.duration, Animation.current_iteration, *Iterations, Animation.alternate ? TEXT(" alternate") : TEXT(""));
		}
		if (static_cast<int32>(Animations.size()) > NumListed)
		{
			Result += FString::Printf(TEXT(" ... %d more"), static_cast<int32>(Animations.size()) - NumListed);
		}
		return Result;
	}

	FString FormatStyleSheetStats(const Rml::ElementDocument* Document)
	{
		Rml::ArcPerf::StyleSheetStats Stats;
		if (!Document || !Rml::ArcPerf::GetStyleSheetStats(Document->GetStyleSheet(), Stats))
		{
			return TEXT("no stylesheet");
		}
		return FString::Printf(
			TEXT("selector nodes %d (styled rules %d), indexed by id %d (%d keys), by class %d (%d keys), by tag %d (%d keys), unindexed %d (tested for every element), ")
			TEXT("cached definitions %d, keyframes %d, named decorators %d"),
			Stats.nodes, Stats.styled_nodes, Stats.indexed_by_id, Stats.id_keys, Stats.indexed_by_class, Stats.class_keys, Stats.indexed_by_tag, Stats.tag_keys,
			Stats.unindexed, Stats.cached_definitions, Stats.keyframes, Stats.named_decorators);
	}

	FString FormatElementsByTag(Rml::Element* Root)
	{
		TMap<FString, int32> Counts;
		int32 Total = 0;
		TArray<Rml::Element*> Stack;
		Stack.Add(Root);
		while (Stack.Num() > 0)
		{
			Rml::Element* Element = Stack.Pop(EAllowShrinking::No);
			if (Element == nullptr)
			{
				continue;
			}
			++Total;
			++Counts.FindOrAdd(ArcRmlStrings::ToFString(Element->GetTagName()));
			const int32 NumChildren = Element->GetNumChildren(true);
			for (int32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex)
			{
				Stack.Add(Element->GetChild(ChildIndex));
			}
		}
		Counts.ValueSort([](const int32 A, const int32 B)
		{
			return A > B;
		});

		TArray<FString> Parts;
		for (const TPair<FString, int32>& Count : Counts)
		{
			Parts.Add(FString::Printf(TEXT("%s %d"), *Count.Key, Count.Value));
		}
		return FString::Printf(TEXT("%d (incl. non-DOM): %s"), Total, *FString::Join(Parts, TEXT(", ")));
	}

	class FScopedRmlPerfSink
	{
	public:
		explicit FScopedRmlPerfSink(Rml::ArcPerf::Sink& Sink)
		{
			Rml::ArcPerf::SetSink(&Sink, ArcRmlUIPerf::GetLevel());
		}
		~FScopedRmlPerfSink()
		{
			Rml::ArcPerf::SetSink(nullptr, 0);
		}
	};
#endif

	class FRootTransitionListener final : public Rml::EventListener
	{
	public:
		FRootTransitionListener(uint32 InViewId, const TSharedRef<FArcRmlViewChannel, ESPMode::ThreadSafe>& InChannel)
			: ViewId(InViewId)
			, Channel(InChannel)
		{
		}

		virtual void ProcessEvent(Rml::Event& Event) override
		{
			if (Event.GetTargetElement() == Event.GetCurrentElement())
			{
				SendIfSettled(Event.GetTargetElement());
			}
		}

		void SendIfSettled(const Rml::Element* Root)
		{
			if (Root && Root->HasRunningAnimations())
			{
				return;
			}

			UE_LOGF(LogArcRmlUI, Verbose, "ArcRmlUI: view %u root transitions settled (root class serial %u)", ViewId, RootClassSerial);
			FArcRmlOutboundEvent Outbound;
			Outbound.Type = EArcRmlOutboundType::RootTransitionEnd;
			Outbound.RootClassSerial = RootClassSerial;
			Channel->PushOutbound(MoveTemp(Outbound));
		}

		uint32 RootClassSerial = 0;

	private:
		uint32 ViewId = 0;
		TSharedRef<FArcRmlViewChannel, ESPMode::ThreadSafe> Channel;
	};
}

struct FArcRmlUIThread::FViewState
{
	uint32 Id = 0;
	Rml::String ContextName;
	Rml::Context* Context = nullptr;
	TUniquePtr<FArcRmlRecordingRenderInterface> Recorder;
	TSharedPtr<FArcRmlSlateElement, ESPMode::ThreadSafe> Element;
	TSharedPtr<FArcRmlViewChannel, ESPMode::ThreadSafe> Channel;
	TUniquePtr<FArcRmlModelStore> ModelStore;
	TUniquePtr<FArcRmlActionRouter> ActionRouter;
	TUniquePtr<ArcRmlUIThreadPrivate::FRootTransitionListener> RootTransitionListener;
	Rml::ElementDocument* Document = nullptr;
	TArray<TPair<FString, bool>> PendingRootClasses;
	Rml::ObserverPtr<Rml::Element> EmbedWrapper;
	Rml::SharedPtr<Rml::StyleSheetContainer> FrameStyleSheet;
	FString EmbeddedPath;
	bool bEmbedPending = false;
	FIntPoint Size = FIntPoint::ZeroValue;
	bool bDirty = true;
	bool bPaused = false;
	bool bRootCheckPending = false;
	double NextUpdateTime = 0.0;
	FArcRmlViewCamera Camera;
	uint64 CameraSerial = 0;
	FString Cursor;
	uint64 AppliedImeGeneration = 0;
	FString DocumentPath;
#if !UE_BUILD_SHIPPING
	FString PendingLayoutDumpDir;
	ArcRmlUIThreadPrivate::FViewPerf Perf;
	ArcRmlUIThreadPrivate::FLoadPerf LoadPerf;
	Rml::ArcPerf::Sink RmlPerf;
	Rml::ArcPerf::LayoutVerifyLog LayoutVerify;
	ArcRmlUIThreadPrivate::FRmlBreakdown UpdateBreakdown;
	ArcRmlUIThreadPrivate::FRmlBreakdown RenderBreakdown;
	TArray<FString> LastWhoLines;
	double LastWhoSeconds = 0.0;
#endif
};

#if !UE_BUILD_SHIPPING
struct FArcRmlUIThread::FAsyncInstallPerf
{
	Rml::ArcPerf::Sink Sink;
	ArcRmlUIThreadPrivate::FRmlBreakdown Breakdown;
};
#endif

FArcRmlUIThread::FArcRmlUIThread(FArcRmlUIThreadDependencies&& InDependencies)
	: Dependencies(MoveTemp(InDependencies))
{
	AssetTextures = MakeShared<FArcRmlAssetTextureRegistry, ESPMode::ThreadSafe>(*this);
	ARCRMLUI_PERF(AsyncInstallPerf = MakeUnique<FAsyncInstallPerf>());
	WakeEvent = FPlatformProcess::GetSynchEventFromPool(false);
	StoppedEvent = FPlatformProcess::GetSynchEventFromPool(true);
}

FArcRmlUIThread::~FArcRmlUIThread()
{
	Shutdown();
	FPlatformProcess::ReturnSynchEventToPool(WakeEvent);
	FPlatformProcess::ReturnSynchEventToPool(StoppedEvent);
	WakeEvent = nullptr;
	StoppedEvent = nullptr;
}

void FArcRmlUIThread::Start()
{
	check(IsInGameThread());
	if (bStarted)
	{
		return;
	}
	bStarted = true;
	ARCRMLUI_PERF(ArcRmlUIPerf::SetDumpWaker([this]() { Wake(); }));

	bInline = !FPlatformProcess::SupportsMultithreading();
	if (bInline)
	{
		InitializeRml();
		InlineTickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FArcRmlUIThread::TickInline));
		return;
	}

	Thread = FRunnableThread::Create(this, TEXT("ArcRmlUIThread"), 0, TPri_Normal);
}

void FArcRmlUIThread::Shutdown()
{
	check(IsInGameThread());
	if (!bStarted)
	{
		return;
	}
	bStarted = false;
	ARCRMLUI_PERF(ArcRmlUIPerf::SetDumpWaker(nullptr));

	if (bInline)
	{
		FTSTicker::RemoveTicker(InlineTickerHandle);
		ShutdownRml();
	}
	else if (Thread)
	{
		Stop();
		StoppedEvent->Wait();
	}

	for (TSharedPtr<FArcRmlSlateElement, ESPMode::ThreadSafe>& Element : ElementsAtShutdown)
	{
		ENQUEUE_RENDER_COMMAND(ArcRmlReleaseAtShutdown)(
			[ReleasedElement = MoveTemp(Element)](FRHICommandListImmediate&) mutable
			{
				ReleasedElement->ReleaseResources_RenderThread();
				ReleasedElement.Reset();
			});
	}
	ElementsAtShutdown.Reset();
	FlushRenderingCommands();
	AssetTextures->ReleaseAll();

	if (Thread)
	{
		Thread->WaitForCompletion();
		delete Thread;
		Thread = nullptr;
	}
}

uint32 FArcRmlUIThread::AddView(const TSharedRef<FArcRmlSlateElement, ESPMode::ThreadSafe>& Element,
	const TSharedRef<FArcRmlViewChannel, ESPMode::ThreadSafe>& Channel)
{
	check(IsInGameThread());
	const uint32 ViewId = NextViewId++;

	FArcRmlCommand Command;
	Command.Type = EArcRmlCommandType::AddView;
	Command.ViewId = ViewId;
	Command.Element = Element;
	Command.Channel = Channel;
	EnqueueCommand(MoveTemp(Command));
	return ViewId;
}

void FArcRmlUIThread::RemoveView(uint32 ViewId)
{
	FArcRmlCommand Command;
	Command.Type = EArcRmlCommandType::RemoveView;
	Command.ViewId = ViewId;
	EnqueueCommand(MoveTemp(Command));
}

void FArcRmlUIThread::LoadDocument(uint32 ViewId, const FString& Path, FIntPoint Size, float DpRatio, double OpenSeconds)
{
	FArcRmlCommand Command;
	Command.Type = EArcRmlCommandType::LoadDocument;
	Command.ViewId = ViewId;
	Command.Path = Path;
	Command.Size = Size;
	Command.DpRatio = DpRatio;
	ARCRMLUI_PERF(Command.OpenSeconds = OpenSeconds);
	EnqueueCommand(MoveTemp(Command));
}

void FArcRmlUIThread::ResizeView(uint32 ViewId, FIntPoint Size, float DpRatio)
{
	FArcRmlCommand Command;
	Command.Type = EArcRmlCommandType::Resize;
	Command.ViewId = ViewId;
	Command.Size = Size;
	Command.DpRatio = DpRatio;
	EnqueueCommand(MoveTemp(Command));
}

void FArcRmlUIThread::SetViewPaused(uint32 ViewId, bool bPaused)
{
	FArcRmlCommand Command;
	Command.Type = EArcRmlCommandType::SetViewPaused;
	Command.ViewId = ViewId;
	Command.bPaused = bPaused;
	EnqueueCommand(MoveTemp(Command));
}

void FArcRmlUIThread::SetRootClass(uint32 ViewId, const FString& Class, bool bSet)
{
	FArcRmlCommand Command;
	Command.Type = EArcRmlCommandType::SetRootClass;
	Command.ViewId = ViewId;
	Command.RootClass = Class;
	Command.bRootClassSet = bSet;
	EnqueueCommand(MoveTemp(Command));
}

void FArcRmlUIThread::ClearDocumentCaches()
{
	FArcRmlCommand Command;
	Command.Type = EArcRmlCommandType::ClearDocumentCaches;
	EnqueueCommand(MoveTemp(Command));
}

void FArcRmlUIThread::UpdateContentRoot(const FArcRmlContentRoot& Root, bool bMounted)
{
	FArcRmlCommand Command;
	Command.Type = EArcRmlCommandType::UpdateContentRoot;
	Command.ContentRoot = Root;
	Command.bContentRootMounted = bMounted;
	EnqueueCommand(MoveTemp(Command));
}

#if WITH_EDITOR
void FArcRmlUIThread::DumpExpandedDocument(const FString& Path)
{
	FArcRmlCommand Command;
	Command.Type = EArcRmlCommandType::DumpExpandedDocument;
	Command.Path = Path;
	EnqueueCommand(MoveTemp(Command));
}

void FArcRmlUIThread::CompileChanged(TArray<FString>&& ChangedPackagePaths)
{
	FArcRmlCommand Command;
	Command.Type = EArcRmlCommandType::CompileChanged;
	Command.Paths = MoveTemp(ChangedPackagePaths);
	EnqueueCommand(MoveTemp(Command));
}

bool FArcRmlUIThread::CompileAllAndWait(TConstArrayView<FString> ExcludedPrefixes, FArcRmlCompileReport& OutReport)
{
	if (bInline && !bRmlInitialized)
	{
		return false;
	}

	FEvent* Done = FPlatformProcess::GetSynchEventFromPool(true);
	FArcRmlCommand Command;
	Command.Type = EArcRmlCommandType::CompileAll;
	Command.Paths = TArray<FString>(ExcludedPrefixes);
	Command.CompileReport = &OutReport;
	Command.CompileDone = Done;
	EnqueueCommand(MoveTemp(Command));

	if (bInline)
	{
		ProcessCommands();
	}

	bool bCompleted = true;
	while (!Done->Wait(100))
	{
		if (StoppedEvent->Wait(0))
		{
			bCompleted = Done->Wait(0);
			break;
		}
	}
	FPlatformProcess::ReturnSynchEventToPool(Done);
	return bCompleted;
}
#endif

#if !UE_BUILD_SHIPPING
void FArcRmlUIThread::ClickElement(uint32 ViewId, const FString& Selector, int32 Index)
{
	FArcRmlCommand Command;
	Command.Type = EArcRmlCommandType::ClickElement;
	Command.ViewId = ViewId;
	Command.Path = Selector;
	Command.ElementIndex = Index;
	EnqueueCommand(MoveTemp(Command));
}

void FArcRmlUIThread::DumpLayout(uint32 ViewId, const FString& OutDir)
{
	FArcRmlCommand Command;
	Command.Type = EArcRmlCommandType::DumpLayout;
	Command.ViewId = ViewId;
	Command.Path = OutDir;
	EnqueueCommand(MoveTemp(Command));
}
#endif

void FArcRmlUIThread::BindModel(uint32 ViewId, const FString& ModelName, FArcRmlValue&& Snapshot)
{
	FArcRmlCommand Command;
	Command.Type = EArcRmlCommandType::BindModel;
	Command.ViewId = ViewId;
	Command.ModelName = ModelName;
	Command.ModelValue = MoveTemp(Snapshot);
	EnqueueCommand(MoveTemp(Command));
}

void FArcRmlUIThread::UnbindModel(uint32 ViewId, const FString& ModelName)
{
	FArcRmlCommand Command;
	Command.Type = EArcRmlCommandType::UnbindModel;
	Command.ViewId = ViewId;
	Command.ModelName = ModelName;
	EnqueueCommand(MoveTemp(Command));
}

void FArcRmlUIThread::EmbedDocument(uint32 ViewId, const FString& Path)
{
	FArcRmlCommand Command;
	Command.Type = EArcRmlCommandType::EmbedDocument;
	Command.ViewId = ViewId;
	Command.Path = Path;
	EnqueueCommand(MoveTemp(Command));
}

void FArcRmlUIThread::ClearEmbed(uint32 ViewId)
{
	FArcRmlCommand Command;
	Command.Type = EArcRmlCommandType::ClearEmbed;
	Command.ViewId = ViewId;
	EnqueueCommand(MoveTemp(Command));
}

void FArcRmlUIThread::ApplyModelDelta(uint32 ViewId, const FString& ModelName, TArray<FArcRmlPathEntry>&& Path, FArcRmlValue&& Value)
{
	FArcRmlCommand Command;
	Command.Type = EArcRmlCommandType::ApplyModelDelta;
	Command.ViewId = ViewId;
	Command.ModelName = ModelName;
	Command.ModelPath = MoveTemp(Path);
	Command.ModelValue = MoveTemp(Value);
	EnqueueCommand(MoveTemp(Command));
}

void FArcRmlUIThread::ApplyModelListOps(uint32 ViewId, const FString& ModelName, TArray<FArcRmlPathEntry>&& Path, TArray<FArcRmlListOp>&& Ops)
{
	FArcRmlCommand Command;
	Command.Type = EArcRmlCommandType::ApplyModelListOps;
	Command.ViewId = ViewId;
	Command.ModelName = ModelName;
	Command.ModelPath = MoveTemp(Path);
	Command.ModelListOps = MoveTemp(Ops);
	EnqueueCommand(MoveTemp(Command));
}

void FArcRmlUIThread::RequestAssetTexture(uint32 ViewId, const FString& AssetPath, const Rml::AssetTextureRequest& TextureRequest)
{
	AssetTextures->Request(ViewId, AssetPath, TextureRequest);
}

void FArcRmlUIThread::InstallAssetTexture(uint32 ViewId, const Rml::AssetTextureRequest& TextureRequest, FIntPoint Size, Rml::AssetTextureToken Token)
{
	FArcRmlCommand Command;
	Command.Type = EArcRmlCommandType::InstallAssetTexture;
	Command.ViewId = ViewId;
	Command.Size = Size;
	Command.AssetTexture = TextureRequest;
	Command.AssetTextureToken = Token;
	EnqueueCommand(MoveTemp(Command));
}

void FArcRmlUIThread::PostInput(FArcRmlInputEvent&& Event)
{
	check(IsInGameThread());
	Inputs.Enqueue(MoveTemp(Event));
	Wake();
}

void FArcRmlUIThread::EnqueueCommand(FArcRmlCommand&& Command)
{
	check(IsInGameThread());
	Commands.Enqueue(MoveTemp(Command));
	Wake();
}

void FArcRmlUIThread::Wake()
{
	if (WakeEvent)
	{
		WakeEvent->Trigger();
	}
}

uint32 FArcRmlUIThread::Run()
{
	if (InitializeRml())
	{
		while (!bStopRequested.load())
		{
			double WaitSeconds = ComputeWaitSeconds();
#if !UE_BUILD_SHIPPING
			if (PumpPerf.bActivity && ArcRmlUIPerf::IsEnabled())
			{
				const double SecondsUntilReport = PerfClock.GetSecondsUntilReport();
				WaitSeconds = WaitSeconds < 0.0 ? SecondsUntilReport : FMath::Min(WaitSeconds, SecondsUntilReport);
			}
			const uint64 WaitStartCycles = FPlatformTime::Cycles64();
#endif
			if (WaitSeconds < 0.0)
			{
				WakeEvent->Wait();
			}
			else if (WaitSeconds > 0.0)
			{
				WakeEvent->Wait(static_cast<uint32>(FMath::CeilToDouble(WaitSeconds * 1000.0)));
			}
			ARCRMLUI_PERF(PumpPerf.WaitMs += ArcRmlUIPerf::SinceMs(WaitStartCycles));

			if (bStopRequested.load())
			{
				break;
			}
			Pump();
		}
	}

	ShutdownRml();
	StoppedEvent->Trigger();
	return 0;
}

void FArcRmlUIThread::Stop()
{
	bStopRequested.store(true);
	Wake();
}

bool FArcRmlUIThread::TickInline(float DeltaTime)
{
	if (bRmlInitialized)
	{
		Pump();
	}
	return true;
}

bool FArcRmlUIThread::InitializeRml()
{
	SystemInterface = MakeUnique<FArcRmlSystemInterface>();
	FileInterface = MakeUnique<FArcRmlFileInterface>(*Dependencies.PlatformFile, Dependencies.ContentRoots);
	TextInputHandler = MakeUnique<FArcRmlTextInputHandler>();
	AsyncJobs = MakeUnique<FArcRmlAsyncJobs>(bInline, [this]() { Wake(); });

	Rml::SetSystemInterface(SystemInterface.Get());
	Rml::SetFileInterface(FileInterface.Get());
	Rml::SetTextInputHandler(TextInputHandler.Get());
	Rml::SetAsyncJobInterface(AsyncJobs.Get());
	Rml::SVG::SetFileReader([Interface = FileInterface.Get()](const Rml::String& Path, Rml::String& OutData) -> bool
		{
			return Interface->ReadFileBytes(Path, OutData);
		});
	Rml::SetTextureFileDecoder([ImageWrapperModule = Dependencies.ImageWrapperModule, Interface = FileInterface.Get()](const Rml::String& Source,
			Rml::Vector2i& OutDimensions, Rml::Vector<Rml::byte>& OutPixels) -> bool
		{
			return ArcRmlTextureDecode::DecodeFile(*ImageWrapperModule, *Interface, Source, OutDimensions, OutPixels);
		});
	Rml::SetAssetTextureRequester([this](const Rml::String& AssetPath, Rml::AssetTextureRequest TextureRequest)
		{
			if (!ActiveView)
			{
				UE_LOGF(LogArcRmlUI, Error, "ArcRmlUI: asset texture '%ls' requested outside a view", *ArcRmlStrings::ToFString(AssetPath));
				return;
			}
			FArcRmlOutboundEvent Event;
			Event.Type = EArcRmlOutboundType::AssetTextureRequest;
			Event.Text = ArcRmlStrings::ToFString(AssetPath);
			Event.AssetTexture = TextureRequest;
			ActiveView->Channel->PushOutbound(MoveTemp(Event));
		});

	if (!Rml::Initialise())
	{
		UE_LOGF(LogArcRmlUI, Error, "ArcRmlUI: Rml::Initialise failed");
		return false;
	}

	bRmlInitialized = true;
	Rml::SetCompiledStyleSheetReader([this](const Rml::String& CompiledPath, Rml::Vector<Rml::byte>& OutBody) -> bool
		{
			const FString Compiled = ArcRmlStrings::ToFString(CompiledPath);
			FArcRmlCompiledFileData Data;
			double ReadMs = 0.0;
			if (!LoadCompiledFile(Compiled.LeftChop(1), EArcRmlCompiledKind::StyleSheet, Data, ReadMs))
			{
				return false;
			}
			const TConstArrayView<uint8> Body = Data.GetBody();
			OutBody.assign(Body.GetData(), Body.GetData() + Body.Num());
			return true;
		});

	Rml::UnorderedMap<Rml::String, Rml::String> GenericFontFamilies;
	for (const TPair<FString, FString>& GenericFamily : Dependencies.GenericFontFamilies)
	{
		GenericFontFamilies[ArcRmlStrings::ToRmlString(GenericFamily.Key)] = ArcRmlStrings::ToRmlString(GenericFamily.Value);
	}
	Rml::SetGenericFontFamilies(GenericFontFamilies, ArcRmlStrings::ToRmlString(Dependencies.FallbackFontFamily));

	int32 FontFaces = 0;
	for (const FArcRmlContentRoot& Root : Dependencies.ContentRoots.Roots)
	{
		FontFaces += LoadFonts(Root);
	}
	if (FontFaces == 0)
	{
		UE_LOGF(LogArcRmlUI, Warning, "ArcRmlUI: no font faces found in any content root's '%ls'; RML text will not render", ArcRmlPaths::FontDirectory);
	}

#if WITH_EDITOR
	Compiler = MakeUnique<FArcRmlDocumentCompiler>(*FileInterface, Dependencies.ContentRoots);
#endif
	return true;
}

int32 FArcRmlUIThread::LoadFonts(const FArcRmlContentRoot& Root)
{
	TArray<FString> FontPaths;
	ArcRmlPaths::FindFontFiles(*Dependencies.PlatformFile, Root, FontPaths);
	for (const FString& FontPath : FontPaths)
	{
		if (!Rml::LoadFontFace(ArcRmlStrings::ToRmlString(FontPath)))
		{
			UE_LOGF(LogArcRmlUI, Warning, "ArcRmlUI: failed to load font face '%ls'", *FontPath);
		}
	}
	return FontPaths.Num();
}

void FArcRmlUIThread::ShutdownRml()
{
	// Every job finishes before Rml::Shutdown, which asserts that none is running.
	if (AsyncJobs)
	{
		AsyncJobs->WaitForAll();
	}

	for (TPair<uint32, TUniquePtr<FViewState>>& Pair : Views)
	{
		FViewState& View = *Pair.Value;
		ActivateView(&View);
		View.EmbedWrapper.reset();
		View.FrameStyleSheet.reset();
		if (View.Context)
		{
			Rml::RemoveContext(View.ContextName);
			View.Context = nullptr;
		}
		View.ModelStore.Reset();
		ActivateView(nullptr);
		ARCRMLUI_PERF(View.RmlPerf.ResetLists());
		ElementsAtShutdown.Add(MoveTemp(View.Element));
	}

	if (bRmlInitialized)
	{
		Rml::Shutdown();
		bRmlInitialized = false;
	}

	Views.Empty();
	AsyncJobs.Reset();
	TextInputHandler.Reset();
#if WITH_EDITOR
	Compiler.Reset();
#endif
	FileInterface.Reset();
	SystemInterface.Reset();
}

void FArcRmlUIThread::Pump()
{
#if UE_BUILD_SHIPPING
	ProcessCommands();
	ProcessInput();
	InstallAsyncResults();
	UpdateViews();
#else
	const uint64 CommandsStartCycles = FPlatformTime::Cycles64();
	ProcessCommands();
	const uint64 InputStartCycles = FPlatformTime::Cycles64();
	ProcessInput();
	const uint64 AsyncStartCycles = FPlatformTime::Cycles64();
	InstallAsyncResults();
	const uint64 UpdateStartCycles = FPlatformTime::Cycles64();
	UpdateViews();
	const uint64 PumpEndCycles = FPlatformTime::Cycles64();

	++PumpPerf.Pumps;
	PumpPerf.CommandsMs += ArcRmlUIPerf::CyclesToMs(CommandsStartCycles, InputStartCycles);
	PumpPerf.InputMs += ArcRmlUIPerf::CyclesToMs(InputStartCycles, AsyncStartCycles);
	PumpPerf.AsyncMs += ArcRmlUIPerf::CyclesToMs(AsyncStartCycles, UpdateStartCycles);
	PumpPerf.UpdateMs += ArcRmlUIPerf::CyclesToMs(UpdateStartCycles, PumpEndCycles);
	PumpPerf.PumpMs += ArcRmlUIPerf::CyclesToMs(CommandsStartCycles, PumpEndCycles);
	ReportPerf();
	if (ArcRmlUIPerf::ConsumeDumpRequest())
	{
		DumpPerf();
	}
#endif
}

#if !UE_BUILD_SHIPPING
void FArcRmlUIThread::ReportPerf()
{
	double ElapsedSeconds = 0.0;
	if (!PerfClock.ShouldReport(ElapsedSeconds))
	{
		return;
	}

	if (PumpPerf.bActivity && ArcRmlUIPerf::IsEnabled())
	{
		TArray<FString> CommandCounts;
		for (int32 TypeIndex = 0; TypeIndex < static_cast<int32>(EArcRmlCommandType::Count); ++TypeIndex)
		{
			if (PumpPerf.CommandCounts[TypeIndex] > 0)
			{
				CommandCounts.Add(FString::Printf(TEXT("%s %d"),
					ArcRmlUIThreadPrivate::GetCommandTypeName(static_cast<EArcRmlCommandType>(TypeIndex)), PumpPerf.CommandCounts[TypeIndex]));
			}
		}

		UE_LOGF(LogArcRmlUI, Verbose, "ArcRmlUI perf: ui thread %.1f s: pumps %d, pump %.2f ms (commands %.2f, input %.2f, async %.2f, update %.2f), wait %.2f ms, inputs %d, async installs %d, commands [%ls]",
			ElapsedSeconds, PumpPerf.Pumps, PumpPerf.PumpMs, PumpPerf.CommandsMs, PumpPerf.InputMs, PumpPerf.AsyncMs, PumpPerf.UpdateMs, PumpPerf.WaitMs, PumpPerf.Inputs,
			PumpPerf.AsyncInstalls,
			*FString::Join(CommandCounts, TEXT(", ")));

		const ArcRmlUIThreadPrivate::FRmlBreakdown& AsyncBreakdown = AsyncInstallPerf->Breakdown;
		if (AsyncBreakdown.TotalMs > 0.0)
		{
			UE_LOGF(LogArcRmlUI, Verbose, "ArcRmlUI perf: ui thread async install breakdown %.1f s: %ls",
				ElapsedSeconds, *ArcRmlUIThreadPrivate::FormatBreakdown(AsyncBreakdown));
		}
		const FString AsyncCounts = ArcRmlUIThreadPrivate::FormatCounts(AsyncBreakdown);
		if (AsyncBreakdown.Frames > 0 && AsyncCounts != TEXT("none"))
		{
			UE_LOGF(LogArcRmlUI, Verbose, "ArcRmlUI perf: ui thread async install counts per install pump: %ls", *AsyncCounts);
		}

		for (TPair<uint32, TUniquePtr<FViewState>>& Pair : Views)
		{
			FViewState& View = *Pair.Value;
			const ArcRmlUIThreadPrivate::FViewPerf& Perf = View.Perf;
			if (Perf.Frames == 0 && !Perf.bPausedChanged)
			{
				continue;
			}

			const double Frames = static_cast<double>(FMath::Max(1, Perf.Frames));
			UE_LOGF(LogArcRmlUI, Verbose,
				"ArcRmlUI perf: view %u '%ls' %.1f s: frames %d (dirty %d, timed %d), update avg %.2f max %.2f ms, render avg %.2f max %.2f ms, publish avg %.2f max %.2f ms, "
				"per frame: commands %.0f draws %.0f layers %.1f composites %.1f, created: filters %d shaders %d geometry %d (%d verts) textures %d, released: geometry %d textures %d, "
				"model deltas %d (list ops %d), inputs %d, next update delay %ls, paused %ls%ls",
				View.Id, *View.DocumentPath, ElapsedSeconds, Perf.Frames, Perf.DirtyFrames, Perf.TimedFrames,
				Perf.Update.GetAverageMs(), Perf.Update.MaxMs, Perf.Render.GetAverageMs(), Perf.Render.MaxMs, Perf.Publish.GetAverageMs(), Perf.Publish.MaxMs,
				static_cast<double>(Perf.Commands) / Frames, static_cast<double>(Perf.Draws) / Frames, static_cast<double>(Perf.LayerPushes) / Frames,
				static_cast<double>(Perf.Composites) / Frames, Perf.FilterCreates, Perf.ShaderCreates, Perf.GeometryCreates, Perf.GeometryVertices, Perf.TextureCreates,
				Perf.GeometryReleases, Perf.TextureReleases, Perf.ModelDeltas, Perf.ModelListOps, Perf.Inputs, *ArcRmlUIThreadPrivate::FormatDelay(Perf.LastNextUpdateDelay),
				View.bPaused ? TEXT("true") : TEXT("false"), Perf.bPausedChanged ? TEXT(" (changed)") : TEXT(""));

			if (View.UpdateBreakdown.Frames > 0)
			{
				UE_LOGF(LogArcRmlUI, Verbose, "ArcRmlUI perf: view %u '%ls' update breakdown %.1f s: %ls",
					View.Id, *View.DocumentPath, ElapsedSeconds, *ArcRmlUIThreadPrivate::FormatBreakdown(View.UpdateBreakdown));
				UE_LOGF(LogArcRmlUI, Verbose, "ArcRmlUI perf: view %u '%ls' update counts per frame: %ls",
					View.Id, *View.DocumentPath, *ArcRmlUIThreadPrivate::FormatCounts(View.UpdateBreakdown));
			}
			if (View.RenderBreakdown.Frames > 0)
			{
				UE_LOGF(LogArcRmlUI, Verbose, "ArcRmlUI perf: view %u '%ls' render breakdown %.1f s: %ls",
					View.Id, *View.DocumentPath, ElapsedSeconds, *ArcRmlUIThreadPrivate::FormatBreakdown(View.RenderBreakdown));
				UE_LOGF(LogArcRmlUI, Verbose, "ArcRmlUI perf: view %u '%ls' render counts per frame: %ls",
					View.Id, *View.DocumentPath, *ArcRmlUIThreadPrivate::FormatCounts(View.RenderBreakdown));
			}

			if (ArcRmlUIPerf::GetLevel() >= 2 && Perf.Frames > 0)
			{
				const int32 TopN = ArcRmlUIPerf::GetTopN();
				View.LastWhoLines.Reset();
				for (int32 ListIndex = 0; ListIndex < ArcRmlUIThreadPrivate::NumRmlElementLists; ++ListIndex)
				{
					const ArcRmlUIThreadPrivate::FRmlElementMap& Map = View.RmlPerf.elements[ListIndex];
					if (!Map.empty())
					{
						View.LastWhoLines.Add(FString::Printf(TEXT("who %s: %s"),
							UTF8_TO_TCHAR(Rml::ArcPerf::GetElementListName(static_cast<Rml::ArcPerf::ElementList>(ListIndex))),
							*ArcRmlUIThreadPrivate::FormatElementList(Map, TopN)));
					}
				}
				for (int32 ListIndex = 0; ListIndex < ArcRmlUIThreadPrivate::NumRmlNamedLists; ++ListIndex)
				{
					const ArcRmlUIThreadPrivate::FRmlNamedMap& Map = View.RmlPerf.named[ListIndex];
					if (!Map.empty())
					{
						View.LastWhoLines.Add(FString::Printf(TEXT("who %s: %s"),
							UTF8_TO_TCHAR(Rml::ArcPerf::GetNamedListName(static_cast<Rml::ArcPerf::NamedList>(ListIndex))),
							*ArcRmlUIThreadPrivate::FormatNamedList(Map, TopN)));
					}
				}
				if (View.Context)
				{
					View.LastWhoLines.Add(FString::Printf(TEXT("active animations: %s"),
						*ArcRmlUIThreadPrivate::FormatAnimations(View.Context->GetRootElement(), TopN)));
				}
				View.LastWhoSeconds = FPlatformTime::Seconds();
				for (const FString& Line : View.LastWhoLines)
				{
					UE_LOGF(LogArcRmlUI, Verbose, "ArcRmlUI perf: view %u '%ls' %ls", View.Id, *View.DocumentPath, *Line);
				}
			}
		}
	}

	PumpPerf = FPumpPerf();
	AsyncInstallPerf->Breakdown = ArcRmlUIThreadPrivate::FRmlBreakdown();
	for (TPair<uint32, TUniquePtr<FViewState>>& Pair : Views)
	{
		FViewState& View = *Pair.Value;
		const double LastNextUpdateDelay = View.Perf.LastNextUpdateDelay;
		View.Perf = ArcRmlUIThreadPrivate::FViewPerf();
		View.Perf.LastNextUpdateDelay = LastNextUpdateDelay;
		View.UpdateBreakdown = ArcRmlUIThreadPrivate::FRmlBreakdown();
		View.RenderBreakdown = ArcRmlUIThreadPrivate::FRmlBreakdown();
		View.RmlPerf.ResetLists();
	}
}

void FArcRmlUIThread::DumpPerf()
{
	for (TPair<uint32, TUniquePtr<FViewState>>& Pair : Views)
	{
		FViewState& View = *Pair.Value;
		if (!View.Context)
		{
			continue;
		}

		Rml::Element* Root = View.Context->GetRootElement();
		UE_LOGF(LogArcRmlUI, Log, "ArcRmlUI perf: dump view %u '%ls' elements %ls", View.Id, *View.DocumentPath,
			*ArcRmlUIThreadPrivate::FormatElementsByTag(Root));
		UE_LOGF(LogArcRmlUI, Log, "ArcRmlUI perf: dump view %u '%ls' stylesheet: %ls", View.Id, *View.DocumentPath,
			*ArcRmlUIThreadPrivate::FormatStyleSheetStats(View.Document));
		UE_LOGF(LogArcRmlUI, Log, "ArcRmlUI perf: dump view %u '%ls' active animations: %ls", View.Id, *View.DocumentPath,
			*ArcRmlUIThreadPrivate::FormatAnimations(Root, MAX_int32));
		if (View.LastWhoLines.Num() == 0)
		{
			UE_LOGF(LogArcRmlUI, Log, "ArcRmlUI perf: dump view %u '%ls' no attribution lists recorded yet (needs ArcRml.PerfLog 2 and one report interval with frames)",
				View.Id, *View.DocumentPath);
		}
		const double WhoAgeSeconds = FPlatformTime::Seconds() - View.LastWhoSeconds;
		for (const FString& Line : View.LastWhoLines)
		{
			UE_LOGF(LogArcRmlUI, Log, "ArcRmlUI perf: dump view %u '%ls' interval reported %.1f s ago: %ls", View.Id, *View.DocumentPath, WhoAgeSeconds, *Line);
		}
	}
}

void FArcRmlUIThread::ReportLayoutVerify()
{
	if (!LayoutVerifyView)
	{
		return;
	}

	FViewState& View = *LayoutVerifyView;
	for (const Rml::ArcPerf::LayoutVerifyPass& Pass : View.LayoutVerify.passes)
	{
		const TCHAR* Check = Pass.update_state ? TEXT("update verify") : Pass.layout_ran ? TEXT("layout verify") : TEXT("layout verify missed invalidation");
		UE_LOGF(LogArcRmlUI, Log,
			"ArcRmlUI perf: view %u '%ls' %ls '%ls': elements %d, mismatched elements %d, mismatches %d, dirty before reference %d, dirty after reference %d",
			View.Id, *View.DocumentPath, Check, *ArcRmlStrings::ToFString(Pass.document), Pass.elements, Pass.mismatched_elements, Pass.mismatches,
			Pass.dirty_before_reference, Pass.dirty_after_reference);
		if (Pass.dirty_before_reference > 0)
		{
			UE_LOGF(LogArcRmlUI, Warning, "ArcRmlUI perf: view %u '%ls' layout verify failure '%ls': %d elements still layout-dirty before the reference",
				View.Id, *View.DocumentPath, *ArcRmlStrings::ToFString(Pass.document), Pass.dirty_before_reference);
		}
		for (int32 Index = 0; Index < static_cast<int32>(Pass.first_mismatches.size()); ++Index)
		{
			const Rml::ArcPerf::LayoutMismatch& Mismatch = Pass.first_mismatches[Index];
			UE_LOGF(LogArcRmlUI, Log, "ArcRmlUI perf: view %u '%ls' %ls mismatch [%d/%d] '%ls' %ls: optimized %ls, reference %ls",
				View.Id, *View.DocumentPath, Check, Index + 1, Pass.mismatches, *ArcRmlStrings::ToFString(Mismatch.element),
				*ArcRmlStrings::ToFString(Mismatch.field), *ArcRmlStrings::ToFString(Mismatch.optimized), *ArcRmlStrings::ToFString(Mismatch.reference));
		}
	}
	View.LayoutVerify.passes.clear();
}
#endif

FArcRmlUIThread::FViewState* FArcRmlUIThread::FindView(uint32 ViewId)
{
	TUniquePtr<FViewState>* Found = Views.Find(ViewId);
	return Found ? Found->Get() : nullptr;
}

void FArcRmlUIThread::ActivateView(FViewState* View)
{
	ActiveView = View;
#if !UE_BUILD_SHIPPING
	Rml::ArcPerf::SetSink(View ? &View->RmlPerf : nullptr, View ? ArcRmlUIPerf::GetLevel() : 0);
	ReportLayoutVerify();
	LayoutVerifyView = View && ArcRmlUIPerf::IsLayoutVerifyEnabled() ? View : nullptr;
	if (LayoutVerifyView)
	{
		LayoutVerifyView->LayoutVerify.max_mismatches = ArcRmlUIPerf::GetTopN();
	}
	Rml::ArcPerf::SetLayoutVerify(LayoutVerifyView ? &LayoutVerifyView->LayoutVerify : nullptr);
#endif
	if (SystemInterface)
	{
		SystemInterface->SetActiveView(View ? &View->Cursor : nullptr, View ? View->Channel.Get() : nullptr);
	}
	if (TextInputHandler)
	{
		TextInputHandler->SetCurrentView(View ? View->Id : 0);
	}
}

void FArcRmlUIThread::ProcessCommands()
{
	FArcRmlCommand Command;
	while (Commands.Dequeue(Command))
	{
#if !UE_BUILD_SHIPPING
		++PumpPerf.CommandCounts[static_cast<int32>(Command.Type)];
		PumpPerf.bActivity = true;
#endif
		switch (Command.Type)
		{
		case EArcRmlCommandType::AddView:
		{
			TUniquePtr<FViewState> View = MakeUnique<FViewState>();
			View->Id = Command.ViewId;
			View->ContextName = Rml::CreateString("ArcRmlView_%u", Command.ViewId);
			View->Recorder = MakeUnique<FArcRmlRecordingRenderInterface>();
			View->Element = MoveTemp(Command.Element);
			View->Channel = MoveTemp(Command.Channel);
			View->ModelStore = MakeUnique<FArcRmlModelStore>(View->Channel.ToSharedRef());
			View->ActionRouter = MakeUnique<FArcRmlActionRouter>(View->Channel.ToSharedRef());
			View->RootTransitionListener = MakeUnique<ArcRmlUIThreadPrivate::FRootTransitionListener>(Command.ViewId, View->Channel.ToSharedRef());
			ActivateView(View.Get());
			View->Context = Rml::CreateContext(View->ContextName, Rml::Vector2i(1, 1), View->Recorder.Get(), nullptr);
			ActivateView(nullptr);
			if (View->Context)
			{
				const FArcRmlViewCamera* Camera = &View->Camera;
				Rml::SetWorldProjector(*View->Context, [Camera](const Rml::WorldPoint& WorldPoint, Rml::Vector2f& OutContextPoint) -> bool
				{
					return ArcRmlUIThreadPrivate::ProjectWorldPoint(*Camera, WorldPoint, OutContextPoint);
				});
			}
			else
			{
				UE_LOGF(LogArcRmlUI, Error, "ArcRmlUI: failed to create RmlUi context for view %u", Command.ViewId);
			}
			Views.Add(Command.ViewId, MoveTemp(View));
			break;
		}
		case EArcRmlCommandType::RemoveView:
		{
			if (Views.Contains(Command.ViewId))
			{
				TUniquePtr<FViewState> View = Views.FindAndRemoveChecked(Command.ViewId);
				DestroyView(*View);
			}
			break;
		}
		case EArcRmlCommandType::LoadDocument:
		{
			FViewState* View = FindView(Command.ViewId);
			if (View && View->Context)
			{
				ARCRMLUI_PERF(View->LoadPerf.OpenSeconds = Command.OpenSeconds);
				View->Size = Command.Size;
				View->Context->SetDimensions(Rml::Vector2i(FMath::Max(1, Command.Size.X), FMath::Max(1, Command.Size.Y)));
				View->Context->SetDensityIndependentPixelRatio(Command.DpRatio);
				LoadDocumentForView(*View, Command.Path);
				if (View->bEmbedPending)
				{
					View->bEmbedPending = false;
					const FString EmbedPath = MoveTemp(View->EmbeddedPath);
					View->EmbeddedPath.Reset();
					if (View->Document)
					{
						EmbedDocumentInView(*View, EmbedPath);
					}
					else
					{
						UE_LOGF(LogArcRmlUI, Error, "ArcRmlUI: view %u has no document, '%ls' is not embedded", View->Id, *EmbedPath);
					}
				}
			}
			break;
		}
		case EArcRmlCommandType::ClearDocumentCaches:
			ClearCaches();
			break;
		case EArcRmlCommandType::InstallAssetTexture:
		{
			FViewState* View = FindView(Command.ViewId);
			if (View && View->Context)
			{
				ActivateView(View);
				if (Command.AssetTextureToken != 0)
				{
					Rml::InstallAssetTexture(View->Recorder.Get(), Command.AssetTexture, Rml::Vector2i(Command.Size.X, Command.Size.Y), Command.AssetTextureToken);
				}
				else
				{
					Rml::FailAssetTexture(View->Recorder.Get(), Command.AssetTexture);
				}
				ActivateView(nullptr);
				View->bDirty = true;
			}
			break;
		}
		case EArcRmlCommandType::UpdateContentRoot:
		{
			// Texture decode jobs (RmlUi TextureDatabase.cpp:153) read files through the root table; none may run while it changes.
			AsyncJobs->WaitForAll();
			if (!Command.bContentRootMounted)
			{
				Dependencies.ContentRoots.Remove(Command.ContentRoot.MountName);
			}
			else if (Dependencies.ContentRoots.Add(Command.ContentRoot))
			{
				const int32 FontFaces = LoadFonts(Command.ContentRoot);
				UE_LOGF(LogArcRmlUI, Log, "ArcRmlUI: content root '/%ls/' added, %d font faces", *Command.ContentRoot.MountName, FontFaces);
			}
			break;
		}
#if WITH_EDITOR
		case EArcRmlCommandType::DumpExpandedDocument:
		{
			FString Expanded;
			FArcRmlComponentExpander Expander(*FileInterface, Dependencies.ContentRoots);
			if (!Expander.ExpandDocument(Command.Path, Expanded) && Expanded.IsEmpty())
			{
				UE_LOGF(LogArcRmlUI, Error, "ArcRmlUI: cannot read document '%ls'", *Command.Path);
				break;
			}
			FString MountName;
			FString RelativePath;
			if (!ArcRmlPaths::SplitPackagePath(ArcRmlPaths::NormalizePackagePath(Command.Path), MountName, RelativePath))
			{
				UE_LOGF(LogArcRmlUI, Error, "ArcRmlUI: '%ls' is not a package path", *Command.Path);
				break;
			}
			const FString OutputPath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("ArcRml"), MountName, RelativePath);
			if (FFileHelper::SaveStringToFile(Expanded, *OutputPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
			{
				UE_LOGF(LogArcRmlUI, Display, "ArcRmlUI: wrote the expanded '%ls' to '%ls'", *Command.Path, *FPaths::ConvertRelativePathToFull(OutputPath));
			}
			else
			{
				UE_LOGF(LogArcRmlUI, Error, "ArcRmlUI: cannot write '%ls'", *OutputPath);
			}
			break;
		}
		case EArcRmlCommandType::CompileChanged:
		{
			const double CompileStartSeconds = FPlatformTime::Seconds();
			FArcRmlCompileReport Report;
			TArray<FString> OpenDocuments;
			for (const TPair<uint32, TUniquePtr<FViewState>>& Pair : Views)
			{
				if (!Pair.Value->DocumentPath.IsEmpty())
				{
					OpenDocuments.AddUnique(Pair.Value->DocumentPath);
				}
				if (!Pair.Value->EmbeddedPath.IsEmpty())
				{
					OpenDocuments.AddUnique(Pair.Value->EmbeddedPath);
				}
			}
			Compiler->CompileChanged(Command.Paths, OpenDocuments, Report);
			UE_LOGF(LogArcRmlUI, Log, "ArcRmlUI: compile changed: %d documents and %d style sheets affected, %d compiled, %d failed, %d orphan binaries removed, %.2f ms",
				Report.Documents, Report.StyleSheets, Report.Compiled.Num(), Report.Failed.Num(), Report.Removed.Num(), (FPlatformTime::Seconds() - CompileStartSeconds) * 1000.0);
			break;
		}
		case EArcRmlCommandType::CompileAll:
		{
			Compiler->CompileAll(Command.Paths, *Command.CompileReport);
			Command.CompileDone->Trigger();
			break;
		}
#endif
#if !UE_BUILD_SHIPPING
		case EArcRmlCommandType::ClickElement:
		{
			if (FViewState* View = FindView(Command.ViewId))
			{
				ClickElementInView(*View, Command.Path, Command.ElementIndex);
			}
			break;
		}
		case EArcRmlCommandType::DumpLayout:
		{
			if (FViewState* View = FindView(Command.ViewId))
			{
				View->PendingLayoutDumpDir = MoveTemp(Command.Path);
				View->bDirty = true;
			}
			break;
		}
#endif
		case EArcRmlCommandType::BindModel:
		{
			FViewState* View = FindView(Command.ViewId);
			if (View && View->Context)
			{
				View->ModelStore->BindModel(*View->Context, Command.ModelName, Command.ModelValue);
			}
			break;
		}
		case EArcRmlCommandType::UnbindModel:
		{
			FViewState* View = FindView(Command.ViewId);
			if (View && View->Context && View->ModelStore->UnbindModel(*View->Context, Command.ModelName))
			{
				View->bDirty = true;
			}
			break;
		}
		case EArcRmlCommandType::EmbedDocument:
		{
			FViewState* View = FindView(Command.ViewId);
			if (!View || !View->Context)
			{
				break;
			}
			if (View->EmbedWrapper || View->bEmbedPending)
			{
				UE_LOGF(LogArcRmlUI, Error, "ArcRmlUI: view %u already embeds '%ls', '%ls' is not embedded", View->Id, *View->EmbeddedPath, *Command.Path);
			}
			else if (View->Document)
			{
				EmbedDocumentInView(*View, Command.Path);
			}
			else if (View->DocumentPath.IsEmpty())
			{
				View->EmbeddedPath = MoveTemp(Command.Path);
				View->bEmbedPending = true;
			}
			else
			{
				UE_LOGF(LogArcRmlUI, Error, "ArcRmlUI: view %u has no document, '%ls' is not embedded", View->Id, *Command.Path);
			}
			break;
		}
		case EArcRmlCommandType::ClearEmbed:
		{
			if (FViewState* View = FindView(Command.ViewId))
			{
				ClearEmbedInView(*View);
			}
			break;
		}
		case EArcRmlCommandType::ApplyModelDelta:
		{
			FViewState* View = FindView(Command.ViewId);
#if !UE_BUILD_SHIPPING
			if (View)
			{
				++View->Perf.ModelDeltas;
			}
#endif
			if (View && View->ModelStore->ApplyDelta(Command.ModelName, Command.ModelPath, Command.ModelValue))
			{
				View->bDirty = true;
			}
			break;
		}
		case EArcRmlCommandType::ApplyModelListOps:
		{
			FViewState* View = FindView(Command.ViewId);
#if !UE_BUILD_SHIPPING
			if (View)
			{
				++View->Perf.ModelDeltas;
				View->Perf.ModelListOps += Command.ModelListOps.Num();
			}
#endif
			if (View && View->ModelStore->ApplyListOps(Command.ModelName, Command.ModelPath, Command.ModelListOps))
			{
				View->bDirty = true;
			}
			break;
		}
		case EArcRmlCommandType::Resize:
		{
			FViewState* View = FindView(Command.ViewId);
			if (View && View->Context)
			{
				ARCRMLUI_PERF(const ArcRmlUIThreadPrivate::FScopedRmlPerfSink PerfSink(View->RmlPerf));
				View->Size = Command.Size;
				View->Context->SetDimensions(Rml::Vector2i(FMath::Max(1, Command.Size.X), FMath::Max(1, Command.Size.Y)));
				View->Context->SetDensityIndependentPixelRatio(Command.DpRatio);
				View->bDirty = true;
			}
			break;
		}
		case EArcRmlCommandType::SetViewPaused:
		{
			FViewState* View = FindView(Command.ViewId);
			if (View && View->bPaused != Command.bPaused)
			{
				View->bPaused = Command.bPaused;
				ARCRMLUI_PERF(View->Perf.bPausedChanged = true);
				if (!View->bPaused)
				{
					View->bDirty = true;
				}
				UE_LOGF(LogArcRmlUI, Verbose, "ArcRmlUI: view %u %ls", Command.ViewId, View->bPaused ? TEXT("paused") : TEXT("resumed"));
			}
			break;
		}
		case EArcRmlCommandType::SetRootClass:
		{
			if (FViewState* View = FindView(Command.ViewId))
			{
				++View->RootTransitionListener->RootClassSerial;
				if (View->Document)
				{
					ARCRMLUI_PERF(const ArcRmlUIThreadPrivate::FScopedRmlPerfSink PerfSink(View->RmlPerf));
					View->Document->SetClass(ArcRmlStrings::ToRmlString(Command.RootClass), Command.bRootClassSet);
				}
				else
				{
					View->PendingRootClasses.RemoveAll([&Command](const TPair<FString, bool>& Pending)
					{
						return Pending.Key == Command.RootClass;
					});
					View->PendingRootClasses.Emplace(MoveTemp(Command.RootClass), Command.bRootClassSet);
				}
				View->bRootCheckPending = true;
				View->bDirty = true;
			}
			break;
		}
		case EArcRmlCommandType::Count:
			break;
		}
	}
}

void FArcRmlUIThread::DestroyView(FViewState& View)
{
	ActivateView(&View);
	View.EmbedWrapper.reset();
	View.FrameStyleSheet.reset();
	if (View.Context)
	{
		Rml::RemoveContext(View.ContextName);
		View.Context = nullptr;
	}
	View.ModelStore.Reset();
	ActivateView(nullptr);
	ARCRMLUI_PERF(View.RmlPerf.ResetLists());

	Rml::ReleaseRenderManagers();
	View.Recorder.Reset();

	ENQUEUE_RENDER_COMMAND(ArcRmlReleaseView)(
		[ReleasedElement = MoveTemp(View.Element), WeakAssetTextures = TWeakPtr<FArcRmlAssetTextureRegistry, ESPMode::ThreadSafe>(AssetTextures),
			ViewId = View.Id](FRHICommandListImmediate&) mutable
		{
			if (ReleasedElement.IsValid())
			{
				ReleasedElement->ReleaseResources_RenderThread();
				ReleasedElement.Reset();
			}
			AsyncTask(ENamedThreads::GameThread, [WeakAssetTextures, ViewId]()
				{
					if (const TSharedPtr<FArcRmlAssetTextureRegistry, ESPMode::ThreadSafe> Registry = WeakAssetTextures.Pin())
					{
						Registry->AcknowledgeRelease(ViewId);
					}
				});
		});

	for (TPair<uint32, TUniquePtr<FViewState>>& Pair : Views)
	{
		Pair.Value->bDirty = true;
	}
}

void FArcRmlUIThread::LoadDocumentForView(FViewState& View, const FString& Path)
{
	View.DocumentPath = Path;
#if !UE_BUILD_SHIPPING
	View.LoadPerf.bPending = false;
	ENQUEUE_RENDER_COMMAND(ArcRmlSetPerfLabel)(
		[Element = View.Element, ViewId = View.Id, Path](FRHICommandListImmediate&)
		{
			Element->SetPerfLabel_RenderThread(ViewId, Path);
		});
	FArcRmlFileInterface::TakeIoStats();
#endif

	if (!Path.EndsWith(TEXT(".rml"), ESearchCase::IgnoreCase))
	{
		UE_LOGF(LogArcRmlUI, Error, "ArcRmlUI: '%ls' is not an .rml document", *Path);
		return;
	}

	FArcRmlCompiledFileData CompiledFile;
	double FileReadMs = 0.0;
	if (!LoadCompiledFile(Path, EArcRmlCompiledKind::Document, CompiledFile, FileReadMs))
	{
		return;
	}

	ARCRMLUI_PERF(const uint64 ReadStartCycles = FPlatformTime::Cycles64());
	Rml::CompiledDocument Compiled;
	Rml::String ReadError;
	const TConstArrayView<uint8> Body = CompiledFile.GetBody();
	if (!Rml::ReadCompiledDocument(Rml::Span<const Rml::byte>(Body.GetData(), static_cast<size_t>(Body.Num())), Compiled, ReadError))
	{
		UE_LOGF(LogArcRmlUI, Error, "ArcRmlUI: the compiled document of '%ls' is unusable: %ls", *Path, *ArcRmlStrings::ToFString(ReadError));
		return;
	}

#if !UE_BUILD_SHIPPING
	const double ReadMs = FileReadMs + ArcRmlUIPerf::SinceMs(ReadStartCycles);
	const uint64 LoadStartCycles = FPlatformTime::Cycles64();
#endif
	ActivateView(&View);
#if !UE_BUILD_SHIPPING
	View.RmlPerf.frame.Reset();
	if (ArcRmlUIPerf::IsEnabled())
	{
		View.RmlPerf.frame.bucket_ms[static_cast<int32>(Rml::ArcPerf::Bucket::CompiledRead)] += ReadMs;
		++View.RmlPerf.frame.bucket_calls[static_cast<int32>(Rml::ArcPerf::Bucket::CompiledRead)];
	}
#endif
	Rml::ElementDocument* Document = View.Context->LoadDocumentCompiled(Compiled, ArcRmlStrings::ToRmlString(Path));
#if !UE_BUILD_SHIPPING
	ArcRmlUIThreadPrivate::FRmlBreakdown LoadBreakdown;
	LoadBreakdown.Add(View.RmlPerf.frame);
	const FArcRmlFileIoStats LoadIo = FArcRmlFileInterface::TakeIoStats();
	if (Document && ArcRmlUIPerf::IsEnabled())
	{
		ArcRmlUIThreadPrivate::FLoadPerf& LoadPerf = View.LoadPerf;
		LoadPerf.bPending = true;
		LoadPerf.ReadMs = ReadMs;
		LoadPerf.ReadBytes = CompiledFile.Bytes.Num();
		LoadPerf.LoadMs = ArcRmlUIPerf::SinceMs(LoadStartCycles);
		LoadPerf.LoadedElements = ArcRmlUIThreadPrivate::CountElements(Document);
	}
#endif
	if (Document)
	{
		View.Document = Document;
		Document->AddEventListener(Rml::EventId::Click, View.ActionRouter.Get());
		Document->AddEventListener(Rml::EventId::Animationend, View.RootTransitionListener.Get());
		Document->AddEventListener(Rml::EventId::Transitionend, View.RootTransitionListener.Get());
		ARCRMLUI_PERF(View.RmlPerf.frame.Reset());
		Document->Show();
#if !UE_BUILD_SHIPPING
		if (ArcRmlUIPerf::IsEnabled())
		{
			ArcRmlUIThreadPrivate::FRmlBreakdown ShowBreakdown;
			ShowBreakdown.Add(View.RmlPerf.frame);
			const FArcRmlFileIoStats ShowIo = FArcRmlFileInterface::TakeIoStats();
			View.RmlPerf.frame.Reset();
			const int32 TopN = ArcRmlUIPerf::GetTopN();
			UE_LOGF(LogArcRmlUI, Verbose, "ArcRmlUI perf: view %u '%ls' load breakdown (LoadDocumentCompiled): %ls",
				View.Id, *Path, *ArcRmlUIThreadPrivate::FormatBreakdown(LoadBreakdown));
			UE_LOGF(LogArcRmlUI, Verbose, "ArcRmlUI perf: view %u '%ls' load counts: %ls", View.Id, *Path, *ArcRmlUIThreadPrivate::FormatCounts(LoadBreakdown));
			UE_LOGF(LogArcRmlUI, Verbose, "ArcRmlUI perf: view %u '%ls' file io: load %lld opens, %lld reads, %lld bytes, %.3f ms; show %lld opens, %lld reads, %lld bytes, %.3f ms",
				View.Id, *Path, LoadIo.Opens, LoadIo.Reads, LoadIo.Bytes, LoadIo.Ms,
				ShowIo.Opens, ShowIo.Reads, ShowIo.Bytes, ShowIo.Ms);
			UE_LOGF(LogArcRmlUI, Verbose, "ArcRmlUI perf: view %u '%ls' show breakdown (ElementDocument::Show): %ls",
				View.Id, *Path, *ArcRmlUIThreadPrivate::FormatBreakdown(ShowBreakdown));
			UE_LOGF(LogArcRmlUI, Verbose, "ArcRmlUI perf: view %u '%ls' load stylesheet: %ls", View.Id, *Path, *ArcRmlUIThreadPrivate::FormatStyleSheetStats(Document));
			const Rml::ArcPerf::NamedList LoadLists[] = {
				Rml::ArcPerf::NamedList::StyleSheetParse,
				Rml::ArcPerf::NamedList::StyleSheetCache,
				Rml::ArcPerf::NamedList::InstancedTags,
				Rml::ArcPerf::NamedList::DataBindingTypes,
			};
			for (const Rml::ArcPerf::NamedList List : LoadLists)
			{
				ArcRmlUIThreadPrivate::FRmlNamedMap& Map = View.RmlPerf.named[static_cast<int32>(List)];
				if (!Map.empty())
				{
					UE_LOGF(LogArcRmlUI, Verbose, "ArcRmlUI perf: view %u '%ls' load %ls: %ls", View.Id, *Path,
						UTF8_TO_TCHAR(Rml::ArcPerf::GetNamedListName(List)), *ArcRmlUIThreadPrivate::FormatNamedList(Map, List == Rml::ArcPerf::NamedList::InstancedTags ? TopN : MAX_int32));
					Map.clear();
				}
			}
			View.RmlPerf.ResetLists();
		}
#endif
		for (const TPair<FString, bool>& RootClass : View.PendingRootClasses)
		{
			Document->SetClass(ArcRmlStrings::ToRmlString(RootClass.Key), RootClass.Value);
		}
	}
	else
	{
		UE_LOGF(LogArcRmlUI, Error, "ArcRmlUI: failed to load document '%ls'", *Path);
	}
	View.PendingRootClasses.Reset();
	ActivateView(nullptr);
	View.bDirty = true;
}

bool FArcRmlUIThread::LoadCompiledFile(const FString& SourcePath, EArcRmlCompiledKind Kind, FArcRmlCompiledFileData& OutData, double& OutReadMs)
{
	const FString CompiledPath = SourcePath + TEXT("c");
	const uint64 RegistryHash = Rml::GetCompiledRegistryHash();
	double ReadStartSeconds = FPlatformTime::Seconds();
	EArcRmlCompiledLoad Result = ArcRmlCompiledFile::Load(FileInterface->ResolvePath(CompiledPath), Kind, RegistryHash, OutData);
	double ReadEndSeconds = FPlatformTime::Seconds();
	const TCHAR* Problem = Result == EArcRmlCompiledLoad::Missing ? TEXT("missing") : TEXT("unusable");
#if WITH_EDITOR
	const bool bStale = Result == EArcRmlCompiledLoad::Loaded && !Compiler->IsFresh(OutData.Header);
	if (bStale)
	{
		Problem = TEXT("stale");
	}
	if (Result != EArcRmlCompiledLoad::Loaded || bStale)
	{
		const double CompileStartSeconds = FPlatformTime::Seconds();
		FArcRmlCompileReport Report;
		const bool bCompiled = Kind == EArcRmlCompiledKind::Document ? Compiler->CompileDocument(SourcePath, Report) : Compiler->CompileStyleSheet(SourcePath, Report);
		if (bCompiled)
		{
			UE_LOGF(LogArcRmlUI, Log, "ArcRmlUI: compiled '%ls' at open, its binary was %ls, %.2f ms", *SourcePath, Problem,
				(FPlatformTime::Seconds() - CompileStartSeconds) * 1000.0);
			ReadStartSeconds = FPlatformTime::Seconds();
			Result = ArcRmlCompiledFile::Load(FileInterface->ResolvePath(CompiledPath), Kind, RegistryHash, OutData);
			ReadEndSeconds = FPlatformTime::Seconds();
			Problem = Result == EArcRmlCompiledLoad::Missing ? TEXT("missing") : TEXT("unusable");
		}
		else if (bStale)
		{
			Result = EArcRmlCompiledLoad::Unusable;
		}
	}
#endif
	OutReadMs = (ReadEndSeconds - ReadStartSeconds) * 1000.0;
	if (Result != EArcRmlCompiledLoad::Loaded)
	{
		UE_LOGF(LogArcRmlUI, Error, "ArcRmlUI: '%ls' has no usable compiled file '%ls' (%ls)", *SourcePath, *CompiledPath, Problem);
		return false;
	}
	return true;
}

void FArcRmlUIThread::EmbedDocumentInView(FViewState& View, const FString& Path)
{
	Rml::Element* Container = nullptr;
	const int32 Containers = ArcRmlUIThreadPrivate::FindEmbedContainer(View.Document, Container);
	if (Containers != 1)
	{
		UE_LOGF(LogArcRmlUI, Error, "ArcRmlUI: '%ls' has %d elements with the '%ls' attribute instead of one, '%ls' is not embedded",
			*View.DocumentPath, Containers, *ArcRmlStrings::ToFString(ArcRmlEmbedAttributes::Container), *Path);
		return;
	}

	FArcRmlCompiledFileData CompiledFile;
	double ReadMs = 0.0;
	if (!LoadCompiledFile(Path, EArcRmlCompiledKind::Document, CompiledFile, ReadMs))
	{
		return;
	}

	Rml::CompiledDocument Compiled;
	Rml::String ReadError;
	const TConstArrayView<uint8> Body = CompiledFile.GetBody();
	if (!Rml::ReadCompiledDocument(Rml::Span<const Rml::byte>(Body.GetData(), static_cast<size_t>(Body.Num())), Compiled, ReadError))
	{
		UE_LOGF(LogArcRmlUI, Error, "ArcRmlUI: the compiled document of '%ls' is unusable: %ls", *Path, *ArcRmlStrings::ToFString(ReadError));
		return;
	}

	ActivateView(&View);
	Rml::SharedPtr<Rml::StyleSheetContainer> TabStyleSheet;
	Rml::Element* Wrapper = View.Context->EmbedDocumentCompiled(Container, Compiled, ArcRmlStrings::ToRmlString(Path), TabStyleSheet);
	if (Wrapper)
	{
		Wrapper->SetAttribute(ArcRmlEmbedAttributes::Wrapper, Rml::String());
		View.EmbedWrapper = Wrapper->GetObserverPtr();
		View.EmbeddedPath = Path;
		if (TabStyleSheet)
		{
			const Rml::StyleSheetContainer* OwnStyleSheet = View.Document->GetStyleSheetContainer();
			View.FrameStyleSheet = OwnStyleSheet ? OwnStyleSheet->CombineStyleSheetContainer(Rml::StyleSheetContainer()) : Rml::MakeShared<Rml::StyleSheetContainer>();
			View.Document->SetStyleSheetContainer(View.FrameStyleSheet->CombineStyleSheetContainer(*TabStyleSheet));
		}
		else
		{
			UE_LOGF(LogArcRmlUI, Error, "ArcRmlUI: '%ls' has no usable style sheet, it is embedded without one", *Path);
		}
	}
	else
	{
		UE_LOGF(LogArcRmlUI, Error, "ArcRmlUI: failed to embed '%ls' into '%ls'", *Path, *View.DocumentPath);
	}
	ActivateView(nullptr);
	View.bDirty = true;
}

void FArcRmlUIThread::ClearEmbedInView(FViewState& View)
{
	View.bEmbedPending = false;
	View.EmbeddedPath.Reset();
	Rml::Element* Wrapper = View.EmbedWrapper.get();
	View.EmbedWrapper.reset();
	if (!Wrapper)
	{
		return;
	}

	ActivateView(&View);
	if (Rml::Element* Parent = Wrapper->GetParentNode())
	{
		Parent->RemoveChild(Wrapper);
	}
	if (View.FrameStyleSheet)
	{
		View.Document->SetStyleSheetContainer(MoveTemp(View.FrameStyleSheet));
	}
	ActivateView(nullptr);
	View.bDirty = true;
}

#if !UE_BUILD_SHIPPING
void FArcRmlUIThread::ClickElementInView(FViewState& View, const FString& Selector, int32 Index)
{
	if (!View.Context)
	{
		return;
	}

	ActivateView(&View);
	View.Context->Update();

	Rml::ElementList Matches;
	View.Context->GetRootElement()->QuerySelectorAll(Matches, ArcRmlStrings::ToRmlString(Selector));
	Rml::Element* Target = nullptr;
	int32 VisibleIndex = 0;
	for (Rml::Element* Match : Matches)
	{
		if (!Match->IsVisible(true))
		{
			continue;
		}
		if (VisibleIndex == Index)
		{
			Target = Match;
			break;
		}
		++VisibleIndex;
	}

	if (Target)
	{
		const Rml::Vector2f Centre = Target->GetAbsoluteOffset(Rml::BoxArea::Border) + Target->GetBox().GetSize(Rml::BoxArea::Border) * 0.5f;
		const int32 X = FMath::RoundToInt(Centre.x);
		const int32 Y = FMath::RoundToInt(Centre.y);
		UE_LOGF(LogArcRmlUI, Display, "ArcRml.Click: clicking visible match %d of '%ls' at (%d, %d)", Index, *Selector, X, Y);
		View.Context->ProcessMouseMove(X, Y, 0);
		View.Context->ProcessMouseButtonDown(0, 0);
		View.Context->ProcessMouseButtonUp(0, 0);
		View.Context->ProcessMouseLeave();
		View.bDirty = true;
	}
	else
	{
		UE_LOGF(LogArcRmlUI, Warning, "ArcRml.Click: '%ls' has no visible match %d", *Selector, Index);
	}
	ActivateView(nullptr);
}

void FArcRmlUIThread::WriteLayoutDump(FViewState& View)
{
	const FString OutDir = MoveTemp(View.PendingLayoutDumpDir);
	View.PendingLayoutDumpDir.Reset();

	ArcRmlLayoutDump::FResult Result;
	Rml::ArcPerf::SetSink(nullptr, 0);
	bool bWritten = false;
	FArcRmlCompiledFileData CompiledFile;
	double ReadMs = 0.0;
	if (LoadCompiledFile(View.DocumentPath, EArcRmlCompiledKind::Document, CompiledFile, ReadMs))
	{
		Rml::CompiledDocument Compiled;
		Rml::String ReadError;
		const TConstArrayView<uint8> Body = CompiledFile.GetBody();
		if (Rml::ReadCompiledDocument(Rml::Span<const Rml::byte>(Body.GetData(), static_cast<size_t>(Body.Num())), Compiled, ReadError))
		{
			Rml::StringList Links;
			Rml::GetCompiledDocumentStyleSheetLinks(Compiled, Links);
			TArray<FString> StyleSheetLinks;
			for (const Rml::String& Link : Links)
			{
				StyleSheetLinks.Add(ArcRmlStrings::ToFString(Link));
			}
			bWritten = ArcRmlLayoutDump::Write(*View.Document, View.DocumentPath, StyleSheetLinks, *FileInterface, Dependencies, OutDir, Result);
		}
	}
	Rml::ArcPerf::SetSink(&View.RmlPerf, ArcRmlUIPerf::GetLevel());
	if (!bWritten)
	{
		UE_LOGF(LogArcRmlUI, Error, "ArcRml.LayoutDump: cannot dump '%ls' to '%ls'", *View.DocumentPath, *OutDir);
		return;
	}
	UE_LOGF(LogArcRmlUI, Display, "ArcRml.LayoutDump: '%ls' %d elements, wrote '%ls' and '%ls'", *View.DocumentPath, Result.Elements, *Result.LayoutPath, *Result.HtmlPath);
}
#endif

void FArcRmlUIThread::ClearCaches()
{
	Rml::Factory::ClearStyleSheetCache();
	Rml::Factory::ClearTemplateCache();
}

void FArcRmlUIThread::ProcessInput()
{
	FArcRmlInputEvent Event;
	while (Inputs.Dequeue(Event))
	{
		FViewState* View = FindView(Event.ViewId);
		if (!View || !View->Context)
		{
			continue;
		}

#if !UE_BUILD_SHIPPING
		++View->Perf.Inputs;
		++PumpPerf.Inputs;
		PumpPerf.bActivity = true;
#endif
		ActivateView(View);
		Rml::Context* Context = View->Context;
		const int32 X = FMath::RoundToInt(Event.Position.X);
		const int32 Y = FMath::RoundToInt(Event.Position.Y);
		switch (Event.Type)
		{
		case EArcRmlInputType::MouseMove:
			Context->ProcessMouseMove(X, Y, Event.Modifiers);
			break;
		case EArcRmlInputType::MouseDown:
			Context->ProcessMouseMove(X, Y, Event.Modifiers);
			Context->ProcessMouseButtonDown(Event.Button, Event.Modifiers);
			break;
		case EArcRmlInputType::MouseUp:
			Context->ProcessMouseMove(X, Y, Event.Modifiers);
			Context->ProcessMouseButtonUp(Event.Button, Event.Modifiers);
			break;
		case EArcRmlInputType::MouseWheel:
			Context->ProcessMouseWheel(Rml::Vector2f(0.0f, -Event.WheelDelta), Event.Modifiers);
			break;
		case EArcRmlInputType::MouseLeave:
			Context->ProcessMouseLeave();
			break;
		case EArcRmlInputType::KeyDown:
			Context->ProcessKeyDown(static_cast<Rml::Input::KeyIdentifier>(Event.Key), Event.Modifiers);
			break;
		case EArcRmlInputType::KeyUp:
			Context->ProcessKeyUp(static_cast<Rml::Input::KeyIdentifier>(Event.Key), Event.Modifiers);
			break;
		case EArcRmlInputType::TextInput:
			Context->ProcessTextInput(ArcRmlStrings::ToRmlString(Event.Text));
			break;
		case EArcRmlInputType::ImeSetText:
		case EArcRmlInputType::ImeSetSelection:
		case EArcRmlInputType::ImeSetComposition:
		{
			if (Rml::TextInputContext* InputContext = TextInputHandler->GetActiveContext(View->Id))
			{
				if (Event.Type == EArcRmlInputType::ImeSetText)
				{
					const Rml::String Text = ArcRmlStrings::ToRmlString(Event.Text);
					InputContext->SetText(Rml::StringView(Text), Event.RangeStart, Event.RangeEnd);
				}
				else if (Event.Type == EArcRmlInputType::ImeSetSelection)
				{
					InputContext->SetSelectionRange(Event.RangeStart, Event.RangeEnd);
				}
				else
				{
					InputContext->SetCompositionRange(Event.RangeStart, Event.RangeEnd);
				}
			}
			View->AppliedImeGeneration = FMath::Max(View->AppliedImeGeneration, Event.Generation);
			break;
		}
		}
		View->bDirty = true;
	}
	ActivateView(nullptr);
}

void FArcRmlUIThread::InstallAsyncResults()
{
#if !UE_BUILD_SHIPPING
	ActivateView(nullptr);
	Rml::ArcPerf::Sink& Sink = AsyncInstallPerf->Sink;
	Sink.frame.Reset();
	Rml::ArcPerf::SetSink(&Sink, ArcRmlUIPerf::GetLevel());
#endif
	const int32 Installed = Rml::ProcessAsyncResults();
#if !UE_BUILD_SHIPPING
	Rml::ArcPerf::SetSink(nullptr, 0);
	Sink.ResetLists();
#endif
	if (Installed == 0)
	{
		return;
	}

#if !UE_BUILD_SHIPPING
	PumpPerf.AsyncInstalls += Installed;
	PumpPerf.bActivity = true;
	AsyncInstallPerf->Breakdown.Add(Sink.frame);
#endif
	for (TPair<uint32, TUniquePtr<FViewState>>& Pair : Views)
	{
		Pair.Value->bDirty = true;
	}
}

void FArcRmlUIThread::UpdateViews()
{
	const double Now = FPlatformTime::Seconds();
	for (TPair<uint32, TUniquePtr<FViewState>>& Pair : Views)
	{
		FViewState& View = *Pair.Value;
		if (!View.Context || View.bPaused || View.Size.X <= 0 || View.Size.Y <= 0)
		{
			continue;
		}

		if (View.Channel->ReadCamera(View.Camera, View.CameraSerial))
		{
			ActivateView(&View);
			if (Rml::ReprojectWorldAnchors(*View.Context))
			{
				View.bDirty = true;
			}
			ActivateView(nullptr);
		}

		if (View.bDirty || Now >= View.NextUpdateTime)
		{
#if !UE_BUILD_SHIPPING
			++(View.bDirty ? View.Perf.DirtyFrames : View.Perf.TimedFrames);
			PumpPerf.bActivity = true;
#endif
			RenderView(View, Now);
		}
	}
}

void FArcRmlUIThread::RenderView(FViewState& View, double Now)
{
	ActivateView(&View);
	ARCRMLUI_PERF(View.RmlPerf.frame.Reset());
	ARCRMLUI_PERF(const uint64 UpdateStartCycles = FPlatformTime::Cycles64());
	View.Context->Update();
	ARCRMLUI_PERF(const double UpdateMs = ArcRmlUIPerf::SinceMs(UpdateStartCycles));
#if !UE_BUILD_SHIPPING
	const Rml::ArcPerf::FrameStats UpdateFrame = View.RmlPerf.frame;
	View.RmlPerf.frame.Reset();
#endif
	if (View.bRootCheckPending)
	{
		View.bRootCheckPending = false;
		View.RootTransitionListener->SendIfSettled(View.Document);
	}
	ARCRMLUI_PERF(const uint64 RenderStartCycles = FPlatformTime::Cycles64());
	View.Recorder->BeginFrame(View.Size);
	View.Context->Render();
	TUniquePtr<FArcRmlRenderFrame> Frame = View.Recorder->EndFrame();
#if !UE_BUILD_SHIPPING
	const double RenderMs = ArcRmlUIPerf::SinceMs(RenderStartCycles);
	ArcRmlUIThreadPrivate::FViewPerf& Perf = View.Perf;
	++Perf.Frames;
	Perf.Update.Add(UpdateMs);
	Perf.Render.Add(RenderMs);
	if (Frame.IsValid() && ArcRmlUIPerf::IsEnabled())
	{
		ArcRmlUIThreadPrivate::AccumulateFrame(Perf, *Frame);
	}
	if (ArcRmlUIPerf::IsEnabled())
	{
		View.UpdateBreakdown.Add(UpdateFrame);
		View.RenderBreakdown.Add(View.RmlPerf.frame);
	}
#endif

	const double Delay = View.Context->GetNextUpdateDelay();
	ARCRMLUI_PERF(Perf.LastNextUpdateDelay = Delay);
	View.NextUpdateTime = Delay < std::numeric_limits<double>::max()
		? Now + FMath::Max(Delay, ArcRmlUIThreadPrivate::GetMinFrameInterval())
		: std::numeric_limits<double>::max();
	View.bDirty = false;

	ENQUEUE_RENDER_COMMAND(ArcRmlSubmitFrame)(
		[Element = View.Element, SubmittedFrame = MoveTemp(Frame)](FRHICommandListImmediate& RHICmdList) mutable
		{
			Element->SubmitFrame_RenderThread(RHICmdList, MoveTemp(SubmittedFrame));
		});

	ARCRMLUI_PERF(const uint64 PublishStartCycles = FPlatformTime::Cycles64());
	PublishSnapshot(View);
#if !UE_BUILD_SHIPPING
	const double PublishMs = ArcRmlUIPerf::SinceMs(PublishStartCycles);
	Perf.Publish.Add(PublishMs);
	if (View.LoadPerf.bPending)
	{
		const ArcRmlUIThreadPrivate::FLoadPerf& LoadPerf = View.LoadPerf;
		View.LoadPerf.bPending = false;
		UE_LOGF(LogArcRmlUI, Verbose,
			"ArcRmlUI perf: view %u '%ls' load: read %.2f ms (%d bytes), load %.2f ms (%d elements), first update %.2f ms (%d elements), first render %.2f ms, "
			"first publish %.2f ms, open to first frame %.2f ms",
			View.Id, *View.DocumentPath, LoadPerf.ReadMs, LoadPerf.ReadBytes, LoadPerf.LoadMs, LoadPerf.LoadedElements, UpdateMs,
			ArcRmlUIThreadPrivate::CountElements(View.Document), RenderMs, PublishMs, (FPlatformTime::Seconds() - LoadPerf.OpenSeconds) * 1000.0);

		ArcRmlUIThreadPrivate::FRmlBreakdown FirstUpdate;
		FirstUpdate.Add(UpdateFrame);
		ArcRmlUIThreadPrivate::FRmlBreakdown FirstRender;
		FirstRender.Add(View.RmlPerf.frame);
		UE_LOGF(LogArcRmlUI, Verbose, "ArcRmlUI perf: view %u '%ls' first update breakdown: %ls", View.Id, *View.DocumentPath,
			*ArcRmlUIThreadPrivate::FormatBreakdown(FirstUpdate));
		UE_LOGF(LogArcRmlUI, Verbose, "ArcRmlUI perf: view %u '%ls' first update counts: %ls", View.Id, *View.DocumentPath,
			*ArcRmlUIThreadPrivate::FormatCounts(FirstUpdate));
		UE_LOGF(LogArcRmlUI, Verbose, "ArcRmlUI perf: view %u '%ls' first render breakdown: %ls", View.Id, *View.DocumentPath,
			*ArcRmlUIThreadPrivate::FormatBreakdown(FirstRender));
		UE_LOGF(LogArcRmlUI, Verbose, "ArcRmlUI perf: view %u '%ls' first render counts: %ls", View.Id, *View.DocumentPath,
			*ArcRmlUIThreadPrivate::FormatCounts(FirstRender));
	}
	if (!View.PendingLayoutDumpDir.IsEmpty() && View.Document)
	{
		WriteLayoutDump(View);
	}
#endif
	ActivateView(nullptr);
}

void FArcRmlUIThread::PublishSnapshot(FViewState& View)
{
	FArcRmlViewSnapshot Snapshot;
	Snapshot.Cursor = View.Cursor;

	Rml::Element* Focus = View.Context->GetFocusElement();
	Snapshot.bHasFocusedElement = Focus != nullptr && Focus != View.Context->GetRootElement() && Focus != Focus->GetOwnerDocument();
	ArcRmlUIThreadPrivate::CollectInteractiveRects(View.Context->GetRootElement(), Snapshot.InteractiveRects);

	if (Rml::TextInputContext* InputContext = TextInputHandler->GetActiveContext(View.Id))
	{
		FArcRmlTextFieldShadow& TextField = Snapshot.TextField;
		TextField.bActive = true;
		if (Rml::ElementFormControl* Control = rmlui_dynamic_cast<Rml::ElementFormControl*>(Focus))
		{
			TextField.Text = ArcRmlStrings::ToFString(Control->GetValue());
		}

		int SelectionStart = 0;
		int SelectionEnd = 0;
		InputContext->GetSelectionRange(SelectionStart, SelectionEnd);
		TextField.SelectionStart = SelectionStart;
		TextField.SelectionEnd = SelectionEnd;

		Rml::Rectanglef Box;
		if (InputContext->GetBoundingBox(Box))
		{
			TextField.Bounds = FIntRect(
				FMath::FloorToInt(Box.Left()), FMath::FloorToInt(Box.Top()),
				FMath::CeilToInt(Box.Right()), FMath::CeilToInt(Box.Bottom()));
		}
		TextField.Generation = View.AppliedImeGeneration;
	}

	View.Channel->PublishSnapshot(MoveTemp(Snapshot));
}

double FArcRmlUIThread::ComputeWaitSeconds() const
{
	const double Now = FPlatformTime::Seconds();
	double Earliest = std::numeric_limits<double>::max();
	for (const TPair<uint32, TUniquePtr<FViewState>>& Pair : Views)
	{
		const FViewState& View = *Pair.Value;
		if (!View.Context || View.bPaused || View.Size.X <= 0 || View.Size.Y <= 0)
		{
			continue;
		}
		if (View.bDirty)
		{
			return 0.0;
		}
		Earliest = FMath::Min(Earliest, View.NextUpdateTime);
	}

	if (Earliest == std::numeric_limits<double>::max())
	{
		return -1.0;
	}
	return FMath::Max(0.0, Earliest - Now);
}
