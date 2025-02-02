#include "app.h"
#include "app_launch.h"

#include <pbnjson.h>

#include "logging.h"
#include "util/path.h"
#include "util/i18n.h"

#include "lunasynccall.h"

static char locale_system[16];

void app_open_url(const char *url) {
    jvalue_ref payload_obj = jobject_create_var(
            jkeyval(J_CSTR_TO_JVAL("id"), J_CSTR_TO_JVAL("com.webos.app.browser")),
            jkeyval(J_CSTR_TO_JVAL("params"), jobject_create_var(
                    jkeyval(J_CSTR_TO_JVAL("target"), j_cstr_to_jval(url)),
                    J_END_OBJ_DECL
            )),
            J_END_OBJ_DECL
    );
    const char *payload = jvalue_stringify(payload_obj);
    HLunaServiceCallSync("luna://com.webos.applicationManager/launch", payload, true, NULL);
    j_release(&payload_obj);
}

void app_init_locale() {
    if (app_configuration->language[0] && strcmp(app_configuration->language, "auto") != 0) {
        commons_log_debug("APP", "Override language to %s", app_configuration->language);
        i18n_setlocale(app_configuration->language);
        return;
    }
    char *payload = NULL;
    commons_log_debug("APP", "Get system locale settings");
    if (!HLunaServiceCallSync("luna://com.webos.settingsservice/getSystemSettings", "{\"key\": \"localeInfo\"}",
                              true, &payload) || !payload) {
        commons_log_warn("APP", "Failed to get system locale settings. Falling back to English.");
        return;
    }
    JSchemaInfo schemaInfo;
    jschema_info_init(&schemaInfo, jschema_all(), NULL, NULL);
    jdomparser_ref parser = jdomparser_create(&schemaInfo, 0);
    jdomparser_feed(parser, payload, (int) strlen(payload));
    jdomparser_end(parser);
    jvalue_ref payload_obj = jdomparser_get_result(parser);
    jvalue_ref locale = jobject_get_nested(payload_obj, "settings", "localeInfo", "locales", "UI", NULL);
    if (jis_string(locale)) {
        raw_buffer buf = jstring_get(locale);
        size_t len = buf.m_len <= 15 ? buf.m_len : 15;
        strncpy(locale_system, buf.m_str, len);
        locale_system[len] = '\0';
        jstring_free_buffer(buf);
        i18n_setlocale(locale_system);
    }
    jdomparser_release(&parser);
}

app_launch_params_t *app_handle_launch(app_t *app, int argc, char *argv[]) {
    (void) app;
    if (argc < 2) {
        return NULL;
    }
    commons_log_info("APP", "Launched with parameters %s", argv[1]);
    JSchemaInfo schema_info;
    jschema_info_init(&schema_info, jschema_all(), NULL, NULL);
    jdomparser_ref parser = jdomparser_create(&schema_info, 0);
    if (!jdomparser_feed(parser, argv[1], (int) strlen(argv[1]))) {
        jdomparser_release(&parser);
        return NULL;
    }
    if (!jdomparser_end(parser)) {
        jdomparser_release(&parser);
        return NULL;
    }
    jvalue_ref params_obj = jdomparser_get_result(parser);
    if (!jis_valid(params_obj)) {
        jdomparser_release(&parser);
        return NULL;
    }
    app_launch_params_t *params = calloc(1, sizeof(app_launch_params_t));
    jvalue_ref host_uuid = jobject_get(params_obj, J_CSTR_TO_BUF("host_uuid"));
    if (jis_string(host_uuid)) {
        raw_buffer buf = jstring_get(host_uuid);
        uuidstr_fromchars(&params->default_host_uuid, buf.m_len, buf.m_str);
    }
    jvalue_ref host_app_id = jobject_get(params_obj, J_CSTR_TO_BUF("host_app_id"));
    if (jis_number(host_app_id)) {
        if (jnumber_get_i32(host_app_id, &params->default_app_id) != CONV_OK) {
            params->default_app_id = 0;
        }
    } else if (jis_string(host_app_id)) {
        raw_buffer buf = jstring_get(host_app_id);
        char *id_str = strndup(buf.m_str, buf.m_len);
        params->default_app_id = strtol(id_str, NULL, 10);
        if (params->default_app_id < 0) {
            params->default_app_id = 0;
        }
        free(id_str);
    }
    jdomparser_release(&parser);
    return params;
}