// CSS Grid formatting context. The grid algorithm is ported from Taffy (https://github.com/DioxusLabs/taffy) src/compute/grid/mod.rs and
// alignment.rs at 66b3364, without subgrid. Taffy is MIT licensed: Copyright (c) 2018 Visly Inc., Copyright (c) 2026 Taffy Authors.
#include "GridFormattingContext.h"
#include "../../../Include/RmlUi/Core/ComputedValues.h"
#include "../../../Include/RmlUi/Core/Element.h"
#include "../../../Include/RmlUi/Core/ElementScroll.h"
#include "../../../Include/RmlUi/Core/Profiling.h"
#include "ContainerBox.h"
#include "LayoutDetails.h"
#include "LayoutMeasureCache.h"
#include "../ArcPerfInternal.h"
#include <float.h>

namespace Rml {

static GridContentAlignment ToGridContentAlignment(Style::JustifyContent justify_content)
{
	switch (justify_content)
	{
	case Style::JustifyContent::FlexStart: return GridContentAlignment::Start;
	case Style::JustifyContent::FlexEnd: return GridContentAlignment::End;
	case Style::JustifyContent::Center: return GridContentAlignment::Center;
	case Style::JustifyContent::SpaceBetween: return GridContentAlignment::SpaceBetween;
	case Style::JustifyContent::SpaceAround: return GridContentAlignment::SpaceAround;
	case Style::JustifyContent::SpaceEvenly: return GridContentAlignment::SpaceEvenly;
	case Style::JustifyContent::Stretch:
	case Style::JustifyContent::Normal: return GridContentAlignment::Stretch;
	}
	return GridContentAlignment::Stretch;
}

static GridContentAlignment ToGridContentAlignment(Style::AlignContent align_content)
{
	switch (align_content)
	{
	case Style::AlignContent::FlexStart: return GridContentAlignment::Start;
	case Style::AlignContent::FlexEnd: return GridContentAlignment::End;
	case Style::AlignContent::Center: return GridContentAlignment::Center;
	case Style::AlignContent::SpaceBetween: return GridContentAlignment::SpaceBetween;
	case Style::AlignContent::SpaceAround: return GridContentAlignment::SpaceAround;
	case Style::AlignContent::SpaceEvenly: return GridContentAlignment::SpaceEvenly;
	case Style::AlignContent::Stretch: return GridContentAlignment::Stretch;
	}
	return GridContentAlignment::Stretch;
}

static Style::AlignSelf ResolveGridSelfAlignment(Style::AlignSelf self_alignment, Style::AlignItems container_alignment)
{
	if (self_alignment == Style::AlignSelf::Auto)
		return static_cast<Style::AlignSelf>(static_cast<int>(container_alignment) + 1);
	return self_alignment;
}

static Style::AlignSelf ResolveGridNormalAlignment(Style::AlignSelf alignment, bool normal_is_start)
{
	if (alignment != Style::AlignSelf::Normal)
		return alignment;
	return normal_is_start ? Style::AlignSelf::FlexStart : Style::AlignSelf::Stretch;
}

static float GridOutOfFlowStaticAlignment(Style::AlignSelf alignment)
{
	switch (alignment)
	{
	case Style::AlignSelf::FlexEnd: return 1.f;
	case Style::AlignSelf::Center: return 0.5f;
	default: return 0.f;
	}
}

static const GridPlacement* GetGridPlacement(const GridDefinitionPtr& definition)
{
	return (definition && definition->type == GridDefinition::Type::Placement) ? &definition->placement : nullptr;
}

static GridItemPlacementStyle ResolveGridItemPlacementStyle(const GridNamedLineResolver& name_resolver, const ComputedValues& computed,
	int explicit_column_count, int explicit_row_count)
{
	const GridDefinitionPtr column_start = computed.grid_column_start();
	const GridDefinitionPtr column_end = computed.grid_column_end();
	const GridDefinitionPtr row_start = computed.grid_row_start();
	const GridDefinitionPtr row_end = computed.grid_row_end();

	GridItemPlacementStyle placement;
	placement.column = name_resolver.ResolveColumnNames(GetGridPlacement(column_start), GetGridPlacement(column_end)).IntoOriginZero(explicit_column_count);
	placement.row = name_resolver.ResolveRowNames(GetGridPlacement(row_start), GetGridPlacement(row_end)).IntoOriginZero(explicit_row_count);
	return placement;
}

static float GridLineAsStartEdge(const Vector<GridTrack>& tracks, GridTrackCounts counts, std::optional<int> line, float fallback)
{
	int index = 0;
	if (!line || !counts.TryOzLineToTrackVecIndex(*line, index))
		return fallback;
	return (size_t(index + 1) < tracks.size() ? tracks[size_t(index + 1)] : tracks[size_t(index)]).offset;
}

static float GridLineAsEndEdge(const Vector<GridTrack>& tracks, GridTrackCounts counts, std::optional<int> line, float fallback)
{
	int index = 0;
	if (!line || !counts.TryOzLineToTrackVecIndex(*line, index))
		return fallback;
	if (index == 0)
		return (tracks.size() > 1 ? tracks[1] : tracks[0]).offset;
	return tracks[size_t(index)].offset;
}

struct GridOutOfFlowChild {
	Element* element;
	ContainerBox* unclaimed_containing_block;
	GridItemPlacementStyle placement;
	Vector2f static_alignment;
};

static GridOptional GridOptionalFromSize(float size)
{
	return size >= 0.f ? GridOptional(size) : GridOptional();
}

static float SumGridTrackBaseSizes(const Vector<GridTrack>& tracks)
{
	float sum = 0.f;
	for (const GridTrack& track : tracks)
		sum += track.base_size;
	return sum;
}

static float ClampGridContainerSize(float size, GridOptional min_size, GridOptional max_size)
{
	if (max_size)
		size = Math::Min(size, *max_size);
	if (min_size)
		size = Math::Max(size, *min_size);
	return Math::Max(size, 0.f);
}

UniquePtr<LayoutBox> GridFormattingContext::Format(ContainerBox* parent_container, Element* element, const Box* override_initial_box)
{
	RMLUI_ZoneScopedC(0xAF4FAF);
	UniquePtr<GridContainer> grid_container_box = MakeUnique<GridContainer>(element, parent_container);

	ElementScroll* element_scroll = element->GetElementScroll();
	const ComputedValues& computed = element->GetComputedValues();

	const Vector2f containing_block = LayoutDetails::GetContainingBlock(parent_container, element->GetPosition()).size;
	RMLUI_ASSERT(containing_block.x >= 0.f);

	Box& box = grid_container_box->GetBox();
	if (override_initial_box)
		box = *override_initial_box;
	else
		LayoutDetails::BuildBox(box, containing_block, element, BuildBoxMode::Block);

	if (UseShrinkToFitProbeBox(parent_container, element, override_initial_box))
		return grid_container_box;

	grid_container_box->ResetScrollbars(box);

	GridFormattingContext context;
	context.grid_container_box = grid_container_box.get();
	context.element_grid = element;
	context.size_only = UseSizeOnlyFormat(element);

	LayoutDetails::GetMinMaxWidth(context.grid_min_size.x, context.grid_max_size.x, computed, box, containing_block.x);
	LayoutDetails::GetMinMaxHeight(context.grid_min_size.y, context.grid_max_size.y, computed, box, containing_block.y);

	const Vector2f box_content_size = box.GetSize();
	const bool auto_width = (box_content_size.x < 0.0f);
	const bool auto_height = (box_content_size.y < 0.0f);

	context.grid_content_offset = box.GetPosition();

	for (int layout_iteration = 0; layout_iteration < 3; layout_iteration++)
	{
		const Vector2f scrollbar_size = {
			element_scroll->GetScrollbarSize(ElementScroll::VERTICAL),
			element_scroll->GetScrollbarSize(ElementScroll::HORIZONTAL),
		};

		context.grid_available_content_size = Math::Max(box_content_size - scrollbar_size, Vector2f(0.f));
		context.grid_content_containing_block = context.grid_available_content_size;

		if (auto_width)
		{
			context.grid_available_content_size.x = -1.f;
			context.grid_content_containing_block.x = containing_block.x;
		}
		if (auto_height)
		{
			context.grid_available_content_size.y = -1.f;
			context.grid_content_containing_block.y = containing_block.y;
		}

		Vector2f grid_resulting_content_size, visible_overflow_size, scrollable_content_size;
		float grid_baseline = 0.f;
		context.Format(grid_resulting_content_size, visible_overflow_size, scrollable_content_size, grid_baseline);

		if (auto_height && !grid_container_box->CatchOverflow(visible_overflow_size, scrollable_content_size, box, context.grid_max_size.y))
			continue;

		Vector2f formatted_content_size = box_content_size;
		if (auto_width)
			formatted_content_size.x = grid_resulting_content_size.x + scrollbar_size.x;
		if (auto_height)
			formatted_content_size.y = grid_resulting_content_size.y + scrollbar_size.y;

		Box sized_box = box;
		sized_box.SetContent(formatted_content_size);

		const float element_baseline =
			sized_box.GetSizeAcross(BoxDirection::Vertical, BoxArea::Border) + sized_box.GetEdge(BoxArea::Margin, BoxEdge::Bottom) - grid_baseline;

		if (grid_container_box->Close(visible_overflow_size, scrollable_content_size, sized_box, element_baseline))
			break;
	}

	return grid_container_box;
}

float GridFormattingContext::GetMaxContentWidth(Element* element)
{
	return LayoutMeasureCache::Lookup(LayoutMeasureCache::Key(LayoutMeasureCache::Kind::GridMaxContentWidth, element), [&]() -> LayoutMeasureCache::Value {
		LayoutMeasureCache::Value value;
		value.size.x = ComputeMaxContentWidth(element);
		return value;
	}).size.x;
}

float GridFormattingContext::ComputeMaxContentWidth(Element* element)
{
	RMLUI_ARC_PERF_FORMAT_SITE(GridMaxContent);
	return ComputeIntrinsicWidth(element, GridAvailableSpace::MaxContent());
}

float GridFormattingContext::GetMinContentWidth(Element* element)
{
	return LayoutMeasureCache::Lookup(LayoutMeasureCache::Key(LayoutMeasureCache::Kind::GridMinContentWidth, element), [&]() -> LayoutMeasureCache::Value {
		LayoutMeasureCache::Value value;
		value.size.x = ComputeMinContentWidth(element);
		return value;
	}).size.x;
}

float GridFormattingContext::ComputeMinContentWidth(Element* element)
{
	RMLUI_ARC_PERF_FORMAT_SITE(GridMinContent);
	return ComputeIntrinsicWidth(element, GridAvailableSpace::MinContent());
}

float GridFormattingContext::ComputeIntrinsicWidth(Element* element, GridAvailableSpace indefinite_width)
{
	const Vector2f infinity(10000.0f, 10000.0f);
	RootBox root(infinity);
	UniquePtr<GridContainer> grid_container_box = MakeUnique<GridContainer>(element, &root);

	GridFormattingContext context;
	context.grid_container_box = grid_container_box.get();
	context.element_grid = element;
	context.grid_available_content_size = Vector2f(-1.f, -1.f);
	context.grid_content_containing_block = infinity;
	context.grid_max_size = Vector2f(FLT_MAX, FLT_MAX);
	context.indefinite_width = indefinite_width;
	context.compute_width_only = true;

	Vector2f grid_resulting_content_size, visible_overflow_size, scrollable_content_size;
	float grid_baseline = 0.f;
	context.Format(grid_resulting_content_size, visible_overflow_size, scrollable_content_size, grid_baseline);
	return grid_resulting_content_size.x;
}

float GridFormattingContext::GetItemContentWidth(Element* element, const Box& box, Vector2f containing_block, GridAvailableSpace available_width)
{
	const LayoutMeasureCache::Key key = LayoutMeasureCache::Key(LayoutMeasureCache::Kind::GridItemContentWidth, element)
											.Add(box)
											.Add(containing_block)
											.Add(float(available_width.type))
											.Add(available_width.value);
	return LayoutMeasureCache::Lookup(key, [&]() -> LayoutMeasureCache::Value {
		LayoutMeasureCache::Value value;
		value.size.x = ComputeItemContentWidth(element, box, containing_block, available_width);
		return value;
	}).size.x;
}

float GridFormattingContext::ComputeItemContentWidth(Element* element, const Box& box, Vector2f containing_block, GridAvailableSpace available_width)
{
	RMLUI_ARC_PERF_FORMAT_SITE(GridItemWidth);
	const Style::Display display = element->GetDisplay();
	if (display == Style::Display::Table || display == Style::Display::InlineTable)
		return 0.f;

	const float edges_x = box.GetSizeAcross(BoxDirection::Horizontal, BoxArea::Border, BoxArea::Padding);
	float constraint_width = 0.f;
	switch (available_width.type)
	{
	case GridAvailableSpace::Type::MinContent: constraint_width = 0.f; break;
	case GridAvailableSpace::Type::MaxContent: constraint_width = Math::Max(containing_block.x, 0.f) + 10000.f; break;
	case GridAvailableSpace::Type::Definite: constraint_width = Math::Max(available_width.value - edges_x, 0.f); break;
	}

	float flex_or_grid_width = 0.f;
	if (LayoutDetails::GetFlexOrGridFitContentWidth(element, constraint_width, flex_or_grid_width))
		return flex_or_grid_width;

	Box format_box = box;
	format_box.SetContent(Vector2f(constraint_width, box.GetSize().y));

	RootBox root(Math::Max(containing_block, Vector2f(0.f)));
	UniquePtr<LayoutBox> layout_box = FormatShrinkToFitProbe(&root, element, &format_box);
	return layout_box ? layout_box->GetShrinkToFitWidth() : 0.f;
}

float GridFormattingContext::MeasureChildSize(const GridItem& item, GridAxis axis, GridOptionalSize known_dimensions, GridOptionalSize parent_size,
	GridAvailableSize available_space) const
{
	if (const GridOptional known = known_dimensions.Get(axis))
		return *known;

	Element* element = item.element;
	const Vector2f containing_block(parent_size.width.value_or(-1.f), parent_size.height.value_or(-1.f));

	Box box;
	LayoutDetails::BuildBox(box, containing_block, element, BuildBoxMode::UnalignedBlock);
	const float edges_x = box.GetSizeAcross(BoxDirection::Horizontal, BoxArea::Border, BoxArea::Padding);
	const float edges_y = box.GetSizeAcross(BoxDirection::Vertical, BoxArea::Border, BoxArea::Padding);

	Vector2f content_size = box.GetSize();
	if (known_dimensions.width)
		content_size.x = Math::Max(*known_dimensions.width - edges_x, 0.f);
	if (known_dimensions.height)
		content_size.y = Math::Max(*known_dimensions.height - edges_y, 0.f);
	box.SetContent(content_size);

	if (axis == GridAxis::Inline)
	{
		if (content_size.x >= 0.f)
			return content_size.x + edges_x;
		return GetItemContentWidth(element, box, containing_block, available_space.width) + edges_x;
	}

	if (content_size.y >= 0.f)
		return content_size.y + edges_y;

	if (content_size.x < 0.f)
	{
		content_size.x = GetItemContentWidth(element, box, containing_block, available_space.width);
		box.SetContent(content_size);
	}

	if (item.aspect_ratio > 0.f)
	{
		const Vector2f ratio_edges = (item.border_box_sizing ? Vector2f(edges_x, edges_y) : Vector2f(0.f));
		return LayoutDetails::ContentHeightFromWidth(content_size.x, item.aspect_ratio, ratio_edges) + edges_y;
	}

	RMLUI_ARC_PERF_FORMAT_SITE(GridMeasure);
	return LayoutMeasureCache::Format(grid_container_box, element, &box).box.GetSize(BoxArea::Border).y;
}

void GridFormattingContext::MeasureChildBaseline(const GridItem& item, GridOptionalSize parent_size, float& out_baseline, float& out_height) const
{
	Element* element = item.element;
	const Vector2f containing_block(parent_size.width.value_or(-1.f), parent_size.height.value_or(-1.f));

	Box box;
	LayoutDetails::BuildBox(box, containing_block, element, BuildBoxMode::UnalignedBlock);
	if (box.GetSize().x < 0.f)
		box.SetContent(Vector2f(GetItemContentWidth(element, box, containing_block, GridAvailableSpace::MaxContent()), box.GetSize().y));

	RMLUI_ARC_PERF_FORMAT_SITE(GridMeasure);
	const LayoutMeasureCache::Value measured = LayoutMeasureCache::Format(grid_container_box, element, &box);
	out_height = measured.box.GetSize(BoxArea::Border).y;
	out_baseline = measured.has_baseline ? measured.baseline : out_height;
}

void GridFormattingContext::Format(Vector2f& grid_resulting_content_size, Vector2f& grid_visible_overflow_size, Vector2f& grid_scrollable_content_size,
	float& grid_baseline) const
{
	const ComputedValues& computed_grid = element_grid->GetComputedValues();

	GridOptionalSize inner_node_size;
	inner_node_size.width = GridOptionalFromSize(grid_available_content_size.x);
	inner_node_size.height = GridOptionalFromSize(grid_available_content_size.y);
	const GridOptionalSize known_node_size = inner_node_size;

	GridOptionalSize inner_min_size;
	inner_min_size.width = (grid_min_size.x > 0.f ? GridOptional(grid_min_size.x) : GridOptional());
	inner_min_size.height = (grid_min_size.y > 0.f ? GridOptional(grid_min_size.y) : GridOptional());
	GridOptionalSize inner_max_size;
	inner_max_size.width = (grid_max_size.x < FLT_MAX ? GridOptional(grid_max_size.x) : GridOptional());
	inner_max_size.height = (grid_max_size.y < FLT_MAX ? GridOptional(grid_max_size.y) : GridOptional());

	GridAvailableSize available_grid_space;
	available_grid_space.width = (inner_node_size.width ? GridAvailableSpace::Definite(*inner_node_size.width) : indefinite_width);
	available_grid_space.height = (inner_node_size.height ? GridAvailableSpace::Definite(*inner_node_size.height) : GridAvailableSpace::MaxContent());

	const GridContentAlignment justify_content = ToGridContentAlignment(computed_grid.justify_content());
	const GridContentAlignment align_content = ToGridContentAlignment(computed_grid.align_content());
	const Style::AlignItems align_items = computed_grid.align_items();
	const Style::AlignItems justify_items = computed_grid.justify_items();

	const GridDefinitionPtr template_columns_definition = computed_grid.grid_template_columns();
	const GridDefinitionPtr template_rows_definition = computed_grid.grid_template_rows();
	const GridDefinitionPtr template_areas_definition = computed_grid.grid_template_areas();
	const GridDefinitionPtr auto_columns_definition = computed_grid.grid_auto_columns();
	const GridDefinitionPtr auto_rows_definition = computed_grid.grid_auto_rows();

	const GridResolvedTrackList template_columns = ResolveGridTrackList(template_columns_definition.get(), element_grid);
	const GridResolvedTrackList template_rows = ResolveGridTrackList(template_rows_definition.get(), element_grid);
	const GridResolvedTrackList auto_columns = ResolveGridTrackList(auto_columns_definition.get(), element_grid);
	const GridResolvedTrackList auto_rows = ResolveGridTrackList(auto_rows_definition.get(), element_grid);
	const GridTemplateAreas* template_areas =
		(template_areas_definition && template_areas_definition->type == GridDefinition::Type::Areas && template_areas_definition->areas.row_count > 0)
		? &template_areas_definition->areas
		: nullptr;

	const GridSizingFunction column_gap = ResolveGridGap(computed_grid.column_gap());
	const GridSizingFunction row_gap = ResolveGridGap(computed_grid.row_gap());

	GridOptionalSize auto_fit_container_size;
	auto_fit_container_size.width = (inner_node_size.width ? inner_node_size.width : (inner_max_size.width ? inner_max_size.width : inner_min_size.width));
	auto_fit_container_size.height =
		(inner_node_size.height ? inner_node_size.height : (inner_max_size.height ? inner_max_size.height : inner_min_size.height));
	if (auto_fit_container_size.width)
		auto_fit_container_size.width = ClampGridContainerSize(*auto_fit_container_size.width, inner_min_size.width, inner_max_size.width);
	if (auto_fit_container_size.height)
		auto_fit_container_size.height = ClampGridContainerSize(*auto_fit_container_size.height, inner_min_size.height, inner_max_size.height);

	const bool column_repetitions_do_not_overflow = (inner_node_size.width || inner_max_size.width);
	const bool row_repetitions_do_not_overflow = (inner_node_size.height || inner_max_size.height);

	int column_auto_repetition_count = 0, grid_template_column_count = 0;
	ComputeGridExplicitSizeInAxis(template_columns, column_gap, auto_fit_container_size.width, column_repetitions_do_not_overflow,
		column_auto_repetition_count, grid_template_column_count);
	int row_auto_repetition_count = 0, grid_template_row_count = 0;
	ComputeGridExplicitSizeInAxis(template_rows, row_gap, auto_fit_container_size.height, row_repetitions_do_not_overflow, row_auto_repetition_count,
		grid_template_row_count);

	GridNamedLineResolver name_resolver(template_columns, template_rows, template_areas, column_auto_repetition_count, row_auto_repetition_count);

	const int explicit_column_count = Math::Min(Math::Max(grid_template_column_count, name_resolver.AreaColumnCount()), GridMaxTracks);
	const int explicit_row_count = Math::Min(Math::Max(grid_template_row_count, name_resolver.AreaRowCount()), GridMaxTracks);
	name_resolver.SetExplicitColumnCount(explicit_column_count);
	name_resolver.SetExplicitRowCount(explicit_row_count);

	Vector<GridItem> items;
	Vector<GridItemPlacementStyle> placements;
	Vector<GridOutOfFlowChild> out_of_flow_children;
	Vector<Element*> grid_children;
	LayoutDetails::CollectBoxChildren(element_grid, element_grid, grid_children);
	items.reserve(grid_children.size());
	placements.reserve(grid_children.size());

	for (int i = 0; i < (int)grid_children.size(); i++)
	{
		Element* element = grid_children[i];
		const ComputedValues& computed = element->GetComputedValues();

		if (computed.display() == Style::Display::None || LayoutDetails::SkipWhiteSpaceTextItem(element))
			continue;

		if (computed.position() == Style::Position::Absolute || computed.position() == Style::Position::Fixed)
		{
			const Vector2f static_alignment(GridOutOfFlowStaticAlignment(ResolveGridSelfAlignment(computed.justify_self(), justify_items)),
				GridOutOfFlowStaticAlignment(ResolveGridSelfAlignment(computed.align_self(), align_items)));
			if (grid_container_box->IsAbsolutePositioningContainingBlock())
			{
				out_of_flow_children.push_back(GridOutOfFlowChild{element, nullptr,
					ResolveGridItemPlacementStyle(name_resolver, computed, explicit_column_count, explicit_row_count), static_alignment});
			}
			else
			{
				out_of_flow_children.push_back(GridOutOfFlowChild{element,
					LayoutDetails::GetContainingBlock(grid_container_box, computed.position()).container, GridItemPlacementStyle(), static_alignment});
			}
			continue;
		}
		if (computed.position() == Style::Position::Relative)
			grid_container_box->AddRelativeElement(element);

		GridItem item;
		item.element = element;
		item.source_order = i;
		item.size[0] = computed.width();
		item.size[1] = computed.height();
		item.min_size[0] = computed.min_width();
		item.min_size[1] = computed.min_height();
		item.min_size_auto[0] = (element->GetLocalProperty(PropertyId::MinWidth) == nullptr);
		item.min_size_auto[1] = (element->GetLocalProperty(PropertyId::MinHeight) == nullptr);
		item.max_size[0] = computed.max_width();
		item.max_size[1] = computed.max_height();
		item.padding[0] = computed.padding_top();
		item.padding[1] = computed.padding_right();
		item.padding[2] = computed.padding_bottom();
		item.padding[3] = computed.padding_left();
		item.border[0] = computed.border_top_width();
		item.border[1] = computed.border_right_width();
		item.border[2] = computed.border_bottom_width();
		item.border[3] = computed.border_left_width();
		item.margin[0] = computed.margin_top();
		item.margin[1] = computed.margin_right();
		item.margin[2] = computed.margin_bottom();
		item.margin[3] = computed.margin_left();
		item.border_box_sizing = (computed.box_sizing() == Style::BoxSizing::BorderBox);
		Vector2f intrinsic_size(-1.f, -1.f);
		float intrinsic_ratio = -1.f;
		const bool replaced_element = element->GetIntrinsicDimensions(intrinsic_size, intrinsic_ratio);
		item.aspect_ratio = (replaced_element ? 0.f : LayoutDetails::GetAspectRatio(computed, false, -1.f));
		item.overflow[0] = computed.overflow_x();
		item.overflow[1] = computed.overflow_y();
		item.align_self =
			ResolveGridNormalAlignment(ResolveGridSelfAlignment(computed.align_self(), align_items), replaced_element || item.aspect_ratio > 0.f);
		item.justify_self = ResolveGridNormalAlignment(ResolveGridSelfAlignment(computed.justify_self(), justify_items), replaced_element);

		items.push_back(std::move(item));
		placements.push_back(ResolveGridItemPlacementStyle(name_resolver, computed, explicit_column_count, explicit_row_count));
	}

	GridTrackCounts estimated_column_counts, estimated_row_counts;
	ComputeGridSizeEstimate(explicit_column_count, explicit_row_count, placements, estimated_column_counts, estimated_row_counts);

	GridCellOccupancyMatrix occupancy(estimated_column_counts, estimated_row_counts);
	PlaceGridItems(occupancy, items, placements, computed_grid.grid_auto_flow());

	const GridTrackCounts final_column_counts = occupancy.TrackCounts(GridAbsoluteAxis::Horizontal);
	const GridTrackCounts final_row_counts = occupancy.TrackCounts(GridAbsoluteAxis::Vertical);

	Vector<GridTrack> columns;
	Vector<GridTrack> rows;
	InitializeGridTracks(columns, final_column_counts, template_columns, auto_columns, column_gap, column_auto_repetition_count, occupancy,
		GridAbsoluteAxis::Horizontal);
	InitializeGridTracks(rows, final_row_counts, template_rows, auto_rows, row_gap, row_auto_repetition_count, occupancy, GridAbsoluteAxis::Vertical);

	ResolveGridItemTrackIndexes(items, final_column_counts, final_row_counts);
	DetermineIfGridItemsCrossFlexibleOrIntrinsicTracks(items, columns, rows);

	bool has_baseline_aligned_item = false;
	for (const GridItem& item : items)
		has_baseline_aligned_item |= item.ParticipatesInBaselineAlignment();

	RunGridTrackSizingAlgorithm(*this, GridAxis::Inline, inner_min_size.width, inner_max_size.width, justify_content, align_content,
		available_grid_space, inner_node_size, columns, rows, items, GridTrackSizeEstimate::MaxTrackSizingFunction, has_baseline_aligned_item);
	const float initial_column_sum = SumGridTrackBaseSizes(columns);
	if (!inner_node_size.width)
		inner_node_size.width = initial_column_sum;

	if (compute_width_only)
	{
		grid_resulting_content_size = Vector2f(ClampGridContainerSize(initial_column_sum, inner_min_size.width, inner_max_size.width), 0.f);
		return;
	}

	for (GridItem& item : items)
		item.grid_area_size_cache.reset();

	RunGridTrackSizingAlgorithm(*this, GridAxis::Block, inner_min_size.height, inner_max_size.height, align_content, justify_content,
		available_grid_space, inner_node_size, rows, columns, items, GridTrackSizeEstimate::BaseSize, false);
	const float initial_row_sum = SumGridTrackBaseSizes(rows);

	Vector2f container_content_box(known_node_size.width ? *known_node_size.width
														 : ClampGridContainerSize(initial_column_sum, inner_min_size.width, inner_max_size.width),
		known_node_size.height ? *known_node_size.height : ClampGridContainerSize(initial_row_sum, inner_min_size.height, inner_max_size.height));

	inner_node_size.width = container_content_box.x;
	inner_node_size.height = container_content_box.y;

	bool has_percentage_column = false;
	bool has_percentage_row = false;
	for (const GridTrack& track : columns)
		has_percentage_column |= track.UsesPercentage();
	for (const GridTrack& track : rows)
		has_percentage_row |= track.UsesPercentage();

	bool rerun_column_sizing = (!known_node_size.width && has_percentage_column);
	bool intrinsic_column_contribution_changed = false;
	if (!rerun_column_sizing)
	{
		for (GridItem& item : items)
		{
			if (!item.crosses_intrinsic_column)
				continue;
			const GridOptionalSize grid_area_size = GridItemAreaSize(item, GridAxis::Inline, columns, rows, inner_node_size, GridTrackSizeEstimate::BaseSize);
			const GridOptionalSize available_space = grid_area_size.With(GridAxis::Inline, GridOptional());
			const float new_min_content_contribution = GridItemMinContentContribution(item, GridAxis::Inline, *this, grid_area_size, available_space);

			intrinsic_column_contribution_changed |= (GridOptional(new_min_content_contribution) != item.min_content_contribution_cache.width);

			item.grid_area_size_cache = grid_area_size;
			item.min_content_contribution_cache.width = new_min_content_contribution;
			item.max_content_contribution_cache.width.reset();
			item.minimum_contribution_cache.width.reset();
		}
		rerun_column_sizing = intrinsic_column_contribution_changed;
	}
	else
	{
		for (GridItem& item : items)
		{
			item.grid_area_size_cache.reset();
			item.min_content_contribution_cache.width.reset();
			item.max_content_contribution_cache.width.reset();
			item.minimum_contribution_cache.width.reset();
		}
	}

	bool rerun_row_sizing = (!known_node_size.height && has_percentage_row);
	bool intrinsic_row_contribution_changed = false;
	if (rerun_row_sizing)
	{
		for (GridItem& item : items)
		{
			item.grid_area_size_cache.reset();
			item.min_content_contribution_cache.height.reset();
			item.max_content_contribution_cache.height.reset();
			item.minimum_contribution_cache.height.reset();
		}
	}

	if (rerun_column_sizing)
	{
		RunGridTrackSizingAlgorithm(*this, GridAxis::Inline, inner_min_size.width, inner_max_size.width, justify_content, align_content,
			available_grid_space, inner_node_size, columns, rows, items, GridTrackSizeEstimate::BaseSize, has_baseline_aligned_item);

		if (!rerun_row_sizing)
		{
			for (GridItem& item : items)
			{
				if (!item.crosses_intrinsic_row)
					continue;
				const GridOptionalSize grid_area_size = GridItemAreaSize(item, GridAxis::Block, rows, columns, inner_node_size, GridTrackSizeEstimate::BaseSize);
				const GridOptionalSize available_space = grid_area_size.With(GridAxis::Block, GridOptional());
				const float new_min_content_contribution = GridItemMinContentContribution(item, GridAxis::Block, *this, grid_area_size, available_space);

				intrinsic_row_contribution_changed |= (GridOptional(new_min_content_contribution) != item.min_content_contribution_cache.height);

				item.grid_area_size_cache = grid_area_size;
				item.min_content_contribution_cache.height = new_min_content_contribution;
				item.max_content_contribution_cache.height.reset();
				item.minimum_contribution_cache.height.reset();
			}
			rerun_row_sizing = intrinsic_row_contribution_changed;
		}
	}

	if (rerun_row_sizing)
	{
		RunGridTrackSizingAlgorithm(*this, GridAxis::Block, inner_min_size.height, inner_max_size.height, align_content, justify_content,
			available_grid_space, inner_node_size, rows, columns, items, GridTrackSizeEstimate::BaseSize, false);
	}

	if (intrinsic_column_contribution_changed && !has_percentage_column && !known_node_size.width)
		container_content_box.x = ClampGridContainerSize(SumGridTrackBaseSizes(columns), inner_min_size.width, inner_max_size.width);
	if (intrinsic_row_contribution_changed && !has_percentage_row && !known_node_size.height)
		container_content_box.y = ClampGridContainerSize(SumGridTrackBaseSizes(rows), inner_min_size.height, inner_max_size.height);

	if (size_only)
	{
		grid_resulting_content_size = container_content_box;
		return;
	}

	AlignGridTracks(container_content_box.x, columns, justify_content);
	AlignGridTracks(container_content_box.y, rows, align_content);

	const Box& grid_box = grid_container_box->GetBox();
	const float padding_left = grid_box.GetEdge(BoxArea::Padding, BoxEdge::Left);
	const float padding_top = grid_box.GetEdge(BoxArea::Padding, BoxEdge::Top);
	const float padding_right = grid_box.GetEdge(BoxArea::Padding, BoxEdge::Right);
	const float padding_bottom = grid_box.GetEdge(BoxArea::Padding, BoxEdge::Bottom);
	for (const GridOutOfFlowChild& child : out_of_flow_children)
	{
		if (child.unclaimed_containing_block)
		{
			child.unclaimed_containing_block->AddAbsoluteElement(child.element, grid_content_offset, element_grid, container_content_box,
				child.static_alignment);
			continue;
		}

		std::optional<int> column_start, column_end, row_start, row_end;
		child.placement.column.ResolveAbsolutelyPositionedGridLines(column_start, column_end);
		child.placement.row.ResolveAbsolutelyPositionedGridLines(row_start, row_end);

		const float area_left = GridLineAsStartEdge(columns, final_column_counts, column_start, -padding_left);
		const float area_right = GridLineAsEndEdge(columns, final_column_counts, column_end, container_content_box.x + padding_right);
		const float area_top = GridLineAsStartEdge(rows, final_row_counts, row_start, -padding_top);
		const float area_bottom = GridLineAsEndEdge(rows, final_row_counts, row_end, container_content_box.y + padding_bottom);

		grid_container_box->AddAbsoluteElementInArea(child.element, grid_content_offset + Vector2f(area_left, area_top),
			Vector2f(area_right - area_left, area_bottom - area_top), child.static_alignment);
	}

	for (GridItem& item : items)
	{
		const float area_top = rows[size_t(item.row_indexes.start + 1)].offset;
		const float area_bottom = rows[size_t(item.row_indexes.end)].offset;
		const float area_left = columns[size_t(item.column_indexes.start + 1)].offset;
		const float area_right = columns[size_t(item.column_indexes.end)].offset;
		const Vector2f area_size(Math::Max(area_right - area_left, 0.f), Math::Max(area_bottom - area_top, 0.f));

		Element* element = item.element;
		const ComputedValues& computed = element->GetComputedValues();

		Box box;
		LayoutDetails::BuildBox(box, area_size, element, BuildBoxMode::UnalignedBlock);
		const float edges_x = box.GetSizeAcross(BoxDirection::Horizontal, BoxArea::Border, BoxArea::Padding);
		const float edges_y = box.GetSizeAcross(BoxDirection::Vertical, BoxArea::Border, BoxArea::Padding);

		float min_width = 0.f, max_width = FLT_MAX, min_height = 0.f, max_height = FLT_MAX;
		LayoutDetails::GetMinMaxWidth(min_width, max_width, computed, box, area_size.x);
		LayoutDetails::GetMinMaxHeight(min_height, max_height, computed, box, area_size.y);
		max_width = Math::Max(max_width, min_width);
		max_height = Math::Max(max_height, min_height);

		auto margin_or_auto = [&box, &item](int edge, BoxEdge box_edge) -> GridOptional {
			if (item.margin[edge].type == Style::LengthPercentageAuto::Auto)
				return GridOptional();
			return box.GetEdge(BoxArea::Margin, box_edge);
		};
		const GridOptional margin_top = margin_or_auto(0, BoxEdge::Top);
		const GridOptional margin_right = margin_or_auto(1, BoxEdge::Right);
		const GridOptional margin_bottom = margin_or_auto(2, BoxEdge::Bottom);
		const GridOptional margin_left = margin_or_auto(3, BoxEdge::Left);

		const Vector2f area_minus_margins(Math::Max(area_size.x - margin_left.value_or(0.f) - margin_right.value_or(0.f), 0.f),
			Math::Max(area_size.y - margin_top.value_or(0.f) - margin_bottom.value_or(0.f) - item.baseline_shim, 0.f));

		float content_width = box.GetSize().x;
		if (content_width < 0.f && margin_left && margin_right && item.justify_self == Style::AlignSelf::Stretch)
			content_width = Math::Max(area_minus_margins.x - edges_x, 0.f);

		float content_height = box.GetSize().y;
		if (content_height < 0.f && margin_top && margin_bottom && item.align_self == Style::AlignSelf::Stretch)
			content_height = Math::Clamp(Math::Max(area_minus_margins.y - edges_y, 0.f), min_height, max_height);

		const Vector2f ratio_edges = (item.border_box_sizing ? Vector2f(edges_x, edges_y) : Vector2f(0.f));
		if (item.aspect_ratio > 0.f && content_width < 0.f && content_height >= 0.f)
			content_width = LayoutDetails::ContentWidthFromHeight(content_height, item.aspect_ratio, ratio_edges);
		if (content_width < 0.f)
			content_width = GetItemContentWidth(element, box, area_size, GridAvailableSpace::Definite(area_minus_margins.x));
		if (item.aspect_ratio > 0.f && content_height < 0.f)
		{
			if (min_height > 0.f)
				min_width = Math::Max(min_width, LayoutDetails::ContentWidthFromHeight(min_height, item.aspect_ratio, ratio_edges));
			if (max_height < FLT_MAX)
				max_width = Math::Max(min_width, Math::Min(max_width, LayoutDetails::ContentWidthFromHeight(max_height, item.aspect_ratio, ratio_edges)));
		}
		content_width = Math::Clamp(content_width, min_width, max_width);
		if (item.aspect_ratio > 0.f && content_height < 0.f)
			content_height = Math::Clamp(LayoutDetails::ContentHeightFromWidth(content_width, item.aspect_ratio, ratio_edges), min_height, max_height);

		box.SetContent(Vector2f(content_width, content_height));

		RMLUI_ARC_PERF_FORMAT_SITE(FinalGrid);
		UniquePtr<LayoutBox> item_layout_box = FormattingContext::FormatIndependent(grid_container_box, element, &box, FormattingContextType::Block);
		const Vector2f item_border_size = element->GetBox().GetSize(BoxArea::Border);

		const GridItemAxisAlignment x_alignment =
			AlignGridItemWithinArea(area_left, area_right, item.justify_self, item_border_size.x, margin_left, margin_right, 0.f);
		const GridItemAxisAlignment y_alignment =
			AlignGridItemWithinArea(area_top, area_bottom, item.align_self, item_border_size.y, margin_top, margin_bottom, item.baseline_shim);

		const Vector2f item_offset(x_alignment.position, y_alignment.position);
		element->SetOffset(grid_content_offset + item_offset, element_grid);

		item.y_position = item_offset.y;
		item.height = item_border_size.y;
		float item_baseline = 0.f;
		if (item_layout_box && item_layout_box->GetBaselineOfLastLine(item_baseline))
			item.final_baseline = item_baseline;

		if (item_layout_box)
			grid_visible_overflow_size = Math::Max(grid_visible_overflow_size, item_offset + item_layout_box->GetVisibleOverflowSize());

		const Vector2f item_scrollable_content_size = item_offset + item_border_size + Vector2f(x_alignment.margin_end, y_alignment.margin_end);
		grid_scrollable_content_size = Math::Max(grid_scrollable_content_size, item_scrollable_content_size);
	}

	if (!items.empty())
	{
		int first_row = items[0].row_indexes.start;
		for (const GridItem& item : items)
			first_row = Math::Min(first_row, item.row_indexes.start);

		const GridItem* baseline_item = nullptr;
		for (const GridItem& item : items)
		{
			if (item.row_indexes.start != first_row)
				continue;
			if (!baseline_item)
				baseline_item = &item;
			if (item.ParticipatesInBaselineAlignment())
			{
				baseline_item = &item;
				break;
			}
		}

		RMLUI_ASSERT(baseline_item);
		grid_baseline = grid_content_offset.y + baseline_item->y_position + baseline_item->final_baseline.value_or(baseline_item->height);
	}

	grid_resulting_content_size = container_content_box;
}

} // namespace Rml
