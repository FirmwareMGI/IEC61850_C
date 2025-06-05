/*
 * client_example_control.c
 *
 * How to control a device ... intended to be used with server_example_control
 */

#include "iec61850_client.h"
#include "hal_thread.h"

#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include "MQTTAsync.h"
#include "cJSON.h"
// #include "yyjson.h"

#define ADDRESS "tcp://172.26.80.1:1884"
// MQTT
#define QOS 1
int disc_finished = 0;
int subscribed = 0;
int finished = 0;
char mqtt_topic_control[20];

bool TopicArrived = false;
const int mqttpayloadSize = 100;
char mqttpayload[mqttpayloadSize] = {'\0'};

void onConnect(void *context, MQTTAsync_successData *response);
void onConnectFailure(void *context, MQTTAsync_failureData *response);
void connlost(void *context, char *cause);
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

EnabledControl enabledControls[MAX_ENABLED_CONTROLS];
int enabledCount = 0;
// CONTROL

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

void connlost(void *context, char *cause)
{
    MQTTAsync client = (MQTTAsync)context;
    MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;
    int rc;

    printf("\nMQTT: Connection lost\n");
    if (cause)
        printf("     cause: %s\n", cause);

    printf("MQTT: Reconnecting\n");
    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;
    conn_opts.onSuccess = onConnect;
    conn_opts.onFailure = onConnectFailure;
    if ((rc = MQTTAsync_connect(client, &conn_opts)) != MQTTASYNC_SUCCESS)
    {
        printf("Failed to start connect, return code %d\n", rc);
        finished = 1;
    }
}

int msgarrvd(void *context, char *topicName, int topicLen, MQTTAsync_message *message)
{
    // printf("MQTT: Message arrived\n");
    // printf(" topic: %s\n", topicName);
    // printf("  message: %.*s\n", message->payloadlen, (char *)message->payload);

    if (strstr(topicName, "control") == 0)
    {

        mqttpayload = msg->to_string();
        std::cout << mqttStrPayload << std::endl;
        TopicArrived = true;
        // MQTTClient_freeMessage(&message);
        // MQTTClient_free(topicName);
        // return;
    }

    // if (msg->get_topic() == "104")
    // {
    // TopicArrived = true;
    // }

    // printf("Received control topic: %s\n", topicName);
    // char *payload = (char *)message->payload;

    // // Parse JSON
    // cJSON *root = cJSON_Parse(payload);
    // if (!root)
    // {
    //     printf("Invalid JSON\n");
    //     MQTTClient_freeMessage(&message);
    //     MQTTClient_free(topicName);
    //     return;
    // }

    // for (int i = 0; i < enabledCount; i++)
    // {
    //     // printf(" - Object: %s | ctlModel: %s\n", enabledControls[i].object, enabledControls[i].ctlModel);
    //     if (strcmp(enabledControls[i].object, topicName) == 0)
    //     {

    //     }
    // }

    MQTTAsync_freeMessage(&message);
    MQTTAsync_free(topicName);
    return 1;
}

void onDisconnectFailure(void *context, MQTTAsync_failureData *response)
{
    printf("MQTT: Disconnect failed, rc %d\n", response->code);
    disc_finished = 1;
}

void onDisconnect(void *context, MQTTAsync_successData *response)
{
    printf("MQTT: Successful disconnection\n");
    disc_finished = 1;
}

void onSubscribe(void *context, MQTTAsync_successData *response)
{
    printf("MQTT: Subscribe succeeded\n");
    subscribed = 1;
}

void onSubscribeFailure(void *context, MQTTAsync_failureData *response)
{
    printf("MQTT: Subscribe failed, rc %d\n", response->code);
    finished = 1;
}

void onConnectFailure(void *context, MQTTAsync_failureData *response)
{
    printf("MQTT: Connect failed, rc %d\n", response->code);
    finished = 1;
}

void onConnect(void *context, MQTTAsync_successData *response)
{
    MQTTAsync client = (MQTTAsync)context;
    MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
    int rc;

    printf("MQTT: Successful connection\n");

    printf("Subscribing to topic %s using QoS %d\n", mqtt_topic_control, QOS);
    opts.onSuccess = onSubscribe;
    opts.onFailure = onSubscribeFailure;
    opts.context = client;
    // if ((rc = MQTTAsync_subscribe(client, TOPIC, QOS, &opts)) != MQTTASYNC_SUCCESS)
    // {
    //     printf("MQTT: Failed to start subscribe, return code %d\n", rc);
    //     finished = 1;
    // }
    // char mqtt_topic_control[20];
    // sprintf(mqtt_topic_control, "#/%d/control/#", master_id_device);
    // printf("MQTT: Topic -> %s\n", mqtt_topic_control);

    if ((rc = MQTTAsync_subscribe(client, mqtt_topic_control, QOS, &opts)) != MQTTASYNC_SUCCESS)
    {
        printf("MQTT: Failed to start subscribe, return code %d\n", rc);
        finished = 1;
    }
}
// MQTT

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

