//
// zxdrag-bar
// by stx4
//

#include <stdlib.h>

#include <cairo.h>
#include <cairo-xcb.h>

#include <xcb/xcb.h>
#include <xcb/xcb_aux.h>

#define WIDTH      128
#define HEIGHT     32
#define PADDING    4
#define BACKGROUND 0x282828
#define FOREGROUND 0xFBF1C7

#define FONT      "Sans"
#define FONT_SIZE 13

static_assert(WIDTH  > 0, "width must be a valid X11 window size");
static_assert(HEIGHT > 0, "height must be a valid X11 window size");
static_assert(PADDING >= 0, "don't clip your bar through the screen");
static_assert(BACKGROUND < 0xFFFFFF, "background must be 24-bit color");
static_assert(FOREGROUND < 0xFFFFFF, "foreground must be 24-bit color");

int main(void) {
	// connect to X server
	auto default_screen = -1;
	xcb_connection_t* conn = xcb_connect(NULL, &default_screen);
	if (xcb_connection_has_error(conn) < 0 || default_screen < 0)
		return 1;
	xcb_screen_t* scr = xcb_aux_get_screen(conn, default_screen);
	xcb_visualtype_t* visual = xcb_aux_get_visualtype(conn, 
		default_screen, scr->root_visual);

	// create our window
	auto x = scr->width_in_pixels  - WIDTH  - PADDING;
	auto y = scr->height_in_pixels - HEIGHT - PADDING;

	uint32_t values[2] = { BACKGROUND, true };
	xcb_window_t wnd = xcb_generate_id(conn);
	xcb_create_window(conn, XCB_COPY_FROM_PARENT, wnd, scr->root,
		x, y, WIDTH, HEIGHT, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT, scr->root_visual,
		XCB_CW_BACK_PIXEL | XCB_CW_OVERRIDE_REDIRECT, values);
	xcb_map_window(conn, wnd);
	xcb_change_property(conn, XCB_PROP_MODE_REPLACE, wnd, XCB_ATOM_WM_NAME,
		XCB_ATOM_STRING, 8, 5, "zxbar");

	// init cairo on window
	cairo_surface_t* surface = cairo_xcb_surface_create(conn, wnd, 
		visual, WIDTH, HEIGHT);
	cairo_t* cr = cairo_create(surface);
	// set font settings
	cairo_select_font_face(cr, FONT, 
		CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
	cairo_set_font_size(cr, FONT_SIZE);

	// push init
	xcb_flush(conn);

	// expose ourselves post-flush (window ready)
	xcb_clear_area(conn, 1, wnd, 0, 0, WIDTH, HEIGHT);
	
	xcb_generic_event_t* event;
	while ((event = xcb_wait_for_event(conn))) {
		switch (event->response_type & ~0x80) {
		// draw bar window
		case XCB_EXPOSE: {
			xcb_clear_area(conn, 0, wnd, 0, 0, 0, 0);
			cairo_move_to(cr, 0, 0);
			cairo_show_text(cr, "Fuck you");
			cairo_surface_flush(surface);
		}
		break;
		}
		free(event);
		xcb_flush(conn);
	}

	cairo_destroy(cr);
	cairo_surface_destroy(surface);

	xcb_disconnect(conn);
	return 0;
}
