#include "LayoutMeasureCache.h"
#include "../ArcPerfInternal.h"
#include "../../../Include/RmlUi/Core/ComputedValues.h"
#include "../../../Include/RmlUi/Core/Element.h"
#include "../../../Include/RmlUi/Core/Log.h"
#include "FormattingContext.h"
#include "LayoutBox.h"
#include "LayoutDetails.h"
#include "LayoutEngine.h"
#include <stdlib.h>
#include <string.h>

namespace Rml {

LayoutMeasureCache* LayoutMeasureCache::current = nullptr;

namespace LayoutMeasureCacheDetail {
	static uint64_t Mix(uint64_t hash, uint64_t value)
	{
		return (hash ^ value) * 1099511628211ull;
	}

#ifdef RMLUI_ARC_PERF
	constexpr ArcPerf::Counter measure_hit_counters[] = {
		ArcPerf::Counter::MeasureShrinkToFitHits,
		ArcPerf::Counter::MeasureFlexMaxContentHits,
		ArcPerf::Counter::MeasureFlexMinContentHits,
		ArcPerf::Counter::MeasureGridMaxContentHits,
		ArcPerf::Counter::MeasureGridMinContentHits,
		ArcPerf::Counter::MeasureGridItemWidthHits,
		ArcPerf::Counter::MeasureFormatHits,
	};
	constexpr ArcPerf::Counter measure_miss_counters[] = {
		ArcPerf::Counter::MeasureShrinkToFitMisses,
		ArcPerf::Counter::MeasureFlexMaxContentMisses,
		ArcPerf::Counter::MeasureFlexMinContentMisses,
		ArcPerf::Counter::MeasureGridMaxContentMisses,
		ArcPerf::Counter::MeasureGridMinContentMisses,
		ArcPerf::Counter::MeasureGridItemWidthMisses,
		ArcPerf::Counter::MeasureFormatMisses,
	};
	static_assert(sizeof(measure_hit_counters) / sizeof(measure_hit_counters[0]) == size_t(LayoutMeasureCache::Kind::Count), "Hit counters out of sync");
	static_assert(sizeof(measure_miss_counters) / sizeof(measure_miss_counters[0]) == size_t(LayoutMeasureCache::Kind::Count),
		"Miss counters out of sync");
#endif
} // namespace LayoutMeasureCacheDetail

LayoutMeasureCache::Key::Key(Kind kind, const Element* element) : element(element), kind(kind) {}

LayoutMeasureCache::Key& LayoutMeasureCache::Key::Add(float value)
{
	RMLUI_ASSERT(num_values < max_values);
	memcpy(&values[num_values], &value, sizeof(value));
	num_values += 1;
	return *this;
}

LayoutMeasureCache::Key& LayoutMeasureCache::Key::Add(Vector2f value)
{
	return Add(value.x).Add(value.y);
}

LayoutMeasureCache::Key& LayoutMeasureCache::Key::Add(const Box& box)
{
	Add(box.GetSize());
	for (const BoxArea area : {BoxArea::Margin, BoxArea::Border, BoxArea::Padding})
	{
		for (const BoxEdge edge : {BoxEdge::Top, BoxEdge::Right, BoxEdge::Bottom, BoxEdge::Left})
			Add(box.GetEdge(area, edge));
	}
	return *this;
}

size_t LayoutMeasureCache::KeyHash::operator()(const Key& key) const
{
	uint64_t hash = 14695981039346656037ull;
	hash = LayoutMeasureCacheDetail::Mix(hash, uint64_t(reinterpret_cast<uintptr_t>(key.element)));
	hash = LayoutMeasureCacheDetail::Mix(hash, uint64_t(key.kind));
	for (int i = 0; i < key.num_values; i++)
		hash = LayoutMeasureCacheDetail::Mix(hash, key.values[i]);
	return size_t(hash);
}

bool LayoutMeasureCache::KeyEqual::operator()(const Key& a, const Key& b) const
{
	return a.element == b.element && a.kind == b.kind && a.num_values == b.num_values &&
		memcmp(a.values, b.values, sizeof(uint32_t) * size_t(a.num_values)) == 0;
}

LayoutMeasureCache::LayoutMeasureCache() : previous(current)
{
	current = this;
}

LayoutMeasureCache::~LayoutMeasureCache()
{
	RMLUI_ASSERT(current == this);
	current = previous;
}

LayoutMeasureCache* LayoutMeasureCache::GetActive()
{
#ifdef RMLUI_ARC_PERF
	if (LayoutEngine::IsReferencePass())
		return nullptr;
#endif
	return current;
}

void LayoutMeasureCache::OnHit(Kind kind)
{
#ifdef RMLUI_ARC_PERF
	if (LayoutEngine::IsReferencePass())
	{
		Log::Message(Log::LT_ERROR, "Layout measure memo hit during the layout verify reference pass.");
		abort();
	}
	ArcPerf::Count(LayoutMeasureCacheDetail::measure_hit_counters[size_t(kind)], 1);
#else
	(void)kind;
#endif
}

void LayoutMeasureCache::OnMiss(Kind kind)
{
#ifdef RMLUI_ARC_PERF
	ArcPerf::Count(LayoutMeasureCacheDetail::measure_miss_counters[size_t(kind)], 1);
#else
	(void)kind;
#endif
}

const LayoutMeasureCache::Value* LayoutMeasureCache::Find(const Key& key) const
{
	const Map::const_iterator it = entries.find(key);
	return it != entries.end() ? &it->second : nullptr;
}

void LayoutMeasureCache::Store(const Key& key, const Value& value)
{
	entries.insert_or_assign(key, value);
}

LayoutMeasureCache::SizeOnlyScope::SizeOnlyScope(const Element* element, bool in_size_only) : cache(GetActive())
{
	if (cache)
	{
		previous = cache->size_only;
		cache->size_only = in_size_only && !HasAutoOverflow(element);
	}
}

LayoutMeasureCache::SizeOnlyScope::~SizeOnlyScope()
{
	if (cache)
		cache->size_only = previous;
}

bool LayoutMeasureCache::IsSizeOnly()
{
	const LayoutMeasureCache* cache = GetActive();
	return cache && cache->size_only;
}

bool LayoutMeasureCache::HasAutoOverflow(const Element* element)
{
	const ComputedValues& computed = element->GetComputedValues();
	return computed.overflow_x() == Style::Overflow::Auto || computed.overflow_y() == Style::Overflow::Auto;
}

LayoutMeasureCache::Value LayoutMeasureCache::Format(ContainerBox* parent_container, Element* element, const Box* override_initial_box)
{
	const bool size_only = (GetActive() && !HasAutoOverflow(element));

	Key key(Kind::Format, element);
	key.Add(LayoutDetails::GetContainingBlock(parent_container, element->GetPosition()).size);
	key.Add(override_initial_box ? 1.f : 0.f);
	if (override_initial_box)
		key.Add(*override_initial_box);
	// A size-only format leaves the overflow and item placement of its subtree unwritten, so it never answers a full-mode lookup.
	key.Add(size_only ? 1.f : 0.f);

	return Lookup(key, [&]() -> Value {
		const SizeOnlyScope size_only_scope(element, size_only);
		UniquePtr<LayoutBox> layout_box = FormattingContext::FormatIndependent(parent_container, element, override_initial_box, FormattingContextType::Block);
		Value value;
		value.box = element->GetBox();
		value.has_baseline = (layout_box && layout_box->GetBaselineOfLastLine(value.baseline));
		return value;
	});
}

} // namespace Rml
