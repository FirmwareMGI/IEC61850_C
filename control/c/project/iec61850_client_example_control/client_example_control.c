/*
 * client_example_control.c
 *
 * How to control a device ... intended to be used with server_example_control
 */

#include "iec61850_client.h"
#include "hal_thread.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <time.h>

#include "MQTTClient.h"
#include "cJSON.h"

#include "log.h"

// MQTT
#define ADDRESS "tcp://172.28.80.1:1883"
#define TIMEOUT 1000L
#define QOS 0
int disc_finished = 0;
int subscribed = 0;
int finished = 0;
char mqtt_topic_control_request[20];
char mqtt_topic_control_response[20];

bool TopicArrived = false;
const int mqttpayloadSize = 200;
char mqttpayload[200] = {'\0'};
char mqtttopic[30];

// MQTT

// SYSTEM
int master_id_device;
// SYSTEM

// CONTROL
#define MAX_ENABLED_CONTROLS 10
typedef struct
{
    char object[128];
    char ctlModel[64];
} EnabledControl;

typedef struct ReceiveControl
{
    char object[64];
    char valueNow[20];
    char lastValue[20];
    char typeData[32];
    char ctlCommand[32];
    bool synchrocheck;
    bool interlocking;
    bool testmode;
    int64_t timestamp;
} ReceiveControl;

typedef struct ResponseControl
{
    char object[64];
    char valueNow[20];
    char lastValue[20];
    char status[20];
    char ctlCommand[32];
    char iecErrorString[50];
    char errorString[40];
    char timestamp[40];
} ResponseControl;

typedef struct IecResponseControl
{
    char status[20];
    char iecErrorString[50];
    int timestamp;
} IecResponseControl;

EnabledControl enabledControls[MAX_ENABLED_CONTROLS];
int enabledCount = 0;
// CONTROL

// FUNCTION
int IEC61850_control_direct_security(IedConnection con, char *control_obj, bool value);
int IEC61850_control_direct_security_ex(IedConnection con, char *control_obj, ReceiveControl rc, ResponseControl *resp);
int IEC61850_control_direct_security_exp(IedConnection con, char *control_obj, bool value);
int IEC61850_control_direct_security_exp_ex(IedConnection con, char *control_obj, ReceiveControl rc, ResponseControl *resp);
int IEC61850_control_direct_normal(IedConnection con, char *control_obj, bool value);
int IEC61850_control_direct_normal_ex(IedConnection con, char *control_obj, ReceiveControl rc, ResponseControl *resp);
int IEC61850_control_sbo_normal(IedConnection con, char *control_obj, bool value);
int IEC61850_control_sbo_normal_ex(IedConnection con, char *control_obj, ReceiveControl rc, ResponseControl *resp);
int IEC61850_control_sbo_security(IedConnection con, char *control_obj, bool value);
int IEC61850_control_sbo_security_ex(IedConnection con, char *control_obj, ReceiveControl rc, ResponseControl *resp);
int IEC61850_control_cancel(IedConnection con, char *control_obj);
int IEC61850_control_cancel_ex(IedConnection con, char *control_obj, ResponseControl *iecRc);
bool str_ends_with(const char *str, const char *suffix);
const char *ControlAddCause_toString(ControlAddCause cause);
const char *ControlLastApplError_toString(ControlLastApplError error);
const char *IEC61850_GetcommandLastApplError_ApplError(ControlObjectClient ctlCommand);
const char *IEC61850_GetcommandLastApplError_AddCause(ControlObjectClient ctlCommand);
const char *IEC61850_GetcommandLastErrorString(ControlObjectClient ctlCommand);
// FUNCTION

const char *current_time_str()
{
    static char buffer[32]; // Static so it's valid after return
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);

    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm_info);
    return buffer;
}

const char *current_time_str_tz()
{
    static char buffer[64];
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now); // localtime includes timezone info

    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S %Z %z", tm_info);
    return buffer;
}

const char *current_time_iso8601_ms_local()
{
    static char buffer[64];
    struct timespec ts;

    // Use CLOCK_REALTIME to get current local time (system time)
    clock_gettime(CLOCK_REALTIME, &ts);

    struct tm tm_info;
    localtime_r(&ts.tv_sec, &tm_info); // Convert to local time (not UTC)

    int millis = ts.tv_nsec / 1000000; // Get milliseconds (divide nanoseconds by 1 million)

    snprintf(buffer, sizeof(buffer), "%04d-%02d-%02dT%02d:%02d:%02d.%03d%+03d%02d",
             tm_info.tm_year + 1900, tm_info.tm_mon + 1, tm_info.tm_mday,
             tm_info.tm_hour, tm_info.tm_min, tm_info.tm_sec, millis,
             tm_info.tm_gmtoff / 3600, (tm_info.tm_gmtoff % 3600) / 60);

    return buffer;
}

const char *current_time_iso8601_ms_utc()
{
    static char buffer[64];
    struct timespec ts;

    clock_gettime(CLOCK_REALTIME, &ts); // Get current UTC time (nanosecond precision)

    struct tm tm_info;
    gmtime_r(&ts.tv_sec, &tm_info); // Convert to UTC tm structure

    int millis = ts.tv_nsec / 1000000; // Get milliseconds (divide nanoseconds by 1 million)

    snprintf(buffer, sizeof(buffer), "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ",
             tm_info.tm_year + 1900, tm_info.tm_mon + 1, tm_info.tm_mday,
             tm_info.tm_hour, tm_info.tm_min, tm_info.tm_sec, millis);

    return buffer;
}

