#include <glib.h>
#include <gio/gio.h>
#include <xwiimote.h>
#include <errno.h>

typedef struct Args {
    GMainLoop *loop;
    char *mac_address;
} Args;

static int board_read(struct xwii_iface *iface, void *userdata) {
    int ret = xwii_iface_open(iface, XWII_IFACE_BALANCE_BOARD);
    if (ret) {
        g_print("Failed to open iface %d\n", ret);
        return -1;
    }

    struct xwii_event event;
    int32_t measurements[100];
    size_t measurements_idx = 0;

    time_t loop_start = time(NULL);
    time_t now = loop_start;
    time_t measure_start = 0;

    unsigned measurements_len = sizeof(measurements)/sizeof(measurements[0]);
    while (now - measure_start < 3 || measure_start == 0) {
        now = time(NULL);

        if (now - loop_start > 30) {
            g_print("Timeout, failed to read within 30 seconds");
            goto done;
        }

        ret = xwii_iface_poll(iface, &event);

        if (ret == -EAGAIN) {
            usleep(50000);
        } else {
            int32_t weight = 0;
            for (int i = 0; i<4; ++i) {
                weight += event.v.abs[i].x;
            }

            // Ignore smallest weight, just noise
            if (weight < 1000) {
                continue;
            }

            // We have a valid measurement, measure for 3 seconds.
            if (measure_start == 0) {
                measure_start = time(NULL);
            }

            measurements[measurements_idx % measurements_len] = weight;
            measurements_idx += 1;
        }
    }

    if (measurements_idx < measurements_len) {
        g_print("Failed to gather sufficient measurement data\n");
    } else {
        double avg = 0;
        for (unsigned i = 0; i<measurements_len; ++i) {
            avg += measurements[i];
        }
        avg /= measurements_len;
        avg /= 100;
        printf("Measured: %f\n", avg);
    }
 done:
    xwii_iface_close(iface, XWII_IFACE_BALANCE_BOARD);
    return 0;
}

static int board_connect(void *userdata) {
    struct xwii_monitor *mon = xwii_monitor_new(TRUE, FALSE);
    if (!mon) {
        g_print("Cannot create monitor\n");
        return 1;
    }

    struct xwii_iface *iface = NULL;
    char *path = NULL;
    char *devtype = NULL;

    // Negotiation takes time, so we have to loop this until we can read the path
    do {
        xwii_monitor_get_fd(mon, TRUE);
        path = xwii_monitor_poll(mon);
        if (path == NULL) {
            usleep(100000);
        }
    } while (path == NULL);

    // Likewise, we need to loop the devtype, until it resolves as something other than 'unknown'
    size_t devtype_count = 0;
    do {
        usleep(500000);
        if (iface != NULL) {
            xwii_iface_unref(iface);
            iface = NULL;
        }

        if (devtype != NULL) {
            free(devtype);
            devtype = NULL;
        }

        int ret = xwii_iface_new(&iface, path);
        if (ret) {
            g_print("Error opening path %s\n", path);
            goto done;
        }

        if (xwii_iface_get_devtype(iface, &devtype) || devtype_count == 10) {
            g_print("Failed to get devtype\n");
            goto done;
        }

        devtype_count += 1;
    } while (strcmp(devtype, "balanceboard") != 0);

    board_read(iface, userdata);
 done:
    if (path != NULL) {
        free(path);
    }
    if (iface != NULL) {
        xwii_iface_unref(iface);
    }
    if (devtype != NULL) {
        free(devtype);
    }
    xwii_monitor_unref(mon);
    return 0;
}

static void bluez_signal_adapter_changed(
    GDBusConnection *conn,
    const gchar *sender,
    const gchar *path,
    const gchar *interface,
    const gchar *signal,
    GVariant *params,
    void *userdata
) {
    GVariantIter *properties = NULL;
    GVariantIter *unknown = NULL;
    const char *iface;
    const char *key;
    GVariant *value = NULL;
    const gchar *signature = g_variant_get_type_string(params);
    Args *args = (Args*)userdata;
    int exit = 0;

    if(strcmp(signature, "(sa{sv}as)") != 0) {
        g_print("Invalid signature for %s: %s != %s", signal, signature, "(sa{sv}as)");
        goto done;
    }

    g_variant_get(params, "(&sa{sv}as)", &iface, &properties, &unknown);
    while(g_variant_iter_next(properties, "{&sv}", &key, &value)) {
        if (g_str_has_suffix(path, args->mac_address) && g_strcmp0(key, "Connected") == 0) {
            if(!g_variant_is_of_type(value, G_VARIANT_TYPE_BOOLEAN)) {
                g_print("Invalid argument type for %s: %s != %s", key,
                        g_variant_get_type_string(value), "b");
                exit=1;
                goto done;
            }

            if (g_variant_get_boolean(value)) {
                g_print("[up] balance board, connecting\n");
                if (!board_connect(userdata) == 0) {
                    exit=1;
                    goto done;
                }
                GError *error = NULL;
                GVariant *result= g_dbus_connection_call_sync(conn,
                                                              "org.bluez",
                                                              path,
                                                              iface,
                                                              "Disconnect",
                                                              NULL,
                                                              NULL,
                                                              G_DBUS_CALL_FLAGS_NONE,
                                                              -1,
                                                              NULL,
                                                              &error);
                g_variant_unref(result);
            } else {
                g_print("[down] balance board\n");
            }
        }
    }
 done:
    if(properties != NULL) {
        g_variant_iter_free(properties);
    }

    if(value != NULL) {
        g_variant_unref(value);
    }

    if(exit == 1) {
        g_main_loop_quit(((Args *)userdata)->loop);
    }
}

int main(int argc, char **argv) {
    if (argc != 2) {
        g_print("Usage: %s <mac address>\n", argv[0]);
        return 1;
    }

    GDBusConnection *con = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, NULL);
    if (con == NULL) {
        g_print("Not able to get connection to system bus\n");
        return 1;
    }

    GMainLoop *loop = g_main_loop_new(NULL, FALSE);

    Args userdata;
    userdata.loop = loop;
    userdata.mac_address = *&argv[1];

    guint sub = g_dbus_connection_signal_subscribe(con,
                                                   "org.bluez",
                                                   "org.freedesktop.DBus.Properties",
                                                   "PropertiesChanged",
                                                   NULL,
                                                   NULL,
                                                   G_DBUS_SIGNAL_FLAGS_NONE,
                                                   bluez_signal_adapter_changed,
                                                   &userdata,
                                                   NULL);

    g_main_loop_run(loop);

    g_dbus_connection_signal_unsubscribe(con, sub);
    g_object_unref(con);

    return 0;
}
