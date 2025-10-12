
#define _POSIX_C_SOURCE 200809L

#include <glib/gi18n.h>
#include <locale.h>       // Должен быть самым первым (для корректной работы setlocale)
#include <libintl.h>


#include "wlr-gamma-control-unstable-v1-client-protocol.h"
#include <errno.h>
#include <gtk/gtk.h>
#include <locale.h>
#include <stdio.h>
#include <sys/mman.h>
#include <wayland-client-protocol.h>
#include <wayland-client.h>

// wayland specifics
static struct zwlr_gamma_control_manager_v1 *gamma_control_manager =
		NULL; // handle to the registry global for gamma management
struct wl_display *display = NULL; // handle to wayland connection
static struct wl_list outputs;	 // list for all present wayland outputs

struct output {
	struct wl_output *wl_output;
	struct zwlr_gamma_control_v1 *gamma_control;
	uint32_t ramp_size;
	int table_fd;
	uint16_t *table;
	struct wl_list link;
};

// GTK specifics
// adjustment widgets
GtkAdjustment *adj_contrast;
GtkAdjustment *adj_brightness;
GtkAdjustment *adj_gamma;

// entry widget, the commandline
GtkEntry *entry_cmd;

#define BUFFER_SIZE 50
char buffer[BUFFER_SIZE];

static void fill_gamma_table(uint16_t *table,
		uint32_t ramp_size,
		double contrast,
		double brightness,
		double gamma) {
	uint16_t *r = table;
	uint16_t *g = table + ramp_size;
	uint16_t *b = table + 2 * ramp_size;
	for (uint32_t i = 0; i < ramp_size; ++i) {
		double val = (double)i / (ramp_size - 1);
		val = contrast * pow(val, 1.0 / gamma) + (brightness - 1);
		if (val > 1.0) {
			val = 1.0;
		} else if (val < 0.0) {
			val = 0.0;
		}
		r[i] = g[i] = b[i] = (uint16_t)(UINT16_MAX * val);
	}
}

static int create_anonymous_file(off_t size) {
	char template[] = "/tmp/wlroots-shared-XXXXXX";
	int fd = mkstemp(template);
	if (fd < 0) {
		return -1;
	}

	int ret;
	do {
		errno = 0;
		ret = ftruncate(fd, size);
	} while (errno == EINTR);
	if (ret < 0) {
		close(fd);
		return -1;
	}

	unlink(template);
	return fd;
}

