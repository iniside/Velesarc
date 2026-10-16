#pragma once

#include "Animation.h"
#include "Element.h"
#include "RenderBox.h"
#include "StyleTypes.h"
#include "Types.h"
#include <cfloat>

namespace Rml {
namespace Style {

	/*
	    A computed value is a value resolved as far as possible :before: introducing layouting. See CSS specs for details of each property.

	    Note: Enums and default values must correspond to the keywords and defaults in `StyleSheetSpecification.cpp`.
	*/

	struct CommonValues {
		CommonValues() :
			display(Display::Inline), position(Position::Static), float_(Float::None), clear(Clear::None), overflow_x(Overflow::Visible),
			overflow_y(Overflow::Visible), visibility(Visibility::Visible),

			has_decorator(false), box_sizing(BoxSizing::ContentBox),

			width_type(LengthPercentageAuto::Auto), height_type(LengthPercentageAuto::Auto),

			margin_top_type(LengthPercentageAuto::Length), margin_right_type(LengthPercentageAuto::Length),
			margin_bottom_type(LengthPercentageAuto::Length), margin_left_type(LengthPercentageAuto::Length),

			padding_top_type(LengthPercentage::Length), padding_right_type(LengthPercentage::Length), padding_bottom_type(LengthPercentage::Length),
			padding_left_type(LengthPercentage::Length),

			top_type(LengthPercentageAuto::Auto), right_type(LengthPercentageAuto::Auto), bottom_type(LengthPercentageAuto::Auto),
			left_type(LengthPercentageAuto::Auto), z_index_type(NumberAuto::Auto)
		{}

		Display display : 5;
		Position position : 2;

		Float float_ : 2;
		Clear clear : 2;

		Overflow overflow_x : 2, overflow_y : 2;
		Visibility visibility : 1;

		bool has_decorator : 1;
		BoxSizing box_sizing : 1;

		LengthPercentageAuto::Type width_type : 2, height_type : 2;

		LengthPercentageAuto::Type margin_top_type : 2, margin_right_type : 2, margin_bottom_type : 2, margin_left_type : 2;
		LengthPercentage::Type padding_top_type : 2, padding_right_type : 2, padding_bottom_type : 2, padding_left_type : 2;
		LengthPercentageAuto::Type top_type : 2, right_type : 2, bottom_type : 2, left_type : 2;

		NumberAuto::Type z_index_type : 1;

		float width_value = 0;
		float height_value = 0;

		float margin_top_value = 0;
		float margin_right_value = 0;
		float margin_bottom_value = 0;
		float margin_left_value = 0;

		float padding_top_value = 0;
		float padding_right_value = 0;
		float padding_bottom_value = 0;
		float padding_left_value = 0;

		float top_value = 0;
		float right_value = 0;
		float bottom_value = 0;
		float left_value = 0;

		float z_index_value = 0;

		uint16_t border_top_width = 0, border_right_width = 0, border_bottom_width = 0, border_left_width = 0;

		Colourb border_top_color{255, 255, 255}, border_right_color{255, 255, 255}, border_bottom_color{255, 255, 255},
			border_left_color{255, 255, 255};

		Colourb background_color = Colourb(0, 0, 0, 0);
	};

	struct InheritedValues {
		InheritedValues() :
			font_weight(FontWeight::Normal), font_kerning(FontKerning::Auto), has_letter_spacing(0), font_style(FontStyle::Normal),
			has_font_effect(false), pointer_events(PointerEvents::Auto), focus(Focus::Auto), text_align(TextAlign::Left),
			text_decoration(TextDecoration::None), text_transform(TextTransform::None), white_space(WhiteSpace::Normal),
			word_break(WordBreak::Normal), direction(Direction::Auto), line_height_inherit_type(LineHeight::Normal)
		{}

		// Font face used to render text and resolve ex properties. Does not represent a true property
		// like most computed values, but placed here as it is used and inherited in a similar manner.
		FontFaceHandle font_face_handle = 0;

