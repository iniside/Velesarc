#include "ArcPerfInternal.h"

#ifdef RMLUI_ARC_PERF

	#include "../../Include/RmlUi/Core/Element.h"
	#include "../../Include/RmlUi/Core/ElementDocument.h"
	#include "../../Include/RmlUi/Core/ElementText.h"
	#include "../../Include/RmlUi/Core/StringUtilities.h"
	#include "../../Include/RmlUi/Core/StyleSheet.h"
	#include "../../Include/RmlUi/Core/StyleSheetSpecification.h"
	#include "ElementAnimation.h"
	#include "ElementMeta.h"
	#include "StyleSheetNode.h"
	#include <algorithm>
	#include <chrono>

namespace Rml {

struct ArcPerfAccess {
	static void CollectAnimations(Element* element, Vector<ArcPerf::AnimationInfo>& out_animations)
	{
		for (const ElementAnimation& animation : element->animations)
		{
			ArcPerf::AnimationInfo info;
			info.element = element->GetObserverPtr();
			info.property = StyleSheetSpecification::GetPropertyName(animation.GetPropertyId());
			switch (animation.GetOrigin())
			{
			case ElementAnimationOrigin::User: info.origin = "user"; break;
			case ElementAnimationOrigin::Animation: info.origin = "animation"; break;
			case ElementAnimationOrigin::Transition: info.origin = "transition"; break;
			}
			if (animation.IsHoldingEndValue())
				info.state = "holding-end-value";
			else if (animation.IsComplete())
				info.state = "complete";
			else
				info.state = "running";
			info.duration = animation.GetDuration();
			info.num_iterations = animation.num_iterations;
			info.current_iteration = animation.current_iteration;
			info.alternate = animation.alternate_direction;
			out_animations.push_back(std::move(info));
		}
	}

	static void CountNodes(const StyleSheetNode* node, ArcPerf::StyleSheetStats& stats)
	{
		stats.nodes += 1;
		if (!node->properties.Empty())
			stats.styled_nodes += 1;
		for (const UniquePtr<StyleSheetNode>& child : node->children)
			CountNodes(child.get(), stats);
	}

	static void GetStyleSheetStats(const StyleSheet* style_sheet, ArcPerf::StyleSheetStats& stats)
	{
		if (style_sheet->root)
			CountNodes(style_sheet->root.get(), stats);

		const StyleSheetIndex& index = style_sheet->styled_node_index;
		for (const auto& entry : index.ids)
			stats.indexed_by_id += int(entry.second.size());
		for (const auto& entry : index.classes)
			stats.indexed_by_class += int(entry.second.size());
		for (const auto& entry : index.tags)
			stats.indexed_by_tag += int(entry.second.size());
		stats.unindexed = int(index.other.size());
		stats.id_keys = int(index.ids.size());
		stats.class_keys = int(index.classes.size());
		stats.tag_keys = int(index.tags.size());
		stats.cached_definitions = int(style_sheet->node_cache.size());
		stats.keyframes = int(style_sheet->keyframes.size());
		stats.named_decorators = int(style_sheet->named_decorator_map.size());
	}

	static Vector2f GetScrollableOverflow(const Element* element) { return element->scrollable_overflow_rectangle; }
	static const OutOfFlowLayoutInputs& GetOutOfFlowLayoutInputs(const Element* element) { return element->out_of_flow_inputs; }

	static bool IsUpdateMarked(const Element* element) { return element->needs_update || element->child_needs_update; }

	static String DescribePendingUpdateState(const Element* element)
	{
		if (!element->HasPendingUpdateState())
			return String();

		StringList names;
		if (element->dirty_definition)
			names.push_back("definition");
		if (element->dirty_child_definitions)
			names.push_back("child definitions");
		if (element->meta->style.AnyPropertiesDirty())
			names.push_back("properties");
		if (element->dirty_animation)
			names.push_back("animation property");
		if (element->dirty_transition)
			names.push_back("transition property");
		if (std::any_of(element->animations.begin(), element->animations.end(),
				[](const ElementAnimation& animation) { return !animation.IsHoldingEndValue(); }))
			names.push_back("running animation");

		String pending;
		for (const String& name : names)
			pending += (pending.empty() ? "" : ", ") + name;
		return pending;
	}
};

namespace ArcPerf {

