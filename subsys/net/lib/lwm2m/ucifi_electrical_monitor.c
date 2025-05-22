#define LOG_MODULE_NAME net_ucifi_electrical_monitor
#define LOG_LEVEL CONFIG_LWM2M_LOG_LEVEL
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(LOG_MODULE_NAME);

#include <stdint.h>
#include <zephyr/init.h>
#include <string.h>

#include "lwm2m_object.h"
#include "lwm2m_engine.h"
#include "ucifi_electrical_monitor.h"

#define EM_VERSION_MAJOR 1
#define EM_VERSION_MINOR 0

#define MAX_INSTANCE_COUNT CONFIG_LWM2M_UCIFI_ELECTRICAL_MONITOR_INSTANCE_COUNT
#define EM_MAX_ID 7
#define RESOURCE_INSTANCE_COUNT (EM_MAX_ID)

static double voltage[MAX_INSTANCE_COUNT];
static double current[MAX_INSTANCE_COUNT];
static double frequency[MAX_INSTANCE_COUNT];
static double active_power[MAX_INSTANCE_COUNT];
static double power_factor[MAX_INSTANCE_COUNT];
static double energy[MAX_INSTANCE_COUNT];

static struct lwm2m_engine_obj electrical_monitor;
static struct lwm2m_engine_obj_field fields[] = {
    OBJ_FIELD_DATA(UCIFI_EM_SUPPLY_VOLTAGE_RID, R_OPT, FLOAT),
    OBJ_FIELD_DATA(UCIFI_EM_SUPPLY_CURRENT_RID, R_OPT, FLOAT),
    OBJ_FIELD_DATA(UCIFI_EM_FREQUENCY_RID, R_OPT, FLOAT),
    OBJ_FIELD_DATA(UCIFI_EM_ACTIVE_POWER_RID, R_OPT, FLOAT),
    OBJ_FIELD_DATA(UCIFI_EM_POWER_FACTOR_RID, R_OPT, FLOAT),
    OBJ_FIELD_DATA(UCIFI_EM_CUMULATED_ACTIVE_ENERGY_RID, R_OPT, FLOAT),
    OBJ_FIELD_EXECUTE_OPT(UCIFI_EM_ENERGY_RESET_RID),
};

static struct lwm2m_engine_obj_inst inst[MAX_INSTANCE_COUNT];
static struct lwm2m_engine_res res[MAX_INSTANCE_COUNT][EM_MAX_ID];
static struct lwm2m_engine_res_inst res_inst[MAX_INSTANCE_COUNT][RESOURCE_INSTANCE_COUNT];

static int reset_energy_cb(uint16_t obj_inst_id, uint8_t *args, uint16_t args_len)
{
    for (int i = 0; i < MAX_INSTANCE_COUNT; i++) {
        if (inst[i].obj && inst[i].obj_inst_id == obj_inst_id) {
            energy[i] = 0;
            lwm2m_notify_observer(UCIFI_OBJECT_ELECTRICAL_MONITOR_ID, obj_inst_id,
                                   UCIFI_EM_CUMULATED_ACTIVE_ENERGY_RID);
            LOG_INF("Energy counter reset for instance %d", obj_inst_id);
            return 0;
        }
    }
    return -ENOENT;
}

static struct lwm2m_engine_obj_inst *em_create(uint16_t obj_inst_id)
{
    int i = 0, j = 0;
    int index = obj_inst_id;

    if (obj_inst_id >= MAX_INSTANCE_COUNT || inst[index].obj != NULL) {
        LOG_ERR("Invalid or already existing instance %d", obj_inst_id);
        return NULL;
    }


    /* Set default values */
    voltage[index] = 0.0;
    current[index] = 0.0;
    frequency[index] = 60.0;
    active_power[index] = 0.0;
    power_factor[index] = 1.0;
    energy[index] = 0.0;

    memset(res[index], 0, sizeof(res[index]));
    init_res_instance(res_inst[index], ARRAY_SIZE(res_inst[index]));

    INIT_OBJ_RES_DATA(UCIFI_EM_SUPPLY_VOLTAGE_RID, res[index], i, res_inst[index], j,
                      &voltage[index], sizeof(voltage[index]));
    INIT_OBJ_RES_DATA(UCIFI_EM_SUPPLY_CURRENT_RID, res[index], i, res_inst[index], j,
                      &current[index], sizeof(current[index]));
    INIT_OBJ_RES_DATA(UCIFI_EM_FREQUENCY_RID, res[index], i, res_inst[index], j,
                      &frequency[index], sizeof(frequency[index]));
    INIT_OBJ_RES_DATA(UCIFI_EM_ACTIVE_POWER_RID, res[index], i, res_inst[index], j,
                      &active_power[index], sizeof(active_power[index]));
    INIT_OBJ_RES_DATA(UCIFI_EM_POWER_FACTOR_RID, res[index], i, res_inst[index], j,
                      &power_factor[index], sizeof(power_factor[index]));
    INIT_OBJ_RES_DATA(UCIFI_EM_CUMULATED_ACTIVE_ENERGY_RID, res[index], i, res_inst[index], j,
                      &energy[index], sizeof(energy[index]));
    INIT_OBJ_RES_EXECUTE(UCIFI_EM_ENERGY_RESET_RID, res[index], i, reset_energy_cb);

    inst[index].resources = res[index];
    inst[index].resource_count = i;

    LOG_DBG("Created uCIFI Electrical Monitor instance: %d", obj_inst_id);
    return &inst[index];
}

static int ucifi_electrical_monitor_init(void)
{
    electrical_monitor.obj_id = UCIFI_OBJECT_ELECTRICAL_MONITOR_ID;
    electrical_monitor.version_major = EM_VERSION_MAJOR;
    electrical_monitor.version_minor = EM_VERSION_MINOR;
    electrical_monitor.is_core = true;
    electrical_monitor.fields = fields;
    electrical_monitor.field_count = ARRAY_SIZE(fields);
    electrical_monitor.max_instance_count = MAX_INSTANCE_COUNT;
    electrical_monitor.create_cb = em_create;

    lwm2m_register_obj(&electrical_monitor);

    return 0;
}

SYS_INIT(ucifi_electrical_monitor_init, APPLICATION, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);