		float font_size = 12.f;

		float opacity = 1;
		Colourb color = Colourb(255, 255, 255);

		FontWeight font_weight : 10;
		FontKerning font_kerning : 2;
		uint16_t has_letter_spacing : 1;

		FontStyle font_style : 1;
		bool has_font_effect : 1;
		PointerEvents pointer_events : 1;
		Focus focus : 1;

		TextAlign text_align : 2;
		TextDecoration text_decoration : 2;
		TextTransform text_transform : 2;
		WhiteSpace white_space : 3;
		WordBreak word_break : 2;

		Direction direction : 2;

		LineHeight::InheritType line_height_inherit_type : 2;
		float line_height = 12.f * LineHeight::NormalFallbackFactor;
		float line_height_inherit = 0.f;

		String language = "";
	};

	struct RareValues {
		RareValues() :
			min_width_type(LengthPercentage::Length), max_width_type(LengthPercentage::Length), min_height_type(LengthPercentage::Length),
			max_height_type(LengthPercentage::Length),

			perspective_origin_x_type(LengthPercentage::Percentage), perspective_origin_y_type(LengthPercentage::Percentage),
			transform_origin_x_type(LengthPercentage::Percentage), transform_origin_y_type(LengthPercentage::Percentage), has_local_transform(false),
			has_local_perspective(false),

			flex_basis_type(LengthPercentageAuto::Auto), row_gap_type(LengthPercentage::Length), column_gap_type(LengthPercentage::Length),

			border_top_left_radius_type(LengthPercentage::Length), border_top_right_radius_type(LengthPercentage::Length),
			border_bottom_right_radius_type(LengthPercentage::Length), border_bottom_left_radius_type(LengthPercentage::Length),

			vertical_align_type(VerticalAlign::Baseline), drag(Drag::None), tab_index(TabIndex::None), overscroll_behavior(OverscrollBehavior::Auto),

			has_mask_image(false), has_filter(false), has_backdrop_filter(false), has_box_shadow(false), text_overflow(TextOverflow::Clip),
			border_top_style(BorderStyle::None), border_right_style(BorderStyle::None), border_bottom_style(BorderStyle::None),
			border_left_style(BorderStyle::None)
		{}

		LengthPercentage::Type min_width_type : 2, max_width_type : 2;
		LengthPercentage::Type min_height_type : 2, max_height_type : 2;

		LengthPercentage::Type perspective_origin_x_type : 1, perspective_origin_y_type : 1;
		LengthPercentage::Type transform_origin_x_type : 1, transform_origin_y_type : 1;
		bool has_local_transform : 1, has_local_perspective : 1;

		LengthPercentageAuto::Type flex_basis_type : 2;
		LengthPercentage::Type row_gap_type : 2, column_gap_type : 2;

		LengthPercentage::Type border_top_left_radius_type : 2, border_top_right_radius_type : 2;
		LengthPercentage::Type border_bottom_right_radius_type : 2, border_bottom_left_radius_type : 2;

		VerticalAlign::Type vertical_align_type : 4;
		Drag drag : 3;
		TabIndex tab_index : 1;
		OverscrollBehavior overscroll_behavior : 1;

		bool has_mask_image : 1;
		bool has_filter : 1;
		bool has_backdrop_filter : 1;
		bool has_box_shadow : 1;

		TextOverflow text_overflow : 2;

		BorderStyle border_top_style : 2, border_right_style : 2, border_bottom_style : 2, border_left_style : 2;

		Clip clip;

		float min_width = 0, max_width = FLT_MAX;
		float min_height = 0, max_height = FLT_MAX;
		float vertical_align_length = 0;

		float perspective = 0;
		float perspective_origin_x = 50.f;
		float perspective_origin_y = 50.f;

		float transform_origin_x = 50.f;
		float transform_origin_y = 50.f;
		float transform_origin_z = 0.0f;