#define MAX_QUEUE 100
#define MAX_PAYLOAD 4096

typedef struct
{
    char topic[256];
    char payload[MAX_PAYLOAD];
} Message;

Message mqttSubData;

// Check if a string ends with another string
bool str_ends_with(const char *str, const char *suffix)
{
    if (!str || !suffix)
        return false;

    size_t str_len = strlen(str);
    size_t suffix_len = strlen(suffix);

    if (suffix_len > str_len)
        return false;

    return strncmp(str + str_len - suffix_len, suffix, suffix_len) == 0;
}

int is_valid_json(const char *payload)
{
    if (payload == NULL)
        return 0;

    cJSON *json = cJSON_Parse(payload);
    if (json == NULL)
    {
        return 0; // Not valid JSON
    }

    cJSON_Delete(json);
    return 1; // Valid JSON
}

// subscribe callback print
void message_arrived_callback(void *context, char *topicName, int topicLen, MQTTClient_message *message)
{
    printf("Message arrived on topic %s\n", topicName);
    // if (/*strstr(topicName, "control") == NULL &&*/ strstr(topicName, "request") == NULL)
    // {
    //     printf("Wrong Topic: %s\n", topicName);
    //     MQTTClient_freeMessage(&message);
    //     MQTTClient_free(topicName);
    //     return 1;
    // }

    if (is_valid_json((char *)message->payload))
    {
        printf(" Requested Control %s: %.*s\n", topicName, message->payloadlen, (char *)message->payload);
        // char *payload = (char *)message->payload;
        strncpy(mqttSubData.topic, topicName, sizeof(mqttSubData.topic) - 1);
        strncpy(mqttSubData.payload, (char *)message->payload, sizeof(mqttSubData.payload) - 1);
        // enqueueMessage(topicName, payload);
        TopicArrived = true;
    }
    else
        printf("Invalid JSON\n");

    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);
}

int processParseJsonReceive(char *payload, ReceiveControl *out)
{
    cJSON *root = cJSON_Parse(payload);
    if (!root)
    {
        fprintf(stderr, "Error before: [%s]\n", cJSON_GetErrorPtr());
        free(root);
        return 1;
    }
    else
    {
        // value
        cJSON *value = cJSON_GetObjectItem(root, "value");
        if (value)
        {
            if (cJSON_IsNumber(value))
            {
                printf("value (number): %d\n", value->valueint);
            }
            else if (cJSON_IsBool(value))
            {
                printf("value (bool): %s\n", cJSON_IsTrue(value) ? "true" : "false");
            }
            else if (cJSON_IsString(value))
            {
                printf("value (string): %s\n", value->valuestring);
            }
            else
            {
                printf("Warning: 'value' has unsupported type\n");
            }
        }
        else
        {
            printf("Warning: 'value' missing\n");
        }

        // ctlCommand
        cJSON *ctlCommand = cJSON_GetObjectItem(root, "ctlCommand");
        if (cJSON_IsString(ctlCommand))
        {
            printf("ctlCommand: %s\n", ctlCommand->valuestring);
        }
        else
        {
            printf("Error: Missing or invalid 'ctlCommand'\n");
            cJSON_Delete(root);
            return -2;
        }

        // lastValue
        cJSON *lastValue = cJSON_GetObjectItem(root, "lastValue");
        if (lastValue)
        {
            if (cJSON_IsNumber(lastValue))
            {
                printf("lastValue (number): %f\n", lastValue->valuedouble);
            }
            else if (cJSON_IsBool(lastValue))
            {
                printf("lastValue (bool): %s\n", cJSON_IsTrue(lastValue) ? "true" : "false");
            }
            else if (cJSON_IsString(lastValue))
            {
                printf("lastValue (string): %s\n", lastValue->valuestring);
            }
            else
            {
                printf("Warning: 'lastValue' has unsupported type\n");
            }
        }
        else
        {
            printf("Warning: 'lastValue' missing\n");
        }

        // interlocking
        cJSON *interlocking = cJSON_GetObjectItem(root, "interlocking");
        if (cJSON_IsBool(interlocking))
        {
            printf("interlocking: %s\n", cJSON_IsTrue(interlocking) ? "true" : "false");
        }
        else
        {
            printf("Error: Missing or invalid 'interlocking'\n");
            cJSON_Delete(root);
            return -3;
        }

        // synchrocheck
        cJSON *synchrocheck = cJSON_GetObjectItem(root, "synchrocheck");
        if (cJSON_IsBool(synchrocheck))
        {
            printf("synchrocheck: %s\n", cJSON_IsTrue(synchrocheck) ? "true" : "false");
        }
        else
        {
            printf("Error: Missing or invalid 'synchrocheck'\n");
            cJSON_Delete(root);
            return -4;
        }

        // testmode
        cJSON *testmode = cJSON_GetObjectItem(root, "testmode");
        if (cJSON_IsBool(testmode))
        {
            printf("testmode: %s\n", cJSON_IsTrue(testmode) ? "true" : "false");
        }
        else
        {
            printf("Error: Missing or invalid 'testmode'\n");
            cJSON_Delete(root);
            return -5;
        }

        // timestamp
        cJSON *timestamp = cJSON_GetObjectItem(root, "timestamp");
        if (cJSON_IsNumber(timestamp))
        {
            int64_t ts = (int64_t)timestamp->valuedouble;
            printf("timestamp: %" PRId64 "\n", ts);
        }
        else
        {
            printf("Error: Missing or invalid 'timestamp'\n");
            cJSON_Delete(root);
            return -6;
        }

        cJSON_Delete(root);
    }
}

