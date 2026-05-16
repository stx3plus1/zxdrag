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

#define CONFIG "/.zxrc"
#define MAX(a,b) (((a)>(b))?(a):(b))

extern char** environ;

int main(void) {
	// connect to X server
	int default_screen = -1;
	xcb_connection_t* conn = xcb_connect(NULL, &default_screen);
	if (xcb_connection_has_error(conn) < 0 || default_screen < 0)
		return 1;
	xcb_screen_t* scr  = xcb_aux_get_screen(conn, default_screen);

	// add substructure notify to root
	uint32_t mask  = XCB_CW_EVENT_MASK;
	uint32_t value = XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY;
	xcb_change_window_attributes(conn, scr->root, mask, &value);

	// grab alt + mouse l for move
	xcb_grab_button(conn, 0, scr->root, 
		XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_POINTER_MOTION | XCB_EVENT_MASK_BUTTON_RELEASE,
		XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC,
		XCB_NONE, XCB_NONE, XCB_BUTTON_INDEX_1, XCB_MOD_MASK_1
	);
	// ''    '' + mouse r for resize
	xcb_grab_button(conn, 0, scr->root, 
		XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_POINTER_MOTION | XCB_EVENT_MASK_BUTTON_RELEASE,
		XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC,
		XCB_NONE, XCB_NONE, XCB_BUTTON_INDEX_3, XCB_MOD_MASK_1
	);

	// ''    '' + f1
	xcb_grab_key(conn, 0, scr->root, 
		XCB_MOD_MASK_1, 67, // keycode
		XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC
	);

	// push init
	xcb_flush(conn);

	// set name
	setenv("XDG_SESSION_TYPE",    "x11",    true);
	setenv("XDG_CURRENT_DESKTOP", "zxdrag", true);

	// run ~/.zxrc
	char* home = getenv("HOME");
	if (home) {
		char rc_path[strlen(home) + strlen(CONFIG) + 1];
		strcpy(rc_path, home);
		strcpy(rc_path + strlen(home), CONFIG);

		pid_t config_pid;
		posix_spawnp(&config_pid, rc_path, NULL, NULL, 
			(char* []){ rc_path, NULL }, environ);
		waitpid(config_pid, NULL, 0);
	}

	bool move = false;
	int start_x,     start_y,
		start_pos_x, start_pos_y, 
	    start_w,     start_h;
	xcb_window_t drag_wnd;
	xcb_window_t raise_wnd;
	xcb_generic_event_t* event;

	while (1) {
		event = xcb_wait_for_event(conn);
		switch (event->response_type & ~0x80) {
		// set focus to new windows
		case XCB_MAP_NOTIFY: {
			xcb_map_notify_event_t* map_event = (void*)event;
			raise_wnd = map_event->window;
			goto raise_window;
		}
		// raise window under cursor
		case XCB_KEY_PRESS: {
			xcb_key_press_event_t* key_event = (void*)event;
			raise_wnd = key_event->child;
		raise_window:
			uint16_t mask  = XCB_CONFIG_WINDOW_STACK_MODE;
			uint32_t value = XCB_STACK_MODE_ABOVE;
			xcb_configure_window(conn, raise_wnd, mask, &value);
			xcb_set_input_focus(conn, XCB_INPUT_FOCUS_NONE, 
				raise_wnd, XCB_CURRENT_TIME);
		}
		break;
		// begin move/resize
		case XCB_BUTTON_PRESS: {
			xcb_button_press_event_t* button_event = (void*)event;

			drag_wnd = button_event->child;
			move     = (button_event->detail == XCB_BUTTON_INDEX_1);
			start_x  = button_event->root_x;
			start_y  = button_event->root_y;

			xcb_get_geometry_reply_t* reply = xcb_get_geometry_reply(conn, 
				xcb_get_geometry(conn, drag_wnd), NULL);
					
			start_pos_x = reply->x;
			start_pos_y = reply->y;

			start_w = reply->width;
			start_h = reply->height;

			free(reply);
		}
		break;
		// end move/resize
		case XCB_BUTTON_RELEASE:
			move = false;
			drag_wnd = XCB_NONE;
			start_x = 0, start_y = 0,
			start_w = 0, start_h = 0;
		break;
		// handle move/resize configuration
		case XCB_MOTION_NOTIFY: {
			xcb_motion_notify_event_t* motion_event = (void*)event;
			int	x_diff = motion_event->root_x - start_x;
			int y_diff = motion_event->root_y - start_y;

			uint16_t mask = move ? (XCB_CONFIG_WINDOW_X     | XCB_CONFIG_WINDOW_Y) : 
			                       (XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT);
			uint32_t values[2] = { 
				MAX(1, (move ? start_pos_x : start_w) + x_diff),
				MAX(1, (move ? start_pos_y : start_h) + y_diff)
			};
			xcb_configure_window(conn, drag_wnd, mask, &values);
		}
		break;
		}

		xcb_flush(conn);
		free(event);
    }

	xcb_disconnect(conn);
	return 0;
}