		float flex_basis = 0;
		float row_gap = 0, column_gap = 0;

		float border_top_left_radius = 0, border_top_right_radius = 0, border_bottom_right_radius = 0, border_bottom_left_radius = 0;
		Colourb image_color = Colourb(255, 255, 255);
		float scrollbar_margin = 0.f;
	};

	class ComputedValues : NonCopyMoveable {
	public:
		explicit ComputedValues(Element* element) : element(element) {}

		// clang-format off

		// -- Common --
		LengthPercentageAuto width()               const { return MakeLengthPercentageAuto(common.width_type, common.width_value); }
		LengthPercentageAuto height()              const { return MakeLengthPercentageAuto(common.height_type, common.height_value); }
		LengthPercentageAuto margin_top()          const { return MakeLengthPercentageAuto(common.margin_top_type, common.margin_top_value); }
		LengthPercentageAuto margin_right()        const { return MakeLengthPercentageAuto(common.margin_right_type, common.margin_right_value); }
		LengthPercentageAuto margin_bottom()       const { return MakeLengthPercentageAuto(common.margin_bottom_type, common.margin_bottom_value); }
		LengthPercentageAuto margin_left()         const { return MakeLengthPercentageAuto(common.margin_left_type, common.margin_left_value); }
		LengthPercentage     padding_top()         const { return MakeLengthPercentage(common.padding_top_type, common.padding_top_value); }
		LengthPercentage     padding_right()       const { return MakeLengthPercentage(common.padding_right_type, common.padding_right_value); }
		LengthPercentage     padding_bottom()      const { return MakeLengthPercentage(common.padding_bottom_type, common.padding_bottom_value); }
		LengthPercentage     padding_left()        const { return MakeLengthPercentage(common.padding_left_type, common.padding_left_value); }
		LengthPercentageAuto top()                 const { return MakeLengthPercentageAuto(common.top_type, common.top_value); }
		LengthPercentageAuto right()               const { return MakeLengthPercentageAuto(common.right_type, common.right_value); }
		LengthPercentageAuto bottom()              const { return MakeLengthPercentageAuto(common.bottom_type, common.bottom_value); }
		LengthPercentageAuto left()                const { return MakeLengthPercentageAuto(common.left_type, common.left_value); }
		NumberAuto           z_index()             const { return NumberAuto(common.z_index_type, common.z_index_value); }
		float                border_top_width()    const { return (float)common.border_top_width; }
		float                border_right_width()  const { return (float)common.border_right_width; }
		float                border_bottom_width() const { return (float)common.border_bottom_width; }
		float                border_left_width()   const { return (float)common.border_left_width; }
		BoxSizing            box_sizing()          const { return common.box_sizing; }
		Display              display()             const { return common.display; }
		Position             position()            const { return common.position; }
		Float                float_()              const { return common.float_; }
		Clear                clear()               const { return common.clear; }
		Overflow             overflow_x()          const { return common.overflow_x; }
		Overflow             overflow_y()          const { return common.overflow_y; }
		Visibility           visibility()          const { return common.visibility; }
		Colourb              background_color()    const { return common.background_color; }
		Colourb              border_top_color()    const { return common.border_top_color; }
		Colourb              border_right_color()  const { return common.border_right_color; }
		Colourb              border_bottom_color() const { return common.border_bottom_color; }
		Colourb              border_left_color()   const { return common.border_left_color; }
		bool                 has_decorator()       const { return common.has_decorator; }