int parseJsonToReceiveControl(const char *jsonStr, ReceiveControl *rc)
{
    if (jsonStr == NULL || rc == NULL)
    {
        return -1; // invalid input
    }

    cJSON *root = cJSON_Parse(jsonStr);
    if (root == NULL)
    {
        return -2; // invalid JSON
    }

// Helper function to copy string safely
#define COPY_STRING_FIELD(field, jsonObj, fieldName, maxLen)                 \
    do                                                                       \
    {                                                                        \
        cJSON *item = cJSON_GetObjectItem(jsonObj, fieldName);               \
        if (cJSON_IsString(item) && (item->valuestring != NULL))             \
        {                                                                    \
            strncpy(rc->field, item->valuestring, maxLen - 1);               \
            rc->field[maxLen - 1] = '\0';                                    \
        }                                                                    \
        else                                                                 \
        {                                                                    \
            rc->field[0] = '\0'; /* empty string if missing or wrong type */ \
        }                                                                    \
    } while (0)

    // Parse typeData
    COPY_STRING_FIELD(object, root, "object", sizeof(rc->object));

    // Parse typeData
    COPY_STRING_FIELD(typeData, root, "typeData", sizeof(rc->typeData));

    // Parse ctlCommand
    COPY_STRING_FIELD(ctlCommand, root, "ctlCommand", sizeof(rc->ctlCommand));

    // Parse value and lastValue - convert any JSON type to string
    cJSON *valueItem = cJSON_GetObjectItem(root, "valueNow");
    if (valueItem)
    {
        if (cJSON_IsBool(valueItem))
        {
            snprintf(rc->valueNow, sizeof(rc->valueNow), "%s", cJSON_IsTrue(valueItem) ? "true" : "false");
        }
        else if (cJSON_IsNumber(valueItem))
        {
            snprintf(rc->valueNow, sizeof(rc->valueNow), "%g", valueItem->valuedouble);
        }
        else if (cJSON_IsString(valueItem))
        {
            strncpy(rc->valueNow, valueItem->valuestring, sizeof(rc->valueNow) - 1);
            rc->valueNow[sizeof(rc->valueNow) - 1] = '\0';
        }
        else
        {
            rc->valueNow[0] = '\0';
        }
    }
    else
    {
        rc->valueNow[0] = '\0';
    }

    cJSON *lastValueItem = cJSON_GetObjectItem(root, "lastValue");
    if (lastValueItem)
    {
        if (cJSON_IsBool(lastValueItem))
        {
            snprintf(rc->lastValue, sizeof(rc->lastValue), "%s", cJSON_IsTrue(lastValueItem) ? "true" : "false");
        }
        else if (cJSON_IsNumber(lastValueItem))
        {
            snprintf(rc->lastValue, sizeof(rc->lastValue), "%g", lastValueItem->valuedouble);
        }
        else if (cJSON_IsString(lastValueItem))
        {
            strncpy(rc->lastValue, lastValueItem->valuestring, sizeof(rc->lastValue) - 1);
            rc->lastValue[sizeof(rc->lastValue) - 1] = '\0';
        }
        else
        {
            rc->lastValue[0] = '\0';
        }
    }
    else
    {
        rc->lastValue[0] = '\0';
    }

    // Parse booleans, default false if missing or wrong type
    cJSON *interlockingItem = cJSON_GetObjectItem(root, "interlocking");
    rc->interlocking = (interlockingItem && cJSON_IsBool(interlockingItem) && cJSON_IsTrue(interlockingItem)) ? true : false;

    cJSON *synchrocheckItem = cJSON_GetObjectItem(root, "synchrocheck");
    rc->synchrocheck = (synchrocheckItem && cJSON_IsBool(synchrocheckItem) && cJSON_IsTrue(synchrocheckItem)) ? true : false;

    cJSON *testmodeItem = cJSON_GetObjectItem(root, "testmode");
    rc->testmode = (testmodeItem && cJSON_IsBool(testmodeItem) && cJSON_IsTrue(testmodeItem)) ? true : false;

    // Parse timestamp (int64)
    cJSON *timestampItem = cJSON_GetObjectItem(root, "timestamp");
    if (timestampItem && cJSON_IsNumber(timestampItem))
    {
        rc->timestamp = (int64_t)timestampItem->valuedouble;
    }
    else
    {
        rc->timestamp = 0;
    }

    cJSON_Delete(root);
    return 0; // success
}

