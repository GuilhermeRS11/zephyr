/*
 * Copyright (c) 2024 Guilherme
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Source material for uCIFI lamp object (3416):
 * https://raw.githubusercontent.com/OpenMobileAlliance/lwm2m-registry/prod/3416.xml
 */

#define LOG_MODULE_NAME net_ucifi_lamp
#define LOG_LEVEL CONFIG_LWM2M_LOG_LEVEL

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(LOG_MODULE_NAME);

#include <stdint.h>
#include <zephyr/init.h>
#include <string.h>

#include "lwm2m_object.h"
#include "lwm2m_engine.h"
#include "ucifi_lamp.h"

#define LAMP_VERSION_MAJOR 1
#define LAMP_VERSION_MINOR 0

#define MAX_INSTANCE_COUNT CONFIG_LWM2M_UCIFI_LAMP_INSTANCE_COUNT
#define LAMP_MAX_ID 6
#define RESOURCE_INSTANCE_COUNT (LAMP_MAX_ID)


/* Resource state variables */
static int8_t command[MAX_INSTANCE_COUNT];
static uint8_t dimming_level[MAX_INSTANCE_COUNT];
static bool failure[MAX_INSTANCE_COUNT];

static struct lwm2m_engine_obj lamp;
static struct lwm2m_engine_obj_field fields[] = {
    OBJ_FIELD_DATA(UCIFI_LAMP_COMMAND_RID, RW, S8),
    OBJ_FIELD_DATA(UCIFI_LAMP_DIMMING_LEVEL_RID, RW, U8),
    OBJ_FIELD_DATA(UCIFI_LAMP_FAILURE_RID, RW, BOOL),
};

static struct lwm2m_engine_obj_inst inst[MAX_INSTANCE_COUNT];
static struct lwm2m_engine_res res[MAX_INSTANCE_COUNT][LAMP_MAX_ID];
static struct lwm2m_engine_res_inst res_inst[MAX_INSTANCE_COUNT][RESOURCE_INSTANCE_COUNT];

static struct lwm2m_engine_obj_inst *lamp_create(uint16_t obj_inst_id)
{
    int index = 0, i = 0, j = 0;

    if (obj_inst_id >= MAX_INSTANCE_COUNT) {
        LOG_ERR("Invalid instance %d", obj_inst_id);
        return NULL;
    }

    if (inst[index].obj != NULL) {
        LOG_ERR("Instance %d already exists", obj_inst_id);
        return NULL;
    }

    /* Set default values */
    command[index] = 0;
    dimming_level[index] = 100;
    failure[index] = false;

    (void)memset(res[index], 0, sizeof(res[index]));
    init_res_instance(res_inst[index], ARRAY_SIZE(res_inst[index]));

    INIT_OBJ_RES_DATA(UCIFI_LAMP_COMMAND_RID, res[index], i, res_inst[index], j,
                      &command[index], sizeof(command[index]));
    INIT_OBJ_RES_DATA(UCIFI_LAMP_DIMMING_LEVEL_RID, res[index], i, res_inst[index], j,
                      &dimming_level[index], sizeof(dimming_level[index]));
    INIT_OBJ_RES_DATA(UCIFI_LAMP_FAILURE_RID, res[index], i, res_inst[index], j,
                      &failure[index], sizeof(failure[index]));

    inst[index].resources = res[index];
    inst[index].resource_count = i;

    LOG_DBG("Created uCIFI Lamp instance: %d", obj_inst_id);
    return &inst[index];
}

static int ucifi_lamp_init(void)
{
    lamp.obj_id = UCIFI_OBJECT_LAMP_ID;
    lamp.version_major = LAMP_VERSION_MAJOR;
    lamp.version_minor = LAMP_VERSION_MINOR;
    lamp.is_core = true;
    lamp.fields = fields;
    lamp.field_count = ARRAY_SIZE(fields);
    lamp.max_instance_count = MAX_INSTANCE_COUNT;
    lamp.create_cb = lamp_create;
    lwm2m_register_obj(&lamp);

    return 0;

}

SYS_INIT(ucifi_lamp_init, APPLICATION, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);