	Sink* active_sink = nullptr;
	int active_level = 0;
	LayoutVerifyLog* active_layout_verify = nullptr;

	namespace {
		using Clock = std::chrono::steady_clock;

		struct StackFrame {
			Bucket bucket;
			Sink* sink;
			Clock::time_point start;
			double child_ms;
		};

		constexpr int MaxDepth = 256;
		StackFrame scope_stack[MaxDepth];
		int scope_depth = 0;

		const char* const bucket_names[] = {
			"update.other",
			"render.other",
			"load.other",

			"update.scroll_controller",
			"update.hover_chain",
			"events.dispatch",
			"data.model_update",
			"data.view_update",
			"data.binding_create",
			"element.update",
			"element.on_update",
			"anim.setup",
			"anim.advance",
			"element.scroll",
			"style.update_properties",
			"style.definition_apply",
			"style.definition_match",
			"style.definition_create",
			"style.pseudo_elements",
			"style.compute",
			"style.var_resolve",
			"style.var_shorthands",
			"style.font_face",
			"style.font_face_create",
			"font.glyph_install",
			"style.on_property_change",
			"effects.instance",
			"effects.decorator_instance",
			"dom.append_child",
			"dom.remove_child",
			"layout.document",
			"layout.block",
			"layout.flex",
			"layout.grid",
			"layout.table",
			"layout.replaced",
			"layout.text_line",
			"layout.clamp_scroll",
			"layout.verify",
			"layout.position",
			"update.release_documents",

			"render.element",
			"render.offsets",
			"render.stacking_context",
			"render.transform",
			"render.effects_data",
			"render.filter_compile",
			"render.shader_compile",
			"render.background_border",
			"render.box_shadow",
			"render.box_shadow_texture",
			"render.on_render",
			"text.geometry",
			"text.font_effects",
			"text.shadows",
			"text.font_atlas",
			"svg.handle",
			"svg.rasterize",
			"svg.upload",
			"image.install",
			"render.image_upload",
			"render.interface_draw",
			"render.compile_geometry",

			"compiled.read",
			"load.xml_parse",
			"load.header",
			"load.template",
			"rcss.factory",
			"rcss.parse",
			"rcss.merge",
			"rcss.compile",
			"rcss.combine_sheets",
			"rcss.build_index",
			"rcss.stylesheet_change",
			"load.instance_element",
			"dom.set_attributes",
			"load.instance_text",
		};
		static_assert(sizeof(bucket_names) / sizeof(bucket_names[0]) == size_t(Bucket::Count), "Bucket names out of sync");