void processIEC61850Control(IedConnection iecConn, const char *ctlModel, ReceiveControl rc, ResponseControl *iecRc)
{

    if (strcmp(rc.ctlCommand, "cancel") == 0)
    {
        log_info("IEC61850: Control :%s", "cancel command");

        ResponseControl RespCancel;
        IEC61850_control_cancel_ex(iecConn, rc.object, &RespCancel);
        memcpy(iecRc, &RespCancel, sizeof(ResponseControl));
    }
    else
    {
        printf("ReceiveControl: %s\n", rc.object);
        if (strcmp(ctlModel, "direct-with-normal-security") == 0)
        {
            ResponseControl RespDirectNormal;
            log_info("IEC61850: Control :%s", "direct normal command");
            IEC61850_control_direct_normal_ex(iecConn, rc.object, rc, &RespDirectNormal);
            memcpy(iecRc, &RespDirectNormal, sizeof(ResponseControl));
        }
        if (strcmp(ctlModel, "sbo-with-normal-security") == 0)
        {
            ResponseControl RespSboNormal;
            log_info("IEC61850: Control :%s", "sbo normal command");
            IEC61850_control_sbo_normal_ex(iecConn, rc.object, rc, &RespSboNormal);
            memcpy(iecRc, &RespSboNormal, sizeof(ResponseControl));
        }
        if (strcmp(ctlModel, "direct-with-enhanced-security") == 0)
        {
            ResponseControl RespDirectSecurity;
            log_info("IEC61850: Control :%s", "direct security command");
            IEC61850_control_direct_security_ex(iecConn, rc.object, rc, &RespDirectSecurity);
            memcpy(iecRc, &RespDirectSecurity, sizeof(ResponseControl));
        }
        if (strcmp(ctlModel, "sbo-with-enhanced-security") == 0)
        {
            ResponseControl RespSboSecurity;
            log_info("IEC61850: Control :%s", "sbo security command");
            IEC61850_control_sbo_security_ex(iecConn, rc.object, rc, &RespSboSecurity);
            memcpy(iecRc, &RespSboSecurity, sizeof(ResponseControl));
        }
    }
}

int MQTT_publish(MQTTClient mqttClient, const char *topic, const char *msg)
{
    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    pubmsg.payload = (void *)msg;
    pubmsg.payloadlen = (int)strlen(msg);
    pubmsg.qos = QOS;
    pubmsg.retained = 0;

    MQTTClient_deliveryToken token;
    int rc = MQTTClient_publishMessage(mqttClient, topic, &pubmsg, &token);
    if (rc != MQTTCLIENT_SUCCESS)
    {
        log_error("MQTT: %s", "Failed to publish message. Error: %d\n", rc);
        return rc;
    }

    MQTTClient_waitForCompletion(mqttClient, token, TIMEOUT);
    return 0;
}

int processResponseControlToJson(ResponseControl *resp, char *outputJson, size_t maxLen)
{
    if (resp == NULL || outputJson == NULL)
        return -1;

    cJSON *root = cJSON_CreateObject();
    if (!root)
        return -1;

    cJSON_AddStringToObject(root, "valueNow", resp->valueNow);
    cJSON_AddStringToObject(root, "lastValue", resp->lastValue);
    cJSON_AddStringToObject(root, "status", resp->status);
    cJSON_AddStringToObject(root, "object", resp->object);
    cJSON_AddStringToObject(root, "ctlCommand", resp->ctlCommand);
    cJSON_AddStringToObject(root, "iecErrorString", resp->iecErrorString);
    cJSON_AddStringToObject(root, "errorString", resp->errorString);
    cJSON_AddStringToObject(root, "timestamp", resp->timestamp);

    char *jsonStr = cJSON_PrintUnformatted(root);
    // printf("%s\n", jsonStr);
    if (!jsonStr)
    {
        cJSON_Delete(root);
        return -1;
    }

    // Copy result to output buffer safely
    strncpy(outputJson, jsonStr, maxLen - 1);
    outputJson[maxLen - 1] = '\0'; // Null-terminate

    // Clean up
    free(jsonStr);
    cJSON_Delete(root);
    return 0;
}

void processMessages(IedConnection iecConn, MQTTClient mqttClient)
{
    // Message msg;
    // while (dequeueMessage(&msg))
    // {
    char respJsonOutput[512];
    if (TopicArrived)
    {
        // log_info("MQTT: Topic received ->  %s", msg.topic);
        // log_info("MQTT: Message received ->%s", msg.payload);
        // printf("DEQUEUE MESSAGE\n");

        ReceiveControl revCtrlObj;
        int ret = parseJsonToReceiveControl(mqttSubData.payload, &revCtrlObj);
        if (ret == 0)
        {
            printf("Parsed ReceiveControl:\n");
            printf(" object: %s\n", revCtrlObj.object);
            printf(" valueNow: %s\n", revCtrlObj.valueNow);
            printf(" lastValue: %s\n", revCtrlObj.lastValue);
            printf(" typeData: %s\n", revCtrlObj.typeData);
            printf(" ctlCommand: %s\n", revCtrlObj.ctlCommand);
            printf(" interlocking: %s\n", revCtrlObj.interlocking ? "true" : "false");
            printf(" synchrocheck: %s\n", revCtrlObj.synchrocheck ? "true" : "false");
            printf(" testmode: %s\n", revCtrlObj.testmode ? "true" : "false");
            printf(" timestamp: %" PRId64 "\n", revCtrlObj.timestamp);

            ResponseControl RespCtl;
            int checkObject = 1;
            for (int i = 0; i < enabledCount; i++)
            {
                if (strcmp(revCtrlObj.object, enabledControls[i].object) == 0)
                {
                    processIEC61850Control(iecConn, enabledControls[i].ctlModel, revCtrlObj, &RespCtl);
                    checkObject = 0;
                    break;
                }
                else
                {
                    checkObject = 1;
                }
            }
            printf("checkObject: %d\n", checkObject);
            if (checkObject != 0)
            {
                strcpy(RespCtl.valueNow, revCtrlObj.valueNow);
                strcpy(RespCtl.ctlCommand, revCtrlObj.ctlCommand);
                strcpy(RespCtl.object, revCtrlObj.object);
                strcpy(RespCtl.lastValue, revCtrlObj.lastValue);
                strcpy(RespCtl.status, "error");
                strcpy(RespCtl.errorString, "none");
                strcpy(RespCtl.iecErrorString, "none");
                strcpy(RespCtl.errorString, "item not found in device");
                strcpy(RespCtl.timestamp, current_time_str());
            }
            ret = processResponseControlToJson(&RespCtl, respJsonOutput, sizeof(respJsonOutput));

            if (ret != 0)
            {
                printf("Failed to generate JSON.\n");
            }
            MQTT_publish(mqttClient, mqtt_topic_control_response, respJsonOutput);
        }
        else
        {
            printf("Failed to parse JSON: %d\n", ret);
        }

        TopicArrived = false;
    }
}

