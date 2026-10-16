#include "GeometryBackgroundBorder.h"
#include "../../Include/RmlUi/Core/Box.h"
#include "../../Include/RmlUi/Core/Math.h"
#include <algorithm>
#include <float.h>

namespace Rml {

GeometryBackgroundBorder::GeometryBackgroundBorder(Vector<Vertex>& vertices, Vector<int>& indices) : vertices(vertices), indices(indices) {}

BorderMetrics GeometryBackgroundBorder::ComputeBorderMetrics(Vector2f outer_position, EdgeSizes edge_sizes, Vector2f inner_size,
	CornerSizes outer_radii_def)
{
	BorderMetrics metrics = {};

	// -- Find the corner positions --

	const Vector2f inner_position = outer_position + Vector2f(edge_sizes[LEFT], edge_sizes[TOP]);
	const Vector2f outer_size = inner_size + Vector2f(edge_sizes[LEFT] + edge_sizes[RIGHT], edge_sizes[TOP] + edge_sizes[BOTTOM]);

	metrics.positions_outer = {
		outer_position,
		outer_position + Vector2f(outer_size.x, 0),
		outer_position + outer_size,
		outer_position + Vector2f(0, outer_size.y),
	};

	metrics.positions_inner = {
		inner_position,
		inner_position + Vector2f(inner_size.x, 0),
		inner_position + inner_size,
		inner_position + Vector2f(0, inner_size.y),
	};

	// -- For curved borders, find the positions to draw ellipses around, and the scaled outer and inner radii --

	const float sum_radius = (outer_radii_def[TOP_LEFT] + outer_radii_def[TOP_RIGHT] + outer_radii_def[BOTTOM_RIGHT] + outer_radii_def[BOTTOM_LEFT]);
	const bool has_radius = (sum_radius > 1.f);

	if (has_radius)
	{
		auto& outer_radii = metrics.outer_radii;
		outer_radii = outer_radii_def;

		// CSS Backgrounds 3 5.5: scale the radii against the border box such that we have no overlapping curves.
		float scale_factor = FLT_MAX;
		scale_factor = Math::Min(scale_factor, outer_size.x / (outer_radii[TOP_LEFT] + outer_radii[TOP_RIGHT]));       // Top
		scale_factor = Math::Min(scale_factor, outer_size.y / (outer_radii[TOP_RIGHT] + outer_radii[BOTTOM_RIGHT]));   // Right
		scale_factor = Math::Min(scale_factor, outer_size.x / (outer_radii[BOTTOM_RIGHT] + outer_radii[BOTTOM_LEFT])); // Bottom
		scale_factor = Math::Min(scale_factor, outer_size.y / (outer_radii[BOTTOM_LEFT] + outer_radii[TOP_LEFT]));     // Left
		scale_factor = Math::Min(1.0f, scale_factor);

		for (float& radius : outer_radii)
			radius = Math::Round(radius * scale_factor);

		// Place the circle/ellipse centers
		metrics.positions_circle_center = {
			metrics.positions_outer[TOP_LEFT] + Vector2f(1, 1) * outer_radii[TOP_LEFT],
			metrics.positions_outer[TOP_RIGHT] + Vector2f(-1, 1) * outer_radii[TOP_RIGHT],
			metrics.positions_outer[BOTTOM_RIGHT] + Vector2f(-1, -1) * outer_radii[BOTTOM_RIGHT],
			metrics.positions_outer[BOTTOM_LEFT] + Vector2f(1, -1) * outer_radii[BOTTOM_LEFT],
		};

		metrics.inner_radii = {
			Vector2f(outer_radii[TOP_LEFT]) - Vector2f(edge_sizes[LEFT], edge_sizes[TOP]),
			Vector2f(outer_radii[TOP_RIGHT]) - Vector2f(edge_sizes[RIGHT], edge_sizes[TOP]),
			Vector2f(outer_radii[BOTTOM_RIGHT]) - Vector2f(edge_sizes[RIGHT], edge_sizes[BOTTOM]),
			Vector2f(outer_radii[BOTTOM_LEFT]) - Vector2f(edge_sizes[LEFT], edge_sizes[BOTTOM]),
		};
	}

	return metrics;
}

void GeometryBackgroundBorder::DrawBackground(const BorderMetrics& metrics, ColourbPremultiplied color)
{
	const int offset_vertices = (int)vertices.size();

	for (int corner = 0; corner < 4; corner++)
		DrawBackgroundCorner(Corner(corner), metrics.positions_inner[corner], metrics.positions_circle_center[corner], metrics.outer_radii[corner],
			metrics.inner_radii[corner], color);

	FillBackground(offset_vertices);
}

void GeometryBackgroundBorder::DrawBorder(const BorderMetrics& metrics, EdgeSizes edge_sizes, const ColourbPremultiplied border_colors[4])
{
	RMLUI_ASSERT(border_colors);

	const int offset_vertices = (int)vertices.size();

	const bool draw_edge[4] = {
		edge_sizes[TOP] > 0 && border_colors[TOP].alpha > 0,
		edge_sizes[RIGHT] > 0 && border_colors[RIGHT].alpha > 0,
		edge_sizes[BOTTOM] > 0 && border_colors[BOTTOM].alpha > 0,
		edge_sizes[LEFT] > 0 && border_colors[LEFT].alpha > 0,
	};

	const bool draw_corner[4] = {
		draw_edge[TOP] || draw_edge[LEFT],
		draw_edge[TOP] || draw_edge[RIGHT],
		draw_edge[BOTTOM] || draw_edge[RIGHT],
		draw_edge[BOTTOM] || draw_edge[LEFT],
	};

	for (int corner = 0; corner < 4; corner++)
	{
		const Edge edge0 = Edge((corner + 3) % 4);
		const Edge edge1 = Edge(corner);

		if (draw_corner[corner])
		{
			DrawBorderCorner(Corner(corner), metrics.positions_outer[corner], metrics.positions_inner[corner],
				metrics.positions_circle_center[corner], metrics.outer_radii[corner], metrics.inner_radii[corner], border_colors[edge0],
				border_colors[edge1]);
		}

		if (draw_edge[edge1])
		{
			RMLUI_ASSERTMSG(draw_corner[corner] && draw_corner[(corner + 1) % 4],
				"Border edges can only be drawn if both of its connected corners are drawn.");

			FillEdge(edge1 == LEFT ? offset_vertices : (int)vertices.size());
		}
	}
}

void GeometryBackgroundBorder::DrawBackgroundCorner(Corner corner, Vector2f pos_inner, Vector2f pos_circle_center, float R, Vector2f r,
	ColourbPremultiplied color)
{
	if (R == 0 || r.x <= 0 || r.y <= 0)
	{
		DrawPoint(pos_inner, color);
	}
	else if (r.x > 0 && r.y > 0)
	{
		const float a0 = float((int)corner + 2) * 0.5f * Math::RMLUI_PI;
		const float a1 = float((int)corner + 3) * 0.5f * Math::RMLUI_PI;
		const int num_points = GetNumPoints(R);
		DrawArc(pos_circle_center, r, a0, a1, color, color, num_points);
	}
}

void GeometryBackgroundBorder::DrawPoint(Vector2f pos, ColourbPremultiplied color)
{
	const int offset_vertices = (int)vertices.size();

	vertices.resize(offset_vertices + 1);

	vertices[offset_vertices].position = pos;
	vertices[offset_vertices].colour = color;
}

void GeometryBackgroundBorder::DrawArc(Vector2f pos_center, Vector2f r, float a0, float a1, ColourbPremultiplied color0, ColourbPremultiplied color1,
	int num_points)
{
	RMLUI_ASSERT(num_points >= 2 && r.x > 0 && r.y > 0);

	const int offset_vertices = (int)vertices.size();

	vertices.resize(offset_vertices + num_points);

	for (int i = 0; i < num_points; i++)
	{
		const float t = float(i) / float(num_points - 1);

		const float a = Math::Lerp(t, a0, a1);

		const Vector2f unit_vector(Math::Cos(a), Math::Sin(a));

		vertices[offset_vertices + i].position = unit_vector * r + pos_center;
		vertices[offset_vertices + i].colour = Math::RoundedLerp(t, color0, color1);
	}
}

void GeometryBackgroundBorder::FillBackground(int index_start)
{
	const int num_added_vertices = (int)vertices.size() - index_start;
	const int offset_indices = (int)indices.size();

	const int num_triangles = (num_added_vertices - 2);

	indices.resize(offset_indices + 3 * num_triangles);

	for (int i = 0; i < num_triangles; i++)
	{
		indices[offset_indices + 3 * i] = index_start;
		indices[offset_indices + 3 * i + 1] = index_start + i + 2;
		indices[offset_indices + 3 * i + 2] = index_start + i + 1;
	}
}

void GeometryBackgroundBorder::DrawBorderCorner(Corner corner, Vector2f pos_outer, Vector2f pos_inner, Vector2f pos_circle_center, float R,
	Vector2f r, ColourbPremultiplied color0, ColourbPremultiplied color1)
{
	const float a0 = float((int)corner + 2) * 0.5f * Math::RMLUI_PI;
	const float a1 = float((int)corner + 3) * 0.5f * Math::RMLUI_PI;

	if (R == 0)
	{
		DrawPointPoint(pos_outer, pos_inner, color0, color1);
	}
	else if (r.x > 0 && r.y > 0)
	{
		DrawArcArc(pos_circle_center, R, r, a0, a1, color0, color1, GetNumPoints(R));
	}
	else
	{
		DrawArcPoint(pos_circle_center, pos_inner, R, a0, a1, color0, color1, GetNumPoints(R));
	}
}

void GeometryBackgroundBorder::DrawPointPoint(Vector2f pos_outer, Vector2f pos_inner, ColourbPremultiplied color0, ColourbPremultiplied color1)
{
	const bool different_color = (color0 != color1);

	vertices.reserve((int)vertices.size() + (different_color ? 4 : 2));

	DrawPoint(pos_inner, color0);
	DrawPoint(pos_outer, color0);

	if (different_color)
	{
		DrawPoint(pos_inner, color1);
		DrawPoint(pos_outer, color1);
	}
}

void GeometryBackgroundBorder::DrawArcArc(Vector2f pos_center, float R, Vector2f r, float a0, float a1, ColourbPremultiplied color0,
	ColourbPremultiplied color1, int num_points)
{
	RMLUI_ASSERT(num_points >= 2 && R > 0 && r.x > 0 && r.y > 0);

	const int num_triangles = 2 * (num_points - 1);

	const int offset_vertices = (int)vertices.size();
	const int offset_indices = (int)indices.size();

	vertices.resize(offset_vertices + 2 * num_points);
	indices.resize(offset_indices + 3 * num_triangles);

	for (int i = 0; i < num_points; i++)
	{
		const float t = float(i) / float(num_points - 1);

		const float a = Math::Lerp(t, a0, a1);
		const ColourbPremultiplied color = Math::RoundedLerp(t, color0, color1);
		const Vector2f unit_vector(Math::Cos(a), Math::Sin(a));

		vertices[offset_vertices + 2 * i].position = unit_vector * r + pos_center;
		vertices[offset_vertices + 2 * i].colour = color;
		vertices[offset_vertices + 2 * i + 1].position = unit_vector * R + pos_center;
		vertices[offset_vertices + 2 * i + 1].colour = color;
	}

	for (int i = 0; i < num_triangles; i += 2)
	{
		indices[offset_indices + 3 * i + 0] = offset_vertices + i + 0;
		indices[offset_indices + 3 * i + 1] = offset_vertices + i + 2;
		indices[offset_indices + 3 * i + 2] = offset_vertices + i + 1;

		indices[offset_indices + 3 * i + 3] = offset_vertices + i + 1;
		indices[offset_indices + 3 * i + 4] = offset_vertices + i + 2;
		indices[offset_indices + 3 * i + 5] = offset_vertices + i + 3;
	}
}

void GeometryBackgroundBorder::DrawArcPoint(Vector2f pos_center, Vector2f pos_inner, float R, float a0, float a1, ColourbPremultiplied color0,
	ColourbPremultiplied color1, int num_points)
{
	RMLUI_ASSERT(R > 0 && num_points >= 2);

	const int offset_vertices = (int)vertices.size();
	vertices.reserve(offset_vertices + num_points + 2);

	// Generate the vertices. We could also split the arc mid-way to create a sharp color transition.
	DrawPoint(pos_inner, color0);
	DrawArc(pos_center, Vector2f(R), a0, a1, color0, color1, num_points);
	DrawPoint(pos_inner, color1);

	RMLUI_ASSERT((int)vertices.size() - offset_vertices == num_points + 2);

	// Swap the last two vertices such that the outer edge vertex is last, see the comment for the border drawing functions. Their colors should
	// already be the same.
	const int last_vertex = (int)vertices.size() - 1;
	std::swap(vertices[last_vertex - 1].position, vertices[last_vertex].position);

	// Generate the indices
	const int num_triangles = (num_points - 1);

	const int i_vertex_inner0 = offset_vertices;
	const int i_vertex_inner1 = last_vertex - 1;

	const int offset_indices = (int)indices.size();
	indices.resize(offset_indices + 3 * num_triangles);

	for (int i = 0; i < num_triangles; i++)
	{
		indices[offset_indices + 3 * i + 0] = (i > num_triangles / 2 ? i_vertex_inner1 : i_vertex_inner0);
		indices[offset_indices + 3 * i + 1] = offset_vertices + i + 2;
		indices[offset_indices + 3 * i + 2] = offset_vertices + i + 1;
	}

	// Since we swapped the last two vertices we also need to change the last triangle.
	indices[offset_indices + 3 * (num_triangles - 1) + 1] = last_vertex;
}

void GeometryBackgroundBorder::FillEdge(int index_next_corner)
{
	const int offset_indices = (int)indices.size();
	const int num_vertices = (int)vertices.size();
	RMLUI_ASSERT(num_vertices >= 2);

	indices.resize(offset_indices + 6);

	indices[offset_indices + 0] = num_vertices - 2;
	indices[offset_indices + 1] = index_next_corner;
	indices[offset_indices + 2] = num_vertices - 1;

	indices[offset_indices + 3] = num_vertices - 1;
	indices[offset_indices + 4] = index_next_corner;
	indices[offset_indices + 5] = index_next_corner + 1;
}

int GeometryBackgroundBorder::GetNumPoints(float R) const
{
	return Math::Clamp(3 + Math::RoundToInteger(R / 6.f), 2, 100);
}

// Centerline construction after Ladybird Libraries/LibWeb/Rust/src/painting/record/paint/border.rs (BSD-2-Clause, see LICENSE-ladybird.txt).
// Dash and dot proportions and pattern fitting after Chromium third_party/blink/renderer/platform/graphics/styled_stroke_data.cc and
// third_party/blink/renderer/core/paint/box_border_painter.cc (BSD-3-Clause, see LICENSE-chromium.txt).
namespace {
	struct CenterlineCorner {
		Vector2f center;
		Vector2f radii;
	};

