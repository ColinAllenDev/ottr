#include <stdio.h>

#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <xkbcommon/xkbcommon.h>

struct ottr_server {
	struct wl_display* wl_display;
	/* Exposes i/o devices */
	struct wlr_backend* backend;
	/* Provides utils and basic drawing API */
	struct wlr_renderer* renderer;
	/* */
};

int main() {
	printf("Hello, Ottr\n");
}