// MQTT

//// ERROR HANDLING IEC61850///////////
const char *ControlAddCause_toString(ControlAddCause cause)
{
    switch (cause)
    {
    case ADD_CAUSE_UNKNOWN:
        return "Unknown";
    case ADD_CAUSE_NOT_SUPPORTED:
        return "Not Supported";
    case ADD_CAUSE_BLOCKED_BY_SWITCHING_HIERARCHY:
        return "Blocked by Switching Hierarchy";
    case ADD_CAUSE_SELECT_FAILED:
        return "Select Failed";
    case ADD_CAUSE_INVALID_POSITION:
        return "Invalid Position";
    case ADD_CAUSE_POSITION_REACHED:
        return "Position Reached";
    case ADD_CAUSE_PARAMETER_CHANGE_IN_EXECUTION:
        return "Parameter Change in Execution";
    case ADD_CAUSE_STEP_LIMIT:
        return "Step Limit Reached";
    case ADD_CAUSE_BLOCKED_BY_MODE:
        return "Blocked by Mode";
    case ADD_CAUSE_BLOCKED_BY_PROCESS:
        return "Blocked by Process";
    case ADD_CAUSE_BLOCKED_BY_INTERLOCKING:
        return "Blocked by Interlocking";
    case ADD_CAUSE_BLOCKED_BY_SYNCHROCHECK:
        return "Blocked by Synchrocheck";
    case ADD_CAUSE_COMMAND_ALREADY_IN_EXECUTION:
        return "Command Already in Execution";
    case ADD_CAUSE_BLOCKED_BY_HEALTH:
        return "Blocked by Health";
    case ADD_CAUSE_1_OF_N_CONTROL:
        return "1 of N Control";
    case ADD_CAUSE_ABORTION_BY_CANCEL:
        return "Abortion by Cancel";
    case ADD_CAUSE_TIME_LIMIT_OVER:
        return "Time Limit Over";
    case ADD_CAUSE_ABORTION_BY_TRIP:
        return "Abortion by Trip";
    case ADD_CAUSE_OBJECT_NOT_SELECTED:
        return "Object Not Selected";
    case ADD_CAUSE_OBJECT_ALREADY_SELECTED:
        return "Object Already Selected";
    case ADD_CAUSE_NO_ACCESS_AUTHORITY:
        return "No Access Authority";
    case ADD_CAUSE_ENDED_WITH_OVERSHOOT:
        return "Ended with Overshoot";
    case ADD_CAUSE_ABORTION_DUE_TO_DEVIATION:
        return "Abortion Due to Deviation";
    case ADD_CAUSE_ABORTION_BY_COMMUNICATION_LOSS:
        return "Abortion by Communication Loss";
    case ADD_CAUSE_ABORTION_BY_COMMAND:
        return "Abortion by Command";
    case ADD_CAUSE_NONE:
        return "None";
    case ADD_CAUSE_INCONSISTENT_PARAMETERS:
        return "Inconsistent Parameters";
    case ADD_CAUSE_LOCKED_BY_OTHER_CLIENT:
        return "Locked by Other Client";
    default:
        return "Invalid Add Cause";
    }
}

const char *ControlLastApplError_toString(ControlLastApplError error)
{
    switch (error)
    {
    case CONTROL_ERROR_NO_ERROR:
        return "No Error";
    case CONTROL_ERROR_UNKNOWN:
        return "Unknown Error";
    case CONTROL_ERROR_TIMEOUT_TEST:
        return "Timeout Test";
    case CONTROL_ERROR_OPERATOR_TEST:
        return "Operator Test";
    default:
        return "Invalid Application Error";
    }
}

const char *IEC61850_GetcommandLastApplError_ApplError(ControlObjectClient ctlCommand)
{
    LastApplError lastApplError = ControlObjectClient_getLastApplError(ctlCommand);
    return ControlLastApplError_toString(lastApplError.error);
}

const char *IEC61850_GetcommandLastApplError_AddCause(ControlObjectClient ctlCommand)
{
    LastApplError lastApplError = ControlObjectClient_getLastApplError(ctlCommand);
    return ControlAddCause_toString(lastApplError.addCause);
}

const char *IEC61850_GetcommandLastErrorString(ControlObjectClient ctlCommand)
{
    IedClientError lastError = ControlObjectClient_getLastError(ctlCommand);
    return IedClientError_toString(lastError);
}

void IEC61850_printError(ControlObjectClient ctlCommand)
{
    printf("%s\n", IEC61850_GetcommandLastErrorString(ctlCommand));
    printf("%s\n", IEC61850_GetcommandLastApplError_ApplError(ctlCommand));
    printf("%s\n", IEC61850_GetcommandLastApplError_AddCause(ctlCommand));
}
//// ERROR HANDLING ///////////