	struct Centerline {
		Vector<Vector2f> points;
		Vector<float> distances;

		float Length() const { return distances.empty() ? 0.f : distances.back(); }

		void Add(Vector2f point)
		{
			if (!points.empty() && (point - points.back()).SquaredMagnitude() < 1e-6f)
				return;
			distances.push_back(points.empty() ? 0.f : distances.back() + (point - points.back()).Magnitude());
			points.push_back(point);
		}

		Vector2f PointAt(float distance, int& out_segment) const
		{
			const int num_points = (int)points.size();
			int segment = 0;
			while (segment + 2 < num_points && distances[segment + 1] < distance)
				segment++;
			out_segment = segment;
			const float segment_length = distances[segment + 1] - distances[segment];
			const float t = (segment_length > 0.f ? Math::Clamp((distance - distances[segment]) / segment_length, 0.f, 1.f) : 0.f);
			return points[segment] + (points[segment + 1] - points[segment]) * t;
		}
	};
} // namespace

static CenterlineCorner GetCenterlineCorner(const BorderMetrics& metrics, const EdgeSizes& edge_sizes, int corner)
{
	static const Vector2f directions[4] = {Vector2f(1, 1), Vector2f(-1, 1), Vector2f(-1, -1), Vector2f(1, -1)};
	const float width_x = edge_sizes[(corner == 0 || corner == 3) ? 3 : 1];
	const float width_y = edge_sizes[(corner == 0 || corner == 1) ? 0 : 2];
	const float radius = metrics.outer_radii[corner];

	const Vector2f extent = {Math::Max(radius, 0.5f * width_x), Math::Max(radius, 0.5f * width_y)};
	const Vector2f center = metrics.positions_outer[corner] + directions[corner] * extent;
	return CenterlineCorner{center, extent - Vector2f(0.5f * width_x, 0.5f * width_y)};
}

static void AppendCenterlineArc(Centerline& line, const CenterlineCorner& corner, float angle0, float angle1)
{
	const float max_radius = Math::Max(corner.radii.x, corner.radii.y);
	const int num_segments = Math::Clamp((int)Math::RoundUp(max_radius * Math::Absolute(angle1 - angle0) * 0.5f), 1, 64);
	for (int i = 0; i <= num_segments; i++)
	{
		const float angle = angle0 + (angle1 - angle0) * float(i) / float(num_segments);
		line.Add(corner.center + Vector2f(corner.radii.x * Math::Cos(angle), corner.radii.y * Math::Sin(angle)));
	}
}

static void EmitStroke(Vector<Vertex>& vertices, Vector<int>& indices, const Vector<Vector2f>& points, float half_width, ColourbPremultiplied color)
{
	const int num_points = (int)points.size();
	if (num_points < 2)
		return;

	auto Normal = [](Vector2f direction) { return Vector2f(-direction.y, direction.x); };

	const int first_vertex = (int)vertices.size();
	for (int i = 0; i < num_points; i++)
	{
		Vector2f offset;
		if (i == 0)
		{
			offset = Normal((points[1] - points[0]).Normalise()) * half_width;
		}
		else if (i == num_points - 1)
		{
			offset = Normal((points[i] - points[i - 1]).Normalise()) * half_width;
		}
		else
		{
			const Vector2f normal0 = Normal((points[i] - points[i - 1]).Normalise());
			const Vector2f normal1 = Normal((points[i + 1] - points[i]).Normalise());
			Vector2f miter = normal0 + normal1;
			if (miter.SquaredMagnitude() < 1e-6f)
				miter = normal0;
			miter = miter.Normalise();
			const float scale = 1.f / Math::Max(miter.DotProduct(normal0), 0.25f);
			offset = miter * (half_width * scale);
		}

		vertices.push_back(Vertex{points[i] + offset, color, Vector2f()});
		vertices.push_back(Vertex{points[i] - offset, color, Vector2f()});
	}

	for (int i = 0; i < num_points - 1; i++)
	{
		const int v = first_vertex + 2 * i;
		indices.insert(indices.end(), {v, v + 1, v + 2, v + 1, v + 3, v + 2});
	}
}

namespace GeometryBackgroundBorderPattern {