		const char* const counter_names[] = {
			"elements updated",
			"elements rendered",
			"style computes",
			"style first computes",
			"style share candidates",
			"style share tuples",
			"style share hits",
			"properties computed",
			"definition lookups",
			"definitions changed",
			"definitions created",
			"selector candidates",
			"selector candidates matched",
			"selector node tests",
			"var resolutions",
			"var shorthand expansions",
			"font face lookups",
			"font handle creations",
			"font glyph batches",
			"font glyphs installed",
			"property change calls",
			"properties changed",
			"layout forcing changes",
			"class changes",
			"pseudo-class changes",
			"layout dirty calls",
			"layout runs",
			"layout residual passes",
			"layout residual dirty elements",
			"layout boundary passes",
			"layout boundary formats",
			"layout full: root dirty",
			"layout full: boundary without inputs",
			"format calls",
			"format calls: final flex item loop",
			"format calls: final grid item loop",
			"format calls: final block flow",
			"format calls: final item loops nested in a measurement",
			"format calls: flex base-size measure",
			"format calls: flex cross measure",
			"format calls: shrink-to-fit probe",
			"format calls: grid item width",
			"format calls: grid item measure",
			"format calls: other (roots, table, absolute)",
			"formats inside flex base-size measures",
			"formats inside flex cross measures",
			"formats inside shrink-to-fit probes",
			"formats inside flex max-content",
			"formats inside flex min-content",
			"formats inside grid max-content",
			"formats inside grid min-content",
			"formats inside grid item width",
			"formats inside grid item measures",
			"measure shrink-to-fit hits",
			"measure shrink-to-fit misses",
			"measure flex max-content hits",
			"measure flex max-content misses",
			"measure flex min-content hits",
			"measure flex min-content misses",
			"measure grid max-content hits",
			"measure grid max-content misses",
			"measure grid min-content hits",
			"measure grid min-content misses",
			"measure grid item width hits",
			"measure grid item width misses",
			"measure format hits",
			"measure format misses",
			"boxes changed",
			"text lines",
			"animations advanced",
			"animation property sets",
			"animation restarts",
			"update requests",
			"events dispatched",
			"data model updates",
			"data variables dirty",
			"data views updated",
			"data views created",
			"data controllers created",
			"data expression seed misses",
			"effects instanced",
			"decorators instanced",
			"effects data reloads",
			"filters compiled",
			"shaders compiled",
			"background/border regens",
			"box-shadow lookups",
			"box-shadow cache misses",
			"text geometry regens",
			"font effects regens",
			"text shadow regens",
			"svg handle lookups",
			"svg documents parsed",
			"svg rasterizations",
			"textures font atlas",
			"textures svg",
			"textures box-shadow",
			"textures image",
			"textures other",
			"geometries made",
			"geometries compiled",
			"draw calls",
			"elements instanced",
			"text nodes instanced",
			"stylesheets parsed",
			"stylesheet cache hits",
			"stylesheet merges",
		};
		static_assert(sizeof(counter_names) / sizeof(counter_names[0]) == size_t(Counter::Count), "Counter names out of sync");

		const char* const element_list_names[] = {
			"style time (incl. definition, compute, property change)",
			"definition match time",
			"layout time (exclusive, per format call)",
			"layout dirtied by",
			"property changes",
			"class/pseudo-class changes",
			"animation advance time",
			"update requests (animations keep the context updating)",
			"text geometry regen",
			"background/border regen",
			"effects instance/data",
			"svg handle",
		};
		static_assert(sizeof(element_list_names) / sizeof(element_list_names[0]) == size_t(ElementList::Count), "Element list names out of sync");

		const char* const named_list_names[] = {
			"changed properties (* forces layout)",
			"var() resolutions by property",
			"dirty css variables",
			"dirty data variables",
			"data view updates by bound variables",
			"update request sources",
			"stylesheet parse by source",
			"stylesheet factory lookups",
			"instanced elements by tag",
			"data bindings created by type",
		};
		static_assert(sizeof(named_list_names) / sizeof(named_list_names[0]) == size_t(NamedList::Count), "Named list names out of sync");

		String DescribeOne(const Element* element)
		{
			if (const ElementText* text = rmlui_dynamic_cast<const ElementText*>(element))
			{
				String snippet = text->GetText().substr(0, 24);
				for (char& c : snippet)
				{
					if (c == '\n' || c == '\r' || c == '\t')
						c = ' ';
				}
				return "#text '" + snippet + (text->GetText().size() > 24 ? "...'" : "'");
			}
			return element->GetAddress(false, false);
		}

		void CollectAnimationsRecursive(Element* element, Vector<AnimationInfo>& out_animations)
		{
			ArcPerfAccess::CollectAnimations(element, out_animations);
			const int num_children = element->GetNumChildren(true);
			for (int i = 0; i < num_children; i++)
				CollectAnimationsRecursive(element->GetChild(i), out_animations);
		}

		struct PositionedBoxState {
			Box box;
			Vector2f offset;
		};

		struct TextLineState {
			String text;
			Vector2f position;
		};

		struct ElementLayoutState {
			ObserverPtr<Element> element;
			const Element* offset_parent_address = nullptr;
			ObserverPtr<Element> offset_parent;
			Box box;
			Vector<PositionedBoxState> additional_boxes;
			Vector2f relative_offset;
			Vector2f absolute_offset;
			float baseline = 0.f;
			Vector2f scrollable_overflow;
			bool is_text = false;
			Vector<TextLineState> text_lines;
		};

