#include <glib.h>
#include <gio/gio.h>
#include <xwiimote.h>

static void bluez_signal_adapter_changed(
    GDBusConnection *conn,
    const gchar *sender,
    const gchar *path,
    const gchar *interface,
    const gchar *signal,
    GVariant *params,
    void *userdata
) {
}

int main(int argc, char **argv) {
    GDBusConnection *con = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, NULL);
    if (con == NULL) {
        g_print("Not able to get connection to system bus\n");
        return 1;
    }

    GMainLoop *loop = g_main_loop_new(NULL, FALSE);

    guint sub = g_dbus_connection_signal_subscribe(con,
                                                   "org.bluez",
                                                   "org.freedesktop.DBus.Properties",
                                                   "PropertiesChanged",
                                                   NULL,
                                                   NULL,
                                                   G_DBUS_SIGNAL_FLAGS_NONE,
                                                   bluez_signal_adapter_changed,
                                                   NULL,
                                                   NULL);

    g_main_loop_run(loop);

    g_dbus_connection_signal_unsubscribe(con, sub);
    g_object_unref(con);

    return 0;
}
