// CSS Grid explicit grid, implicit grid, named lines and item placement. Ported from Taffy (https://github.com/DioxusLabs/taffy) at 66b3364:
// src/compute/grid/explicit_grid.rs, implicit_grid.rs, placement.rs, types/{cell_occupancy,coordinates,grid_track,grid_track_counts,named}.rs.
// Taffy is MIT licensed: Copyright (c) 2018 Visly Inc., Copyright (c) 2026 Taffy Authors.
#include "../../../Include/RmlUi/Core/Element.h"
#include "../../../Include/RmlUi/Core/Math.h"
#include "../CalcExpression.h"
#include "GridLayoutTypes.h"
#include <algorithm>
#include <float.h>

namespace Rml {

float GridAvailableSpace::ComputeFreeSpace(float used_space) const
{
	switch (type)
	{
	case Type::MaxContent: return FLT_MAX;
	case Type::MinContent: return 0.f;
	case Type::Definite: return value - used_space;
	}
	return 0.f;
}

bool GridSizingFunction::HasDefiniteValue(GridOptional parent_size) const
{
	switch (kind)
	{
	case GridSizingKind::Length: return true;
	case GridSizingKind::Percentage:
	case GridSizingKind::Calc: return parent_size.has_value();
	default: return false;
	}
}

GridOptional GridSizingFunction::DefiniteValue(GridOptional parent_size) const
{
	switch (kind)
	{
	case GridSizingKind::Length: return value;
	case GridSizingKind::Percentage: return parent_size ? GridOptional(*parent_size * value) : GridOptional();
	case GridSizingKind::Calc: return parent_size && calc ? GridOptional(calc->Resolve(*parent_size)) : GridOptional();
	default: return GridOptional();
	}
}

GridOptional GridSizingFunction::DefiniteLimit(GridOptional parent_size) const
{
	switch (kind)
	{
	case GridSizingKind::FitContentLength: return value;
	case GridSizingKind::FitContentPercentage: return parent_size ? GridOptional(*parent_size * value) : GridOptional();
	default: return DefiniteValue(parent_size);
	}
}

GridTrack GridTrack::Track(GridSizingFunction min, GridSizingFunction max)
{
	GridTrack track;
	track.min_track_sizing_function = min;
	track.max_track_sizing_function = max;
	return track;
}

GridTrack GridTrack::Gutter(GridSizingFunction size)
{
	GridTrack track;
	track.is_gutter = true;
	track.min_track_sizing_function = size;
	track.max_track_sizing_function = size;
	return track;
}

void GridTrack::Collapse()
{
	is_collapsed = true;
	min_track_sizing_function = GridSizingFunction::Zero();
	max_track_sizing_function = GridSizingFunction::Zero();
}

float GridTrack::FitContentLimit(GridOptional axis_available_grid_space) const
{
	switch (max_track_sizing_function.kind)
	{
	case GridSizingKind::FitContentLength: return max_track_sizing_function.value;
	case GridSizingKind::FitContentPercentage:
		return axis_available_grid_space ? *axis_available_grid_space * max_track_sizing_function.value : FLT_MAX;
	default: return FLT_MAX;
	}
}

float GridTrack::FitContentLimitedGrowthLimit(GridOptional axis_available_grid_space) const
{
	return Math::Min(growth_limit, FitContentLimit(axis_available_grid_space));
}

static GridSizingFunction ResolveGridSizingValue(const GridTrackSizingValue& value, Element* element)
{
	GridSizingFunction result;
	switch (value.type)
	{
	case GridTrackSizingType::Auto: result.kind = GridSizingKind::Auto; break;
	case GridTrackSizingType::Length:
		result.kind = GridSizingKind::Length;
		result.value = element->ResolveLength(value.value);
		break;
	case GridTrackSizingType::Percentage:
		result.kind = GridSizingKind::Percentage;
		result.value = value.value.number * 0.01f;
		break;
	case GridTrackSizingType::Calc:
		result.kind = GridSizingKind::Calc;
		result.calc = ResolveCalcLengths(value.calc, [element](NumericValue length) { return element->ResolveLength(length); });
		if (result.calc && !result.calc->HasPercentage())
		{
			result.kind = GridSizingKind::Length;
			result.value = result.calc->Resolve(0.f);
			result.calc = nullptr;
		}
		break;
	case GridTrackSizingType::Flex:
		result.kind = GridSizingKind::Fr;
		result.value = value.value.number;
		break;
	case GridTrackSizingType::MinContent: result.kind = GridSizingKind::MinContent; break;
	case GridTrackSizingType::MaxContent: result.kind = GridSizingKind::MaxContent; break;
	case GridTrackSizingType::FitContentLength:
		result.kind = GridSizingKind::FitContentLength;
		result.value = element->ResolveLength(value.value);
		break;
	case GridTrackSizingType::FitContentPercentage:
		result.kind = GridSizingKind::FitContentPercentage;
		result.value = value.value.number * 0.01f;
		break;
	}
	return result;
}

GridResolvedTrackList ResolveGridTrackList(const GridDefinition* definition, Element* element)
{
	GridResolvedTrackList result;
	if (!definition || definition->type != GridDefinition::Type::TrackList)
		return result;

	const GridTrackList& list = definition->track_list;
	result.line_names = &list.line_names;
	result.components.reserve(list.components.size());
	for (const GridTrackComponent& component : list.components)
	{
		GridResolvedComponent resolved;
		resolved.repeat = component.repeat;
		resolved.repeat_count = component.repeat_count;
		resolved.line_names = &component.line_names;
		resolved.tracks.reserve(component.tracks.size());
		for (const GridTrackSize& track : component.tracks)
			resolved.tracks.push_back(GridResolvedTrackSize{ResolveGridSizingValue(track.min, element), ResolveGridSizingValue(track.max, element)});
		result.components.push_back(std::move(resolved));
	}
	return result;
}

GridSizingFunction ResolveGridGap(Style::LengthPercentage gap)
{
	if (gap.type == Style::LengthPercentage::Percentage)
		return GridSizingFunction{GridSizingKind::Percentage, gap.value * 0.01f};
	if (gap.type == Style::LengthPercentage::Calc)
		return GridSizingFunction{GridSizingKind::Calc, 0.f, std::move(gap.calc)};
	return GridSizingFunction{GridSizingKind::Length, gap.value};
}

static GridPlacementLine GridLineIntoOriginZero(GridPlacementLine line, int explicit_track_count)
{
	switch (line.kind)
	{
	case GridPlacementKind::Auto: return line;
	case GridPlacementKind::Span: return GridPlacementLine{GridPlacementKind::Span, Math::Clamp(line.value, 1, GridMaxTracks)};
	case GridPlacementKind::Line:
	{
		if (line.value == 0)
			return GridPlacementLine{};
		const int oz_line = (line.value > 0 ? line.value - 1 : line.value + explicit_track_count + 1);
		return GridPlacementLine{GridPlacementKind::Line, Math::Clamp(oz_line, GridMinOzLine, GridMaxOzLine)};
	}
	}
	return GridPlacementLine{};
}

GridAxisPlacement GridAxisPlacement::IntoOriginZero(int explicit_track_count) const
{
	return GridAxisPlacement{GridLineIntoOriginZero(start, explicit_track_count), GridLineIntoOriginZero(end, explicit_track_count)};
}

GridLineRange GridAxisPlacement::ResolveDefiniteGridLines() const
{
	const GridPlacementKind start_kind = start.kind;
	const GridPlacementKind end_kind = end.kind;
	if (start_kind == GridPlacementKind::Line && end_kind == GridPlacementKind::Line)
	{
		if (start.value == end.value)
			return GridLineRange{start.value, start.value + 1};
		return GridLineRange{Math::Min(start.value, end.value), Math::Max(start.value, end.value)};
	}
	if (start_kind == GridPlacementKind::Line && end_kind == GridPlacementKind::Span)
		return GridLineRange{start.value, start.value + end.value};
	if (start_kind == GridPlacementKind::Line)
		return GridLineRange{start.value, start.value + 1};
	if (end_kind == GridPlacementKind::Line && start_kind == GridPlacementKind::Span)
		return GridLineRange{end.value - start.value, end.value};
	if (end_kind == GridPlacementKind::Line)
		return GridLineRange{end.value - 1, end.value};

	RMLUI_ERRORMSG("ResolveDefiniteGridLines called on an indefinite grid placement.");
	return GridLineRange{0, 1};
}

int GridAxisPlacement::IndefiniteSpan() const
{
	if (start.kind == GridPlacementKind::Span)
		return start.value;
	if (end.kind == GridPlacementKind::Span)
		return end.value;
	return 1;
}

void GridAxisPlacement::ResolveAbsolutelyPositionedGridLines(std::optional<int>& out_start, std::optional<int>& out_end) const
{
	out_start.reset();
	out_end.reset();
	if (start.kind == GridPlacementKind::Line && end.kind == GridPlacementKind::Line)
	{
		if (start.value == end.value)
		{
			out_start = start.value;
			out_end = start.value + 1;
		}
		else
		{
			out_start = Math::Min(start.value, end.value);
			out_end = Math::Max(start.value, end.value);
		}
	}
	else if (start.kind == GridPlacementKind::Line && end.kind == GridPlacementKind::Span)
	{
		out_start = start.value;
		out_end = start.value + end.value;
	}
	else if (start.kind == GridPlacementKind::Line)
	{
		out_start = start.value;
	}
	else if (start.kind == GridPlacementKind::Span && end.kind == GridPlacementKind::Line)
	{
		out_start = end.value - start.value;
		out_end = end.value;
	}
	else if (end.kind == GridPlacementKind::Line)
	{
		out_end = end.value;
	}
}

void ComputeGridExplicitSizeInAxis(const GridResolvedTrackList& track_template, GridSizingFunction gap, GridOptional auto_fit_container_size,
	bool max_repetitions_that_do_not_overflow, int& out_auto_repetition_count, int& out_template_track_count)
{
	out_auto_repetition_count = 0;
	out_template_track_count = 0;

	if (track_template.components.empty())
		return;

	for (const GridResolvedComponent& component : track_template.components)
	{
		if (component.repeat != GridRepeatType::None && component.tracks.empty())
			return;
	}

	long long non_auto_repeating_track_count = 0;
	int auto_repetition_definitions = 0;
	bool all_track_defs_have_fixed_component = true;
	for (const GridResolvedComponent& component : track_template.components)
	{
		switch (component.repeat)
		{
		case GridRepeatType::None: non_auto_repeating_track_count += 1; break;
		case GridRepeatType::Count: non_auto_repeating_track_count += (long long)component.repeat_count * (long long)component.tracks.size(); break;
		case GridRepeatType::AutoFill:
		case GridRepeatType::AutoFit: auto_repetition_definitions += 1; break;
		}
		for (const GridResolvedTrackSize& track : component.tracks)
			all_track_defs_have_fixed_component &= track.HasFixedComponent();
	}
	const int non_auto_count = int(Math::Min(non_auto_repeating_track_count, (long long)GridMaxTracks));

	const bool template_is_valid = (auto_repetition_definitions == 0 || (auto_repetition_definitions == 1 && all_track_defs_have_fixed_component));
	if (!template_is_valid)
		return;

	if (auto_repetition_definitions == 0)
	{
		out_template_track_count = non_auto_count;
		return;
	}

	long long auto_repeat_insertion_point = 0;
	const GridResolvedComponent* repetition_definition = nullptr;
	for (const GridResolvedComponent& component : track_template.components)
	{
		if (component.IsAutoRepetition())
		{
			repetition_definition = &component;
			break;
		}
		auto_repeat_insertion_point += (component.repeat == GridRepeatType::Count ? (long long)component.repeat_count * (long long)component.tracks.size() : 1);
	}
	RMLUI_ASSERT(repetition_definition);
	const int repetition_track_count = int(repetition_definition->tracks.size());

	long long num_repetitions = 1;
	if (auto_fit_container_size)
	{
		const float inner_container_size = *auto_fit_container_size;
		const GridOptional parent_size = inner_container_size;

		auto track_definite_value = [parent_size](const GridResolvedTrackSize& track) -> float {
			const GridOptional max_size = track.max.DefiniteValue(parent_size);
			const GridOptional min_size = track.min.DefiniteValue(parent_size);
			if (max_size)
				return min_size ? Math::Max(*max_size, *min_size) : *max_size;
			return min_size.value_or(0.f);
		};

		float non_repeating_track_used_space = 0.f;
		for (const GridResolvedComponent& component : track_template.components)
		{
			if (component.repeat == GridRepeatType::None)
			{
				non_repeating_track_used_space += track_definite_value(component.tracks[0]);
			}
			else if (component.repeat == GridRepeatType::Count)
			{
				float sum = 0.f;
				for (const GridResolvedTrackSize& track : component.tracks)
					sum += track_definite_value(track);
				non_repeating_track_used_space += sum * float(component.repeat_count);
			}
		}

		const float gap_size = gap.DefiniteValue(parent_size).value_or(0.f);

		float per_repetition_track_used_space = 0.f;
		for (const GridResolvedTrackSize& track : repetition_definition->tracks)
			per_repetition_track_used_space += track_definite_value(track);

		const float first_repetition_and_non_repeating_tracks_used_space = non_repeating_track_used_space + per_repetition_track_used_space +
			float(Math::Max(non_auto_count + repetition_track_count - 1, 0)) * gap_size;

		if (first_repetition_and_non_repeating_tracks_used_space > inner_container_size)
		{
			num_repetitions = 1;
		}
		else
		{
			const float per_repetition_gap_used_space = float(repetition_track_count) * gap_size;
			const float per_repetition_used_space = per_repetition_track_used_space + per_repetition_gap_used_space;
			const float num_repetition_that_fit = (inner_container_size - first_repetition_and_non_repeating_tracks_used_space) / per_repetition_used_space;

			const double rounded = double(max_repetitions_that_do_not_overflow ? Math::RoundDown(num_repetition_that_fit) : Math::RoundUp(num_repetition_that_fit));
			const double clamped = (rounded != rounded ? double(GridMaxTracks) : Math::Clamp(rounded, 0.0, double(GridMaxTracks)));
			num_repetitions = (long long)clamped + 1;
		}
	}

	const long long remaining_tracks = Math::Max((long long)GridMaxTracks - auto_repeat_insertion_point, 0LL);
	if (remaining_tracks == 0)
	{
		num_repetitions = 0;
	}
	else
	{
		const long long max_repetitions = (remaining_tracks + repetition_track_count - 1) / repetition_track_count;
		num_repetitions = Math::Clamp(num_repetitions, 1LL, max_repetitions);
	}

	out_auto_repetition_count = int(num_repetitions);
	out_template_track_count = int(Math::Min((long long)non_auto_count + (long long)repetition_track_count * num_repetitions, (long long)GridMaxTracks));
}

static void PushGridTrack(Vector<GridTrack>& tracks, const GridResolvedTrackSize& track_size, GridSizingFunction gap)
{
	tracks.push_back(GridTrack::Track(track_size.min, track_size.max));
	tracks.push_back(GridTrack::Gutter(gap));
}

void InitializeGridTracks(Vector<GridTrack>& tracks, GridTrackCounts counts, const GridResolvedTrackList& track_template,
	const GridResolvedTrackList& auto_tracks, GridSizingFunction gap, int auto_repetition_count, const GridCellOccupancyMatrix& occupancy,
	GridAbsoluteAxis axis)
{
	tracks.clear();
	tracks.reserve(size_t(counts.Len()) * 2 + 1);
	tracks.push_back(GridTrack::Gutter(gap));

	Vector<GridResolvedTrackSize> auto_track_list;
	for (const GridResolvedComponent& component : auto_tracks.components)
		auto_track_list.insert(auto_track_list.end(), component.tracks.begin(), component.tracks.end());

	const int auto_track_count = int(auto_track_list.size());
	const GridResolvedTrackSize auto_track = GridResolvedTrackSize{GridSizingFunction{}, GridSizingFunction{}};

	if (counts.negative_implicit > 0)
	{
		const int offset = (auto_track_count == 0 ? 0 : auto_track_count - (counts.negative_implicit % auto_track_count));
		for (int i = 0; i < counts.negative_implicit; i++)
			PushGridTrack(tracks, auto_track_count == 0 ? auto_track : auto_track_list[size_t((offset + i) % auto_track_count)], gap);
	}

	int current_track_index = counts.negative_implicit;
	const int explicit_track_limit = counts.negative_implicit + counts.explicit_count;

	auto track_has_items = [&occupancy, axis](int track_index) -> bool {
		return axis == GridAbsoluteAxis::Horizontal ? occupancy.ColumnIsOccupied(track_index) : occupancy.RowIsOccupied(track_index);
	};

	if (counts.explicit_count > 0)
	{
		for (const GridResolvedComponent& component : track_template.components)
		{
			switch (component.repeat)
			{
			case GridRepeatType::None:
			{
				if (current_track_index < explicit_track_limit)
				{
					PushGridTrack(tracks, component.tracks[0], gap);
					current_track_index++;
				}
			}
			break;
			case GridRepeatType::Count:
			{
				const int track_count = int(component.tracks.size());
				const int repeated_track_count = Math::Min(track_count * component.repeat_count, explicit_track_limit - current_track_index);
				for (int i = 0; i < repeated_track_count; i++)
				{
					PushGridTrack(tracks, component.tracks[size_t(i % track_count)], gap);
					current_track_index++;
				}
			}
			break;
			case GridRepeatType::AutoFill:
			case GridRepeatType::AutoFit:
			{
				const int track_count = int(component.tracks.size());
				const int auto_repeated_track_count = Math::Min(track_count * auto_repetition_count, explicit_track_limit - current_track_index);
				for (int i = 0; i < auto_repeated_track_count; i++)
				{
					const GridResolvedTrackSize& track_def = component.tracks[size_t(i % track_count)];
					GridTrack track = GridTrack::Track(track_def.min, track_def.max);
					GridTrack gutter = GridTrack::Gutter(gap);

					if (component.repeat == GridRepeatType::AutoFit && !track_has_items(current_track_index))
					{
						track.Collapse();
						gutter.Collapse();
					}

					tracks.push_back(track);
					tracks.push_back(gutter);
					current_track_index++;
				}

				const bool is_last = (current_track_index == counts.Len());
				if (component.repeat == GridRepeatType::AutoFit && is_last)
				{
					for (int i = int(tracks.size()) - 1; i >= 0; i--)
					{
						GridTrack& previous_track = tracks[size_t(i)];
						if (!previous_track.is_gutter && !previous_track.is_collapsed)
							break;
						previous_track.Collapse();
					}
				}
			}
			break;
			}
		}
	}

	const int grid_area_tracks = (counts.negative_implicit + counts.explicit_count) - current_track_index;
	const int positive_track_count = counts.positive_implicit + grid_area_tracks;
	for (int i = 0; i < positive_track_count; i++)
		PushGridTrack(tracks, auto_track_count == 0 ? auto_track : auto_track_list[size_t(i % auto_track_count)], gap);

	tracks.front().Collapse();
	tracks.back().Collapse();
}

GridCellOccupancyMatrix::GridCellOccupancyMatrix(GridTrackCounts columns, GridTrackCounts rows) :
	columns(columns), rows(rows), cells(size_t(columns.Len() * rows.Len()), GridCellOccupancyState::Unoccupied)
{}

void GridCellOccupancyMatrix::ExpandToFitRange(GridLineRange row_span, GridLineRange column_span)
{
	const int required_negative_rows = Math::Max(-rows.negative_implicit - row_span.start, 0);
	const int required_positive_rows = Math::Max(row_span.end - rows.ImplicitEndLine(), 0);
	const int required_negative_columns = Math::Max(-columns.negative_implicit - column_span.start, 0);
	const int required_positive_columns = Math::Max(column_span.end - columns.ImplicitEndLine(), 0);

	if (required_negative_rows == 0 && required_positive_rows == 0 && required_negative_columns == 0 && required_positive_columns == 0)
		return;

	GridTrackCounts new_rows = rows;
	new_rows.negative_implicit += required_negative_rows;
	new_rows.positive_implicit += required_positive_rows;
	GridTrackCounts new_columns = columns;
	new_columns.negative_implicit += required_negative_columns;
	new_columns.positive_implicit += required_positive_columns;

	Vector<GridCellOccupancyState> new_cells(size_t(new_rows.Len() * new_columns.Len()), GridCellOccupancyState::Unoccupied);
	for (int row_index = 0; row_index < rows.Len(); row_index++)
	{
		for (int column_index = 0; column_index < columns.Len(); column_index++)
		{
			const size_t new_index = size_t((row_index + required_negative_rows) * new_columns.Len() + column_index + required_negative_columns);
			new_cells[new_index] = Get(row_index, column_index);
		}
	}

	cells = std::move(new_cells);
	rows = new_rows;
	columns = new_columns;
}

void GridCellOccupancyMatrix::MarkAreaAs(GridAbsoluteAxis primary_axis, GridLineRange primary_span, GridLineRange secondary_span,
	GridCellOccupancyState value)
{
	const GridLineRange row_span = (primary_axis == GridAbsoluteAxis::Horizontal ? secondary_span : primary_span);
	const GridLineRange column_span = (primary_axis == GridAbsoluteAxis::Horizontal ? primary_span : secondary_span);

	ExpandToFitRange(row_span, column_span);

	const int row_begin = rows.OzLineToNextTrack(row_span.start);
	const int row_end = rows.OzLineToNextTrack(row_span.end);
	const int column_begin = columns.OzLineToNextTrack(column_span.start);
	const int column_end = columns.OzLineToNextTrack(column_span.end);
	for (int row_index = row_begin; row_index < row_end; row_index++)
	{
		for (int column_index = column_begin; column_index < column_end; column_index++)
			cells[size_t(row_index * columns.Len() + column_index)] = value;
	}
}

bool GridCellOccupancyMatrix::LineAreaCollisionJump(GridAbsoluteAxis primary_axis, GridLineRange primary_span, GridLineRange secondary_span,
	int& out_next_position) const
{
	const GridTrackCounts& primary_counts = TrackCounts(primary_axis);
	const GridTrackCounts& secondary_counts = TrackCounts(GridOtherAbsoluteAxis(primary_axis));

	const int secondary_begin = Math::Max(secondary_counts.OzLineToNextTrack(secondary_span.start), 0);
	const int secondary_end = Math::Min(secondary_counts.OzLineToNextTrack(secondary_span.end), secondary_counts.Len());
	const int primary_begin = Math::Max(primary_counts.OzLineToNextTrack(primary_span.start), 0);
	const int primary_end = Math::Min(primary_counts.OzLineToNextTrack(primary_span.end), primary_counts.Len());

	int extent = -1;
	for (int secondary_index = secondary_begin; secondary_index < secondary_end; secondary_index++)
	{
		for (int primary_index = primary_end - 1; primary_index >= primary_begin && primary_index > extent; primary_index--)
		{
			const GridCellOccupancyState state =
				(primary_axis == GridAbsoluteAxis::Horizontal ? Get(secondary_index, primary_index) : Get(primary_index, secondary_index));
			if (state != GridCellOccupancyState::Unoccupied)
			{
				extent = primary_index;
				break;
			}
		}
	}

	if (extent < 0)
		return false;

	out_next_position = primary_counts.TrackToPrevOzLine(extent) + 1;
	return true;
}

bool GridCellOccupancyMatrix::OccupiedTrackJump(GridAbsoluteAxis axis, GridLineRange span, int& out_next_position) const
{
	const GridTrackCounts& counts = TrackCounts(axis);
	const int begin = Math::Max(counts.OzLineToNextTrack(span.start), 0);
	const int end = Math::Min(counts.OzLineToNextTrack(span.end), counts.Len());
	for (int index = end - 1; index >= begin; index--)
	{
		const bool occupied = (axis == GridAbsoluteAxis::Vertical ? RowIsOccupied(index) : ColumnIsOccupied(index));
		if (occupied)
		{
			out_next_position = counts.TrackToPrevOzLine(index) + 1;
			return true;
		}
	}
	return false;
}

bool GridCellOccupancyMatrix::RowIsOccupied(int row_index) const
{
	if (row_index < 0 || row_index >= rows.Len())
		return false;
	for (int column_index = 0; column_index < columns.Len(); column_index++)
	{
		if (Get(row_index, column_index) != GridCellOccupancyState::Unoccupied)
			return true;
	}
	return false;
}

bool GridCellOccupancyMatrix::ColumnIsOccupied(int column_index) const
{
	if (column_index < 0 || column_index >= columns.Len())
		return false;
	for (int row_index = 0; row_index < rows.Len(); row_index++)
	{
		if (Get(row_index, column_index) != GridCellOccupancyState::Unoccupied)
			return true;
	}
	return false;
}

bool GridCellOccupancyMatrix::LastOfType(GridAbsoluteAxis track_type, int start_at, GridCellOccupancyState kind, int& out_line) const
{
	const GridAbsoluteAxis other_axis = GridOtherAbsoluteAxis(track_type);
	const int track_index = TrackCounts(other_axis).OzLineToNextTrack(start_at);
	if (track_index < 0 || track_index >= TrackCounts(other_axis).Len())
		return false;

	const GridTrackCounts& cell_counts = TrackCounts(track_type);
	for (int cell_index = cell_counts.Len() - 1; cell_index >= 0; cell_index--)
	{
		const GridCellOccupancyState state =
			(other_axis == GridAbsoluteAxis::Vertical ? Get(track_index, cell_index) : Get(cell_index, track_index));
		if (state == kind)
		{
			out_line = cell_counts.TrackToPrevOzLine(cell_index);
			return true;
		}
	}
	return false;
}

static void UpsertGridLineName(UnorderedMap<String, Vector<int>>& lines, const String& name, int line)
{
	lines[name].push_back(line);
}

void GridNamedLineResolver::AddTemplateLineNames(LineMap& lines, const GridResolvedTrackList& tracks, int auto_repetitions)
{
	const size_t component_count = tracks.components.size();
	int current_line = 0;
	for (size_t i = 0; i <= component_count; i++)
	{
		current_line += 1;
		if (tracks.line_names && i < tracks.line_names->size())
		{
			for (const String& name : (*tracks.line_names)[i])
				UpsertGridLineName(lines, name, current_line);
		}

		if (i == component_count || tracks.components[i].repeat == GridRepeatType::None)
			continue;

		const GridResolvedComponent& repeat = tracks.components[i];
		const int repeat_count = (repeat.repeat == GridRepeatType::Count ? repeat.repeat_count : auto_repetitions);
		const int lines_per_repetition = int(repeat.tracks.size());

		for (int repetition = 0; repetition < repeat_count; repetition++)
		{
			if (repeat.line_names)
			{
				for (size_t set_index = 0; set_index < repeat.line_names->size(); set_index++)
				{
					for (const String& name : (*repeat.line_names)[set_index])
						UpsertGridLineName(lines, name, current_line + int(set_index));
				}
			}
			current_line += lines_per_repetition;

			if (current_line > GridMaxTracks)
				break;
		}

		if (repeat_count > 0)
			current_line = Math::Max(current_line - 1, 0);
	}
}

GridNamedLineResolver::GridNamedLineResolver(const GridResolvedTrackList& columns, const GridResolvedTrackList& rows, const GridTemplateAreas* areas,
	int column_auto_repetitions, int row_auto_repetitions)
{
	AddTemplateLineNames(column_lines, columns, column_auto_repetitions);
	AddTemplateLineNames(row_lines, rows, row_auto_repetitions);

	if (areas)
	{
		area_column_count = areas->column_count;
		area_row_count = areas->row_count;
		for (const GridTemplateArea& area : areas->areas)
		{
			UpsertGridLineName(column_lines, area.name + "-start", area.column_start);
			UpsertGridLineName(column_lines, area.name + "-end", area.column_end);
			UpsertGridLineName(row_lines, area.name + "-start", area.row_start);
			UpsertGridLineName(row_lines, area.name + "-end", area.row_end);
		}
	}

	for (LineMap* map : {&column_lines, &row_lines})
	{
		for (LineMap::value_type& entry : *map)
		{
			Vector<int>& lines = entry.second;
			std::sort(lines.begin(), lines.end());
			lines.erase(std::unique(lines.begin(), lines.end()), lines.end());
		}
	}
}

static int GetGridLineFromList(const int* lines, int line_count, int explicit_track_count, int index)
{
	const int abs_index = (index < 0 ? -index : index);
	long long line = 0;
	if (abs_index <= line_count)
	{
		line = (index > 0 ? lines[abs_index - 1] : lines[line_count - abs_index]);
	}
	else
	{
		const long long remaining_lines = (long long)(abs_index - line_count) * (index > 0 ? 1 : -1);
		line = (index > 0 ? explicit_track_count + 1 + remaining_lines : -(explicit_track_count + 1 + remaining_lines));
	}
	return int(Math::Clamp(line, -100000LL, 100000LL));
}

enum class GridLineFilter : uint8_t { All, After, Before };

static int FindGridLineIndex(const UnorderedMap<String, Vector<int>>& lines, int explicit_track_count, const String& name, int index, bool is_end,
	GridLineFilter filter, int filter_line)
{
	if (index == 0)
		index = 1;

	auto lookup = [&](const String& key, int& out_line) -> bool {
		UnorderedMap<String, Vector<int>>::const_iterator it = lines.find(key);
		if (it == lines.end())
			return false;

		const Vector<int>& all_lines = it->second;
		const int* begin = all_lines.data();
		int count = int(all_lines.size());
		if (filter == GridLineFilter::After)
		{
			const int point = int(std::upper_bound(all_lines.begin(), all_lines.end(), filter_line) - all_lines.begin());
			begin += point;
			count -= point;
		}
		else if (filter == GridLineFilter::Before)
		{
			count = int(std::lower_bound(all_lines.begin(), all_lines.end(), filter_line) - all_lines.begin());
		}
		out_line = GetGridLineFromList(begin, count, explicit_track_count, index);
		return true;
	};

	int line = 0;
	if (lookup(name, line))
		return line;
	if (lookup(name + (is_end ? "-end" : "-start"), line))
		return line;

	const long long fallback = (index > 0 ? (long long)explicit_track_count + 1 + index : -((long long)explicit_track_count + 1 + index));
	return int(Math::Clamp(fallback, -100000LL, 100000LL));
}

GridAxisPlacement GridNamedLineResolver::ResolveLineNames(const LineMap& lines, int explicit_track_count, const GridPlacement* start,
	const GridPlacement* end)
{
	enum class Kind { Auto, Line, NamedLine, Span, NamedSpan };
	struct Resolved {
		Kind kind;
		int value;
		const String* name;
	};

	auto classify = [](const GridPlacement* placement) -> Resolved {
		if (!placement || placement->type == GridPlacementType::Auto)
			return Resolved{Kind::Auto, 0, nullptr};
		if (placement->type == GridPlacementType::Line)
			return placement->name.empty() ? Resolved{Kind::Line, placement->index, nullptr} : Resolved{Kind::NamedLine, placement->index, &placement->name};
		return placement->name.empty() ? Resolved{Kind::Span, placement->index, nullptr} : Resolved{Kind::NamedSpan, placement->index, &placement->name};
	};

	Resolved start_resolved = classify(start);
	Resolved end_resolved = classify(end);

	if (start_resolved.kind == Kind::NamedLine)
		start_resolved = Resolved{Kind::Line,
			FindGridLineIndex(lines, explicit_track_count, *start_resolved.name, start_resolved.value, false, GridLineFilter::All, 0), nullptr};
	if (end_resolved.kind == Kind::NamedLine)
		end_resolved = Resolved{Kind::Line,
			FindGridLineIndex(lines, explicit_track_count, *end_resolved.name, end_resolved.value, true, GridLineFilter::All, 0), nullptr};

	if (start_resolved.kind == Kind::Line && end_resolved.kind == Kind::NamedSpan)
	{
		const int normalized_start_line =
			(start_resolved.value > 0 ? start_resolved.value : Math::Max(explicit_track_count + 1 + start_resolved.value, 0));
		const int end_line = FindGridLineIndex(lines, explicit_track_count, *end_resolved.name, end_resolved.value, true, GridLineFilter::After,
			normalized_start_line);
		return GridAxisPlacement{GridPlacementLine{GridPlacementKind::Line, start_resolved.value}, GridPlacementLine{GridPlacementKind::Line, end_line}};
	}

	if (start_resolved.kind == Kind::NamedSpan && end_resolved.kind == Kind::Line)
	{
		const int normalized_end_line = (end_resolved.value > 0 ? end_resolved.value : Math::Max(explicit_track_count + 1 + end_resolved.value, 0));
		const int start_line = FindGridLineIndex(lines, explicit_track_count, *start_resolved.name, start_resolved.value, false,
			GridLineFilter::Before, normalized_end_line);
		return GridAxisPlacement{GridPlacementLine{GridPlacementKind::Line, start_line}, GridPlacementLine{GridPlacementKind::Line, end_resolved.value}};
	}

	auto to_line = [](const Resolved& resolved) -> GridPlacementLine {
		switch (resolved.kind)
		{
		case Kind::Line: return GridPlacementLine{GridPlacementKind::Line, resolved.value};
		case Kind::Span: return GridPlacementLine{GridPlacementKind::Span, resolved.value};
		case Kind::NamedSpan: return GridPlacementLine{GridPlacementKind::Span, 1};
		default: return GridPlacementLine{};
		}
	};
	return GridAxisPlacement{to_line(start_resolved), to_line(end_resolved)};
}

GridAxisPlacement GridNamedLineResolver::ResolveColumnNames(const GridPlacement* start, const GridPlacement* end) const
{
	return ResolveLineNames(column_lines, explicit_column_count, start, end);
}

GridAxisPlacement GridNamedLineResolver::ResolveRowNames(const GridPlacement* start, const GridPlacement* end) const
{
	return ResolveLineNames(row_lines, explicit_row_count, start, end);
}

static void GridChildMinLineMaxLineSpan(const GridAxisPlacement& placement, int& out_min, int& out_max, int& out_span)
{
	const GridPlacementLine& start = placement.start;
	const GridPlacementLine& end = placement.end;
	const bool start_is_line = (start.kind == GridPlacementKind::Line);
	const bool end_is_line = (end.kind == GridPlacementKind::Line);

	int min_line = 0;
	int max_line = 0;
	if (start_is_line && end_is_line)
	{
		min_line = (start.value == end.value ? start.value : Math::Min(start.value, end.value));
		max_line = (start.value == end.value ? start.value + 1 : Math::Max(start.value, end.value));
	}
	else if (start_is_line)
	{
		min_line = start.value;
		max_line = (end.kind == GridPlacementKind::Span ? start.value + end.value : start.value + 1);
	}
	else if (end_is_line)
	{
		min_line = (start.kind == GridPlacementKind::Span ? end.value - start.value : end.value - 1);
		max_line = end.value;
	}

	out_span = (!start_is_line && !end_is_line ? placement.IndefiniteSpan() : 1);
	out_min = Math::Max(min_line, GridMinOzLine);
	out_max = Math::Min(max_line, GridMaxOzLine);
}

void ComputeGridSizeEstimate(int explicit_column_count, int explicit_row_count, const Vector<GridItemPlacementStyle>& placements,
	GridTrackCounts& out_column_counts, GridTrackCounts& out_row_counts)
{
	int column_min = 0, column_max = 0, column_max_span = 0;
	int row_min = 0, row_max = 0, row_max_span = 0;
	for (const GridItemPlacementStyle& placement : placements)
	{
		int child_min = 0, child_max = 0, child_span = 0;
		GridChildMinLineMaxLineSpan(placement.column, child_min, child_max, child_span);
		column_min = Math::Min(column_min, child_min);
		column_max = Math::Max(column_max, child_max);
		column_max_span = Math::Max(column_max_span, child_span);

		GridChildMinLineMaxLineSpan(placement.row, child_min, child_max, child_span);
		row_min = Math::Min(row_min, child_min);
		row_max = Math::Max(row_max, child_max);
		row_max_span = Math::Max(row_max_span, child_span);
	}

	auto make_counts = [](int min_line, int max_line, int max_span, int explicit_count) -> GridTrackCounts {
		GridTrackCounts counts;
		counts.negative_implicit = (min_line < 0 ? -min_line : 0);
		counts.explicit_count = explicit_count;
		counts.positive_implicit = (max_line > explicit_count ? max_line - explicit_count : 0);
		if (counts.Len() < max_span)
			counts.positive_implicit = max_span - counts.explicit_count - counts.negative_implicit;
		return counts;
	};

	out_column_counts = make_counts(column_min, column_max, column_max_span, explicit_column_count);
	out_row_counts = make_counts(row_min, row_max, row_max_span, explicit_row_count);
}

static GridAbsoluteAxis GridAutoFlowPrimaryAxis(Style::GridAutoFlow auto_flow)
{
	return (auto_flow == Style::GridAutoFlow::Row || auto_flow == Style::GridAutoFlow::RowDense) ? GridAbsoluteAxis::Horizontal
																							   : GridAbsoluteAxis::Vertical;
}

static bool GridAutoFlowIsDense(Style::GridAutoFlow auto_flow)
{
	return auto_flow == Style::GridAutoFlow::RowDense || auto_flow == Style::GridAutoFlow::ColumnDense;
}

static const GridAxisPlacement& GetGridAxisPlacement(const GridItemPlacementStyle& placement, GridAbsoluteAxis axis)
{
	return axis == GridAbsoluteAxis::Horizontal ? placement.column : placement.row;
}

static GridLineRange ResolveIndefiniteGridSpan(int position, int span)
{
	const long long end = (long long)position + span;
	return GridLineRange{position, int(Math::Clamp(end, -100000LL, 100000LL))};
}

static GridLineRange ClampGridSpanToLimitedGrid(GridLineRange span)
{
	const int start = Math::Clamp(span.start, GridMinOzLine, GridMaxOzLine - 1);
	const int end = Math::Clamp(span.end, start + 1, GridMaxOzLine);
	return GridLineRange{start, end};
}

static void RecordGridPlacement(GridCellOccupancyMatrix& occupancy, GridItem& item, GridAbsoluteAxis primary_axis, GridLineRange primary_span,
	GridLineRange secondary_span, GridCellOccupancyState placement_type)
{
	primary_span = ClampGridSpanToLimitedGrid(primary_span);
	secondary_span = ClampGridSpanToLimitedGrid(secondary_span);

	occupancy.MarkAreaAs(primary_axis, primary_span, secondary_span, placement_type);

	item.column = (primary_axis == GridAbsoluteAxis::Horizontal ? primary_span : secondary_span);
	item.row = (primary_axis == GridAbsoluteAxis::Horizontal ? secondary_span : primary_span);
}

static void PlaceDefiniteSecondaryAxisItem(const GridCellOccupancyMatrix& occupancy, const GridItemPlacementStyle& placement,
	Style::GridAutoFlow auto_flow, GridLineRange& out_primary_span, GridLineRange& out_secondary_span)
{
	const GridAbsoluteAxis primary_axis = GridAutoFlowPrimaryAxis(auto_flow);
	const GridAbsoluteAxis secondary_axis = GridOtherAbsoluteAxis(primary_axis);
	const int primary_axis_grid_start_line = occupancy.TrackCounts(primary_axis).ImplicitStartLine();

	const GridLineRange secondary_axis_placement = GetGridAxisPlacement(placement, secondary_axis).ResolveDefiniteGridLines();

	int position = primary_axis_grid_start_line;
	if (!GridAutoFlowIsDense(auto_flow))
	{
		int last_auto_placed = 0;
		if (occupancy.LastOfType(primary_axis, secondary_axis_placement.start, GridCellOccupancyState::AutoPlaced, last_auto_placed))
			position = last_auto_placed;
	}
	const int primary_axis_span = GetGridAxisPlacement(placement, primary_axis).IndefiniteSpan();

	while (true)
	{
		const GridLineRange primary_axis_placement = ResolveIndefiniteGridSpan(position, primary_axis_span);
		int next_position = 0;
		if (!occupancy.LineAreaCollisionJump(primary_axis, primary_axis_placement, secondary_axis_placement, next_position))
		{
			out_primary_span = primary_axis_placement;
			out_secondary_span = secondary_axis_placement;
			return;
		}
		position = next_position;
	}
}

static void PlaceIndefinitelyPositionedItem(const GridCellOccupancyMatrix& occupancy, const GridItemPlacementStyle& placement,
	Style::GridAutoFlow auto_flow, int grid_position_primary, int grid_position_secondary, GridLineRange& out_primary_span,
	GridLineRange& out_secondary_span)
{
	const GridAbsoluteAxis primary_axis = GridAutoFlowPrimaryAxis(auto_flow);
	const GridAbsoluteAxis secondary_axis = GridOtherAbsoluteAxis(primary_axis);
	const bool dense = GridAutoFlowIsDense(auto_flow);

	const GridAxisPlacement& primary_placement_style = GetGridAxisPlacement(placement, primary_axis);
	const GridAxisPlacement& secondary_placement_style = GetGridAxisPlacement(placement, secondary_axis);

	const int secondary_span = secondary_placement_style.IndefiniteSpan();
	const int primary_axis_grid_start_line = occupancy.TrackCounts(primary_axis).ImplicitStartLine();
	const int primary_axis_grid_end_line = occupancy.TrackCounts(primary_axis).ImplicitEndLine();
	const int secondary_axis_grid_start_line = occupancy.TrackCounts(secondary_axis).ImplicitStartLine();

	int primary_index = grid_position_primary;
	int secondary_index = grid_position_secondary;

	if (primary_placement_style.IsDefinite())
	{
		const GridLineRange primary_span = primary_placement_style.ResolveDefiniteGridLines();

		if (dense)
			secondary_index = secondary_axis_grid_start_line;
		else if (primary_span.start < primary_index)
			secondary_index = secondary_index + 1;

		while (true)
		{
			const GridLineRange secondary_range = ResolveIndefiniteGridSpan(secondary_index, secondary_span);
			int next_position = 0;
			if (occupancy.LineAreaCollisionJump(secondary_axis, secondary_range, primary_span, next_position))
			{
				secondary_index = next_position;
				continue;
			}
			out_primary_span = primary_span;
			out_secondary_span = secondary_range;
			return;
		}
	}

	const int primary_span_count = primary_placement_style.IndefiniteSpan();
	const bool spans_all_primary_tracks = (primary_span_count >= occupancy.TrackCounts(primary_axis).Len());

	while (true)
	{
		const GridLineRange primary_span = ResolveIndefiniteGridSpan(primary_index, primary_span_count);
		const GridLineRange secondary_range = ResolveIndefiniteGridSpan(secondary_index, secondary_span);

		if (primary_span.end > primary_axis_grid_end_line)
		{
			if (primary_index == primary_axis_grid_start_line)
			{
				out_primary_span = primary_span;
				out_secondary_span = secondary_range;
				return;
			}
			secondary_index = secondary_index + 1;
			primary_index = primary_axis_grid_start_line;
			continue;
		}

		int next_position = 0;
		if (spans_all_primary_tracks)
		{
			if (occupancy.OccupiedTrackJump(secondary_axis, secondary_range, next_position))
			{
				secondary_index = next_position;
				primary_index = primary_axis_grid_start_line;
				continue;
			}
			out_primary_span = primary_span;
			out_secondary_span = secondary_range;
			return;
		}

		if (occupancy.LineAreaCollisionJump(primary_axis, primary_span, secondary_range, next_position))
		{
			primary_index = next_position;
			continue;
		}

		out_primary_span = primary_span;
		out_secondary_span = secondary_range;
		return;
	}
}

void PlaceGridItems(GridCellOccupancyMatrix& occupancy, Vector<GridItem>& items, const Vector<GridItemPlacementStyle>& placements,
	Style::GridAutoFlow auto_flow)
{
	RMLUI_ASSERT(items.size() == placements.size());
	const GridAbsoluteAxis primary_axis = GridAutoFlowPrimaryAxis(auto_flow);
	const GridAbsoluteAxis secondary_axis = GridOtherAbsoluteAxis(primary_axis);

	for (size_t i = 0; i < items.size(); i++)
	{
		const GridItemPlacementStyle& placement = placements[i];
		if (!(placement.column.IsDefinite() && placement.row.IsDefinite()))
			continue;

		const GridLineRange primary_span = GetGridAxisPlacement(placement, primary_axis).ResolveDefiniteGridLines();
		const GridLineRange secondary_span = GetGridAxisPlacement(placement, secondary_axis).ResolveDefiniteGridLines();
		RecordGridPlacement(occupancy, items[i], primary_axis, primary_span, secondary_span, GridCellOccupancyState::DefinitelyPlaced);
	}

	for (size_t i = 0; i < items.size(); i++)
	{
		const GridItemPlacementStyle& placement = placements[i];
		if (!(GetGridAxisPlacement(placement, secondary_axis).IsDefinite() && !GetGridAxisPlacement(placement, primary_axis).IsDefinite()))
			continue;

		GridLineRange primary_span, secondary_span;
		PlaceDefiniteSecondaryAxisItem(occupancy, placement, auto_flow, primary_span, secondary_span);
		RecordGridPlacement(occupancy, items[i], primary_axis, primary_span, secondary_span, GridCellOccupancyState::AutoPlaced);
	}

	const int primary_axis_grid_start_line = occupancy.TrackCounts(primary_axis).ImplicitStartLine();
	const int secondary_axis_grid_start_line = occupancy.TrackCounts(secondary_axis).ImplicitStartLine();
	int grid_position_primary = primary_axis_grid_start_line;
	int grid_position_secondary = secondary_axis_grid_start_line;

	for (size_t i = 0; i < items.size(); i++)
	{
		const GridItemPlacementStyle& placement = placements[i];
		if (GetGridAxisPlacement(placement, secondary_axis).IsDefinite())
			continue;

		GridLineRange primary_span, secondary_span;
		PlaceIndefinitelyPositionedItem(occupancy, placement, auto_flow, grid_position_primary, grid_position_secondary, primary_span, secondary_span);
		RecordGridPlacement(occupancy, items[i], primary_axis, primary_span, secondary_span, GridCellOccupancyState::AutoPlaced);

		if (GridAutoFlowIsDense(auto_flow))
		{
			grid_position_primary = primary_axis_grid_start_line;
			grid_position_secondary = secondary_axis_grid_start_line;
		}
		else
		{
			grid_position_primary = primary_span.end;
			grid_position_secondary = secondary_span.start;
		}
	}
}

} // namespace Rml