		const char* const box_edge_names[3][4] = {
			{"margin top", "margin right", "margin bottom", "margin left"},
			{"border top", "border right", "border bottom", "border left"},
			{"padding top", "padding right", "padding bottom", "padding left"},
		};

		void CaptureLayoutRecursive(Element* element, Vector<ElementLayoutState>& out_states)
		{
			out_states.emplace_back();
			ElementLayoutState& state = out_states.back();
			state.element = element->GetObserverPtr();
			if (Element* offset_parent = element->GetOffsetParent())
			{
				state.offset_parent_address = offset_parent;
				state.offset_parent = offset_parent->GetObserverPtr();
			}
			state.box = element->GetBox();
			const int num_boxes = element->GetNumBoxes();
			for (int i = 1; i < num_boxes; i++)
			{
				PositionedBoxState positioned;
				positioned.box = element->GetBox(i, positioned.offset);
				state.additional_boxes.push_back(positioned);
			}
			state.relative_offset = Vector2f(element->GetOffsetLeft(), element->GetOffsetTop());
			state.absolute_offset = element->GetAbsoluteOffset(BoxArea::Border);
			state.baseline = element->GetBaseline();
			state.scrollable_overflow = ArcPerfAccess::GetScrollableOverflow(element);
			if (const ElementText* text = rmlui_dynamic_cast<const ElementText*>(element))
			{
				state.is_text = true;
				for (const ElementText::Line& line : text->GetLines())
					state.text_lines.push_back(TextLineState{line.text, line.position});
			}

			const int num_children = element->GetNumChildren(true);
			for (int i = 0; i < num_children; i++)
				CaptureLayoutRecursive(element->GetChild(i), out_states);
		}

		bool SameFloat(float a, float b)
		{
			return a == b || (a != a && b != b);
		}

		bool SameVector(Vector2f a, Vector2f b)
		{
			return SameFloat(a.x, b.x) && SameFloat(a.y, b.y);
		}

		String FormatFloat(float value)
		{
			return CreateString("%.9g", value);
		}

		String FormatVector(Vector2f value)
		{
			return CreateString("(%.9g, %.9g)", value.x, value.y);
		}

		String DescribeOffsetParent(const ElementLayoutState& state)
		{
			if (!state.offset_parent_address)
				return "none";
			return DescribeElement(state.offset_parent.get(), 0);
		}

		class LayoutComparison {
		public:
			LayoutComparison(LayoutVerifyPass& pass, int max_mismatches) : pass(pass), max_mismatches(max_mismatches) {}

			void BeginElement(const Element* in_element)
			{
				element = in_element;
				element_mismatched = false;
			}

			void EndElement()
			{
				if (element_mismatched)
					pass.mismatched_elements += 1;
			}

			void Mismatch(const char* group, int index, const char* name, String optimized, String reference)
			{
				pass.mismatches += 1;
				element_mismatched = true;
				if ((int)pass.first_mismatches.size() >= max_mismatches)
					return;

				LayoutMismatch mismatch;
				mismatch.element = DescribeElement(element, 2);
				mismatch.field = (group ? CreateString("%s[%d] %s", group, index, name) : String(name));
				mismatch.optimized = std::move(optimized);
				mismatch.reference = std::move(reference);
				pass.first_mismatches.push_back(std::move(mismatch));
			}

			void CompareInt(const char* group, int index, const char* name, int optimized, int reference)
			{
				if (optimized != reference)
					Mismatch(group, index, name, CreateString("%d", optimized), CreateString("%d", reference));
			}

			void CompareFloat(const char* group, int index, const char* name, float optimized, float reference)
			{
				if (!SameFloat(optimized, reference))
					Mismatch(group, index, name, FormatFloat(optimized), FormatFloat(reference));
			}

			void CompareVector(const char* group, int index, const char* name, Vector2f optimized, Vector2f reference)
			{
				if (!SameVector(optimized, reference))
					Mismatch(group, index, name, FormatVector(optimized), FormatVector(reference));
			}