	struct Pattern {
		bool round = false;
		float dash = 0.f;
		float period = 0.f;
		int num_dots = 0;
	};

	float SelectBestDashGap(float stroke_length, float dash_length, float gap_length, bool closed_path)
	{
		const float available_length = (closed_path ? stroke_length : stroke_length + gap_length);
		const float min_num_dashes = Math::RoundDown(available_length / (dash_length + gap_length));
		const float max_num_dashes = min_num_dashes + 1.f;
		const float min_num_gaps = (closed_path ? min_num_dashes : min_num_dashes - 1.f);
		const float max_num_gaps = (closed_path ? max_num_dashes : max_num_dashes - 1.f);
		const float min_gap = (stroke_length - min_num_dashes * dash_length) / min_num_gaps;
		const float max_gap = (stroke_length - max_num_dashes * dash_length) / max_num_gaps;
		return (max_gap <= 0.f || Math::Absolute(min_gap - gap_length) < Math::Absolute(max_gap - gap_length)) ? min_gap : max_gap;
	}

	// Returns false when the path is too short for a pattern and the side is drawn solid.
	bool FitPattern(float length, float width, bool dotted, bool closed, Pattern& out)
	{
		out = Pattern{};

		// Dotted borders up to 3px wide are square dots, drawn as dashes as long as their gaps.
		if (!dotted || width <= 3.f)
		{
			float dash_length = width;
			float gap_length = width;
			if (!dotted)
			{
				dash_length *= (width >= 3.f ? 2.f : 3.f);
				gap_length *= (width >= 3.f ? 1.f : 2.f);
			}
			if (length <= 2.f * dash_length)
				return false;

			const float two_dashes_with_gap_length = 2.f * dash_length + gap_length + (closed ? gap_length : 0.f);
			if (length <= two_dashes_with_gap_length)
			{
				const float multiplier = length / two_dashes_with_gap_length;
				out.dash = dash_length * multiplier;
				out.period = (dash_length + gap_length) * multiplier;
				return true;
			}

			const float gap = (dotted ? gap_length : SelectBestDashGap(length, dash_length, gap_length, closed));
			out.dash = dash_length;
			out.period = dash_length + gap;
			return true;
		}

		out.round = true;
		out.period = 2.f * width;
		if (length < 2.f * width)
		{
			out.num_dots = 1;
			return true;
		}

		const float gap = SelectBestDashGap(length, width, width, closed);
		if (!(gap < FLT_MAX))
		{
			out.num_dots = 1;
			return true;
		}
		out.period = gap + width;
		out.num_dots = Math::Max(closed ? Math::RoundToInteger(length / out.period) : Math::RoundToInteger((length - width) / out.period) + 1, 1);
		return true;
	}

