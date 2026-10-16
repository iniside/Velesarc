#pragma once

#include "../../../Include/RmlUi/Core/Box.h"
#include "../../../Include/RmlUi/Core/Types.h"
#include <stdint.h>

namespace Rml {

class ContainerBox;

/// Measurement memo of one LayoutEngine::FormatElement pass; a nested pass installs its own and restores the outer one.
class LayoutMeasureCache {
public:
	enum class Kind : uint8_t {
		ShrinkToFitWidth,
		FlexMaxContentWidth,
		FlexMinContentWidth,
		GridMaxContentWidth,
		GridMinContentWidth,
		GridItemContentWidth,
		Format,
		Count
	};

	struct Key {
		Key(Kind kind, const Element* element);
		Key& Add(float value);
		Key& Add(Vector2f value);
		Key& Add(const Box& box);

		static constexpr int max_values = 20;
		const Element* element;
		Kind kind;
		int num_values = 0;
		uint32_t values[max_values] = {};
	};

	struct Value {
		Vector2f size;
		Box box;
		float baseline = 0.f;
		bool has_baseline = false;
	};

	LayoutMeasureCache();
	~LayoutMeasureCache();
	LayoutMeasureCache(const LayoutMeasureCache&) = delete;
	LayoutMeasureCache& operator=(const LayoutMeasureCache&) = delete;

	/// Returns the memoized value for the key, or runs compute and memoizes its result. Runs compute without the memo
	/// outside a format pass and during the layout verify reference pass.
	template <typename ComputeFn>
	static Value Lookup(const Key& key, ComputeFn&& compute)
	{
		LayoutMeasureCache* cache = GetActive();
		if (!cache)
			return compute();

		if (const Value* hit = cache->Find(key))
		{
			OnHit(key.kind);
			return *hit;
		}

		OnMiss(key.kind);
		Value value = compute();
		cache->Store(key, value);
		return value;
	}

	/// Formats the element as an independent block-level formatting context under the parent container and returns its
	/// resulting box and last-line baseline, memoized by the element, its containing block and the override box.
	static Value Format(ContainerBox* parent_container, Element* element, const Box* override_initial_box);

	class SizeOnlyScope {
	public:
		SizeOnlyScope(const Element* element, bool size_only);
		~SizeOnlyScope();
		SizeOnlyScope(const SizeOnlyScope&) = delete;
		SizeOnlyScope& operator=(const SizeOnlyScope&) = delete;

	private:
		LayoutMeasureCache* cache;
		bool previous = false;
	};

	static bool IsSizeOnly();

private:
	struct KeyHash {
		size_t operator()(const Key& key) const;
	};
	struct KeyEqual {
		bool operator()(const Key& a, const Key& b) const;
	};
	using Map = robin_hood::unordered_flat_map<Key, Value, KeyHash, KeyEqual>;

	static LayoutMeasureCache* GetActive();
	static bool HasAutoOverflow(const Element* element);
	static void OnHit(Kind kind);
	static void OnMiss(Kind kind);

	const Value* Find(const Key& key) const;
	void Store(const Key& key, const Value& value);

	static LayoutMeasureCache* current;

	LayoutMeasureCache* previous;
	Map entries;
	bool size_only = false;
};

} // namespace Rml
