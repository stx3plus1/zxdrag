//
// zxdrag
// by stx4
//

#include <spawn.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

#include <xcb/xcb.h>
#include <xcb/xproto.h>
#include <xcb/xcb_aux.h>

#define INIT "/.zxrc"
#define MOD_MASK XCB_MOD_MASK_1

#define BORDER_WIDTH 4
#define BORDER_COLOR 0xFC6712

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

extern char** environ;

int main(void) {
	// connect to X server
	int default_screen = -1;
	xcb_connection_t* conn = xcb_connect(NULL, &default_screen);
	if (xcb_connection_has_error(conn) < 0 || default_screen < 0)
		return 1;
	xcb_screen_t* scr = xcb_aux_get_screen(conn, default_screen);

	// add substructure notify to root
	uint32_t mask  = XCB_CW_EVENT_MASK;
	uint32_t value = XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY;
	xcb_change_window_attributes(conn, scr->root, mask, &value);

	// grab mouse l for click to raise
	xcb_grab_button(conn, 1, scr->root, 
		XCB_EVENT_MASK_BUTTON_PRESS,
		XCB_GRAB_MODE_SYNC, XCB_GRAB_MODE_ASYNC,
		XCB_NONE, XCB_NONE, XCB_BUTTON_INDEX_1, XCB_MOD_MASK_ANY
	);
	// grab mask + mouse l for move
	xcb_grab_button(conn, 1, scr->root, 
		XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_POINTER_MOTION,
		XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC,
		XCB_NONE, XCB_NONE, XCB_BUTTON_INDEX_1, MOD_MASK
	);
	// ''     '' + mouse r for resize
	xcb_grab_button(conn, 0, scr->root, 
		XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_POINTER_MOTION,
		XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC,
		XCB_NONE, XCB_NONE, XCB_BUTTON_INDEX_3, MOD_MASK
	);
	// ''     '' + f1 for raise
	xcb_grab_key(conn, 0, scr->root, 
		MOD_MASK, 67, // keycode
		XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC
	);

	// invisible focus window
	xcb_window_t invis_wnd = xcb_generate_id(conn);
	xcb_create_window(conn, XCB_COPY_FROM_PARENT, invis_wnd, scr->root,
		-1, -1, 1, 1, 0, XCB_WINDOW_CLASS_INPUT_ONLY, scr->root_visual,
		0, NULL);
	xcb_map_window(conn, invis_wnd);

	// push init
	xcb_flush(conn);

	// set name
	setenv("XDG_SESSION_TYPE",    "x11",    true);
	setenv("XDG_CURRENT_DESKTOP", "zxdrag", true);

	// run ~/.zxrc
	char* home = getenv("HOME");
	if (home) {
		char rc_path[strlen(home) + strlen(INIT) + 1];
		strcpy(rc_path, home);
		strcpy(rc_path + strlen(home), INIT);

		pid_t config_pid;
		posix_spawnp(&config_pid, rc_path, NULL, NULL, 
			(char* []){ rc_path, NULL }, environ);
		waitpid(config_pid, NULL, 0);
	}

	bool move;
	int start_x,     start_y,
		start_pos_x, start_pos_y, 
		start_w,     start_h;
	xcb_window_t drag_wnd,
	             raise_wnd;
	xcb_generic_event_t* event;

	while ((event = xcb_wait_for_event(conn))) {
		switch (event->response_type & ~0x80) {
		// raise invisible window when focus dies
		case XCB_UNMAP_NOTIFY: {
			xcb_unmap_notify_event_t* unmap_event = (void*)event;
			if (unmap_event->window != raise_wnd)
				break;
			raise_wnd = invis_wnd;
			goto raise_window;
		}
		// set up new windows
		case XCB_MAP_NOTIFY: {
			xcb_map_notify_event_t* map_event = (void*)event;
			raise_wnd = map_event->window;

			// should decorate and raise?
			xcb_get_window_attributes_reply_t* reply =
				xcb_get_window_attributes_reply(conn,
				xcb_get_window_attributes(conn, raise_wnd), NULL);
			if (!reply || reply->override_redirect)
				break;

			// set border width
			uint16_t mask  = XCB_CONFIG_WINDOW_BORDER_WIDTH;
			uint32_t value = BORDER_WIDTH;
			xcb_configure_window(conn, raise_wnd, mask, &value);

			// set border color
			mask  = XCB_CW_BORDER_PIXEL;
			value = BORDER_COLOR;
			xcb_change_window_attributes(conn, raise_wnd, mask, &value);

			// now raise it
			goto raise_window;
		}
		// raise window under cursor
		case XCB_KEY_PRESS: {
			xcb_key_press_event_t* key_event = (void*)event;
			raise_wnd = key_event->child;
		raise_window:
			if (raise_wnd == XCB_NONE)
				break;
			uint16_t mask  = XCB_CONFIG_WINDOW_STACK_MODE;
			uint32_t value = XCB_STACK_MODE_ABOVE;
			xcb_configure_window(conn, raise_wnd, mask, &value);
			xcb_set_input_focus(conn, XCB_INPUT_FOCUS_NONE, 
				raise_wnd, XCB_CURRENT_TIME);
		}
		break;
		// begin move/resize or raise depending on mod
		case XCB_BUTTON_PRESS: {
			xcb_button_press_event_t* button_event = (void*)event;
			if (!(button_event->state & MOD_MASK)) {
				xcb_allow_events(conn, XCB_ALLOW_REPLAY_POINTER,
					button_event->time);
				raise_wnd = button_event->child;
				goto raise_window;
			}

			drag_wnd = button_event->child;
			move     = (button_event->detail == XCB_BUTTON_INDEX_1);
			start_x  = button_event->root_x;
			start_y  = button_event->root_y;

			xcb_get_geometry_reply_t* reply = xcb_get_geometry_reply(conn, 
				xcb_get_geometry(conn, drag_wnd), NULL);
			if (!reply)
				break;
					
			start_pos_x = reply->x;
			start_pos_y = reply->y;

			start_w = reply->width;
			start_h = reply->height;

			free(reply);
		}
		break;
		// handle move/resize configuration
		case XCB_MOTION_NOTIFY: {
			xcb_motion_notify_event_t* motion_event = (void*)event;
			int	x_diff = motion_event->root_x - start_x;
			int y_diff = motion_event->root_y - start_y;

			uint16_t mask = move ? 
				(XCB_CONFIG_WINDOW_X     | XCB_CONFIG_WINDOW_Y) :
				(XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT);
			uint32_t values[2] = { 
				MIN(MAX(0, (move ? start_pos_x : start_w) + x_diff),
				scr->width_in_pixels - (move ? start_w + BORDER_WIDTH : 0)),
				MIN(MAX(0, (move ? start_pos_y : start_h) + y_diff), 
				scr->height_in_pixels - (move ? start_h + BORDER_WIDTH : 0))
			};
			xcb_configure_window(conn, drag_wnd, mask, &values);
		}
		break;
		}
		free(event);
		xcb_flush(conn);
    }

	xcb_destroy_window(conn, invis_wnd);
	xcb_disconnect(conn);
	return 0;
}

