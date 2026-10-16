// CSS Grid track sizing, item contributions and alignment. Ported from Taffy (https://github.com/DioxusLabs/taffy) at 66b3364:
// src/compute/grid/track_sizing.rs, alignment.rs, types/grid_item.rs and src/compute/common/alignment.rs.
// Taffy is MIT licensed: Copyright (c) 2018 Visly Inc., Copyright (c) 2026 Taffy Authors.
#include "../../../Include/RmlUi/Core/Math.h"
#include "../CalcExpression.h"
#include "GridLayoutTypes.h"
#include <algorithm>
#include <cstddef>
#include <float.h>

namespace Rml {

// Taffy uses f32::INFINITY for unbounded growth limits; FLT_MAX keeps the same comparisons without relying on IEEE infinities.
static constexpr float GridInfinity = FLT_MAX;

struct GridTrackSpan {
	GridTrack* data;
	int size;

	GridTrack* begin() const { return data; }
	GridTrack* end() const { return data + size; }
	GridTrack& operator[](int index) const { return data[index]; }
};

static GridTrackSpan GridItemSpannedTracks(const GridItem& item, GridAxis axis, Vector<GridTrack>& axis_tracks)
{
	const int start = item.TrackRangeStart(axis);
	const int end = item.TrackRangeEnd(axis);
	return GridTrackSpan{axis_tracks.data() + start, Math::Max(end - start, 0)};
}

static int GridAxisIndex(GridAxis axis)
{
	return axis == GridAxis::Inline ? 0 : 1;
}

static GridOptional ResolveGridLengthPercentageAuto(Style::LengthPercentageAuto value, GridOptional basis)
{
	switch (value.type)
	{
	case Style::LengthPercentageAuto::Length: return value.value;
	case Style::LengthPercentageAuto::Percentage: return basis ? GridOptional(*basis * value.value * 0.01f) : GridOptional();
	case Style::LengthPercentageAuto::Calc: return basis && value.calc ? GridOptional(value.calc->Resolve(*basis)) : GridOptional();
	case Style::LengthPercentageAuto::Auto: break;
	}
	return GridOptional();
}

static GridOptional ResolveGridLengthPercentage(Style::LengthPercentage value, GridOptional basis)
{
	if (value.type == Style::LengthPercentage::Length)
		return value.value;
	if (value.type == Style::LengthPercentage::Calc)
		return basis && value.calc ? GridOptional(value.calc->Resolve(*basis)) : GridOptional();
	return basis ? GridOptional(*basis * value.value * 0.01f) : GridOptional();
}

static GridOptional ClampGridOptional(GridOptional value, GridOptional min, GridOptional max)
{
	if (!value)
		return value;
	float result = *value;
	if (max)
		result = Math::Min(result, *max);
	if (min)
		result = Math::Max(result, *min);
	return result;
}

static float GridItemPaddingBorder(const GridItem& item, int axis_index, GridOptional width_basis)
{
	const int edge_a = (axis_index == 0 ? 3 : 0);
	const int edge_b = (axis_index == 0 ? 1 : 2);
	return Math::Max(ResolveGridLengthPercentage(item.padding[edge_a], width_basis).value_or(0.f), 0.f) +
		Math::Max(ResolveGridLengthPercentage(item.padding[edge_b], width_basis).value_or(0.f), 0.f) + item.border[edge_a] + item.border[edge_b];
}

static GridOptional GridItemPreferredSize(const GridItem& item, int axis_index, GridOptional basis, float padding_border)
{
	GridOptional size = ResolveGridLengthPercentageAuto(item.size[axis_index], basis);
	if (size && !item.border_box_sizing)
		*size += padding_border;
	return size;
}

static GridOptional GridItemMinSize(const GridItem& item, int axis_index, GridOptional basis, float padding_border)
{
	if (item.min_size_auto[axis_index])
		return GridOptional();
	GridOptional size = ResolveGridLengthPercentage(item.min_size[axis_index], basis);
	if (size && !item.border_box_sizing)
		*size += padding_border;
	return size;
}

static GridOptional GridItemMaxSize(const GridItem& item, int axis_index, GridOptional basis, float padding_border)
{
	const Style::LengthPercentage max_size = item.max_size[axis_index];
	if (max_size.type == Style::LengthPercentage::Length && max_size.value >= FLT_MAX)
		return GridOptional();
	GridOptional size = ResolveGridLengthPercentage(max_size, basis);
	if (size && !item.border_box_sizing)
		*size += padding_border;
	return size;
}

static bool IsGridMarginAuto(const GridItem& item, int edge)
{
	return item.margin[edge].type == Style::LengthPercentageAuto::Auto;
}

static float ResolveGridMarginOrZero(const GridItem& item, int edge, GridOptional basis)
{
	return ResolveGridLengthPercentageAuto(item.margin[edge], basis).value_or(0.f);
}

static GridOptionalSize GridItemMarginsAxisSumsWithBaselineShims(const GridItem& item, GridOptional inner_node_width)
{
	const GridOptional zero = 0.f;
	GridOptionalSize result;
	result.width = ResolveGridMarginOrZero(item, 3, zero) + ResolveGridMarginOrZero(item, 1, zero);
	result.height = ResolveGridMarginOrZero(item, 0, inner_node_width) + item.baseline_shim + ResolveGridMarginOrZero(item, 2, inner_node_width);
	return result;
}

bool GridItem::HasCyclicBlockSizeDependency() const
{
	return (size[1].type == Style::LengthPercentageAuto::Percentage || size[1].type == Style::LengthPercentageAuto::Calc) &&
		(crosses_intrinsic_row || crosses_flexible_row);
}

bool GridItem::ParticipatesInBaselineAlignment() const
{
	return align_self == Style::AlignSelf::Baseline && !HasAutoBlockMargin() && !HasCyclicBlockSizeDependency();
}

static GridOptionalSize ApplyGridAspectRatio(const GridItem& item, GridOptionalSize size, float padding_border_x, float padding_border_y)
{
	if (item.aspect_ratio <= 0.f)
		return size;

	const float excluded_x = (item.border_box_sizing ? 0.f : padding_border_x);
	const float excluded_y = (item.border_box_sizing ? 0.f : padding_border_y);
	if (size.width && !size.height)
		size.height = Math::Max(*size.width - excluded_x, 0.f) / item.aspect_ratio + excluded_y;
	else if (size.height && !size.width)
		size.width = Math::Max(*size.height - excluded_y, 0.f) * item.aspect_ratio + excluded_x;
	return size;
}

static GridOptionalSize GridItemPreferredSizes(const GridItem& item, GridOptionalSize grid_area_size)
{
	const float padding_border_x = GridItemPaddingBorder(item, 0, grid_area_size.width);
	const float padding_border_y = GridItemPaddingBorder(item, 1, grid_area_size.width);

	GridOptionalSize size;
	size.width = GridItemPreferredSize(item, 0, grid_area_size.width, padding_border_x);
	size.height = GridItemPreferredSize(item, 1, grid_area_size.height, padding_border_y);
	return ApplyGridAspectRatio(item, size, padding_border_x, padding_border_y);
}

static GridOptionalSize ComputeGridItemKnownDimensions(const GridItem& item, GridOptionalSize grid_area_size)
{
	const GridOptionalSize margins = GridItemMarginsAxisSumsWithBaselineShims(item, grid_area_size.width);
	const float padding_border_x = GridItemPaddingBorder(item, 0, grid_area_size.width);
	const float padding_border_y = GridItemPaddingBorder(item, 1, grid_area_size.width);

	GridOptionalSize min_size, max_size;
	for (int axis_index = 0; axis_index < 2; axis_index++)
	{
		const GridAxis axis = (axis_index == 0 ? GridAxis::Inline : GridAxis::Block);
		const GridOptional area = grid_area_size.Get(axis);
		const float padding_border = (axis_index == 0 ? padding_border_x : padding_border_y);
		min_size.Set(axis, GridItemMinSize(item, axis_index, area, padding_border));
		max_size.Set(axis, GridItemMaxSize(item, axis_index, area, padding_border));
	}
	min_size = ApplyGridAspectRatio(item, min_size, padding_border_x, padding_border_y);
	max_size = ApplyGridAspectRatio(item, max_size, padding_border_x, padding_border_y);
	min_size.width = Math::Max(min_size.width.value_or(padding_border_x), padding_border_x);
	min_size.height = Math::Max(min_size.height.value_or(padding_border_y), padding_border_y);

	GridOptionalSize size = GridItemPreferredSizes(item, grid_area_size);
	for (int axis_index = 0; axis_index < 2; axis_index++)
	{
		const GridAxis axis = (axis_index == 0 ? GridAxis::Inline : GridAxis::Block);
		if (size.Get(axis))
			continue;

		const GridOptional area = grid_area_size.Get(axis);
		const GridOptional area_minus_margins = area ? GridOptional(Math::Max(*area - *margins.Get(axis), 0.f)) : GridOptional();
		const bool margin_auto = (axis_index == 0 ? (IsGridMarginAuto(item, 3) || IsGridMarginAuto(item, 1))
												  : (IsGridMarginAuto(item, 0) || IsGridMarginAuto(item, 2)));
		const Style::AlignSelf alignment = (axis_index == 0 ? item.justify_self : item.align_self);
		if (!margin_auto && alignment == Style::AlignSelf::Stretch)
			size.Set(axis, ClampGridOptional(area_minus_margins, min_size.Get(axis), max_size.Get(axis)));
	}
	size = ApplyGridAspectRatio(item, size, padding_border_x, padding_border_y);

	GridOptionalSize result;
	result.width = ClampGridOptional(size.width, min_size.width, max_size.width);
	result.height = ClampGridOptional(size.height, min_size.height, max_size.height);
	return result;
}

static GridOptionalSize GridItemKnownDimensionsCached(GridItem& item, GridOptionalSize grid_area_size)
{
	if (item.has_known_dimensions_cache && item.known_dimensions_cache_area == grid_area_size)
		return item.known_dimensions_cache_value;

	const GridOptionalSize known_dimensions = ComputeGridItemKnownDimensions(item, grid_area_size);
	item.has_known_dimensions_cache = true;
	item.known_dimensions_cache_area = grid_area_size;
	item.known_dimensions_cache_value = known_dimensions;
	return known_dimensions;
}

static GridOptionalSize GridItemAvailableSpaceMinusMargins(const GridItem& item, GridOptionalSize grid_area_size, GridOptionalSize available_space)
{
	if (!available_space.width && !available_space.height)
		return available_space;

	const float margin_horizontal = ResolveGridMarginOrZero(item, 3, grid_area_size.width) + ResolveGridMarginOrZero(item, 1, grid_area_size.width);
	const float margin_vertical = ResolveGridMarginOrZero(item, 0, grid_area_size.width) + ResolveGridMarginOrZero(item, 2, grid_area_size.width);

	GridOptionalSize result;
	if (available_space.width)
		result.width = Math::Max(*available_space.width - margin_horizontal, 0.f);
	if (available_space.height)
		result.height = Math::Max(*available_space.height - margin_vertical - item.baseline_shim, 0.f);
	return result;
}

static GridAvailableSize GridItemContributionAvailableSpace(const GridItem& item, GridOptionalSize grid_area_size, GridOptionalSize available_space,
	GridAvailableSpace indefinite)
{
	const GridOptionalSize minus_margins = GridItemAvailableSpaceMinusMargins(item, grid_area_size, available_space);
	GridAvailableSize result;
	result.width = (minus_margins.width ? GridAvailableSpace::Definite(*minus_margins.width) : indefinite);
	result.height = (minus_margins.height ? GridAvailableSpace::Definite(*minus_margins.height) : indefinite);
	return result;
}

float GridItemMinContentContribution(GridItem& item, GridAxis axis, const GridItemMeasurer& tree, GridOptionalSize grid_area_size,
	GridOptionalSize available_space)
{
	const GridOptionalSize known_dimensions = GridItemKnownDimensionsCached(item, grid_area_size);
	if (const GridOptional size = known_dimensions.Get(axis))
		return *size;

	return tree.MeasureChildSize(item, axis, known_dimensions, grid_area_size,
		GridItemContributionAvailableSpace(item, grid_area_size, available_space, GridAvailableSpace::MinContent()));
}

static float GridItemMaxContentContribution(GridItem& item, GridAxis axis, const GridItemMeasurer& tree, GridOptionalSize grid_area_size,
	GridOptionalSize available_space)
{
	const GridOptionalSize known_dimensions = GridItemKnownDimensionsCached(item, grid_area_size);
	if (const GridOptional size = known_dimensions.Get(axis))
		return *size;

	return tree.MeasureChildSize(item, axis, known_dimensions, grid_area_size,
		GridItemContributionAvailableSpace(item, grid_area_size, available_space, GridAvailableSpace::MaxContent()));
}

static float GridItemMinContentContributionCached(GridItem& item, GridAxis axis, const GridItemMeasurer& tree, GridOptionalSize grid_area_size,
	GridOptionalSize available_space)
{
	if (const GridOptional cached = item.min_content_contribution_cache.Get(axis))
		return *cached;
	const float size = GridItemMinContentContribution(item, axis, tree, grid_area_size, available_space);
	item.min_content_contribution_cache.Set(axis, size);
	return size;
}

static float GridItemMaxContentContributionCached(GridItem& item, GridAxis axis, const GridItemMeasurer& tree, GridOptionalSize grid_area_size,
	GridOptionalSize available_space)
{
	if (const GridOptional cached = item.max_content_contribution_cache.Get(axis))
		return *cached;
	const float size = GridItemMaxContentContribution(item, axis, tree, grid_area_size, available_space);
	item.max_content_contribution_cache.Set(axis, size);
	return size;
}

static GridOptional GridItemSpannedTrackLimit(const GridItem& item, GridAxis axis, Vector<GridTrack>& axis_tracks, GridOptional axis_parent_size)
{
	float limit = 0.f;
	for (const GridTrack& track : GridItemSpannedTracks(item, axis, axis_tracks))
	{
		const GridOptional track_limit = track.max_track_sizing_function.DefiniteLimit(axis_parent_size);
		if (!track_limit)
			return GridOptional();
		limit += *track_limit;
	}
	return limit;
}

static GridOptional GridItemSpannedFixedTrackLimit(const GridItem& item, GridAxis axis, Vector<GridTrack>& axis_tracks, GridOptional axis_parent_size)
{
	float limit = 0.f;
	for (const GridTrack& track : GridItemSpannedTracks(item, axis, axis_tracks))
	{
		const GridOptional track_limit = track.max_track_sizing_function.DefiniteValue(axis_parent_size);
		if (!track_limit)
			return GridOptional();
		limit += *track_limit;
	}
	return limit;
}

static float GridItemMinimumContribution(GridItem& item, const GridItemMeasurer& tree, GridAxis axis, Vector<GridTrack>& axis_tracks,
	GridOptionalSize grid_area_size, GridOptionalSize inner_node_size)
{
	const int axis_index = GridAxisIndex(axis);
	const float padding_border = GridItemPaddingBorder(item, axis_index, grid_area_size.width);

	if (const GridOptional size = GridItemPreferredSizes(item, grid_area_size).Get(axis))
		return *size;
	if (const GridOptional min_size = GridItemMinSize(item, axis_index, grid_area_size.Get(axis), padding_border))
		return *min_size;
	if (item.IsScrollContainer(axis))
		return 0.f;

	const GridTrackSpan item_axis_tracks = GridItemSpannedTracks(item, axis, axis_tracks);
	bool spans_auto_min_track = false;
	bool spans_a_flexible_track = false;
	for (const GridTrack& track : item_axis_tracks)
	{
		spans_auto_min_track |= track.min_track_sizing_function.IsAuto();
		spans_a_flexible_track |= track.max_track_sizing_function.IsFr();
	}
	const bool only_span_one_track = (item_axis_tracks.size == 1);

	const bool use_content_based_minimum = spans_auto_min_track && (only_span_one_track || !spans_a_flexible_track);
	if (!use_content_based_minimum)
		return 0.f;

	float minimum_contribution = GridItemMinContentContributionCached(item, axis, tree, grid_area_size, grid_area_size);
	if (const GridOptional limit = GridItemSpannedFixedTrackLimit(item, axis, axis_tracks, inner_node_size.Get(axis)))
		minimum_contribution = Math::Min(minimum_contribution, *limit);
	return minimum_contribution;
}

static float GridItemMinimumContributionCached(GridItem& item, const GridItemMeasurer& tree, GridAxis axis, Vector<GridTrack>& axis_tracks,
	GridOptionalSize grid_area_size, GridOptionalSize inner_node_size)
{
	if (const GridOptional cached = item.minimum_contribution_cache.Get(axis))
		return *cached;
	const float size = GridItemMinimumContribution(item, tree, axis, axis_tracks, grid_area_size, inner_node_size);
	item.minimum_contribution_cache.Set(axis, size);
	return size;
}

static GridOptional GridTrackSizeEstimateValue(const GridTrack& track, GridOptional parent_size, GridTrackSizeEstimate estimate)
{
	if (estimate == GridTrackSizeEstimate::BaseSize)
		return track.base_size;
	return track.max_track_sizing_function.DefiniteValue(parent_size);
}

GridOptionalSize GridItemAreaSize(const GridItem& item, GridAxis axis, const Vector<GridTrack>& axis_tracks, const Vector<GridTrack>& other_axis_tracks,
	GridOptionalSize available_space, GridTrackSizeEstimate estimate)
{
	GridOptionalSize size;

	{
		GridOptional sum = 0.f;
		for (int i = item.TrackRangeStart(axis); i < item.TrackRangeEnd(axis) && sum; i++)
		{
			const GridTrack& track = axis_tracks[size_t(i)];
			const GridOptional min_size = track.min_track_sizing_function.DefiniteValue(available_space.Get(axis));
			const GridOptional max_size = track.max_track_sizing_function.DefiniteValue(available_space.Get(axis));
			if (min_size && max_size && *min_size == *max_size)
				*sum += track.base_size;
			else
				sum.reset();
		}
		size.Set(axis, sum);
	}

	{
		const GridAxis other_axis = GridOtherAxis(axis);
		GridOptional sum = 0.f;
		for (int i = item.TrackRangeStart(other_axis); i < item.TrackRangeEnd(other_axis) && sum; i++)
		{
			const GridTrack& track = other_axis_tracks[size_t(i)];
			const GridOptional estimate_value = GridTrackSizeEstimateValue(track, available_space.Get(other_axis), estimate);
			if (estimate_value)
				*sum += *estimate_value + track.content_alignment_adjustment;
			else
				sum.reset();
		}
		size.Set(other_axis, sum);
	}

	return size;
}

struct GridIntrinsicSizeMeasurer {
	const GridItemMeasurer& tree;
	const Vector<GridTrack>& other_axis_tracks;
	GridTrackSizeEstimate estimate;
	GridAxis axis;
	GridOptionalSize inner_node_size;