			void CompareBox(const char* group, int index, const Box& optimized, const Box& reference)
			{
				CompareVector(group, index, "content size", optimized.GetSize(), reference.GetSize());
				const BoxArea areas[] = {BoxArea::Margin, BoxArea::Border, BoxArea::Padding};
				const BoxEdge edges[] = {BoxEdge::Top, BoxEdge::Right, BoxEdge::Bottom, BoxEdge::Left};
				for (int area_index = 0; area_index < 3; area_index++)
				{
					for (int edge_index = 0; edge_index < 4; edge_index++)
					{
						CompareFloat(group, index, box_edge_names[area_index][edge_index], optimized.GetEdge(areas[area_index], edges[edge_index]),
							reference.GetEdge(areas[area_index], edges[edge_index]));
					}
				}
			}

			void CompareElement(const ElementLayoutState& optimized, const ElementLayoutState& reference)
			{
				if (optimized.offset_parent_address != reference.offset_parent_address)
					Mismatch(nullptr, 0, "offset parent", DescribeOffsetParent(optimized), DescribeOffsetParent(reference));

				CompareBox(nullptr, 0, optimized.box, reference.box);

				const int optimized_boxes = 1 + (int)optimized.additional_boxes.size();
				const int reference_boxes = 1 + (int)reference.additional_boxes.size();
				CompareInt(nullptr, 0, "boxes", optimized_boxes, reference_boxes);
				if (optimized_boxes == reference_boxes)
				{
					for (int i = 0; i < (int)optimized.additional_boxes.size(); i++)
					{
						CompareBox("box", i + 1, optimized.additional_boxes[i].box, reference.additional_boxes[i].box);
						CompareVector("box", i + 1, "offset", optimized.additional_boxes[i].offset, reference.additional_boxes[i].offset);
					}
				}

				CompareVector(nullptr, 0, "relative offset", optimized.relative_offset, reference.relative_offset);
				CompareVector(nullptr, 0, "absolute offset", optimized.absolute_offset, reference.absolute_offset);
				CompareFloat(nullptr, 0, "baseline", optimized.baseline, reference.baseline);
				CompareVector(nullptr, 0, "scrollable overflow", optimized.scrollable_overflow, reference.scrollable_overflow);

				if (optimized.is_text || reference.is_text)
				{
					const int optimized_lines = (int)optimized.text_lines.size();
					const int reference_lines = (int)reference.text_lines.size();
					CompareInt(nullptr, 0, "text lines", optimized_lines, reference_lines);
					if (optimized_lines == reference_lines)
					{
						for (int i = 0; i < optimized_lines; i++)
						{
							const TextLineState& optimized_line = optimized.text_lines[i];
							const TextLineState& reference_line = reference.text_lines[i];
							CompareVector("text line", i, "position", optimized_line.position, reference_line.position);
							if (optimized_line.text != reference_line.text)
								Mismatch("text line", i, "text", "'" + optimized_line.text + "'", "'" + reference_line.text + "'");
						}
					}
				}
			}

		private:
			LayoutVerifyPass& pass;
			int max_mismatches;
			const Element* element = nullptr;
			bool element_mismatched = false;
		};

		void CompareLayouts(const Vector<ElementLayoutState>& optimized, const Vector<ElementLayoutState>& reference, LayoutVerifyPass& pass,
			int max_mismatches)
		{
			UnorderedMap<const Element*, size_t> optimized_indices;
			optimized_indices.reserve(optimized.size());
			for (size_t i = 0; i < optimized.size(); i++)
			{
				if (const Element* element = optimized[i].element.get())
					optimized_indices[element] = i;
			}

			Vector<bool> matched(optimized.size(), false);
			LayoutComparison comparison(pass, max_mismatches);
			for (const ElementLayoutState& reference_state : reference)
			{
				comparison.BeginElement(reference_state.element.get());
				UnorderedMap<const Element*, size_t>::const_iterator it = optimized_indices.find(reference_state.element.get());
				if (it == optimized_indices.end())
				{
					comparison.Mismatch(nullptr, 0, "element", "absent", "present");
				}
				else
				{
					matched[it->second] = true;
					comparison.CompareElement(optimized[it->second], reference_state);
				}
				comparison.EndElement();
			}

			for (size_t i = 0; i < optimized.size(); i++)
			{
				if (matched[i])
					continue;
				comparison.BeginElement(optimized[i].element.get());
				comparison.Mismatch(nullptr, 0, "element", "present", "absent");
				comparison.EndElement();
			}
		}