static void commandTerminationHandler(void *parameter, ControlObjectClient connection)
{

    LastApplError lastApplError = ControlObjectClient_getLastApplError(connection);

    /* if lastApplError.error != 0 this indicates a CommandTermination- */
    if (lastApplError.error != 0)
    {
        printf("Received CommandTermination-.\n");
        printf(" LastApplError: %i\n", lastApplError.error);
        printf("      addCause: %i\n", lastApplError.addCause);
    }
    else
        printf("Received CommandTermination+.\n");
}

MmsValue *createDynamicMMS(char *_type, char *value)
{
    log_info("VALUE: %s", value);
    if (strcmp(_type, "boolean") == 0)
    {
        if (strcmp(value, "false") == 0)
            return MmsValue_newBoolean(false);
        else if (strcmp(value, "true") == 0)
            return MmsValue_newBoolean(true);
    }
    if (strcmp(_type, "bit-string") == 0)
    {
        int intBitstring = atoi(value);
        MmsValue *valBitString = MmsValue_newBitString(8);
        // MmsValue_setBitStringFromInteger(valBitString, intBitstring);
        return valBitString;
    }
    if (strcmp(_type, "integer") == 0)
        return MmsValue_newInteger(atoi(value));
}

int IEC61850_control_direct_normal_ex(IedConnection con, char *control_obj, ReceiveControl rc, ResponseControl *resp)
{
    MmsValue *ctlVal = NULL;
    IedClientError error;

    /************************
     * Direct control
     ***********************/
    ControlObjectClient control = ControlObjectClient_create(control_obj, con);
    strcpy(resp->ctlCommand, "direct");
    strcpy(resp->object, control_obj);
    strcpy(resp->lastValue, "none");
    strcpy(resp->iecErrorString, "none");
    strcpy(resp->errorString, "none");

    if (control)
    {
        // ctlVal = MmsValue_newBoolean(true);
        ctlVal = createDynamicMMS(rc.typeData, rc.valueNow);
        strcpy(resp->valueNow, rc.valueNow);

        ControlObjectClient_setOrigin(control, NULL, 3);

        if (ControlObjectClient_operate(control, ctlVal, 0 /* operate now */))
        {
            printf("%s operated successfully\n", control_obj);
            strcpy(resp->status, "success");
        }
        else
        {
            printf("failed to operate %s\n", control_obj);
            strcpy(resp->status, "failed");
            strcpy(resp->errorString, "value not changed");
        }
        MmsValue_delete(ctlVal);
        ControlObjectClient_destroy(control);
    }
    else
    {
        printf("Control object %s not found in server\n", control_obj);
        strcpy(resp->status, "failed");
        strcpy(resp->errorString, "item not found");
    }
    strcpy(resp->timestamp, current_time_str());
}

int IEC61850_control_sbo_normal_ex(IedConnection con, char *control_obj, ReceiveControl rc, ResponseControl *resp)
{
    MmsValue *ctlVal = NULL;
    MmsValue *stVal = NULL;
    /************************
     * Select before operate
     ***********************/
    const char *select_normal_ctl_obj = control_obj;
    ControlObjectClient control = ControlObjectClient_create(select_normal_ctl_obj, con);
    strcpy(resp->ctlCommand, "sbo");
    strcpy(resp->object, control_obj);
    strcpy(resp->lastValue, "none");
    strcpy(resp->iecErrorString, "none");
    strcpy(resp->errorString, "none");

    if (control)
    {
        if (ControlObjectClient_select(control))
        {
            ctlVal = createDynamicMMS(rc.typeData, rc.valueNow);
            strcpy(resp->valueNow, rc.valueNow);
            if (ControlObjectClient_operate(control, ctlVal, 0 /* operate now */))
            {
                printf("%s operated successfully\n", select_normal_ctl_obj);
                strcpy(resp->status, "success");
            }
            else
            {
                printf("failed to operate %s!\n", select_normal_ctl_obj);
                strcpy(resp->status, "failed");
                strcpy(resp->errorString, "value not changed");
            }

            MmsValue_delete(ctlVal);
        }
        else
        {
            printf("failed to select %s!\n", select_normal_ctl_obj);
        }

        ControlObjectClient_destroy(control);
    }
    else
    {
        printf("Control object %s not found in server\n", select_normal_ctl_obj);
        strcpy(resp->status, "failed");
        strcpy(resp->errorString, "item not found");
    }
    strcpy(resp->errorString, "value not changed");
}

int IEC61850_control_direct_security_ex(IedConnection con, char *control_obj, ReceiveControl rc, ResponseControl *resp)
{
    MmsValue *ctlVal = NULL;
    // /****************************************
    //  * Direct control with enhanced security
    //  ****************************************/
    // printf("******* Direct control with enhanced security *******\n");
    const char *direct_security_ctl_obj = control_obj; // control_obj;
    // const char *direct_security_ctl_obj_val = "BCUCPLCONTROL1/GBAY1.Loc.stVal";
    strcpy(resp->ctlCommand, "direct");
    strcpy(resp->object, control_obj);
    strcpy(resp->lastValue, "none");
    strcpy(resp->iecErrorString, "none");
    strcpy(resp->errorString, "none");
    ControlObjectClient control = ControlObjectClient_create(direct_security_ctl_obj, con);

    if (control)
    {
        ControlObjectClient_setCommandTerminationHandler(control, commandTerminationHandler, NULL);

        ctlVal = createDynamicMMS(rc.typeData, rc.valueNow);
        strcpy(resp->valueNow, rc.valueNow);
        if (ControlObjectClient_operate(control, ctlVal, 0 /* operate now */))
        {
            printf("%s operated successfully\n", direct_security_ctl_obj);
            strcpy(resp->valueNow, rc.valueNow);
            strcpy(resp->status, "success");
        }
        else
        {
            printf("failed to operate %s\n", direct_security_ctl_obj);
            strcpy(resp->status, "failed");
            strcpy(resp->errorString, "failed");
        }

        MmsValue_delete(ctlVal);

        /* Wait for command termination message */
        Thread_sleep(1000);

        ControlObjectClient_destroy(control);
    }
    else
    {
        printf("Control object %s not found in server\n", direct_security_ctl_obj);
        strcpy(resp->status, "failed");
        strcpy(resp->errorString, "item not found");
    }
    strcpy(resp->timestamp, current_time_str());
}