	void EmitConvexPolygon(Vector<Vertex>& vertices, Vector<int>& indices, Vector<Vector2f> polygon, const Vector<Vector2f>& clip,
		ColourbPremultiplied color)
	{
		const int num_clip = (int)clip.size();
		if (num_clip >= 3)
		{
			float orientation = 0.f;
			for (int i = 0; i < num_clip; i++)
			{
				const Vector2f a = clip[i];
				const Vector2f b = clip[(i + 1) % num_clip];
				orientation += a.x * b.y - b.x * a.y;
			}

			Vector<Vector2f> scratch;
			for (int i = 0; i < num_clip && polygon.size() >= 3; i++)
			{
				const Vector2f a = clip[i];
				const Vector2f edge = clip[(i + 1) % num_clip] - a;
				if (edge.SquaredMagnitude() < 1e-8f)
					continue;

				scratch.clear();
				const int num_points = (int)polygon.size();
				for (int j = 0; j < num_points; j++)
				{
					const Vector2f current = polygon[j];
					const Vector2f next = polygon[(j + 1) % num_points];
					const float d_current = (edge.x * (current.y - a.y) - edge.y * (current.x - a.x)) * orientation;
					const float d_next = (edge.x * (next.y - a.y) - edge.y * (next.x - a.x)) * orientation;
					if (d_current >= 0.f)
						scratch.push_back(current);
					if ((d_current >= 0.f) != (d_next >= 0.f))
						scratch.push_back(current + (next - current) * (d_current / (d_current - d_next)));
				}
				polygon.swap(scratch);
			}
		}

		const int num_points = (int)polygon.size();
		if (num_points < 3)
			return;

		const int first_vertex = (int)vertices.size();
		for (const Vector2f& point : polygon)
			vertices.push_back(Vertex{point, color, Vector2f()});
		for (int i = 1; i + 1 < num_points; i++)
			indices.insert(indices.end(), {first_vertex, first_vertex + i, first_vertex + i + 1});
	}

