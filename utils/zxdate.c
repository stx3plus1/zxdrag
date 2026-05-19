//
// zxdrag-bar
// by stx4
//

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <cairo.h>
#include <cairo-xcb.h>

#include <xcb/xcb.h>
#include <xcb/xcb_aux.h>

#define WIDTH      160
#define HEIGHT     32
#define PADDING    4
#define BACKGROUND 0x282828
#define FOREGROUND 0xFBF1C7

#define FONT       "Departure Mono"
#define FONT_SIZE  11

#define REFRESH    2.0

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

	uint16_t mask = XCB_CW_BACK_PIXEL | XCB_CW_OVERRIDE_REDIRECT |
		XCB_CW_EVENT_MASK;
	uint32_t values[3] = { BACKGROUND, true, XCB_EVENT_MASK_EXPOSURE };
	xcb_window_t wnd = xcb_generate_id(conn);
	xcb_create_window(conn, XCB_COPY_FROM_PARENT, wnd, scr->root,
		x, y, WIDTH, HEIGHT, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT, 
		scr->root_visual, mask, values);
	xcb_map_window(conn, wnd);
	xcb_change_property(conn, XCB_PROP_MODE_REPLACE, wnd, XCB_ATOM_WM_NAME,
		XCB_ATOM_STRING, 8, 5, "zxbar");

	// init cairo on window
	cairo_surface_t* surface = cairo_xcb_surface_create(conn, wnd, 
		visual, WIDTH, HEIGHT);

	// push init
	xcb_flush(conn);

	// clear post-flush for ready window
	xcb_clear_area(conn, true, wnd, 0, 0, WIDTH, HEIGHT);

	xcb_generic_event_t* event;
	time_t last = time(NULL), current;

	while (1) {
		current = time(NULL);
		if (difftime(current, last) >= REFRESH) {
			xcb_clear_area(conn, true, wnd, 0, 0, WIDTH, HEIGHT);
			xcb_flush(conn);
			last = current;
		}

		event = xcb_poll_for_event(conn);
		if (!event) {
			usleep(1000);
			continue;
		}

		switch (event->response_type & ~0x80) {
		// redraw entire bar window
		case XCB_EXPOSE: {
			xcb_clear_area(conn, false, wnd, 0, 0, 0, 0);

			cairo_t* cr = cairo_create(surface);
			cairo_select_font_face(cr, FONT, 
				CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
			cairo_set_font_size(cr, FONT_SIZE);
			cairo_set_source_rgb(cr, (FOREGROUND >> 16) & 0xFF,
				(FOREGROUND >> 8) & 0xFF, FOREGROUND & 0xFF);

			time_t t      = time(NULL);
			struct tm* tm = localtime(&t);

			char* date = (char*)malloc(64 * sizeof(char));
			strftime(date, 63, "%H:%M %d/%m/%Y", tm);

			cairo_text_extents_t extents;
			cairo_text_extents(cr, date, &extents);

			// 80x24 compat strikes again
			auto x = (WIDTH  / 2) - (extents.width  / 2) -extents.x_bearing;
			auto y = (HEIGHT / 2) - (extents.height / 2) -extents.y_bearing;
			cairo_move_to(cr, x, y);
			cairo_show_text(cr, date);

			cairo_surface_flush(surface);

			free(date);
			cairo_destroy(cr);
		}
		break;
		}
		free(event);
		xcb_flush(conn);
	}

	cairo_surface_destroy(surface);
	xcb_disconnect(conn);
	return 0;
}