		void MergeStat(ElementStat& target, const ElementStat& source)
		{
			target.ms += source.ms;
			target.count += source.count;
			for (const auto& detail : source.details)
				target.details[detail.first] += detail.second;
		}

		// A null key aggregates elements destroyed during the interval, so reused addresses never drop recorded time.
		ElementStat& FindElementStat(ElementList list, const Element* element)
		{
			UnorderedMap<const Element*, ElementStat>& map = active_sink->elements[size_t(list)];
			if (!element)
				return map[nullptr];

			auto it = map.find(element);
			if (it != map.end() && it->second.element.get() != element)
			{
				ElementStat stale = std::move(it->second);
				map.erase(it);
				MergeStat(map[nullptr], stale);
			}

			ElementStat& stat = map[element];
			if (!stat.element)
				stat.element = const_cast<Element*>(element)->GetObserverPtr();
			return stat;
		}
	} // namespace

	Scope::Scope(Bucket bucket, Element* in_element, ElementList in_list, bool active) :
		entered(active && active_sink && Enter(bucket)), list(in_list)
	{
		if (entered && in_element && Detailed())
		{
			element = in_element->GetObserverPtr();
			element_tracked = true;
		}
	}

	bool Scope::Enter(Bucket bucket)
	{
		if (scope_depth >= MaxDepth)
			return false;
		StackFrame& frame = scope_stack[scope_depth++];
		frame.bucket = bucket;
		frame.sink = active_sink;
		frame.child_ms = 0.0;
		active_sink->frame.bucket_calls[size_t(bucket)] += 1;
		frame.start = Clock::now();
		return true;
	}

	void Scope::Leave()
	{
		const Clock::time_point now = Clock::now();
		StackFrame& frame = scope_stack[--scope_depth];
		const double inclusive_ms = std::chrono::duration<double, std::milli>(now - frame.start).count();
		const double exclusive_ms = inclusive_ms - frame.child_ms;
		frame.sink->frame.bucket_ms[size_t(frame.bucket)] += exclusive_ms;
		if (scope_depth > 0)
			scope_stack[scope_depth - 1].child_ms += inclusive_ms;

		if (element_tracked && active_sink == frame.sink)
			RecordElement(list, element.get(), list == ElementList::LayoutTime ? exclusive_ms : inclusive_ms, 1, nullptr);
	}

	void RecordElement(ElementList list, const Element* element, double ms, int count, const String* detail)
	{
		if (!Detailed())
			return;
		ElementStat& stat = FindElementStat(list, element);
		stat.ms += ms;
		stat.count += count;
		if (detail)
			stat.details[*detail] += 1;
	}

	void RecordElementDetail(ElementList list, const Element* element, const String& detail)
	{
		RecordElement(list, element, 0.0, 1, &detail);
	}

	void RecordNamed(NamedList list, const String& name, double ms, int count)
	{
		if (!active_sink)
			return;
		NamedStat& stat = active_sink->named[size_t(list)][name];
		stat.ms += ms;
		stat.count += count;
	}

	const char* GetCurrentBucketName()
	{
		return scope_depth > 0 ? bucket_names[size_t(scope_stack[scope_depth - 1].bucket)] : "(outside rmlui call)";
	}

	Counter GetCurrentTextureCounter()
	{
		for (int i = scope_depth - 1; i >= 0; i--)
		{
			switch (scope_stack[i].bucket)
			{
			case Bucket::FontAtlas: return Counter::TexturesFontAtlas;
			case Bucket::SvgUpload: return Counter::TexturesSvg;
			case Bucket::BoxShadowTexture: return Counter::TexturesBoxShadow;
			case Bucket::ImageUpload: return Counter::TexturesImage;
			default: break;
			}
		}
		return Counter::TexturesOther;
	}

	FormatSite pending_format_site = FormatSite::None;
	int format_measure_depth = 0;