	void EmitDot(Vector<Vertex>& vertices, Vector<int>& indices, Vector2f center, float radius, const Vector<Vector2f>& clip,
		ColourbPremultiplied color)
	{
		const int num_segments = Math::Clamp((int)Math::RoundUp(4.f * radius), 12, 48);
		Vector<Vector2f> polygon;
		polygon.reserve(num_segments);
		for (int i = 0; i < num_segments; i++)
		{
			const float angle = 2.f * Math::RMLUI_PI * float(i) / float(num_segments);
			polygon.push_back(center + Vector2f(Math::Cos(angle), Math::Sin(angle)) * radius);
		}
		EmitConvexPolygon(vertices, indices, std::move(polygon), clip, color);
	}

	void EmitCenterlinePiece(Vector<Vertex>& vertices, Vector<int>& indices, const Centerline& line, float begin, float end, float half_width,
		ColourbPremultiplied color)
	{
		if (end - begin <= 1e-3f)
			return;

		Vector<Vector2f> points;
		int begin_segment = 0;
		int end_segment = 0;
		points.push_back(line.PointAt(begin, begin_segment));
		const Vector2f end_point = line.PointAt(end, end_segment);
		for (int point_index = begin_segment + 1; point_index <= end_segment; point_index++)
		{
			if ((line.points[point_index] - points.back()).SquaredMagnitude() > 1e-6f)
				points.push_back(line.points[point_index]);
		}
		if ((end_point - points.back()).SquaredMagnitude() > 1e-6f)
			points.push_back(end_point);

		EmitStroke(vertices, indices, points, half_width, color);
	}