	GridOptionalSize GridAreaSize(GridItem& item, const Vector<GridTrack>& axis_tracks) const
	{
		if (!item.grid_area_size_cache)
			item.grid_area_size_cache = GridItemAreaSize(item, axis, axis_tracks, other_axis_tracks, inner_node_size, estimate);
		return *item.grid_area_size_cache;
	}

	float MinContentContribution(GridItem& item, Vector<GridTrack>& axis_tracks) const
	{
		const GridOptionalSize grid_area_size = GridAreaSize(item, axis_tracks);
		const GridOptionalSize available_space = grid_area_size.With(axis, GridOptional());
		const GridOptionalSize margin_axis_sums = GridItemMarginsAxisSumsWithBaselineShims(item, available_space.width);
		return GridItemMinContentContributionCached(item, axis, tree, grid_area_size, available_space) + *margin_axis_sums.Get(axis);
	}

	float MaxContentContribution(GridItem& item, Vector<GridTrack>& axis_tracks) const
	{
		const GridOptionalSize grid_area_size = GridAreaSize(item, axis_tracks);
		const GridOptionalSize available_space = grid_area_size.With(axis, GridOptional());
		const GridOptionalSize margin_axis_sums = GridItemMarginsAxisSumsWithBaselineShims(item, available_space.width);
		return GridItemMaxContentContributionCached(item, axis, tree, grid_area_size, available_space) + *margin_axis_sums.Get(axis);
	}