	namespace {
		bool IsMeasureSite(FormatSite site)
		{
			return site != FormatSite::None && site != FormatSite::FinalFlex && site != FormatSite::FinalGrid && site != FormatSite::FinalBlock;
		}

		bool TakesNextFormat(FormatSite site)
		{
			return site != FormatSite::FlexMaxContent && site != FormatSite::FlexMinContent && site != FormatSite::GridMaxContent &&
				site != FormatSite::GridMinContent;
		}

		Counter InclusiveCounter(FormatSite site)
		{
			switch (site)
			{
			case FormatSite::FlexMain: return Counter::FormatsInFlexMain;
			case FormatSite::FlexCross: return Counter::FormatsInFlexCross;
			case FormatSite::ShrinkToFit: return Counter::FormatsInShrinkToFit;
			case FormatSite::FlexMaxContent: return Counter::FormatsInFlexMaxContent;
			case FormatSite::FlexMinContent: return Counter::FormatsInFlexMinContent;
			case FormatSite::GridMaxContent: return Counter::FormatsInGridMaxContent;
			case FormatSite::GridMinContent: return Counter::FormatsInGridMinContent;
			case FormatSite::GridItemWidth: return Counter::FormatsInGridItemWidth;
			case FormatSite::GridMeasure: return Counter::FormatsInGridMeasure;
			default: return Counter::Count;
			}
		}

		Counter DirectCounter(FormatSite site)
		{
			const bool nested = format_measure_depth > 0;
			switch (site)
			{
			case FormatSite::FinalFlex: return nested ? Counter::FormatNestedFinal : Counter::FormatFinalFlex;
			case FormatSite::FinalGrid: return nested ? Counter::FormatNestedFinal : Counter::FormatFinalGrid;
			case FormatSite::FinalBlock: return nested ? Counter::FormatNestedFinal : Counter::FormatFinalBlock;
			case FormatSite::FlexMain: return Counter::FormatFlexMain;
			case FormatSite::FlexCross: return Counter::FormatFlexCross;
			case FormatSite::ShrinkToFit: return Counter::FormatShrinkToFit;
			case FormatSite::GridItemWidth: return Counter::FormatGridItemWidth;
			case FormatSite::GridMeasure: return Counter::FormatGridMeasure;
			default: return Counter::FormatOther;
			}
		}
	} // namespace

	void CountFormatCall()
	{
		if (!active_sink)
			return;
		Count(Counter::FormatCalls, 1);
		Count(DirectCounter(pending_format_site), 1);
		pending_format_site = FormatSite::None;
	}

	FormatSiteScope::FormatSiteScope(FormatSite in_site) : sink(active_sink), site(in_site)
	{
		if (!sink)
			return;
		pending_format_site = TakesNextFormat(site) ? site : FormatSite::None;
		start_format_calls = sink->frame.counters[size_t(Counter::FormatCalls)];
		if (IsMeasureSite(site))
			format_measure_depth += 1;
	}

	FormatSiteScope::~FormatSiteScope()
	{
		if (!sink)
			return;
		pending_format_site = FormatSite::None;
		if (IsMeasureSite(site))
			format_measure_depth -= 1;
		const Counter inclusive = InclusiveCounter(site);
		if (inclusive != Counter::Count && sink == active_sink)
			Count(inclusive, active_sink->frame.counters[size_t(Counter::FormatCalls)] - start_format_calls);
	}

	void SetSink(Sink* sink, int level)
	{
		active_sink = (level > 0 ? sink : nullptr);
		active_level = (active_sink ? level : 0);
	}

	void SetLayoutVerify(LayoutVerifyLog* log)
	{
		active_layout_verify = log;
	}

