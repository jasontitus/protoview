/* TPMS Sensor list management.
 * Extracts sensor data from decoded messages and maintains a list of
 * unique sensors with their latest readings. */

#include "app.h"
#include <string.h>

#define TPMS_LOG_PATH APP_DATA_PATH("tpms_log.csv")
#define TPMS_DEBUG_LOG_PATH APP_DATA_PATH("tpms_debug.csv")

/* Initialize the sensor list. */
void tpms_sensor_list_init(TPMSSensorList *list) {
    memset(list, 0, sizeof(TPMSSensorList));
}

/* Clear all sensors from the list. */
void tpms_sensor_list_clear(TPMSSensorList *list) {
    list->count = 0;
    memset(list->sensors, 0, sizeof(list->sensors));
}

/* Find a field in a fieldset by name. Returns NULL if not found. */
static ProtoViewField *fieldset_find(ProtoViewFieldSet *fs, const char *name) {
    for (uint32_t i = 0; i < fs->numfields; i++) {
        if (strcmp(fs->fields[i]->name, name) == 0)
            return fs->fields[i];
    }
    return NULL;
}

/* Find a sensor in the list by its ID. Returns index or -1. */
static int sensor_list_find(TPMSSensorList *list, uint8_t *id, uint8_t id_len) {
    for (uint32_t i = 0; i < list->count; i++) {
        if (list->sensors[i].id_len == id_len &&
            memcmp(list->sensors[i].id, id, id_len) == 0)
        {
            return (int)i;
        }
    }
    return -1;
}

/* Append a sensor reading to the log file on the SD card.
 * Format: ID_hex,protocol,pressure_psi,temperature_f,rx_count */
void tpms_save_to_file(ProtoViewApp *app, TPMSSensor *sensor) {
    if (!app->storage) return;

    File *file = storage_file_alloc(app->storage);
    if (!file) return;

    /* Ensure the app data directory exists. */
    FuriString *dir_path = furi_string_alloc_set(APP_DATA_PATH(""));
    storage_common_resolve_path_and_ensure_app_directory(app->storage, dir_path);
    furi_string_free(dir_path);

    /* Write CSV header if file is new. */
    bool is_new = !storage_file_exists(app->storage, TPMS_LOG_PATH);

    if (storage_file_open(file, TPMS_LOG_PATH, FSAM_WRITE, FSOM_OPEN_APPEND)) {
        if (is_new) {
            const char *header = "id,protocol,pressure_psi,temperature_f,rx_count\n";
            storage_file_write(file, header, strlen(header));
        }

        /* Format ID as hex string. */
        char id_hex[TPMS_ID_MAX_BYTES * 2 + 1];
        for (uint8_t i = 0; i < sensor->id_len; i++) {
            snprintf(id_hex + i * 2, 3, "%02X", sensor->id[i]);
        }
        id_hex[sensor->id_len * 2] = '\0';

        /* Format the CSV line. */
        char line[128];
        int len = snprintf(line, sizeof(line), "%s,%s,",
                           id_hex, sensor->protocol);

        if (sensor->has_pressure)
            len += snprintf(line + len, sizeof(line) - len, "%.1f,",
                            (double)sensor->pressure_psi);
        else
            len += snprintf(line + len, sizeof(line) - len, ",");

        if (sensor->has_temperature)
            len += snprintf(line + len, sizeof(line) - len, "%d,",
                            sensor->temperature_f);
        else
            len += snprintf(line + len, sizeof(line) - len, ",");

        len += snprintf(line + len, sizeof(line) - len, "%lu\n",
                        (unsigned long)sensor->rx_count);

        storage_file_write(file, line, len);
    }

    storage_file_close(file);
    storage_file_free(file);
}

/* Write a debug event to the SD card log.
 * Format: ts_ms,event,modulation,scans,coherent,tries,decoded,detail */
void tpms_debug_log(ProtoViewApp *app, const char *event, const char *detail) {
    if (!app->debug_logging || !app->storage) return;

    File *file = storage_file_alloc(app->storage);
    if (!file) return;

    FuriString *dir_path = furi_string_alloc_set(APP_DATA_PATH(""));
    storage_common_resolve_path_and_ensure_app_directory(app->storage, dir_path);
    furi_string_free(dir_path);

    bool is_new = !storage_file_exists(app->storage, TPMS_DEBUG_LOG_PATH);

    if (storage_file_open(file, TPMS_DEBUG_LOG_PATH, FSAM_WRITE, FSOM_OPEN_APPEND)) {
        if (is_new) {
            const char *header =
                "ts_ms,event,modulation,scans,coherent,tries,decoded,detail\n";
            storage_file_write(file, header, strlen(header));
        }

        uint32_t ts = furi_get_tick();
        const char *mod_name = ProtoViewModulations[app->modulation].name;

        char line[192];
        int len = snprintf(
            line, sizeof(line),
            "%lu,%s,%s,%lu,%lu,%lu,%lu,%s\n",
            (unsigned long)ts,
            event,
            mod_name,
            (unsigned long)app->dbg_scan_count,
            (unsigned long)app->dbg_coherent_count,
            (unsigned long)app->dbg_decode_try_count,
            (unsigned long)app->dbg_decode_ok_count,
            detail ? detail : "");

        storage_file_write(file, line, len);
    }

    storage_file_close(file);
    storage_file_free(file);
}