	struct EndpointDots {
		bool start_dot = false;
		bool end_dot = false;
		float start_dot_length = 0.f;
		float end_dot_length = 0.f;
		float line_begin = 0.f;
		float line_end = 0.f;
	};

	EndpointDots EnforceDotsAtEndpoints(float length, int width)
	{
		const int path_length = Math::RoundToInteger(length);
		const int mod_4 = path_length % 4;
		const int mod_6 = path_length % 6;
		bool use_start_dot = false;
		int start_dot_growth = 0;
		int start_line_offset = 0;
		bool use_end_dot = false;
		int end_dot_growth = 0;
		if ((width == 1 && path_length % 2 == 0) || (width == 3 && mod_6 == 0))
		{
			use_start_dot = true;
			start_dot_growth = 1;
			start_line_offset = 1;
		}
		if ((width == 2 && (mod_4 == 0 || mod_4 == 1)) || (width == 3 && (mod_6 == 1 || mod_6 == 2)))
		{
			use_start_dot = true;
			start_line_offset = -1;
		}
		if ((width == 2 && mod_4 == 0) || (width == 3 && mod_6 == 1))
			use_end_dot = true;
		if ((width == 2 && mod_4 == 3) || (width == 3 && (mod_6 == 4 || mod_6 == 5)))
		{
			use_start_dot = true;
			start_line_offset = 1;
		}
		if (width == 3 && mod_6 == 5)
		{
			use_end_dot = true;
		}
		else if (width == 3 && mod_6 == 0)
		{
			use_end_dot = true;
			end_dot_growth = 1;
		}

		EndpointDots dots;
		dots.line_end = length;
		if (use_start_dot)
		{
			dots.start_dot = true;
			dots.start_dot_length = float(width + start_dot_growth);
			dots.line_begin = float(2 * width + start_line_offset);
		}
		if (use_end_dot)
		{
			dots.end_dot = true;
			dots.end_dot_length = float(width + end_dot_growth);
			dots.line_end = length - float(width + end_dot_growth + 1);
		}
		return dots;
	}