		// -- Inherited --
		String         font_family()      const;
		String         cursor()           const;
		FontFaceHandle font_face_handle() const { return inherited.font_face_handle; }
		float          font_size()        const { return inherited.font_size; }
		float          letter_spacing()   const;
		bool           has_font_effect()  const { return inherited.has_font_effect; }
		FontStyle      font_style()       const { return inherited.font_style; }
		FontWeight     font_weight()      const { return inherited.font_weight; }
		FontKerning    font_kerning()     const { return inherited.font_kerning; }
		PointerEvents  pointer_events()   const { return inherited.pointer_events; }
		Focus          focus()            const { return inherited.focus; }
		TextAlign      text_align()       const { return inherited.text_align; }
		TextDecoration text_decoration()  const { return inherited.text_decoration; }
		TextTransform  text_transform()   const { return inherited.text_transform; }
		WhiteSpace     white_space()      const { return inherited.white_space; }
		WordBreak      word_break()       const { return inherited.word_break; }
		Colourb        color()            const { return inherited.color; }
		float          opacity()          const { return inherited.opacity; }
		LineHeight     line_height()      const { return LineHeight(inherited.line_height, inherited.line_height_inherit_type, inherited.line_height_inherit); }
		const String&  language()         const { return inherited.language; }
		Direction      direction()        const { return inherited.direction; }