static int create_gamma_table(uint32_t ramp_size, uint16_t **table) {
	size_t table_size = ramp_size * 3 * sizeof(uint16_t);
	int fd = create_anonymous_file(table_size);
	if (fd < 0) {
		fprintf(stderr, "failed to create anonymous file\n");
		return -1;
	}

	void *data =
			mmap(NULL, table_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (data == MAP_FAILED) {
		fprintf(stderr, "failed to mmap()\n");
		close(fd);
		return -1;
	}

	*table = data;
	return fd;
}

static void gamma_control_handle_gamma_size(void *data,
		struct zwlr_gamma_control_v1 *gamma_control,
		uint32_t ramp_size) {
	struct output *output = data;
	output->ramp_size = ramp_size;
}

static void gamma_control_handle_failed(void *data,
		struct zwlr_gamma_control_v1 *gamma_control) {
	fprintf(stderr, "failed to set gamma table\n");
	exit(EXIT_FAILURE);
}

static const struct zwlr_gamma_control_v1_listener gamma_control_listener = {
		.gamma_size = gamma_control_handle_gamma_size,
		.failed = gamma_control_handle_failed,
};

static void registry_handle_global(void *data,
		struct wl_registry *registry,
		uint32_t name,
		const char *interface,
		uint32_t version) {
	// bind wl_output registry global
	if (strcmp(interface, wl_output_interface.name) == 0) {
		struct output *output = calloc(1, sizeof(struct output));
		output->wl_output =
				wl_registry_bind(registry, name, &wl_output_interface, 1);
		wl_list_insert(&outputs, &output->link);
	} else if (strcmp(interface,
					   zwlr_gamma_control_manager_v1_interface.name) == 0) {
		gamma_control_manager = wl_registry_bind(
				registry, name, &zwlr_gamma_control_manager_v1_interface, 1);
	}
}

static void registry_handle_global_remove(void *data,
		struct wl_registry *registry,
		uint32_t name) {
	// Who cares?
}

static const struct wl_registry_listener registry_listener = {
		.global = registry_handle_global,
		.global_remove = registry_handle_global_remove,
};

void wl_set_cbg(double contrast, double brightness, double gamma) {
	struct output *output;
	wl_list_for_each(output, &outputs, link) {
		output->table_fd =
				create_gamma_table(output->ramp_size, &output->table);
		if (output->table_fd < 0) {
			exit(EXIT_FAILURE);
		}

		fill_gamma_table(
				output->table, output->ramp_size, contrast, brightness, gamma);
		zwlr_gamma_control_v1_set_gamma(
				output->gamma_control, output->table_fd);
		close(output->table_fd);
	}

	wl_display_roundtrip(display);
}

void gtk_update_cmd_entry(double contrast, double brightness, double gamma) {
	// temporarily set the locale to US, because printf might format the
	// decimal point as comma on some locales
	char *old_locale, *saved_locale;
	old_locale = setlocale(LC_NUMERIC, NULL);
	saved_locale = strdup(old_locale);
	setlocale(LC_NUMERIC, "en_US.UTF-8");
	snprintf(buffer,
			BUFFER_SIZE,
			"wl-gammactl -c %.3f -b %.3f -g %.3f",
			contrast,
			brightness,
			gamma);
	setlocale(LC_NUMERIC, saved_locale);
	free(saved_locale);

	// display command line
	gtk_entry_set_text(entry_cmd, buffer);
}

// --------------------------------------
void set_cbg(double contrast, double brightness, double gamma) {
	// update the command line to represent the new values
	gtk_update_cmd_entry(contrast, brightness, gamma);

	// set the actual values
	wl_set_cbg(contrast, brightness, gamma);
}

void on_adj_value_changed() {
	double contrast = gtk_adjustment_get_value(adj_contrast);
	double brightness = gtk_adjustment_get_value(adj_brightness);
	double gamma = gtk_adjustment_get_value(adj_gamma);
	set_cbg(contrast, brightness, gamma);
}

// called when window is closed
void on_window_main_destroy() { gtk_main_quit(); }

void on_button_reset_clicked() {
	gtk_adjustment_set_value(adj_contrast, 1.0);
	gtk_adjustment_set_value(adj_brightness, 1.0);
	gtk_adjustment_set_value(adj_gamma, 1.0);
}

void setup_gui(int argc, char *argv[]) {
	GtkBuilder *builder;
	GtkWidget *window;

	gtk_init(&argc, &argv);

	// builder = gtk_builder_new_from_file("glade/window_main.glade");
	builder = gtk_builder_new_from_resource(
			"/wl_gammactl/glade/window_main.glade");

	window = GTK_WIDGET(gtk_builder_get_object(builder, "window_main"));
	gtk_builder_connect_signals(builder, NULL);

	// get pointers to the adjustments
	adj_contrast =
			GTK_ADJUSTMENT(gtk_builder_get_object(builder, "adj_contrast"));
	adj_brightness =
			GTK_ADJUSTMENT(gtk_builder_get_object(builder, "adj_brightness"));
	adj_gamma = GTK_ADJUSTMENT(gtk_builder_get_object(builder, "adj_gamma"));

	// set the increment to 0.001, glade does not allow this..
	gtk_adjustment_set_step_increment(adj_contrast, 0.001);
	gtk_adjustment_set_step_increment(adj_brightness, 0.001);
	gtk_adjustment_set_step_increment(adj_gamma, 0.001);

	// get pointer to entry
	entry_cmd = GTK_ENTRY(gtk_builder_get_object(builder, "entry_cmd"));

	// initial update to have some start values
	on_adj_value_changed();

	g_object_unref(builder);

	gtk_widget_show(window);
	gtk_main();
}

static const char usage[] = "usage: wl-gammactl [options]\n"
							"  -h          show this help message\n"
							"  -c <value>  set contrast (default: 1)\n"
							"  -b <value>  set brightness (default: 1)\n"
							"  -g <value>  set gamma (default: 1)\n"
							"  no options to show the gui\n";

int run_cmdline(int argc, char *argv[]) {
	double contrast = 1, brightness = 1, gamma = 1;
	int opt;
	while ((opt = getopt(argc, argv, "hc:b:g:")) != -1) {
		switch (opt) {
		case 'c':
			contrast = strtod(optarg, NULL);
			break;
		case 'b':
			brightness = strtod(optarg, NULL);
			break;
		case 'g':
			gamma = strtod(optarg, NULL);
			break;
		case 'h':
		default:
			fprintf(stderr, usage);
			return opt == 'h' ? EXIT_SUCCESS : EXIT_FAILURE;
		}
	}

	wl_set_cbg(contrast, brightness, gamma);

	while (wl_display_dispatch(display) != -1) {
		// This space is intentionnally left blank
	}
	return EXIT_SUCCESS;
}

int main(int argc, char *argv[]) {
    // 1. Инициализация локализации (должна быть ДО gtk_init)
    setlocale(LC_ALL, "");
    bindtextdomain("wl-gammactl", "/usr/share/locale");
    bind_textdomain_codeset("wl-gammactl", "UTF-8");  // Важно для корректного отображения
    textdomain("wl-gammactl");


	// init the list of outputs
	wl_list_init(&outputs);

	// connect to wayland display
	display = wl_display_connect(NULL);
	if (display == NULL) {
		fprintf(stderr, "cannot connect to display\n");
		exit(EXIT_FAILURE);
	}

	// get handle on the registry
	struct wl_registry *registry = wl_display_get_registry(display);
	wl_registry_add_listener(registry, &registry_listener, NULL);
	// send our messages (async), does this happen anyway??
	wl_display_dispatch(display);
	// block here because we want to wait for our async reply of the server
	wl_display_roundtrip(display);

	// check if we got a handle to the gamma controller global
	if (gamma_control_manager == NULL) {
		fprintf(stderr,
				"compositor doesn't support "
				"wlr-gamma-control-unstable-v1\n");
		return EXIT_FAILURE;
	}

	// now we go into the gamma specifics
	// for each output in our stored list:
	//  1. create a gamma controller for this specific output
	//  2. add a listener function
	struct output *output;
	wl_list_for_each(output, &outputs, link) {
		output->gamma_control = zwlr_gamma_control_manager_v1_get_gamma_control(
				gamma_control_manager, output->wl_output);
		zwlr_gamma_control_v1_add_listener(
				output->gamma_control, &gamma_control_listener, output);
	}
	// once again, wait until this has been set and is ready
	wl_display_roundtrip(display);

	if (argc > 1)
		return run_cmdline(argc, argv);
	else
		setup_gui(argc, argv);

	return EXIT_SUCCESS;
}