/* Extract TPMS sensor data from the currently decoded message and
 * add or update it in the sensor list.
 * Returns true if a valid TPMS sensor was extracted. */
bool tpms_extract_and_store(ProtoViewApp *app) {
    if (!app->msg_info || !app->msg_info->fieldset) return false;

    ProtoViewFieldSet *fs = app->msg_info->fieldset;
    ProtoViewField *id_field = fieldset_find(fs, "Tire ID");
    if (!id_field) return false; /* Not a TPMS message. */
    if (id_field->type != FieldTypeBytes) return false;

    TPMSSensor sensor;
    memset(&sensor, 0, sizeof(sensor));

    /* Extract tire ID. */
    uint8_t id_bytes = (id_field->len + 1) / 2; /* len is in nibbles. */
    if (id_bytes > TPMS_ID_MAX_BYTES) id_bytes = TPMS_ID_MAX_BYTES;
    memcpy(sensor.id, id_field->bytes, id_bytes);
    sensor.id_len = id_bytes;

    /* Protocol name. */
    snprintf(sensor.protocol, sizeof(sensor.protocol), "%s",
             app->msg_info->decoder->name);

    /* Extract pressure. Decoders output either "Pressure kpa" or
     * "Pressure psi". Normalize to PSI. */
    ProtoViewField *pressure_kpa = fieldset_find(fs, "Pressure kpa");
    ProtoViewField *pressure_psi = fieldset_find(fs, "Pressure psi");

    if (pressure_psi && pressure_psi->type == FieldTypeFloat) {
        sensor.pressure_psi = pressure_psi->fvalue;
        sensor.has_pressure = true;
    } else if (pressure_kpa && pressure_kpa->type == FieldTypeFloat) {
        /* Convert kPa to PSI. */
        sensor.pressure_psi = pressure_kpa->fvalue * 0.14503774f;
        sensor.has_pressure = true;
    }

    /* Extract temperature. Decoders output "Temperature C".
     * Convert to Fahrenheit. */
    ProtoViewField *temp_c = fieldset_find(fs, "Temperature C");
    if (temp_c && temp_c->type == FieldTypeSignedInt) {
        sensor.temperature_f = (int)(temp_c->value * 9 / 5 + 32);
        sensor.has_temperature = true;
    }

    sensor.last_seen = furi_get_tick();
    sensor.rx_count = 1;

    /* Find existing sensor or add new one. */
    TPMSSensor *saved = NULL;
    int idx = sensor_list_find(&app->sensor_list, sensor.id, sensor.id_len);
    if (idx >= 0) {
        /* Update existing sensor. */
        saved = &app->sensor_list.sensors[idx];
        if (sensor.has_pressure) {
            saved->pressure_psi = sensor.pressure_psi;
            saved->has_pressure = true;
        }
        if (sensor.has_temperature) {
            saved->temperature_f = sensor.temperature_f;
            saved->has_temperature = true;
        }
        saved->last_seen = sensor.last_seen;
        saved->rx_count++;
        /* Update protocol name in case a more specific decoder matched. */
        snprintf(saved->protocol, sizeof(saved->protocol), "%s",
                 sensor.protocol);
    } else if (app->sensor_list.count < TPMS_MAX_SENSORS) {
        /* Add new sensor. */
        app->sensor_list.sensors[app->sensor_list.count] = sensor;
        saved = &app->sensor_list.sensors[app->sensor_list.count];
        app->sensor_list.count++;
    }

    /* Persist to SD card so data survives crashes. */
    if (saved) {
        tpms_save_to_file(app, saved);
    }

    /* Notify the user: vibrate + green LED for new TPMS data. */
    static const NotificationSequence tpms_seq = {
        &message_vibro_on,
        &message_green_255,
        &message_delay_50,
        &message_green_0,
        &message_vibro_off,
        NULL
    };
    notification_message(app->notification, &tpms_seq);

    return true;
}