		// -- Rare --
		MinWidth          min_width()                  const { return MakeLengthPercentage(rare.min_width_type, rare.min_width); }
		MaxWidth          max_width()                  const { return MakeLengthPercentage(rare.max_width_type, rare.max_width); }
		MinHeight         min_height()                 const { return MakeLengthPercentage(rare.min_height_type, rare.min_height); }
		MaxHeight         max_height()                 const { return MakeLengthPercentage(rare.max_height_type, rare.max_height); }
		VerticalAlign     vertical_align()             const { return VerticalAlign(rare.vertical_align_type, rare.vertical_align_length); }
		const             AnimationList* animation()   const;
		const             TransitionList* transition() const;
		float             perspective()                const { return rare.perspective; }
		PerspectiveOrigin perspective_origin_x()       const { return LengthPercentage(rare.perspective_origin_x_type, rare.perspective_origin_x); }
		PerspectiveOrigin perspective_origin_y()       const { return LengthPercentage(rare.perspective_origin_y_type, rare.perspective_origin_y); }
		TransformPtr      transform()                  const { return GetLocalProperty(PropertyId::Transform, TransformPtr()); }
		TransformOrigin   transform_origin_x()         const { return LengthPercentage(rare.transform_origin_x_type, rare.transform_origin_x); }
		TransformOrigin   transform_origin_y()         const { return LengthPercentage(rare.transform_origin_y_type, rare.transform_origin_y); }
		float             transform_origin_z()         const { return rare.transform_origin_z; }
		bool              has_local_transform()        const { return rare.has_local_transform; }
		bool              has_local_perspective()      const { return rare.has_local_perspective; }
		AlignContent      align_content()              const { return GetLocalPropertyKeyword(PropertyId::AlignContent, AlignContent::Stretch); }
		AlignItems        align_items()                const { return GetLocalPropertyKeyword(PropertyId::AlignItems, AlignItems::Normal); }
		AlignSelf         align_self()                 const { return GetLocalPropertyKeyword(PropertyId::AlignSelf, AlignSelf::Auto); }
		FlexDirection     flex_direction()             const { return GetLocalPropertyKeyword(PropertyId::FlexDirection, FlexDirection::Row); }
		FlexWrap          flex_wrap()                  const { return GetLocalPropertyKeyword(PropertyId::FlexWrap, FlexWrap::Nowrap); }
		JustifyContent    justify_content()            const { return GetLocalPropertyKeyword(PropertyId::JustifyContent, JustifyContent::Normal); }
		AlignItems        justify_items()              const { return GetLocalPropertyKeyword(PropertyId::JustifyItems, AlignItems::Normal); }
		AlignSelf         justify_self()               const { return GetLocalPropertyKeyword(PropertyId::JustifySelf, AlignSelf::Auto); }
		GridAutoFlow      grid_auto_flow()             const { return GetLocalPropertyKeyword(PropertyId::GridAutoFlow, GridAutoFlow::Row); }
		GridDefinitionPtr grid_template_columns()      const { return GetLocalProperty(PropertyId::GridTemplateColumns, GridDefinitionPtr()); }
		GridDefinitionPtr grid_template_rows()         const { return GetLocalProperty(PropertyId::GridTemplateRows, GridDefinitionPtr()); }
		GridDefinitionPtr grid_template_areas()        const { return GetLocalProperty(PropertyId::GridTemplateAreas, GridDefinitionPtr()); }
		GridDefinitionPtr grid_auto_columns()          const { return GetLocalProperty(PropertyId::GridAutoColumns, GridDefinitionPtr()); }
		GridDefinitionPtr grid_auto_rows()             const { return GetLocalProperty(PropertyId::GridAutoRows, GridDefinitionPtr()); }
		GridDefinitionPtr grid_column_start()          const { return GetLocalProperty(PropertyId::GridColumnStart, GridDefinitionPtr()); }
		GridDefinitionPtr grid_column_end()            const { return GetLocalProperty(PropertyId::GridColumnEnd, GridDefinitionPtr()); }
		GridDefinitionPtr grid_row_start()             const { return GetLocalProperty(PropertyId::GridRowStart, GridDefinitionPtr()); }
		GridDefinitionPtr grid_row_end()               const { return GetLocalProperty(PropertyId::GridRowEnd, GridDefinitionPtr()); }
		AspectRatio       aspect_ratio()               const { const Vector2f v = GetLocalProperty(PropertyId::AspectRatio, Vector2f(0.f, 0.f)); return {v.x, v.y != 0.f}; }
		float             flex_grow()                  const { return GetLocalProperty(PropertyId::FlexGrow, 0.f); }
		float             flex_shrink()                const { return GetLocalProperty(PropertyId::FlexShrink, 1.f); }
		FlexBasis         flex_basis()                 const { return MakeLengthPercentageAuto(rare.flex_basis_type, rare.flex_basis); }
		LengthPercentage  border_top_left_radius()     const { return MakeLengthPercentage(rare.border_top_left_radius_type, rare.border_top_left_radius); }
		LengthPercentage  border_top_right_radius()    const { return MakeLengthPercentage(rare.border_top_right_radius_type, rare.border_top_right_radius); }
		LengthPercentage  border_bottom_right_radius() const { return MakeLengthPercentage(rare.border_bottom_right_radius_type, rare.border_bottom_right_radius); }
		LengthPercentage  border_bottom_left_radius()  const { return MakeLengthPercentage(rare.border_bottom_left_radius_type, rare.border_bottom_left_radius); }
		// Resolves the corner radii, percentages refer to the smaller side of the border box since corners are circular.
		CornerSizes       border_radius(Vector2f border_box_size) const;
		bool              has_border_radius()          const;
		TextOverflow      text_overflow()              const { return rare.text_overflow; }
		String            text_overflow_string()       const { return GetLocalProperty(PropertyId::TextOverflow, String());; }
		Clip              clip()                       const { return rare.clip; }
		Drag              drag()                       const { return rare.drag; }
		TabIndex          tab_index()                  const { return rare.tab_index; }
		Colourb           image_color()                const { return rare.image_color; }
		LengthPercentage  row_gap()                    const { return MakeLengthPercentage(rare.row_gap_type, rare.row_gap); }
		LengthPercentage  column_gap()                 const { return MakeLengthPercentage(rare.column_gap_type, rare.column_gap); }
		OverscrollBehavior overscroll_behavior()       const { return rare.overscroll_behavior; }
		float             scrollbar_margin()           const { return rare.scrollbar_margin; }
		bool              has_mask_image()             const { return rare.has_mask_image; }
		bool              has_filter()                 const { return rare.has_filter; }
		bool              has_backdrop_filter()        const { return rare.has_backdrop_filter; }
		bool              has_box_shadow()             const { return rare.has_box_shadow; }
		BorderStyle       border_top_style()           const { return rare.border_top_style; }
		BorderStyle       border_right_style()         const { return rare.border_right_style; }
		BorderStyle       border_bottom_style()        const { return rare.border_bottom_style; }
		BorderStyle       border_left_style()          const { return rare.border_left_style; }