int IEC61850_control_direct_normal(IedConnection con, char *control_obj, bool value)
{
    MmsValue *ctlVal = NULL;
    MmsValue *stVal = NULL;
    IedClientError error;

    /************************
     * Direct control
     ***********************/
    printf("******* Direct control *******\n");
    // const char *control_obj_val = "BCUCPLCONTROL1/GBAY1.Loc.stVal";
    ControlObjectClient control = ControlObjectClient_create(control_obj, con);

    if (control)
    {
        ctlVal = MmsValue_newBoolean(true);

        ControlObjectClient_setOrigin(control, NULL, 3);

        if (ControlObjectClient_operate(control, ctlVal, 0 /* operate now */))
        {
            printf("%s operated successfully\n", control_obj);

            return IED_ERROR_OK;
        }
        else
        {
            printf("failed to operate %s\n", control_obj);
        }
        MmsValue_delete(ctlVal);
        ControlObjectClient_destroy(control);
    }
    else
    {
        printf("Control object %s not found in server\n", control_obj);
        return IED_ERROR_OK;
    }
}

int IEC61850_control_sbo_normal(IedConnection con, char *control_obj, bool value)
{
    MmsValue *ctlVal = NULL;
    MmsValue *stVal = NULL;
    /************************
     * Select before operate
     ***********************/
    printf("******* Select before operate *******\n");
    const char *select_normal_ctl_obj = control_obj;
    // const char *select_normal_ctl_obj_val = "BCUCPLCONTROL1/CSWI1.Pos.stVal";
    ControlObjectClient control = ControlObjectClient_create(select_normal_ctl_obj, con);

    if (control)
    {
        if (ControlObjectClient_select(control))
        {

            ctlVal = MmsValue_newBoolean(false);

            if (ControlObjectClient_operate(control, ctlVal, 0 /* operate now */))
            {
                printf("%s operated successfully\n", select_normal_ctl_obj);
            }
            else
            {
                printf("failed to operate %s!\n", select_normal_ctl_obj);
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
    }
}

int IEC61850_control_direct_security(IedConnection con, char *control_obj, bool value)
{
    MmsValue *ctlVal = NULL;
    MmsValue *stVal = NULL;
    // /****************************************
    //  * Direct control with enhanced security
    //  ****************************************/
    printf("******* Direct control with enhanced security *******\n");
    const char *direct_security_ctl_obj = "BCUCPLCONTROL1/GBAY1.Loc";
    const char *direct_security_ctl_obj_val = "BCUCPLCONTROL1/GBAY1.Loc.stVal";
    ControlObjectClient control = ControlObjectClient_create(direct_security_ctl_obj, con);

    if (control)
    {
        ControlObjectClient_setCommandTerminationHandler(control, commandTerminationHandler, NULL);

        ctlVal = MmsValue_newBoolean(false);

        if (ControlObjectClient_operate(control, ctlVal, 0 /* operate now */))
        {
            printf("%s operated successfully\n", direct_security_ctl_obj);
        }
        else
        {
            printf("failed to operate %s\n", direct_security_ctl_obj);
        }

        MmsValue_delete(ctlVal);

        /* Wait for command termination message */
        Thread_sleep(1000);

        ControlObjectClient_destroy(control);

        // /* Check if status value has changed */

        // stVal = IedConnection_readObject(con, &error, direct_security_ctl_obj_val, IEC61850_FC_ST);

        // if (error == IED_ERROR_OK)
        // {
        //     bool state = MmsValue_getBoolean(stVal);

        //     printf("New status of %s: %i\n", direct_security_ctl_obj_val, state);

        //     MmsValue_delete(stVal);
        // }
        // else
        // {
        //     printf("Reading status for %s failed!\n", direct_security_ctl_obj);
        // }
    }
    else
    {
        printf("Control object %s not found in server\n", direct_security_ctl_obj);
    }
}

int IEC61850_control_sbo_security(IedConnection con, char *control_obj, bool value)
{
    MmsValue *ctlVal = NULL;
    MmsValue *stVal = NULL;
    // /***********************************************
    //  * Select before operate with enhanced security
    //  ***********************************************/
    printf("******* Select before operate with enhanced security *******\n");
    const char *select_security_ctl_obj = "BCUCPLCONTROL1/CSWI1.Pos";
    const char *select_security_ctl_obj_val = "BCUCPLCONTROL1/CSWI1.Pos.stVal";
    ControlObjectClient control = ControlObjectClient_create(select_security_ctl_obj, con);

    if (control)
    {
        ControlObjectClient_setCommandTerminationHandler(control, commandTerminationHandler, NULL);

        ctlVal = MmsValue_newBoolean(true);

        if (ControlObjectClient_selectWithValue(control, ctlVal))
        {

            if (ControlObjectClient_operate(control, ctlVal, 0 /* operate now */))
            {
                printf("%s operated successfully\n", select_security_ctl_obj);
            }
            else
            {
                printf("failed to operate %s!\n", select_security_ctl_obj);
            }
        }
        else
        {
            printf("failed to select %s!\n", select_security_ctl_obj);
        }

        MmsValue_delete(ctlVal);

        /* Wait for command termination message */
        Thread_sleep(1000);

        ControlObjectClient_destroy(control);
    }
    else
    {
        printf("Control object %s not found in server\n", select_security_ctl_obj);
    }
}

int IEC61850_control_direct_security_exp(IedConnection con, char *control_obj, bool value)
{
    MmsValue *ctlVal = NULL;
    MmsValue *stVal = NULL;
    // /*********************************************************************
    //  * Direct control with enhanced security (expect CommandTermination-)
    //  *********************************************************************/
    printf("******* Direct control with enhanced security (expect CommandTermination-) *******\n");
    const char *direct_sec_ct_ctl_obj = control_obj;
    // const char *direct_sec_ct_ctl_obj_val = "BCUCPLCONTROL1/GBAY1.Loc.stVal";
    ControlObjectClient control = ControlObjectClient_create(direct_sec_ct_ctl_obj, con);

    if (control)
    {
        ControlObjectClient_setCommandTerminationHandler(control, commandTerminationHandler, NULL);

        ctlVal = MmsValue_newBoolean(true);

        if (ControlObjectClient_operate(control, ctlVal, 0 /* operate now */))
        {
            printf("%s operated successfully\n", direct_sec_ct_ctl_obj);
        }
        else
        {
            printf("failed to operate %s\n", direct_sec_ct_ctl_obj);
        }

        MmsValue_delete(ctlVal);

        /* Wait for command termination message */
        Thread_sleep(1000);

        ControlObjectClient_destroy(control);
    }
    else
    {
        printf("Control object %s not found in server\n", direct_sec_ct_ctl_obj);
    }
}

int IEC61850_control_cancel(IedConnection con, char *control_obj, bool value)
{
    MmsValue *ctlVal = NULL;
    MmsValue *stVal = NULL;

    // /*********************************************************************
    //  * Cancel
    //  *********************************************************************/
    printf("******* Cancel *******\n");
    const char *cancel_obj = control_obj;
    ControlObjectClient control = ControlObjectClient_create(cancel_obj, con);

    if (control)
    {
        ControlObjectClient_setCommandTerminationHandler(control, commandTerminationHandler, NULL);

        ctlVal = MmsValue_newBoolean(true);
        if (ControlObjectClient_cancel(control /* operate now */))
        {
            printf("%s cancel successfully\n", cancel_obj);
        }
        else
        {
            printf("failed to cancel %s\n", cancel_obj);
        }

        MmsValue_delete(ctlVal);

        /* Wait for command termination message */
        Thread_sleep(1000);

        ControlObjectClient_destroy(control);
    }
    else
    {
        printf("Control object %s not found in server\n", cancel_obj);
    }
}

void controlHandle()
{
    if (TopicArrived)
    {
    }
    TopicArrived = false;
}

int main(int argc, char **argv)
{
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
        printf("master_id_device: %d\n", master_id_device);
        sprintf(mqtt_topic_control, "+/%d/control/#", master_id_device);
        printf("MQTT: Topic -> %s\n", mqtt_topic_control);
    }

    // Prepare array for enabled controls
    // EnabledControl *enabledControls = malloc(sizeof(EnabledControl) * 100); // Adjust size as needed
    // int enabledCount = 0;

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

    // Initialize MQTT
    // MQTT
    MQTTAsync client;
    MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;
    MQTTAsync_disconnectOptions disc_opts = MQTTAsync_disconnectOptions_initializer;
    int rc;
    int ch;

    const char *uri = ADDRESS;
    printf("Using server at %s\n", uri);

    if ((rc = MQTTAsync_create(&client, uri, MQTT_CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL)) != MQTTASYNC_SUCCESS)
    {
        printf("Failed to create client, return code %d\n", rc);
        rc = EXIT_FAILURE;
        return 1;
    }

    if ((rc = MQTTAsync_setCallbacks(client, client, connlost, msgarrvd, NULL)) != MQTTASYNC_SUCCESS)
    {
        printf("Failed to set callbacks, return code %d\n", rc);
        rc = EXIT_FAILURE;
        return 1;
    }

    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;
    conn_opts.onSuccess = onConnect;
    conn_opts.onFailure = onConnectFailure;
    conn_opts.context = client;
    if ((rc = MQTTAsync_connect(client, &conn_opts)) != MQTTASYNC_SUCCESS)
    {
        printf("Failed to start connect, return code %d\n", rc);
        rc = EXIT_FAILURE;
        return 1;
    }

    // MQTT

    ////////////// MQTT ////////////

    int tcpPort = 102;

    IedClientError error;

    IedConnection con = IedConnection_create();

    IedConnection_connect(con, &error, hostname, tcpPort);

    if (error == IED_ERROR_OK)
    {
        printf("IEC61850: Connection Success\n");
    }
    else
    {
        printf("IEC61850: Connection failed | reason: %s \n", IedClientError_toString(error));
    }
    while (1)
    {
        Thread_sleep(500);
        controlHandle();
    }

    cJSON_Delete(json);
    free(json_data);
    // free(enabledControls);
    IedConnection_destroy(con);
    MQTTClient_destroy(&client);

    return 0;
}