int IEC61850_control_sbo_security_ex(IedConnection con, char *control_obj, ReceiveControl rc, ResponseControl *resp)
{
    MmsValue *ctlVal = NULL;
    // /***********************************************
    //  * Select before operate with enhanced security
    //  ***********************************************/
    const char *select_security_ctl_obj = control_obj;
    ControlObjectClient control = ControlObjectClient_create(select_security_ctl_obj, con);

    strcpy(resp->ctlCommand, "sbo");
    strcpy(resp->object, control_obj);
    strcpy(resp->lastValue, "none");
    strcpy(resp->iecErrorString, "none");
    strcpy(resp->errorString, "none");

    if (control)
    {
        ControlObjectClient_setCommandTerminationHandler(control, commandTerminationHandler, NULL);

        ctlVal = createDynamicMMS(rc.typeData, rc.valueNow);
        strcpy(resp->valueNow, rc.valueNow);

        if (ControlObjectClient_selectWithValue(control, ctlVal))
        {

            if (ControlObjectClient_operate(control, ctlVal, 0 /* operate now */))
            {
                printf("%s operated successfully\n", select_security_ctl_obj);
                strcpy(resp->status, "success");
            }
            else
            {
                printf("failed to operate %s!\n", select_security_ctl_obj);
                strcpy(resp->status, "failed");
                strcpy(resp->errorString, "value not changed");
            }
        }
        else
        {
            printf("failed to select %s!\n", select_security_ctl_obj);
        }

        MmsValue_delete(ctlVal);

        Thread_sleep(1000);

        ControlObjectClient_destroy(control);
    }
    else
    {
        printf("Control object %s not found in server\n", select_security_ctl_obj);
        strcpy(resp->status, "failed");
        strcpy(resp->errorString, "item not found");
    }
}

int IEC61850_control_direct_security_exp_ex(IedConnection con, char *control_obj, ReceiveControl rc, ResponseControl *resp)
{
    MmsValue *ctlVal = NULL;
    MmsValue *stVal = NULL;
    // /*********************************************************************
    //  * Direct control with enhanced security (expect CommandTermination-)
    //  *********************************************************************/
    const char *direct_sec_ct_ctl_obj = control_obj;
    strcpy(resp->ctlCommand, "direct");
    strcpy(resp->object, control_obj);
    strcpy(resp->lastValue, "none");
    strcpy(resp->iecErrorString, "none");
    strcpy(resp->errorString, "none");
    ControlObjectClient control = ControlObjectClient_create(direct_sec_ct_ctl_obj, con);

    if (control)
    {
        ControlObjectClient_setCommandTerminationHandler(control, commandTerminationHandler, NULL);
        ctlVal = createDynamicMMS(rc.typeData, rc.valueNow);
        strcpy(resp->valueNow, rc.valueNow);
        if (ControlObjectClient_operate(control, ctlVal, 0 /* operate now */))
        {
            printf("%s operated successfully\n", direct_sec_ct_ctl_obj);
            strcpy(resp->status, "success");
        }
        else
        {
            printf("failed to operate %s\n", direct_sec_ct_ctl_obj);
            strcpy(resp->status, "failed");
            strcpy(resp->errorString, "value not changed");
        }

        MmsValue_delete(ctlVal);
        Thread_sleep(1000);

        ControlObjectClient_destroy(control);
    }
    else
    {
        printf("Control object %s not found in server\n", direct_sec_ct_ctl_obj);
        strcpy(resp->status, "failed");
        strcpy(resp->errorString, "item not found");
    }
    strcpy(resp->timestamp, current_time_str());
}

int IEC61850_control_cancel_ex(IedConnection con, char *control_obj, ResponseControl *resp)
{
    // /*********************************************************************
    //  * Cancel
    //  *********************************************************************/
    const char *cancel_obj = control_obj;
    ControlObjectClient control = ControlObjectClient_create(cancel_obj, con);
    strcpy(resp->ctlCommand, "cancel");
    strcpy(resp->object, control_obj);
    strcpy(resp->valueNow, "none");
    strcpy(resp->lastValue, "none");
    strcpy(resp->iecErrorString, "none");
    strcpy(resp->errorString, "none");
    if (control)
    {
        ControlObjectClient_setCommandTerminationHandler(control, commandTerminationHandler, NULL);

        if (ControlObjectClient_cancel(control /* operate now */))
        {
            printf("%s cancel successfully\n", cancel_obj);
            strcpy(resp->status, "success");
        }
        else
        {
            printf("failed to cancel %s\n", cancel_obj);
            strcpy(resp->status, "failed");
            strcpy(resp->errorString, "object not found");
        }

        Thread_sleep(1000);

        ControlObjectClient_destroy(control);
    }
    else
    {
        printf("Control object %s not found in server\n", cancel_obj);
        strcpy(resp->status, "failed");
        strcpy(resp->errorString, "item not found");
    }
    strcpy(resp->timestamp, current_time_str());
}