		// -- Assignment --
		// Common
		void width              (LengthPercentageAuto value) { common.width_type          = value.type; common.width_value          = StoreCalc(std::move(value)); }
		void height             (LengthPercentageAuto value) { common.height_type         = value.type; common.height_value         = StoreCalc(std::move(value)); }
		void margin_top         (LengthPercentageAuto value) { common.margin_top_type     = value.type; common.margin_top_value     = StoreCalc(std::move(value)); }
		void margin_right       (LengthPercentageAuto value) { common.margin_right_type   = value.type; common.margin_right_value   = StoreCalc(std::move(value)); }
		void margin_bottom      (LengthPercentageAuto value) { common.margin_bottom_type  = value.type; common.margin_bottom_value  = StoreCalc(std::move(value)); }
		void margin_left        (LengthPercentageAuto value) { common.margin_left_type    = value.type; common.margin_left_value    = StoreCalc(std::move(value)); }
		void padding_top        (LengthPercentage value)     { common.padding_top_type    = value.type; common.padding_top_value    = StoreCalc(std::move(value)); }
		void padding_right      (LengthPercentage value)     { common.padding_right_type  = value.type; common.padding_right_value  = StoreCalc(std::move(value)); }
		void padding_bottom     (LengthPercentage value)     { common.padding_bottom_type = value.type; common.padding_bottom_value = StoreCalc(std::move(value)); }
		void padding_left       (LengthPercentage value)     { common.padding_left_type   = value.type; common.padding_left_value   = StoreCalc(std::move(value)); }
		void top                (LengthPercentageAuto value) { common.top_type            = value.type; common.top_value            = StoreCalc(std::move(value)); }
		void right              (LengthPercentageAuto value) { common.right_type          = value.type; common.right_value          = StoreCalc(std::move(value)); }
		void bottom             (LengthPercentageAuto value) { common.bottom_type         = value.type; common.bottom_value         = StoreCalc(std::move(value)); }
		void left               (LengthPercentageAuto value) { common.left_type           = value.type; common.left_value           = StoreCalc(std::move(value)); }
		void z_index            (NumberAuto value)           { common.z_index_type        = value.type; common.z_index_value        = value.value; }
		void border_top_width   (int16_t value)              { common.border_top_width    = value; }
		void border_right_width (int16_t value)              { common.border_right_width  = value; }
		void border_bottom_width(int16_t value)              { common.border_bottom_width = value; }
		void border_left_width  (int16_t value)              { common.border_left_width   = value; }
		void box_sizing         (BoxSizing value)            { common.box_sizing          = value; }
		void display            (Display value)              { common.display             = value; }
		void position           (Position value)             { common.position            = value; }
		void float_             (Float value)                { common.float_              = value; }
		void clear              (Clear value)                { common.clear               = value; }
		void overflow_x         (Overflow value)             { common.overflow_x          = value; }
		void overflow_y         (Overflow value)             { common.overflow_y          = value; }
		void visibility         (Visibility value)           { common.visibility          = value; }
		void background_color   (Colourb value)              { common.background_color    = value; }
		void border_top_color   (Colourb value)              { common.border_top_color    = value; }
		void border_right_color (Colourb value)              { common.border_right_color  = value; }
		void border_bottom_color(Colourb value)              { common.border_bottom_color = value; }
		void border_left_color  (Colourb value)              { common.border_left_color   = value; }
		void has_decorator      (bool value)                 { common.has_decorator       = value; }
		// Inherited
		void font_face_handle  (FontFaceHandle value) { inherited.font_face_handle   = value; }
		void font_size         (float value)          { inherited.font_size          = value; }
		void has_letter_spacing(bool value)           { inherited.has_letter_spacing = value; }
		void has_font_effect   (bool value)           { inherited.has_font_effect    = value; }
		void font_style        (FontStyle value)      { inherited.font_style         = value; }
		void font_weight       (FontWeight value)     { inherited.font_weight        = value; }
		void font_kerning      (FontKerning value)    { inherited.font_kerning       = value; }
		void pointer_events    (PointerEvents value)  { inherited.pointer_events     = value; }
		void focus             (Focus value)          { inherited.focus              = value; }
		void text_align        (TextAlign value)      { inherited.text_align         = value; }
		void text_decoration   (TextDecoration value) { inherited.text_decoration    = value; }
		void text_transform    (TextTransform value)  { inherited.text_transform     = value; }
		void white_space       (WhiteSpace value)     { inherited.white_space        = value; }
		void word_break        (WordBreak value)      { inherited.word_break         = value; }
		void color             (Colourb value)        { inherited.color              = value; }
		void opacity           (float value)          { inherited.opacity            = value; }
		void line_height       (LineHeight value)     { inherited.line_height = value.value; inherited.line_height_inherit_type = value.inherit_type; inherited.line_height_inherit = value.inherit_value;  }
		void language          (const String& value)  { inherited.language           = value; }
		void direction         (Direction value)      { inherited.direction          = value; }
		// Rare
		void min_width                 (MinWidth value)          { rare.min_width_type             = value.type; rare.min_width                  = StoreCalc(std::move(value)); }
		void max_width                 (MaxWidth value)          { rare.max_width_type             = value.type; rare.max_width                  = StoreCalc(std::move(value)); }
		void min_height                (MinHeight value)         { rare.min_height_type            = value.type; rare.min_height                 = StoreCalc(std::move(value)); }
		void max_height                (MaxHeight value)         { rare.max_height_type            = value.type; rare.max_height                 = StoreCalc(std::move(value)); }
		void vertical_align            (VerticalAlign value)     { rare.vertical_align_type        = value.type; rare.vertical_align_length      = value.value; }
		void perspective_origin_x      (PerspectiveOrigin value) { rare.perspective_origin_x_type  = value.type; rare.perspective_origin_x       = value.value; }
		void perspective_origin_y      (PerspectiveOrigin value) { rare.perspective_origin_y_type  = value.type; rare.perspective_origin_y       = value.value; }
		void transform_origin_x        (TransformOrigin value)   { rare.transform_origin_x_type    = value.type; rare.transform_origin_x         = value.value; }
		void transform_origin_y        (TransformOrigin value)   { rare.transform_origin_y_type    = value.type; rare.transform_origin_y         = value.value; }
		void row_gap                   (LengthPercentage value)  { rare.row_gap_type               = value.type; rare.row_gap                    = StoreCalc(std::move(value)); }
		void column_gap                (LengthPercentage value)  { rare.column_gap_type            = value.type; rare.column_gap                 = StoreCalc(std::move(value)); }
		void flex_basis                (FlexBasis value)         { rare.flex_basis_type            = value.type; rare.flex_basis                 = StoreCalc(std::move(value)); }
		void transform_origin_z        (float value)             { rare.transform_origin_z         = value; }
		void perspective               (float value)             { rare.perspective                = value; }
		void has_local_perspective     (bool value)              { rare.has_local_perspective      = value; }
		void has_local_transform       (bool value)              { rare.has_local_transform        = value; }
		void border_top_left_radius    (LengthPercentage value)  { rare.border_top_left_radius_type     = value.type; rare.border_top_left_radius     = StoreCalc(std::move(value)); }
		void border_top_right_radius   (LengthPercentage value)  { rare.border_top_right_radius_type    = value.type; rare.border_top_right_radius    = StoreCalc(std::move(value)); }
		void border_bottom_right_radius(LengthPercentage value)  { rare.border_bottom_right_radius_type = value.type; rare.border_bottom_right_radius = StoreCalc(std::move(value)); }
		void border_bottom_left_radius (LengthPercentage value)  { rare.border_bottom_left_radius_type  = value.type; rare.border_bottom_left_radius  = StoreCalc(std::move(value)); }
		void text_overflow             (TextOverflow value)      { rare.text_overflow              = value; }
		void clip                      (Clip value)              { rare.clip                       = value; }
		void drag                      (Drag value)              { rare.drag                       = value; }
		void tab_index                 (TabIndex value)          { rare.tab_index                  = value; }
		void image_color               (Colourb value)           { rare.image_color                = value; }
		void overscroll_behavior       (OverscrollBehavior value){ rare.overscroll_behavior        = value; }
		void scrollbar_margin          (float value)             { rare.scrollbar_margin           = value; }
		void has_mask_image            (bool value)              { rare.has_mask_image             = value; }
		void has_filter                (bool value)              { rare.has_filter                 = value; }
		void has_backdrop_filter       (bool value)              { rare.has_backdrop_filter        = value; }
		void has_box_shadow            (bool value)              { rare.has_box_shadow             = value; }
		void border_top_style          (BorderStyle value)       { rare.border_top_style           = value; }
		void border_right_style        (BorderStyle value)       { rare.border_right_style         = value; }
		void border_bottom_style       (BorderStyle value)       { rare.border_bottom_style        = value; }
		void border_left_style         (BorderStyle value)       { rare.border_left_style          = value; }