	void VerifyLayout(ElementDocument* document, bool layout_ran, const Function<int()>& count_dirty, const Function<void()>& reference_layout)
	{
		LayoutVerifyLog* log = active_layout_verify;
		RMLUI_ARC_PERF_SCOPE(LayoutVerify);
		SuspendRecording suspend;
		active_layout_verify = nullptr;

		const int dirty_before_reference = count_dirty();

		Vector<ElementLayoutState> optimized;
		CaptureLayoutRecursive(document, optimized);

		reference_layout();

		const int dirty_after_reference = count_dirty();

		Vector<ElementLayoutState> reference;
		CaptureLayoutRecursive(document, reference);

		log->passes.emplace_back();
		LayoutVerifyPass& pass = log->passes.back();
		pass.document = document->GetSourceURL();
		pass.layout_ran = layout_ran;
		pass.dirty_before_reference = dirty_before_reference;
		pass.dirty_after_reference = dirty_after_reference;
		pass.elements = (int)reference.size();
		CompareLayouts(optimized, reference, pass, log->max_mismatches);

		active_layout_verify = log;
	}

	static void VerifyUpdateStateRecursive(Element* element, bool parent_walked, LayoutVerifyPass& pass, int max_mismatches)
	{
		const bool walked = parent_walked && ArcPerfAccess::IsUpdateMarked(element);
		pass.elements += 1;

		if (!walked)
		{
			String pending = ArcPerfAccess::DescribePendingUpdateState(element);
			if (!pending.empty())
			{
				pass.mismatched_elements += 1;
				pass.mismatches += 1;
				if ((int)pass.first_mismatches.size() < max_mismatches)
				{
					LayoutMismatch mismatch;
					mismatch.element = DescribeElement(element, 2);
					mismatch.field = "pending update";
					mismatch.optimized = "skipped";
					mismatch.reference = std::move(pending);
					pass.first_mismatches.push_back(std::move(mismatch));
				}
			}
		}

		const int num_children = element->GetNumChildren(true);
		for (int i = 0; i < num_children; i++)
			VerifyUpdateStateRecursive(element->GetChild(i), walked, pass, max_mismatches);
	}

	void VerifyUpdateState(ElementDocument* document)
	{
		LayoutVerifyLog* log = active_layout_verify;
		RMLUI_ARC_PERF_SCOPE(LayoutVerify);
		SuspendRecording suspend;

		log->passes.emplace_back();
		LayoutVerifyPass& pass = log->passes.back();
		pass.document = document->GetSourceURL();
		pass.update_state = true;
		VerifyUpdateStateRecursive(document, true, pass, log->max_mismatches);
	}

	const char* GetBucketName(Bucket bucket)
	{
		return size_t(bucket) < size_t(Bucket::Count) ? bucket_names[size_t(bucket)] : "";
	}

	const char* GetCounterName(Counter counter)
	{
		return size_t(counter) < size_t(Counter::Count) ? counter_names[size_t(counter)] : "";
	}

	const char* GetElementListName(ElementList list)
	{
		return size_t(list) < size_t(ElementList::Count) ? element_list_names[size_t(list)] : "";
	}

	const char* GetNamedListName(NamedList list)
	{
		return size_t(list) < size_t(NamedList::Count) ? named_list_names[size_t(list)] : "";
	}

	String DescribeElement(const Element* element, int num_ancestors)
	{
		if (!element)
			return "(destroyed)";
		String result = DescribeOne(element);
		const Element* ancestor = element->GetParentNode();
		for (int i = 0; i < num_ancestors && ancestor; i++, ancestor = ancestor->GetParentNode())
		{
			result += " < ";
			result += DescribeOne(ancestor);
		}
		return result;
	}

	void CollectAnimations(Element* root, Vector<AnimationInfo>& out_animations)
	{
		if (root)
			CollectAnimationsRecursive(root, out_animations);
	}

	bool GetStyleSheetStats(const StyleSheet* style_sheet, StyleSheetStats& out_stats)
	{
		out_stats = StyleSheetStats();
		if (!style_sheet)
			return false;
		ArcPerfAccess::GetStyleSheetStats(style_sheet, out_stats);
		return true;
	}

	const OutOfFlowLayoutInputs* GetOutOfFlowLayoutInputs(const Element* element)
	{
		if (!element)
			return nullptr;
		const OutOfFlowLayoutInputs& inputs = ArcPerfAccess::GetOutOfFlowLayoutInputs(element);
		return inputs.placed ? &inputs : nullptr;
	}

} // namespace ArcPerf
} // namespace Rml

#endif