int main(int argc, char **argv)
{
    //// PARSING JSON FILE ////
    if (argc != 2)
    {
        printf("Usage: ./client_example_control <json_string_path>\n");
        return 1;
    }

    char *json_string = argv[1];
    // open the JSON file
    FILE *fp = fopen(json_string, "r");
    if (fp == NULL)
    {
        printf("Error: Unable to open the file.\n");
        return 1;
    }

    fseek(fp, 0, SEEK_END);
    long len = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    char *json_data = malloc(len + 1);
    fread(json_data, 1, len, fp);
    json_data[len] = '\0';
    fclose(fp);

    // Parse JSON
    cJSON *json = cJSON_Parse(json_data);
    if (!json)
    {
        fprintf(stderr, "Error before: [%s]\n", cJSON_GetErrorPtr());
        free(json_data);
        return 1;
    }

    // access the JSON data
    char *hostname;
    int master_id_device = 0;
    char *MQTT_CLIENTID;
    cJSON *localIP = cJSON_GetObjectItemCaseSensitive(json, "localIP");
    if (cJSON_IsString(localIP) && (localIP->valuestring != NULL))
    {
        printf("localIP: %s\n", localIP->valuestring);
        hostname = localIP->valuestring;
        srand(time(NULL));
        int random_number = (rand() % 90000) + 10000;
        char snum[5];
        sprintf(snum, "%d", random_number);
        MQTT_CLIENTID = strcat(snum, localIP->valuestring);
        printf("MQTT_CLIENTID: %s\n", MQTT_CLIENTID);
    }

    cJSON *id_device = cJSON_GetObjectItemCaseSensitive(json, "idDevice");
    if (cJSON_IsNumber(id_device) && (id_device->valueint != NULL))
    {
        master_id_device = id_device->valueint;
        // printf("master_id_device: %d\n", master_id_device);
        sprintf(mqtt_topic_control_request, "+/%d/control/request", master_id_device);
        sprintf(mqtt_topic_control_response, "DMS/%d/control/response", master_id_device);
        printf("MQTT: Topic -> %s\n", mqtt_topic_control_request);
    }

    cJSON *controlArray = cJSON_GetObjectItem(json, "control");
    if (cJSON_IsArray(controlArray))
    {
        int size = cJSON_GetArraySize(controlArray);
        for (int i = 0; i < size; i++)
        {
            cJSON *item = cJSON_GetArrayItem(controlArray, i);
            cJSON *enabled = cJSON_GetObjectItem(item, "enabled");
            if (cJSON_IsBool(enabled) && cJSON_IsTrue(enabled))
            {
                cJSON *object = cJSON_GetObjectItem(item, "object");
                cJSON *ctlModel = cJSON_GetObjectItem(item, "ctlModel");

                if (cJSON_IsString(object) && cJSON_IsString(ctlModel))
                {
                    strncpy(enabledControls[enabledCount].object, object->valuestring, sizeof(enabledControls[enabledCount].object) - 1);
                    strncpy(enabledControls[enabledCount].ctlModel, ctlModel->valuestring, sizeof(enabledControls[enabledCount].ctlModel) - 1);
                    enabledCount++;
                }
            }
        }
    }

    printf("Found %d enabled control items:\n", enabledCount);
    for (int i = 0; i < enabledCount; i++)
    {
        printf(" - Object: %s | ctlModel: %s\n", enabledControls[i].object, enabledControls[i].ctlModel);
    }

    //// PARSING JSON FILE ////

    // Initialize MQTT
    // MQTT
    MQTTClient client;
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    int rc;
    int ch;

    const char *uri = ADDRESS;
    printf("MQTT: Using server at %s\n", uri);

    MQTTClient_create(&client, ADDRESS, MQTT_CLIENTID,
                      MQTTCLIENT_PERSISTENCE_NONE, NULL);

    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;
    MQTTClient_setCallbacks(client, NULL, NULL, message_arrived_callback, NULL);

    rc = MQTTClient_connect(client, &conn_opts);
    if (rc != MQTTCLIENT_SUCCESS)
    {
        fprintf(stderr, "MQTT: Failed to connect, return code %d\n", rc);
        return rc;
    }

    rc = MQTTClient_subscribe(client, mqtt_topic_control_request, QOS);
    if (rc != MQTTCLIENT_SUCCESS)
    {
        fprintf(stderr, "MQTT: Failed to subscribe to topic %s, return code %d\n", mqtt_topic_control_request, rc);
    }
    else
    {
        printf("MQTT: Subscribed to topic %s\n", mqtt_topic_control_request);
    }
    // MQTT

    ////////////// MQTT ////////////

    int tcpPort = 102;

    IedClientError error;

    IedConnection con = IedConnection_create();

    IedConnection_connect(con, &error, hostname, tcpPort);

    if (error == IED_ERROR_OK)
    {
        log_info("IEC61850 %s", "Connection Success");
    }
    else
    {
        log_info("IEC61850 %s", "Connection failed | reason:", IedClientError_toString(error));
    }
    while (1)
    {
        Thread_sleep(500);
        processMessages(con, client);
    }

    cJSON_Delete(json);
    free(json_data);
    IedConnection_close(con);
    IedConnection_destroy(con);
    MQTTClient_disconnect(client, 10000);
    MQTTClient_destroy(&client);

    return 0;
}