		// clang-format on

		// -- Management --
		void CopyNonInherited(const ComputedValues& other)
		{
			common = other.common;
			rare = other.rare;
			calc_values = other.calc_values;
		}
		void CopyInherited(const ComputedValues& parent) { inherited = parent.inherited; }

	private:
		template <typename T>
		inline T GetLocalPropertyKeyword(PropertyId id, T default_value) const
		{
			if (auto p = GetLocalPropertyWithResolvedVariables(id))
				return static_cast<T>(p->Get<int>());
			return default_value;
		}
		template <typename T>
		inline T GetLocalProperty(PropertyId id, T default_value) const
		{
			if (auto p = GetLocalPropertyWithResolvedVariables(id))
				return p->Get<T>();
			return default_value;
		}

		const Property* GetLocalPropertyWithResolvedVariables(PropertyId id) const;

		LengthPercentageAuto MakeLengthPercentageAuto(LengthPercentageAuto::Type type, float value) const
		{
			return type == LengthPercentageAuto::Calc ? LengthPercentageAuto(calc_values[size_t(value)]) : LengthPercentageAuto(type, value);
		}
		LengthPercentage MakeLengthPercentage(LengthPercentage::Type type, float value) const
		{
			return type == LengthPercentage::Calc ? LengthPercentage(calc_values[size_t(value)]) : LengthPercentage(type, value);
		}
		// The value field of a calc length stores its index in calc_values.
		template <typename T>
		float StoreCalc(T value)
		{
			if (value.type != T::Calc)
				return value.value;
			calc_values.push_back(std::move(value.calc));
			return float(calc_values.size() - 1);
		}

		Element* element = nullptr;

		CommonValues common;
		InheritedValues inherited;
		RareValues rare;
		Vector<CalcExpressionPtr> calc_values;
	};

} // namespace Style

// Resolves a computed LengthPercentage(Auto) value to the base unit 'px'.
// Percentages are scaled by the base value, if definite (>= 0), otherwise return the default value.
// Auto lengths always return the default value.
RMLUICORE_API float ResolveValueOr(Style::LengthPercentageAuto length, float base_value, float default_value);
RMLUICORE_API float ResolveValueOr(Style::LengthPercentage length, float base_value, float default_value);

RMLUICORE_API_INLINE float ResolveValue(Style::LengthPercentageAuto length, float base_value)
{
	return ResolveValueOr(length, base_value, 0.f);
}
RMLUICORE_API_INLINE float ResolveValue(Style::LengthPercentage length, float base_value)
{
	return ResolveValueOr(length, base_value, 0.f);
}

using ComputedValues = Style::ComputedValues;

} // namespace Rml
