#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <stdint.h>

#include <wayland-server-core.h>
#include <wayland-util.h>
#include <wlr/backend.h>
#include <wlr/render/allocator.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/util/log.h>
#include <xkbcommon/xkbcommon.h>

#define OTR_LOG_LVL WLR_DEBUG

typedef struct otr_server {
	struct wl_display* wl_display;
	/* Exposes i/o devices */
	struct wlr_backend* backend;
	/* Provides utils and basic drawing API */
	struct wlr_renderer* renderer;
	/* Allocates memory to pixel buffers */
	struct wlr_allocator* allocator;
	/* The scene-graph used to lay out windows */
	struct wlr_scene* scene;
	struct wlr_scene_output_layout* scene_layout;
	/* Helper to arrange outputs in 2D space */
	struct wlr_output_layout* output_layout;
	/* Doubly linked-list of wayland outputs */
	struct wl_list outputs; 
	/* Listen for output event signals */
	struct wl_listener new_output_listener;
} otr_server;

typedef struct otr_output {
	struct wl_list link;
	struct otr_server* server;
	struct wlr_output* wlr_output;
	struct wl_listener frame;
	struct wl_listener request_state;
	struct wl_listener destroy;
} otr_output;

static void otr_new_output(struct wl_listener* listener, void* out_data);

/* otr_new_output
** Callback notification function triggered when a new output (display, screen) becomes available
*/
void otr_new_output(struct wl_listener* listener, void* out_data) 
{
	/* The event raised by the backend when a new output becomes available */
	struct otr_server* server = wl_container_of(listener, server, new_output_listener);	
	struct wlr_output* wlr_output = out_data;

	/* Configure the output created by the backend to use custom allocator and renderer */
	wlr_output_init_render(wlr_output, server->allocator, server->renderer);

	/* Initialize the output's state machine and enables it */
	struct wlr_output_state state;
	wlr_output_state_init(&state);
	wlr_output_state_set_enabled(&state, true);

	/* The output mode is a tuple of (width, height, refresh_rate).
	** TODO: Allow user to configure the output mode instead of preferred
	*/
	struct wlr_output_mode* mode = wlr_output_preferred_mode(wlr_output);
	if (mode != NULL) wlr_output_state_set_mode(&state, mode);

	/* Automatically apply the new output state */
	wlr_output_commit_state(wlr_output, &state);
	wlr_output_state_finish(&state);

	/* Allocate and configure this output's state */ 
	struct otr_output* output = calloc(1, sizeof(otr_output));
	output->wlr_output = wlr_output;
	output->server = server;

	/* TODO: Setup a listener for the frame event */
	/* TODO: Setup a listener for the destroy event */

	/* Append server outputs with current output */
	wl_list_insert(&server->outputs, &output->link);

	/* Add this output to the server's output_layout. The add-auto function arranges outputs from left-to-right.
	** The layout utility adds a wl_output global to the display, 
	** which Wayland clients can access for DPI, scale factor, etc.
	** TODO: Allow user to configure arrangement of outputs in the layout
	*/
	struct wlr_output_layout_output* layout_output = wlr_output_layout_add_auto(server->output_layout, wlr_output);	
	struct wlr_scene_output* scene_output = wlr_scene_output_create(server->scene, wlr_output);
	wlr_scene_output_layout_add_output(server->scene_layout, layout_output, scene_output);
}

int main() {
	/* Initialize wlroots logger */
	wlr_log_init(OTR_LOG_LVL, NULL);

	/* Initialize ottr server */
	otr_server server = {0};

	/* The wayland display is managed by libwayland. 
	** It handles accepting clients from the unix socket, managing wayland globals, etc. */
	server.wl_display = wl_display_create();

	/* The wlroots backend is a feature which abstracts the underlying input and output hardware. 
	** The autocreate option will choose the most suitable backend based on the current environment,
	** i.e. opening an X11 window if an X11 server is running.
	*/
	server.backend = wlr_backend_autocreate(wl_display_get_event_loop(server.wl_display), NULL);
	if (server.backend == NULL) {
		wlr_log(WLR_ERROR, "failed to create wlr_backend");
		return -1;
	}

	/* Autocreate a renderer (Pixman, GLES2, Vulkan, etc) */
	server.renderer = wlr_renderer_autocreate(server.backend);
	if (server.backend == NULL) {
		wlr_log(WLR_ERROR, "failed to create wlr_renderer");
		return -1;
	}

	/* Initialize display */
	wlr_renderer_init_wl_display(server.renderer, server.wl_display);

	/* An allocator is the bridge between the renderer and the backend. 
	** It handles buffer creation, allowing wlroots to render onto the screen.
	*/
	server.allocator = wlr_allocator_autocreate(server.backend, server.renderer);
	if (server.allocator == NULL) {
		wlr_log(WLR_ERROR, "failed to create wlr_allocator");
		return -1;
	}

	/* A compositor is necessary for clients to allocate surfaces. */
	wlr_compositor_create(server.wl_display, 5, server.renderer);
	wlr_subcompositor_create(server.wl_display);
	wlr_data_device_manager_create(server.wl_display);

	/* Create an output layout, a wlroots utility for working with screen layouts */
	server.output_layout = wlr_output_layout_create(server.wl_display);		

	/* Configure a listener to be notified when new outputs are available on the backend */
	wl_list_init(&server.outputs);
	server.new_output_listener.notify = otr_new_output;
	wl_signal_add(&server.backend->events.new_output, &server.new_output_listener);

	/* Create a scene graph to handle rendering and damage tracking.
	** The compositor must add items to be rendered to the scene graph at the proper 
	** positions and then call wlr_scene_output_commit() to render a frame if necessary.
	*/
	server.scene = wlr_scene_create();
	server.scene_layout = wlr_scene_attach_output_layout(server.scene, server.output_layout);

	/* Add a unix socket to the wayland display */
	const char* socket = wl_display_add_socket_auto(server.wl_display);
	if (!socket) {
		wlr_log(WLR_ERROR, "failed to add unix socket to display");
		wlr_backend_destroy(server.backend);
		return -1;
	}

	/* Start the backend */
	if (!wlr_backend_start(server.backend)) {
		wlr_backend_destroy(server.backend);
		wl_display_destroy(server.wl_display);
		return -1;
	}

	/* TODO: Set WAYLAND_DISPLAY to the unix socket and run startup cmd */
	setenv("WAYLAND_DISPLAY", socket, true);

	/* Run the wayland event loop */
	wlr_log(WLR_INFO, "Running Ottr...");
	wl_display_run(server.wl_display);

	/* Cleanup: destroy all clients and shut down the server */
	wl_display_destroy_clients(server.wl_display);
	wl_list_remove(&server.new_output_listener.link);
	wlr_scene_node_destroy(&server.scene->tree.node);
	wlr_allocator_destroy(server.allocator);
	wlr_renderer_destroy(server.renderer);
	wlr_backend_destroy(server.backend);
	wl_display_destroy(server.wl_display);

	return 0;
}