	float MinimumContribution(GridItem& item, Vector<GridTrack>& axis_tracks) const
	{
		const GridOptionalSize grid_area_size = GridAreaSize(item, axis_tracks);
		const GridOptionalSize available_space = grid_area_size.With(axis, GridOptional());
		const GridOptionalSize margin_axis_sums = GridItemMarginsAxisSumsWithBaselineShims(item, available_space.width);
		return GridItemMinimumContributionCached(item, tree, axis, axis_tracks, grid_area_size, inner_node_size) + *margin_axis_sums.Get(axis);
	}
};

static float ComputeGridAlignmentGutterAdjustment(GridContentAlignment alignment, GridOptional axis_inner_node_size, GridTrackSizeEstimate estimate,
	const Vector<GridTrack>& tracks)
{
	if (tracks.size() <= 1)
		return 0.f;

	int outer_gutter_weight = 0;
	int inner_gutter_weight = 0;
	switch (alignment)
	{
	case GridContentAlignment::Start:
	case GridContentAlignment::End:
	case GridContentAlignment::Center: outer_gutter_weight = 1; break;
	case GridContentAlignment::Stretch: break;
	case GridContentAlignment::SpaceBetween: inner_gutter_weight = 1; break;
	case GridContentAlignment::SpaceAround:
		outer_gutter_weight = 1;
		inner_gutter_weight = 2;
		break;
	case GridContentAlignment::SpaceEvenly:
		outer_gutter_weight = 1;
		inner_gutter_weight = 1;
		break;
	}

	if (inner_gutter_weight == 0 || !axis_inner_node_size)
		return 0.f;

	float free_space = 0.f;
	GridOptional track_size_sum = 0.f;
	for (const GridTrack& track : tracks)
	{
		const GridOptional estimate_value = GridTrackSizeEstimateValue(track, axis_inner_node_size, estimate);
		if (!estimate_value)
		{
			track_size_sum.reset();
			break;
		}
		*track_size_sum += *estimate_value;
	}
	if (track_size_sum)
		free_space = Math::Max(0.f, *axis_inner_node_size - *track_size_sum);

	const int weighted_track_count = (int(tracks.size() - 3) / 2) * inner_gutter_weight + 2 * outer_gutter_weight;
	if (weighted_track_count <= 0)
		return 0.f;
	return (free_space / float(weighted_track_count)) * float(inner_gutter_weight);
}

void ResolveGridItemTrackIndexes(Vector<GridItem>& items, GridTrackCounts column_counts, GridTrackCounts row_counts)
{
	for (GridItem& item : items)
	{
		item.column_indexes = GridLineRange{column_counts.OzLineToTrackVecIndex(item.column.start), column_counts.OzLineToTrackVecIndex(item.column.end)};
		item.row_indexes = GridLineRange{row_counts.OzLineToTrackVecIndex(item.row.start), row_counts.OzLineToTrackVecIndex(item.row.end)};
	}
}

void DetermineIfGridItemsCrossFlexibleOrIntrinsicTracks(Vector<GridItem>& items, const Vector<GridTrack>& columns, const Vector<GridTrack>& rows)
{
	for (GridItem& item : items)
	{
		item.crosses_flexible_column = false;
		item.crosses_intrinsic_column = false;
		for (int i = item.TrackRangeStart(GridAxis::Inline); i < item.TrackRangeEnd(GridAxis::Inline); i++)
		{
			item.crosses_flexible_column |= columns[size_t(i)].IsFlexible();
			item.crosses_intrinsic_column |= columns[size_t(i)].HasIntrinsicSizingFunction();
		}

		item.crosses_flexible_row = false;
		item.crosses_intrinsic_row = false;
		for (int i = item.TrackRangeStart(GridAxis::Block); i < item.TrackRangeEnd(GridAxis::Block); i++)
		{
			item.crosses_flexible_row |= rows[size_t(i)].IsFlexible();
			item.crosses_intrinsic_row |= rows[size_t(i)].HasIntrinsicSizingFunction();
		}
	}
}

static void FlushPlannedBaseSizeIncreases(Vector<GridTrack>& tracks)
{
	for (GridTrack& track : tracks)
	{
		track.base_size += track.base_size_planned_increase;
		track.base_size_planned_increase = 0.f;
	}
}

static void FlushPlannedGrowthLimitIncreases(Vector<GridTrack>& tracks, bool set_infinitely_growable)
{
	for (GridTrack& track : tracks)
	{
		if (track.growth_limit_planned_increase > 0.f)
		{
			track.growth_limit = (track.growth_limit == GridInfinity ? track.base_size + track.growth_limit_planned_increase
																	 : track.growth_limit + track.growth_limit_planned_increase);
			track.infinitely_growable = set_infinitely_growable;
		}
		else
		{
			track.infinitely_growable = false;
		}
		track.growth_limit_planned_increase = 0.f;
	}
}

static void InitializeGridTrackSizes(Vector<GridTrack>& axis_tracks, GridOptional axis_inner_node_size)
{
	for (GridTrack& track : axis_tracks)
	{
		track.base_size = track.min_track_sizing_function.DefiniteValue(axis_inner_node_size).value_or(0.f);
		track.growth_limit = track.max_track_sizing_function.DefiniteValue(axis_inner_node_size).value_or(GridInfinity);
		if (track.growth_limit < track.base_size)
			track.growth_limit = track.base_size;
	}
}

static void ResolveGridItemBaselines(const GridItemMeasurer& tree, GridAxis axis, Vector<GridItem>& items, GridOptionalSize inner_node_size)
{
	const GridAxis other_axis = GridOtherAxis(axis);
	Vector<GridItem*> sorted_items;
	sorted_items.reserve(items.size());
	for (GridItem& item : items)
		sorted_items.push_back(&item);
	std::stable_sort(sorted_items.begin(), sorted_items.end(),
		[other_axis](const GridItem* a, const GridItem* b) { return a->Placement(other_axis).start < b->Placement(other_axis).start; });

	size_t row_begin = 0;
	while (row_begin < sorted_items.size())
	{
		const int current_row = sorted_items[row_begin]->Placement(other_axis).start;
		size_t row_end = row_begin;
		while (row_end < sorted_items.size() && sorted_items[row_end]->Placement(other_axis).start == current_row)
			row_end++;

		int row_baseline_item_count = 0;
		for (size_t i = row_begin; i < row_end; i++)
			row_baseline_item_count += (sorted_items[i]->ParticipatesInBaselineAlignment() ? 1 : 0);

		if (row_baseline_item_count > 1)
		{
			float row_max_baseline = -FLT_MAX;
			for (size_t i = row_begin; i < row_end; i++)
			{
				GridItem& item = *sorted_items[i];
				if (!item.ParticipatesInBaselineAlignment())
					continue;

				float baseline = 0.f;
				float height = 0.f;
				tree.MeasureChildBaseline(item, inner_node_size, baseline, height);
				if (item.IsScrollContainer(GridAxis::Block))
					baseline = Math::Clamp(baseline, 0.f, height);

				item.baseline = baseline + ResolveGridMarginOrZero(item, 0, inner_node_size.width);
				row_max_baseline = Math::Max(row_max_baseline, *item.baseline);
			}

			for (size_t i = row_begin; i < row_end; i++)
			{
				GridItem& item = *sorted_items[i];
				if (item.ParticipatesInBaselineAlignment())
				{
					item.baseline_shim = row_max_baseline - item.baseline.value_or(0.f);
					item.has_known_dimensions_cache = false;
				}
			}
		}

		row_begin = row_end;
	}
}

static float CrossedGridFlexFactorSum(GridTrackSpan tracks)
{
	float sum = 0.f;
	for (const GridTrack& track : tracks)
		sum += track.FlexFactor();
	return sum;
}

template <typename AffectedFn, typename ProportionFn, typename PropertyFn, typename LimitFn>
static float DistributeGridSpaceUpToLimits(float space_to_distribute, GridTrackSpan tracks, AffectedFn&& track_is_affected,
	ProportionFn&& track_distribution_proportion, PropertyFn&& track_affected_property, LimitFn&& track_limit)
{
	constexpr float threshold = 0.01f;
	const int max_iterations = tracks.size + 1;

	for (int iteration = 0; iteration < max_iterations; iteration++)
	{
		if (space_to_distribute <= threshold)
			break;

		float proportion_sum = 0.f;
		float min_increase_limit = FLT_MAX;
		bool any_growable = false;
		for (const GridTrack& track : tracks)
		{
			if (!(track_affected_property(track) + track.item_incurred_increase < track_limit(track)) || !track_is_affected(track))
				continue;
			proportion_sum += track_distribution_proportion(track);
			const float increase_limit =
				(track_limit(track) - track_affected_property(track) - track.item_incurred_increase) / track_distribution_proportion(track);
			min_increase_limit = (any_growable ? Math::Min(min_increase_limit, increase_limit) : increase_limit);
			any_growable = true;
		}

		if (proportion_sum == 0.f)
			break;

		const float iteration_item_incurred_increase = Math::Min(min_increase_limit, space_to_distribute / proportion_sum);

		for (GridTrack& track : tracks)
		{
			if (!track_is_affected(track))
				continue;
			const float increase = iteration_item_incurred_increase * track_distribution_proportion(track);
			if (increase > 0.f && track_affected_property(track) + track.item_incurred_increase + increase <= track_limit(track) + threshold)
			{
				track.item_incurred_increase += increase;
				space_to_distribute -= increase;
			}
		}
	}

	return space_to_distribute;
}

enum class GridIntrinsicContributionType { Minimum, Maximum };

template <typename AffectedFn, typename ProportionFn, typename LimitFn>
static void DistributeGridItemSpaceToBaseSizeInner(float space, GridTrackSpan tracks, AffectedFn&& track_is_affected,
	ProportionFn&& track_distribution_proportion, LimitFn&& track_limit, GridIntrinsicContributionType contribution_type,
	GridOptional axis_inner_node_size)
{
	bool any_affected = false;
	for (const GridTrack& track : tracks)
		any_affected |= track_is_affected(track);
	if (space == 0.f || !any_affected)
		return;

	auto get_base_size = [](const GridTrack& track) { return track.base_size; };

	float track_sizes = 0.f;
	for (const GridTrack& track : tracks)
		track_sizes += track.base_size;
	float extra_space = Math::Max(0.f, space - track_sizes);

	extra_space = DistributeGridSpaceUpToLimits(extra_space, tracks, track_is_affected, track_distribution_proportion, get_base_size, track_limit);

	if (extra_space > 0.000001f)
	{
		auto beyond_limit_filter = [contribution_type](const GridTrack& track) {
			return contribution_type == GridIntrinsicContributionType::Minimum ? track.max_track_sizing_function.IsIntrinsic()
																			 : track.max_track_sizing_function.IsMaxOrFitContent();
		};

		int number_of_tracks = 0;
		for (const GridTrack& track : tracks)
			number_of_tracks += (track_is_affected(track) && beyond_limit_filter(track)) ? 1 : 0;
		const bool use_filter = (number_of_tracks > 0);

		DistributeGridSpaceUpToLimits(
			extra_space, tracks, [&](const GridTrack& track) { return track_is_affected(track) && (!use_filter || beyond_limit_filter(track)); },
			track_distribution_proportion, get_base_size, [axis_inner_node_size](const GridTrack& track) { return track.FitContentLimit(axis_inner_node_size); });
	}

	for (GridTrack& track : tracks)
	{
		if (track.item_incurred_increase > track.base_size_planned_increase)
			track.base_size_planned_increase = track.item_incurred_increase;
		track.item_incurred_increase = 0.f;
	}
}

template <typename AffectedFn, typename LimitFn>
static void DistributeGridItemSpaceToBaseSize(bool is_flex, float space, GridTrackSpan tracks, AffectedFn&& track_is_affected, LimitFn&& track_limit,
	GridIntrinsicContributionType contribution_type, GridOptional axis_inner_node_size)
{
	auto unit_proportion = [](const GridTrack&) { return 1.f; };
	if (is_flex)
	{
		auto flex_filter = [&](const GridTrack& track) { return track.IsFlexible() && track_is_affected(track); };

		float flex_factor_sum = 0.f;
		for (const GridTrack& track : tracks)
		{
			if (flex_filter(track))
				flex_factor_sum += track.FlexFactor();
		}

		if (flex_factor_sum > 0.f)
			DistributeGridItemSpaceToBaseSizeInner(space, tracks, flex_filter, [](const GridTrack& track) { return track.FlexFactor(); }, track_limit,
				contribution_type, axis_inner_node_size);
		else
			DistributeGridItemSpaceToBaseSizeInner(space, tracks, flex_filter, unit_proportion, track_limit, contribution_type, axis_inner_node_size);
	}
	else
	{
		DistributeGridItemSpaceToBaseSizeInner(space, tracks, track_is_affected, unit_proportion, track_limit, contribution_type, axis_inner_node_size);
	}
}

template <typename AffectedFn>
static void DistributeGridItemSpaceToGrowthLimit(float space, GridTrackSpan tracks, AffectedFn&& track_is_affected, GridOptional axis_inner_node_size)
{
	int number_of_affected_tracks = 0;
	for (const GridTrack& track : tracks)
		number_of_affected_tracks += track_is_affected(track) ? 1 : 0;
	if (space == 0.f || number_of_affected_tracks == 0)
		return;

	float track_sizes = 0.f;
	for (const GridTrack& track : tracks)
		track_sizes += (track.growth_limit == GridInfinity ? track.base_size : track.growth_limit);
	const float extra_space = Math::Max(0.f, space - track_sizes);

	auto is_growable = [axis_inner_node_size](const GridTrack& track) {
		return track.infinitely_growable || track.FitContentLimitedGrowthLimit(axis_inner_node_size) == GridInfinity;
	};

	int number_of_growable_tracks = 0;
	for (const GridTrack& track : tracks)
		number_of_growable_tracks += (track_is_affected(track) && is_growable(track)) ? 1 : 0;

	if (number_of_growable_tracks > 0)
	{
		const float item_incurred_increase = extra_space / float(number_of_growable_tracks);
		for (GridTrack& track : tracks)
		{
			if (track_is_affected(track) && is_growable(track))
				track.item_incurred_increase = item_incurred_increase;
		}
	}
	else
	{
		DistributeGridSpaceUpToLimits(
			extra_space, tracks, track_is_affected, [](const GridTrack&) { return 1.f; },
			[](const GridTrack& track) { return track.growth_limit == GridInfinity ? track.base_size : track.growth_limit; },
			[axis_inner_node_size](const GridTrack& track) { return track.FitContentLimit(axis_inner_node_size); });
	}

	for (GridTrack& track : tracks)
	{
		if (track.item_incurred_increase > track.growth_limit_planned_increase)
			track.growth_limit_planned_increase = track.item_incurred_increase;
		track.item_incurred_increase = 0.f;
	}
}

static void ResolveIntrinsicGridTrackSizes(const GridItemMeasurer& tree, GridAxis axis, Vector<GridTrack>& axis_tracks,
	const Vector<GridTrack>& other_axis_tracks, Vector<GridItem>& items, GridAvailableSpace axis_available_grid_space, GridOptionalSize inner_node_size,
	GridTrackSizeEstimate estimate)
{
	Vector<GridItem*> sorted_items;
	sorted_items.reserve(items.size());
	for (GridItem& item : items)
	{
		if (!item.CrossesFlexibleTrack(axis))
			sorted_items.push_back(&item);
	}
	const size_t non_flex_item_count = sorted_items.size();
	for (GridItem& item : items)
	{
		if (item.CrossesFlexibleTrack(axis))
			sorted_items.push_back(&item);
	}
	std::stable_sort(sorted_items.begin(), sorted_items.begin() + std::ptrdiff_t(non_flex_item_count),
		[axis](const GridItem* a, const GridItem* b) { return a->Span(axis) < b->Span(axis); });

	const GridOptional axis_inner_node_size = inner_node_size.Get(axis);
	const GridIntrinsicSizeMeasurer item_sizer{tree, other_axis_tracks, estimate, axis, inner_node_size};
	const bool sizing_under_content_constraint = (axis_available_grid_space.type != GridAvailableSpace::Type::Definite);

	size_t batch_begin = 0;
	bool previous_batch_is_flex = false;
	while (!previous_batch_is_flex && batch_begin < sorted_items.size())
	{
		const int batch_span = sorted_items[batch_begin]->Span(axis);
		const bool is_flex = sorted_items[batch_begin]->CrossesFlexibleTrack(axis);
		size_t batch_end = batch_begin + 1;
		if (is_flex)
		{
			batch_end = sorted_items.size();
		}
		else
		{
			while (batch_end < sorted_items.size() && !sorted_items[batch_end]->CrossesFlexibleTrack(axis) &&
				sorted_items[batch_end]->Span(axis) <= batch_span)
				batch_end++;
		}
		previous_batch_is_flex = is_flex;

		if (!is_flex && batch_span == 1)
		{
			for (size_t batch_index = batch_begin; batch_index < batch_end; batch_index++)
			{
				GridItem& item = *sorted_items[batch_index];
				const size_t track_index = size_t(item.PlacementIndexes(axis).start + 1);

				float new_base_size = axis_tracks[track_index].base_size;
				switch (axis_tracks[track_index].min_track_sizing_function.kind)
				{
				case GridSizingKind::MinContent: new_base_size = Math::Max(new_base_size, item_sizer.MinContentContribution(item, axis_tracks)); break;
				case GridSizingKind::Percentage:
				case GridSizingKind::Calc:
					if (!axis_inner_node_size)
						new_base_size = Math::Max(new_base_size, item_sizer.MinContentContribution(item, axis_tracks));
					break;
				case GridSizingKind::MaxContent: new_base_size = Math::Max(new_base_size, item_sizer.MaxContentContribution(item, axis_tracks)); break;
				case GridSizingKind::Auto:
				{
					float space = 0.f;
					if (sizing_under_content_constraint && !item.IsScrollContainer(axis))
					{
						const float axis_minimum_size = item_sizer.MinimumContribution(item, axis_tracks);
						float axis_min_content_size = item_sizer.MinContentContribution(item, axis_tracks);
						if (const GridOptional limit = axis_tracks[track_index].max_track_sizing_function.DefiniteLimit(axis_inner_node_size))
							axis_min_content_size = Math::Min(axis_min_content_size, *limit);
						space = Math::Max(axis_min_content_size, axis_minimum_size);
					}
					else
					{
						space = item_sizer.MinimumContribution(item, axis_tracks);
					}
					new_base_size = Math::Max(new_base_size, space);
				}
				break;
				default: break;
				}
				axis_tracks[track_index].base_size = new_base_size;

				const GridTrack& track = axis_tracks[track_index];
				const GridSizingFunction max_track_sizing_function = track.max_track_sizing_function;
				const float fit_content_limit = track.FitContentLimit(axis_inner_node_size);
				const GridOptional min_content_contribution =
					(track.HasIntrinsicSizingFunction() ? GridOptional(item_sizer.MinContentContribution(item, axis_tracks)) : GridOptional());

				GridOptional planned_increase;
				if (max_track_sizing_function.IsFitContent())
				{
					const float max_content_contribution = Math::Min(item_sizer.MaxContentContribution(item, axis_tracks), fit_content_limit);
					if (min_content_contribution && !item.IsScrollContainer(axis))
						planned_increase = Math::Max(*min_content_contribution, max_content_contribution);
					else
						planned_increase = max_content_contribution;
				}
				else if (max_track_sizing_function.IsMaxContentAlike() || (max_track_sizing_function.UsesPercentage() && !axis_inner_node_size))
				{
					planned_increase = item_sizer.MaxContentContribution(item, axis_tracks);
				}
				else if (max_track_sizing_function.IsIntrinsic())
				{
					planned_increase = min_content_contribution;
				}

				if (planned_increase)
				{
					GridTrack& mutable_track = axis_tracks[track_index];
					mutable_track.growth_limit_planned_increase = Math::Max(mutable_track.growth_limit_planned_increase, *planned_increase);
				}
			}

			for (GridTrack& track : axis_tracks)
			{
				if (track.growth_limit_planned_increase > 0.f)
				{
					track.growth_limit = (track.growth_limit == GridInfinity ? track.growth_limit_planned_increase
																			 : Math::Max(track.growth_limit, track.growth_limit_planned_increase));
				}
				track.infinitely_growable = false;
				track.growth_limit_planned_increase = 0.f;
				if (track.growth_limit < track.base_size)
					track.growth_limit = track.base_size;
			}

			batch_begin = batch_end;
			continue;
		}

		auto has_intrinsic_min_track_sizing_function = [axis_inner_node_size](const GridTrack& track) {
			return !track.min_track_sizing_function.DefiniteValue(axis_inner_node_size).has_value();
		};
		auto growth_limit_of = [](const GridTrack& track) { return track.growth_limit; };
		auto fit_content_limited_growth_limit = [axis_inner_node_size](const GridTrack& track) {
			return track.FitContentLimitedGrowthLimit(axis_inner_node_size);
		};

		for (size_t batch_index = batch_begin; batch_index < batch_end; batch_index++)
		{
			GridItem& item = *sorted_items[batch_index];
			if (!item.CrossesIntrinsicTrack(axis))
				continue;

			float space = 0.f;
			if (sizing_under_content_constraint && !item.IsScrollContainer(axis))
			{
				const float axis_minimum_size = item_sizer.MinimumContribution(item, axis_tracks);
				float axis_min_content_size = item_sizer.MinContentContribution(item, axis_tracks);
				if (const GridOptional limit = GridItemSpannedTrackLimit(item, axis, axis_tracks, axis_inner_node_size))
					axis_min_content_size = Math::Min(axis_min_content_size, *limit);
				const float limited_min_content = Math::Max(axis_min_content_size, axis_minimum_size);

				if (is_flex)
				{
					const GridTrackSpan spanned_tracks = GridItemSpannedTracks(item, axis, axis_tracks);
					float inflexible_sizes = 0.f;
					for (const GridTrack& track : spanned_tracks)
					{
						if (!track.IsFlexible())
							inflexible_sizes += track.base_size;
					}
					const float scale = Math::Min(CrossedGridFlexFactorSum(spanned_tracks), 1.f);
					const float excess = Math::Max(limited_min_content - inflexible_sizes, 0.f);
					space = Math::Max(axis_minimum_size, inflexible_sizes + excess * scale);
				}
				else
				{
					space = limited_min_content;
				}
			}
			else
			{
				space = item_sizer.MinimumContribution(item, axis_tracks);
			}

			const GridTrackSpan tracks = GridItemSpannedTracks(item, axis, axis_tracks);
			if (space > 0.f)
			{
				if (item.IsScrollContainer(axis))
					DistributeGridItemSpaceToBaseSize(is_flex, space, tracks, has_intrinsic_min_track_sizing_function, fit_content_limited_growth_limit,
						GridIntrinsicContributionType::Minimum, axis_inner_node_size);
				else
					DistributeGridItemSpaceToBaseSize(is_flex, space, tracks, has_intrinsic_min_track_sizing_function, growth_limit_of,
						GridIntrinsicContributionType::Minimum, axis_inner_node_size);
			}
		}
		FlushPlannedBaseSizeIncreases(axis_tracks);

		auto has_min_or_max_content_min_track_sizing_function = [](const GridTrack& track) {
			return track.min_track_sizing_function.IsMinOrMaxContent();
		};
		for (size_t batch_index = batch_begin; batch_index < batch_end; batch_index++)
		{
			GridItem& item = *sorted_items[batch_index];
			const GridTrackSpan tracks = GridItemSpannedTracks(item, axis, axis_tracks);
			bool spans_matching = false;
			for (const GridTrack& track : tracks)
				spans_matching |= has_min_or_max_content_min_track_sizing_function(track);
			if (!spans_matching)
				continue;

			const float space = item_sizer.MinContentContribution(item, axis_tracks);
			if (space > 0.f)
			{
				if (item.IsScrollContainer(axis))
					DistributeGridItemSpaceToBaseSize(is_flex, space, tracks, has_min_or_max_content_min_track_sizing_function,
						fit_content_limited_growth_limit, GridIntrinsicContributionType::Minimum, axis_inner_node_size);
				else
					DistributeGridItemSpaceToBaseSize(is_flex, space, tracks, has_min_or_max_content_min_track_sizing_function, growth_limit_of,
						GridIntrinsicContributionType::Minimum, axis_inner_node_size);
			}
		}
		FlushPlannedBaseSizeIncreases(axis_tracks);

		auto has_max_content_min_track_sizing_function = [](const GridTrack& track) { return track.min_track_sizing_function.IsMaxContent(); };
		if (axis_available_grid_space.type == GridAvailableSpace::Type::MaxContent)
		{
			auto has_auto_min_track_sizing_function = [](const GridTrack& track) {
				return track.min_track_sizing_function.IsAuto() && !track.max_track_sizing_function.IsMinContent();
			};

			for (size_t batch_index = batch_begin; batch_index < batch_end; batch_index++)
			{
				GridItem& item = *sorted_items[batch_index];
				const GridTrackSpan tracks = GridItemSpannedTracks(item, axis, axis_tracks);
				bool spans_matching = false;
				bool spans_max_content_min = false;
				for (const GridTrack& track : tracks)
				{
					spans_matching |= has_auto_min_track_sizing_function(track) || has_max_content_min_track_sizing_function(track);
					spans_max_content_min |= has_max_content_min_track_sizing_function(track);
				}
				if (!spans_matching)
					continue;

				float space = item_sizer.MaxContentContribution(item, axis_tracks);
				if (const GridOptional limit = GridItemSpannedTrackLimit(item, axis, axis_tracks, axis_inner_node_size))
					space = Math::Min(space, *limit);

				if (is_flex)
				{
					float inflexible_sizes = 0.f;
					for (const GridTrack& track : tracks)
					{
						if (!track.IsFlexible())
							inflexible_sizes += track.base_size;
					}
					const float scale = Math::Min(CrossedGridFlexFactorSum(tracks), 1.f);
					space = inflexible_sizes + Math::Max(space - inflexible_sizes, 0.f) * scale;
				}

				if (space > 0.f)
				{
					if (spans_max_content_min)
						DistributeGridItemSpaceToBaseSize(is_flex, space, tracks, has_max_content_min_track_sizing_function,
							[](const GridTrack&) { return GridInfinity; }, GridIntrinsicContributionType::Maximum, axis_inner_node_size);
					else
						DistributeGridItemSpaceToBaseSize(is_flex, space, tracks, has_auto_min_track_sizing_function, fit_content_limited_growth_limit,
							GridIntrinsicContributionType::Maximum, axis_inner_node_size);
				}
			}
			FlushPlannedBaseSizeIncreases(axis_tracks);
		}

		for (size_t batch_index = batch_begin; batch_index < batch_end; batch_index++)
		{
			GridItem& item = *sorted_items[batch_index];
			const GridTrackSpan tracks = GridItemSpannedTracks(item, axis, axis_tracks);
			bool spans_matching = false;
			for (const GridTrack& track : tracks)
				spans_matching |= has_max_content_min_track_sizing_function(track);
			if (!spans_matching)
				continue;

			const float space = item_sizer.MaxContentContribution(item, axis_tracks);
			if (space > 0.f)
				DistributeGridItemSpaceToBaseSize(is_flex, space, tracks, has_max_content_min_track_sizing_function, growth_limit_of,
					GridIntrinsicContributionType::Maximum, axis_inner_node_size);
		}
		FlushPlannedBaseSizeIncreases(axis_tracks);

		for (GridTrack& track : axis_tracks)
		{
			if (track.growth_limit < track.base_size)
				track.growth_limit = track.base_size;
		}

		if (!is_flex)
		{
			auto has_intrinsic_max_track_sizing_function = [axis_inner_node_size](const GridTrack& track) {
				return !track.max_track_sizing_function.HasDefiniteValue(axis_inner_node_size);
			};
			for (size_t batch_index = batch_begin; batch_index < batch_end; batch_index++)
			{
				GridItem& item = *sorted_items[batch_index];
				const GridTrackSpan tracks = GridItemSpannedTracks(item, axis, axis_tracks);
				bool spans_matching = false;
				for (const GridTrack& track : tracks)
					spans_matching |= has_intrinsic_max_track_sizing_function(track);
				if (!spans_matching)
					continue;

				const float space = item_sizer.MinContentContribution(item, axis_tracks);
				if (space > 0.f)
					DistributeGridItemSpaceToGrowthLimit(space, tracks, has_intrinsic_max_track_sizing_function, inner_node_size.Get(axis));
			}
			FlushPlannedGrowthLimitIncreases(axis_tracks, true);

			auto has_max_content_max_track_sizing_function = [axis_inner_node_size](const GridTrack& track) {
				return track.max_track_sizing_function.IsMaxContentAlike() || (track.max_track_sizing_function.UsesPercentage() && !axis_inner_node_size);
			};
			for (size_t batch_index = batch_begin; batch_index < batch_end; batch_index++)
			{
				GridItem& item = *sorted_items[batch_index];
				const GridTrackSpan tracks = GridItemSpannedTracks(item, axis, axis_tracks);
				bool spans_matching = false;
				for (const GridTrack& track : tracks)
					spans_matching |= has_max_content_max_track_sizing_function(track);
				if (!spans_matching)
					continue;

				const float space = item_sizer.MaxContentContribution(item, axis_tracks);
				if (space > 0.f)
					DistributeGridItemSpaceToGrowthLimit(space, tracks, has_max_content_max_track_sizing_function, inner_node_size.Get(axis));
			}
			FlushPlannedGrowthLimitIncreases(axis_tracks, false);
		}

		batch_begin = batch_end;
	}

	for (GridTrack& track : axis_tracks)
	{
		if (track.growth_limit == GridInfinity)
			track.growth_limit = track.base_size;
	}
}

static void MaximiseGridTracks(Vector<GridTrack>& axis_tracks, GridOptional axis_inner_node_size, GridAvailableSpace axis_available_grid_space)
{
	float used_space = 0.f;
	for (const GridTrack& track : axis_tracks)
		used_space += track.base_size;

	const float free_space = axis_available_grid_space.ComputeFreeSpace(used_space);
	if (free_space == GridInfinity)
	{
		for (GridTrack& track : axis_tracks)
			track.base_size = track.growth_limit;
	}
	else if (free_space > 0.f)
	{
		DistributeGridSpaceUpToLimits(
			free_space, GridTrackSpan{axis_tracks.data(), int(axis_tracks.size())}, [](const GridTrack&) { return true; },
			[](const GridTrack&) { return 1.f; }, [](const GridTrack& track) { return track.base_size; },
			[axis_inner_node_size](const GridTrack& track) { return track.FitContentLimitedGrowthLimit(axis_inner_node_size); });
		for (GridTrack& track : axis_tracks)
		{
			track.base_size += track.item_incurred_increase;
			track.item_incurred_increase = 0.f;
		}
	}
}

static float FindGridSizeOfFr(GridTrackSpan tracks, float space_to_fill)
{
	if (space_to_fill == 0.f)
		return 0.f;

	float hypothetical_fr_size = GridInfinity;
	float previous_iter_hypothetical_fr_size = GridInfinity;
	const int max_iterations = tracks.size + 1;
	for (int iteration = 0; iteration < max_iterations; iteration++)
	{
		float used_space = 0.f;
		float naive_flex_factor_sum = 0.f;
		for (const GridTrack& track : tracks)
		{
			if (track.max_track_sizing_function.IsFr() && track.max_track_sizing_function.value * hypothetical_fr_size >= track.base_size)
				naive_flex_factor_sum += track.max_track_sizing_function.value;
			else
				used_space += track.base_size;
		}
		const float leftover_space = space_to_fill - used_space;
		const float flex_factor = Math::Max(naive_flex_factor_sum, 1.f);

		previous_iter_hypothetical_fr_size = hypothetical_fr_size;
		hypothetical_fr_size = leftover_space / flex_factor;

		bool hypothetical_fr_size_is_valid = true;
		for (const GridTrack& track : tracks)
		{
			if (!track.max_track_sizing_function.IsFr())
				continue;
			const float track_flex_factor = track.max_track_sizing_function.value;
			if (!(track_flex_factor * hypothetical_fr_size >= track.base_size || track_flex_factor * previous_iter_hypothetical_fr_size < track.base_size))
			{
				hypothetical_fr_size_is_valid = false;
				break;
			}
		}
		if (hypothetical_fr_size_is_valid)
			break;
	}

	return hypothetical_fr_size;
}

static void ExpandFlexibleGridTracks(const GridItemMeasurer& tree, GridAxis axis, Vector<GridTrack>& axis_tracks, const Vector<GridTrack>& other_axis_tracks,
	Vector<GridItem>& items, GridOptional axis_min_size, GridOptional axis_max_size, GridAvailableSpace axis_available_space_for_expansion,
	GridOptionalSize inner_node_size, GridTrackSizeEstimate estimate)
{
	const GridIntrinsicSizeMeasurer item_sizer{tree, other_axis_tracks, estimate, axis, inner_node_size};
	const GridTrackSpan all_tracks{axis_tracks.data(), int(axis_tracks.size())};

	float flex_fraction = 0.f;
	switch (axis_available_space_for_expansion.type)
	{
	case GridAvailableSpace::Type::Definite:
	{
		const float available_space = axis_available_space_for_expansion.value;
		float used_space = 0.f;
		for (const GridTrack& track : axis_tracks)
			used_space += track.base_size;
		const float free_space = available_space - used_space;
		flex_fraction = (free_space <= 0.f ? 0.f : FindGridSizeOfFr(all_tracks, available_space));
	}
	break;
	case GridAvailableSpace::Type::MinContent: flex_fraction = 0.f; break;
	case GridAvailableSpace::Type::MaxContent:
	{
		float track_fraction = 0.f;
		for (const GridTrack& track : axis_tracks)
		{
			if (!track.max_track_sizing_function.IsFr())
				continue;
			const float flex_factor = track.FlexFactor();
			track_fraction = Math::Max(track_fraction, flex_factor > 1.f ? track.base_size / flex_factor : track.base_size);
		}

		float item_fraction = 0.f;
		for (GridItem& item : items)
		{
			if (!item.CrossesFlexibleTrack(axis))
				continue;
			const float max_content_contribution = item_sizer.MaxContentContribution(item, axis_tracks);
			item_fraction = Math::Max(item_fraction, FindGridSizeOfFr(GridItemSpannedTracks(item, axis, axis_tracks), max_content_contribution));
		}

		flex_fraction = Math::Max(track_fraction, item_fraction);

		float hypothetical_grid_size = 0.f;
		for (const GridTrack& track : axis_tracks)
		{
			if (track.max_track_sizing_function.IsFr())
				hypothetical_grid_size += Math::Max(track.base_size, track.max_track_sizing_function.value * flex_fraction);
			else
				hypothetical_grid_size += track.base_size;
		}

		const float min_size = axis_min_size.value_or(0.f);
		const float max_size = axis_max_size.value_or(GridInfinity);
		if (hypothetical_grid_size < min_size)
			flex_fraction = FindGridSizeOfFr(all_tracks, min_size);
		else if (hypothetical_grid_size > max_size)
			flex_fraction = FindGridSizeOfFr(all_tracks, max_size);
	}
	break;
	}

	for (GridTrack& track : axis_tracks)
	{
		if (track.max_track_sizing_function.IsFr())
			track.base_size = Math::Max(track.base_size, track.max_track_sizing_function.value * flex_fraction);
	}
}

static void StretchAutoGridTracks(Vector<GridTrack>& axis_tracks, GridOptional axis_min_size, GridAvailableSpace axis_available_space_for_expansion)
{
	int num_auto_tracks = 0;
	float used_space = 0.f;
	for (const GridTrack& track : axis_tracks)
	{
		num_auto_tracks += track.max_track_sizing_function.IsAuto() ? 1 : 0;
		used_space += track.base_size;
	}
	if (num_auto_tracks == 0)
		return;

	float free_space = 0.f;
	if (axis_available_space_for_expansion.IsDefinite())
		free_space = axis_available_space_for_expansion.ComputeFreeSpace(used_space);
	else if (axis_min_size)
		free_space = *axis_min_size - used_space;

	if (free_space > 0.f)
	{
		const float extra_space_per_auto_track = free_space / float(num_auto_tracks);
		for (GridTrack& track : axis_tracks)
		{
			if (track.max_track_sizing_function.IsAuto())
				track.base_size += extra_space_per_auto_track;
		}
	}
}

void RunGridTrackSizingAlgorithm(const GridItemMeasurer& tree, GridAxis axis, GridOptional axis_min_size, GridOptional axis_max_size,
	GridContentAlignment axis_alignment, GridContentAlignment other_axis_alignment, GridAvailableSize available_grid_space,
	GridOptionalSize inner_node_size, Vector<GridTrack>& axis_tracks, Vector<GridTrack>& other_axis_tracks, Vector<GridItem>& items,
	GridTrackSizeEstimate estimate, bool has_baseline_aligned_item)
{
	const GridOptional percentage_basis = (inner_node_size.Get(axis) ? inner_node_size.Get(axis) : axis_min_size);
	InitializeGridTrackSizes(axis_tracks, percentage_basis);

	if (has_baseline_aligned_item)
		ResolveGridItemBaselines(tree, axis, items, inner_node_size);

	bool all_tracks_final = true;
	for (const GridTrack& track : axis_tracks)
	{
		if (!(track.base_size == track.growth_limit && track.min_track_sizing_function.DefiniteValue(percentage_basis)))
		{
			all_tracks_final = false;
			break;
		}
	}
	if (all_tracks_final)
		return;

	const float gutter_alignment_adjustment =
		ComputeGridAlignmentGutterAdjustment(other_axis_alignment, inner_node_size.Get(GridOtherAxis(axis)), estimate, other_axis_tracks);
	if (other_axis_tracks.size() > 3)
	{
		for (size_t i = 2; i < other_axis_tracks.size(); i += 2)
			other_axis_tracks[i].content_alignment_adjustment = gutter_alignment_adjustment;
	}

	ResolveIntrinsicGridTrackSizes(tree, axis, axis_tracks, other_axis_tracks, items, available_grid_space.Get(axis), inner_node_size, estimate);

	MaximiseGridTracks(axis_tracks, inner_node_size.Get(axis), available_grid_space.Get(axis));

	GridAvailableSpace axis_available_space_for_expansion;
	if (const GridOptional inner_size = inner_node_size.Get(axis))
		axis_available_space_for_expansion = GridAvailableSpace::Definite(*inner_size);
	else if (available_grid_space.Get(axis).type == GridAvailableSpace::Type::MinContent)
		axis_available_space_for_expansion = GridAvailableSpace::MinContent();
	else
		axis_available_space_for_expansion = GridAvailableSpace::MaxContent();

	ExpandFlexibleGridTracks(tree, axis, axis_tracks, other_axis_tracks, items, axis_min_size, axis_max_size, axis_available_space_for_expansion,
		inner_node_size, estimate);

	if (axis_alignment == GridContentAlignment::Stretch)
		StretchAutoGridTracks(axis_tracks, axis_min_size, axis_available_space_for_expansion);
}

static GridContentAlignment ApplyGridAlignmentFallback(float free_space, int num_items, GridContentAlignment alignment)
{
	bool is_safe = false;
	if (num_items <= 1 || free_space <= 0.f)
	{
		if (alignment == GridContentAlignment::Stretch || alignment == GridContentAlignment::SpaceBetween)
		{
			alignment = GridContentAlignment::Start;
			is_safe = true;
		}
		else if (alignment == GridContentAlignment::SpaceAround || alignment == GridContentAlignment::SpaceEvenly)
		{
			alignment = GridContentAlignment::Center;
			is_safe = true;
		}
	}

	if (free_space <= 0.f && is_safe)
		alignment = GridContentAlignment::Start;

	return alignment;
}

static float ComputeGridAlignmentOffset(float free_space, int num_items, GridContentAlignment alignment, bool is_first)
{
	if (is_first)
	{
		switch (alignment)
		{
		case GridContentAlignment::Start:
		case GridContentAlignment::Stretch:
		case GridContentAlignment::SpaceBetween: return 0.f;
		case GridContentAlignment::End: return free_space;
		case GridContentAlignment::Center: return free_space / 2.f;
		case GridContentAlignment::SpaceAround: return free_space >= 0.f ? (free_space / float(Math::Max(num_items, 1))) / 2.f : free_space / 2.f;
		case GridContentAlignment::SpaceEvenly: return free_space >= 0.f ? free_space / float(Math::Max(num_items + 1, 1)) : free_space / 2.f;
		}
		return 0.f;
	}

	const float positive_free_space = Math::Max(free_space, 0.f);
	switch (alignment)
	{
	case GridContentAlignment::SpaceBetween: return positive_free_space / float(Math::Max(num_items - 1, 1));
	case GridContentAlignment::SpaceAround: return positive_free_space / float(Math::Max(num_items, 1));
	case GridContentAlignment::SpaceEvenly: return positive_free_space / float(Math::Max(num_items + 1, 1));
	default: return 0.f;
	}
}

void AlignGridTracks(float grid_container_content_box_size, Vector<GridTrack>& tracks, GridContentAlignment track_alignment_style)
{
	float used_size = 0.f;
	int num_tracks = 0;
	for (size_t i = 0; i < tracks.size(); i++)
	{
		used_size += tracks[i].base_size;
		if (i % 2 == 1 && !tracks[i].is_collapsed)
			num_tracks++;
	}
	const float free_space = grid_container_content_box_size - used_size;

	const GridContentAlignment track_alignment = ApplyGridAlignmentFallback(free_space, num_tracks, track_alignment_style);
	const float empty_grid_offset = (num_tracks == 0 ? ComputeGridAlignmentOffset(free_space, num_tracks, track_alignment, true) : 0.f);

	float total_offset = empty_grid_offset;
	bool seen_non_collapsed_track = false;
	for (size_t i = 0; i < tracks.size(); i++)
	{
		GridTrack& track = tracks[i];
		const bool is_gutter = (i % 2 == 0);
		const bool is_non_collapsed_track = (!is_gutter && !track.is_collapsed);
		const bool is_first = (is_non_collapsed_track && !seen_non_collapsed_track);

		const float offset = (is_non_collapsed_track ? ComputeGridAlignmentOffset(free_space, num_tracks, track_alignment, is_first) : 0.f);

		track.offset = total_offset + offset;
		total_offset = total_offset + offset + track.base_size;
		if (is_non_collapsed_track)
			seen_non_collapsed_track = true;
	}
}

GridItemAxisAlignment AlignGridItemWithinArea(float area_start, float area_end, Style::AlignSelf alignment_style, float resolved_size,
	GridOptional margin_start, GridOptional margin_end, float baseline_shim)
{
	const float non_auto_margin_start = margin_start.value_or(0.f) + baseline_shim;
	const float non_auto_margin_end = margin_end.value_or(0.f);
	const float grid_area_size = Math::Max(area_end - area_start, 0.f);
	const float free_space = Math::Max(grid_area_size - resolved_size - (non_auto_margin_start + non_auto_margin_end), 0.f);

	const int auto_margin_count = (margin_start ? 0 : 1) + (margin_end ? 0 : 1);
	const float auto_margin_size = (auto_margin_count > 0 ? free_space / float(auto_margin_count) : 0.f);

	GridItemAxisAlignment result;
	result.margin_start = margin_start.value_or(auto_margin_size) + baseline_shim;
	result.margin_end = margin_end.value_or(auto_margin_size);

	float offset = 0.f;
	switch (alignment_style)
	{
	case Style::AlignSelf::FlexEnd: offset = grid_area_size - resolved_size - result.margin_end; break;
	case Style::AlignSelf::Center: offset = (grid_area_size - resolved_size + result.margin_start - result.margin_end) / 2.f; break;
	case Style::AlignSelf::Auto:
	case Style::AlignSelf::FlexStart:
	case Style::AlignSelf::Baseline:
	case Style::AlignSelf::Stretch:
	case Style::AlignSelf::Normal: offset = result.margin_start; break;
	}

	result.position = area_start + offset;
	return result;
}

} // namespace Rml
