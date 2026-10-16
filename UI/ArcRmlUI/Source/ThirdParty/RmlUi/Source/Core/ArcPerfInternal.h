#pragma once

#include "../../Include/RmlUi/Core/ArcPerf.h"

#ifdef RMLUI_ARC_PERF

	#include <chrono>

namespace Rml {

class ElementDocument;

namespace ArcPerf {

	extern Sink* active_sink;
	extern int active_level;
	extern LayoutVerifyLog* active_layout_verify;

	inline bool Enabled()
	{
		return active_sink != nullptr;
	}

	inline bool Detailed()
	{
		return active_sink != nullptr && active_level >= 2;
	}

	inline void Count(Counter counter, int64_t amount)
	{
		if (active_sink)
			active_sink->frame.counters[size_t(counter)] += amount;
	}

	class Scope {
	public:
		explicit Scope(Bucket bucket, bool active = true) : entered(active && active_sink && Enter(bucket)) {}
		Scope(Bucket bucket, Element* element, ElementList list, bool active = true);
		~Scope()
		{
			if (entered)
				Leave();
		}
		Scope(const Scope&) = delete;
		Scope& operator=(const Scope&) = delete;

	private:
		static bool Enter(Bucket bucket);
		void Leave();

		bool entered = false;
		bool element_tracked = false;
		ObserverPtr<Element> element;
		ElementList list = ElementList::Count;
	};

	enum class FormatSite : uint8_t {
		None,
		FinalFlex,
		FinalGrid,
		FinalBlock,
		FlexMain,
		FlexCross,
		ShrinkToFit,
		FlexMaxContent,
		FlexMinContent,
		GridMaxContent,
		GridMinContent,
		GridItemWidth,
		GridMeasure,
	};

	extern FormatSite pending_format_site;
	extern int format_measure_depth;

	void CountFormatCall();

	// Tags the next FormatIndependent call made inside it with the site, and counts every format call made inside it for measurement sites.
	class FormatSiteScope {
	public:
		explicit FormatSiteScope(FormatSite site);
		~FormatSiteScope();
		FormatSiteScope(const FormatSiteScope&) = delete;
		FormatSiteScope& operator=(const FormatSiteScope&) = delete;

	private:
		Sink* sink = nullptr;
		FormatSite site = FormatSite::None;
		int64_t start_format_calls = 0;
	};

	class Stopwatch {
	public:
		explicit Stopwatch(bool active)
		{
			if (active)
				start = std::chrono::steady_clock::now();
		}
		double ElapsedMs() const { return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count(); }

	private:
		std::chrono::steady_clock::time_point start;
	};

	void RecordNamed(NamedList list, const String& name, double ms, int count);

	class NamedTimer {
	public:
		NamedTimer(NamedList list, const String& name) : active(Detailed()), stopwatch(active), list(list)
		{
			if (active)
				this->name = name;
		}
		~NamedTimer()
		{
			if (active)
				RecordNamed(list, name, stopwatch.ElapsedMs(), 1);
		}
		NamedTimer(const NamedTimer&) = delete;
		NamedTimer& operator=(const NamedTimer&) = delete;

	private:
		bool active;
		Stopwatch stopwatch;
		NamedList list;
		String name;
	};

	class SuspendRecording {
	public:
		SuspendRecording() : sink(active_sink), level(active_level)
		{
			active_sink = nullptr;
			active_level = 0;
		}
		~SuspendRecording()
		{
			active_sink = sink;
			active_level = level;
		}
		SuspendRecording(const SuspendRecording&) = delete;
		SuspendRecording& operator=(const SuspendRecording&) = delete;

	private:
		Sink* sink;
		int level;
	};

	// count_dirty returns the document's layout dirty marks: dirty elements, recorded boundaries and the layout flag.
	void VerifyLayout(ElementDocument* document, bool layout_ran, const Function<int()>& count_dirty, const Function<void()>& reference_layout);
	void VerifyUpdateState(ElementDocument* document);

	void RecordElement(ElementList list, const Element* element, double ms, int count, const String* detail);
	void RecordElementDetail(ElementList list, const Element* element, const String& detail);
	const char* GetCurrentBucketName();
	Counter GetCurrentTextureCounter();

} // namespace ArcPerf
} // namespace Rml

	#define RMLUI_ARC_PERF_CONCAT_INNER(a, b) a##b
	#define RMLUI_ARC_PERF_CONCAT(a, b) RMLUI_ARC_PERF_CONCAT_INNER(a, b)
	#define RMLUI_ARC_PERF_SCOPE(bucket) ::Rml::ArcPerf::Scope RMLUI_ARC_PERF_CONCAT(arc_perf_scope_, __LINE__)(::Rml::ArcPerf::Bucket::bucket)
	#define RMLUI_ARC_PERF_ELEMENT_SCOPE(bucket, element, list) \
		::Rml::ArcPerf::Scope RMLUI_ARC_PERF_CONCAT(arc_perf_scope_, __LINE__)(::Rml::ArcPerf::Bucket::bucket, element, ::Rml::ArcPerf::ElementList::list)
	#define RMLUI_ARC_PERF_SCOPE_IF(condition, bucket) \
		::Rml::ArcPerf::Scope RMLUI_ARC_PERF_CONCAT(arc_perf_scope_, __LINE__)(::Rml::ArcPerf::Bucket::bucket, (condition))
	#define RMLUI_ARC_PERF_ELEMENT_SCOPE_IF(condition, bucket, element, list) \
		::Rml::ArcPerf::Scope RMLUI_ARC_PERF_CONCAT(arc_perf_scope_, __LINE__)(::Rml::ArcPerf::Bucket::bucket, element, ::Rml::ArcPerf::ElementList::list, (condition))
	#define RMLUI_ARC_PERF_COUNT(counter, amount) ::Rml::ArcPerf::Count(::Rml::ArcPerf::Counter::counter, amount)
	#define RMLUI_ARC_PERF_FORMAT_SITE(site) 		::Rml::ArcPerf::FormatSiteScope RMLUI_ARC_PERF_CONCAT(arc_perf_format_site_, __LINE__)(::Rml::ArcPerf::FormatSite::site)
	#define RMLUI_ARC_PERF_CODE(...) __VA_ARGS__

#else

	#define RMLUI_ARC_PERF_SCOPE(bucket)
	#define RMLUI_ARC_PERF_ELEMENT_SCOPE(bucket, element, list)
	#define RMLUI_ARC_PERF_SCOPE_IF(condition, bucket)
	#define RMLUI_ARC_PERF_ELEMENT_SCOPE_IF(condition, bucket, element, list)
	#define RMLUI_ARC_PERF_COUNT(counter, amount)
	#define RMLUI_ARC_PERF_FORMAT_SITE(site)
	#define RMLUI_ARC_PERF_CODE(...)

#endif