	void EmitSideRun(Vector<Vertex>& vertices, Vector<int>& indices, Vector2f outer_start, Vector2f direction, Vector2f inward,
		const Vector<Vector2f>& trapezoid, float begin, float end, ColourbPremultiplied color)
	{
		const Vector2f p0 = outer_start + direction * begin;
		const Vector2f p1 = outer_start + direction * end;
		EmitConvexPolygon(vertices, indices, {p0, p1, p1 + inward, p0 + inward}, trapezoid, color);
	}

	bool InnerCornerRounded(const BorderMetrics& metrics, int corner)
	{
		return metrics.inner_radii[corner].x > 0.f || metrics.inner_radii[corner].y > 0.f;
	}

} // namespace GeometryBackgroundBorderPattern

void GeometryBackgroundBorder::DrawPatternedBorder(const BorderMetrics& metrics, EdgeSizes edge_sizes, const ColourbPremultiplied border_colors[4],
	const Style::BorderStyle border_styles[4], bool out_drawn[4])
{
	using Style::BorderStyle;
	using GeometryBackgroundBorderPattern::Pattern;
	constexpr float pi = Math::RMLUI_PI;

	bool patterned[4];
	bool curved[4];
	bool any_curved = false;
	for (int edge = 0; edge < 4; edge++)
	{
		out_drawn[edge] = false;
		patterned[edge] = (border_styles[edge] == BorderStyle::Dashed || border_styles[edge] == BorderStyle::Dotted) && edge_sizes[edge] > 0.f &&
			border_colors[edge].alpha > 0;
		curved[edge] = GeometryBackgroundBorderPattern::InnerCornerRounded(metrics, edge) ||
			GeometryBackgroundBorderPattern::InnerCornerRounded(metrics, (edge + 1) % 4);
		any_curved = any_curved || (patterned[edge] && curved[edge]);
	}

	Centerline closed_line;
	float side_end[4] = {};
	if (any_curved)
	{
		CenterlineCorner corners[4];
		for (int corner = 0; corner < 4; corner++)
			corners[corner] = GetCenterlineCorner(metrics, edge_sizes, corner);

		closed_line.Add(corners[TOP_LEFT].center + Vector2f(0.f, -corners[TOP_LEFT].radii.y));
		AppendCenterlineArc(closed_line, corners[TOP_RIGHT], 1.5f * pi, 1.75f * pi);
		side_end[TOP] = closed_line.Length();
		AppendCenterlineArc(closed_line, corners[TOP_RIGHT], 1.75f * pi, 2.f * pi);
		AppendCenterlineArc(closed_line, corners[BOTTOM_RIGHT], 0.f, 0.25f * pi);
		side_end[RIGHT] = closed_line.Length();
		AppendCenterlineArc(closed_line, corners[BOTTOM_RIGHT], 0.25f * pi, 0.5f * pi);
		AppendCenterlineArc(closed_line, corners[BOTTOM_LEFT], 0.5f * pi, 0.75f * pi);
		side_end[BOTTOM] = closed_line.Length();
		AppendCenterlineArc(closed_line, corners[BOTTOM_LEFT], 0.75f * pi, pi);
		AppendCenterlineArc(closed_line, corners[TOP_LEFT], pi, 1.25f * pi);
		side_end[LEFT] = closed_line.Length();
		AppendCenterlineArc(closed_line, corners[TOP_LEFT], 1.25f * pi, 1.5f * pi);
		closed_line.Add(closed_line.points.front());
	}

	for (int edge = 0; edge < 4; edge++)
	{
		if (!patterned[edge])
			continue;

		const float width = edge_sizes[edge];
		const bool dotted = (border_styles[edge] == BorderStyle::Dotted);
		const ColourbPremultiplied color = border_colors[edge];

		if (curved[edge])
		{
			// The closed centerline starts where the top edge leaves the top-left corner, so the top side also owns its end.
			const float length = closed_line.Length();
			Pattern pattern;
			if (closed_line.points.size() < 2 || !GeometryBackgroundBorderPattern::FitPattern(length, width, dotted, true, pattern))
				continue;

			const float ranges[2][2] = {
				{edge == TOP ? 0.f : side_end[(edge + 3) % 4], side_end[edge]},
				{edge == TOP ? side_end[LEFT] : 0.f, edge == TOP ? length : 0.f},
			};

			if (pattern.round)
			{
				for (int i = 0; i < pattern.num_dots; i++)
				{
					const float distance = float(i) * pattern.period;
					const bool inside = (distance >= ranges[0][0] && distance < ranges[0][1]) || (distance >= ranges[1][0] && distance < ranges[1][1]);
					if (!inside)
						continue;
					int segment = 0;
					GeometryBackgroundBorderPattern::EmitDot(vertices, indices, closed_line.PointAt(distance, segment), 0.5f * width, {}, color);
				}
			}
			else
			{
				for (float begin = 0.f; begin < length - 1e-2f; begin += pattern.period)
				{
					const float end = Math::Min(begin + pattern.dash, length);
					for (int range = 0; range < 2; range++)
					{
						GeometryBackgroundBorderPattern::EmitCenterlinePiece(vertices, indices, closed_line, Math::Max(begin, ranges[range][0]),
							Math::Min(end, ranges[range][1]), 0.5f * width, color);
					}
				}
			}
			out_drawn[edge] = true;
			continue;
		}

		// Chromium box_border_painter.cc:582-596 draws straight sides from the top or left end.
		const bool reversed = (edge == BOTTOM || edge == LEFT);
		const Vector2f outer_start = metrics.positions_outer[reversed ? (edge + 1) % 4 : edge];
		const Vector2f outer_end = metrics.positions_outer[reversed ? edge : (edge + 1) % 4];
		const Vector2f inner_start = metrics.positions_inner[reversed ? (edge + 1) % 4 : edge];
		const Vector2f inner_end = metrics.positions_inner[reversed ? edge : (edge + 1) % 4];
		const float length = (outer_end - outer_start).Magnitude();

		Pattern pattern;
		if (length <= 0.f || !GeometryBackgroundBorderPattern::FitPattern(length, width, dotted, false, pattern))
			continue;

		const Vector2f direction = (outer_end - outer_start) / length;
		const Vector2f inward = Vector2f(-direction.y, direction.x) * (reversed ? -width : width);
		const Vector<Vector2f> trapezoid = {outer_start, outer_end, inner_end, inner_start};

		if (pattern.round)
		{
			for (int i = 0; i < pattern.num_dots; i++)
			{
				const Vector2f center = outer_start + direction * (0.5f * width + float(i) * pattern.period) + inward * 0.5f;
				GeometryBackgroundBorderPattern::EmitDot(vertices, indices, center, 0.5f * width, trapezoid, color);
			}
		}
		else
		{
			GeometryBackgroundBorderPattern::EndpointDots dots;
			dots.line_end = length;
			if (dotted)
				dots = GeometryBackgroundBorderPattern::EnforceDotsAtEndpoints(length, Math::Clamp(Math::RoundToInteger(width), 1, 3));

			if (dots.start_dot)
				GeometryBackgroundBorderPattern::EmitSideRun(vertices, indices, outer_start, direction, inward, trapezoid, 0.f,
					Math::Min(dots.start_dot_length, length), color);
			for (float begin = dots.line_begin; begin < dots.line_end - 1e-2f; begin += pattern.period)
				GeometryBackgroundBorderPattern::EmitSideRun(vertices, indices, outer_start, direction, inward, trapezoid, begin,
					Math::Min(begin + pattern.dash, dots.line_end), color);
			if (dots.end_dot)
				GeometryBackgroundBorderPattern::EmitSideRun(vertices, indices, outer_start, direction, inward, trapezoid,
					Math::Max(length - dots.end_dot_length, 0.f), length, color);
		}
		out_drawn[edge] = true;
	}
}

} // namespace Rml
