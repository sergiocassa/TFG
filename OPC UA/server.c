#include "open62541.h"
#include <signal.h>
#include <stdlib.h>
#include <time.h>

UA_Boolean running = true;

static void stopHandler(int sign) {
    running = false;
}

void updateRandomPressure(UA_Server *server, UA_NodeId *nodeId) {
    int randomPressure = rand() % 31;
    UA_Variant value;
    UA_Variant_setScalar(&value, &randomPressure, &UA_TYPES[UA_TYPES_INT32]);

    UA_Server_writeValue(server, *nodeId, value);
}

UA_NodeId posicionNodeId;

static UA_StatusCode setPosicionMethodCallback(UA_Server *server,
                                               const UA_NodeId *sessionId, void *sessionContext,
                                               const UA_NodeId *methodId, void *methodContext,
                                               const UA_NodeId *objectId, void *objectContext,
                                               size_t inputSize, const UA_Variant *input,
                                               size_t outputSize, UA_Variant *output) {
    if(inputSize != 1 || UA_Variant_isScalar(&input[0]) == UA_FALSE || input[0].type != &UA_TYPES[UA_TYPES_INT32]) {
        return UA_STATUSCODE_BADINVALIDARGUMENT;
    }

    int32_t newPosicion = *(int32_t*)input[0].data;
    
    UA_Variant value;
    UA_Variant_setScalar(&value, &newPosicion, &UA_TYPES[UA_TYPES_INT32]);
    UA_StatusCode status = UA_Server_writeValue(server, posicionNodeId, value);

    if(status == UA_STATUSCODE_GOOD) {
        printf("Posicion actualizada a: %d\n", newPosicion);
    } else {
        printf("Error al actualizar posicion: %s\n", UA_StatusCode_name(status));
    }
    
    return status;
}

static UA_StatusCode getPosicionMethodCallback(UA_Server *server,
                                               const UA_NodeId *sessionId, void *sessionContext,
                                               const UA_NodeId *methodId, void *methodContext,
                                               const UA_NodeId *objectId, void *objectContext,
                                               size_t inputSize, const UA_Variant *input,
                                               size_t outputSize, UA_Variant *output) {
    if(inputSize != 0) {
        return UA_STATUSCODE_BADINVALIDARGUMENT;
    }

    UA_Variant positionValue;
    UA_Server_readValue(server, posicionNodeId, &positionValue);

    UA_Variant_copy(&positionValue, &output[0]);

    printf("Posicion obtenida: %d\n", *(int32_t*)output[0].data);

    return UA_STATUSCODE_GOOD;
}

static UA_StatusCode multiplyPosicionMethodCallback(UA_Server *server,
                                                    const UA_NodeId *sessionId, void *sessionContext,
                                                    const UA_NodeId *methodId, void *methodContext,
                                                    const UA_NodeId *objectId, void *objectContext,
                                                    size_t inputSize, const UA_Variant *input,
                                                    size_t outputSize, UA_Variant *output) {
    if (inputSize != 1 || UA_Variant_isScalar(&input[0]) == UA_FALSE || input[0].type != &UA_TYPES[UA_TYPES_INT32]) {
        return UA_STATUSCODE_BADINVALIDARGUMENT;
    }

    int32_t multiplier = *(int32_t*)input[0].data;

    UA_Variant positionValue;
    UA_Server_readValue(server, posicionNodeId, &positionValue);
    int32_t currentPosicion = *(int32_t*)positionValue.data;

    int32_t newPosicion = currentPosicion * multiplier;

    printf("Resultado de multiplicar posicion (%d) por %d es: %d\n", currentPosicion, multiplier, newPosicion);

    if (outputSize > 0) {
        UA_Variant_setScalarCopy(output, &newPosicion, &UA_TYPES[UA_TYPES_INT32]);
    }

    return UA_STATUSCODE_GOOD;
}

int main(void) {
    UA_Server *server = UA_Server_new();
    UA_ServerConfig_setDefault(UA_Server_getConfig(server));

    srand(time(NULL)); 

    UA_NodeId robotNodeId;
    UA_ObjectAttributes robotAttr = UA_ObjectAttributes_default;
    robotAttr.displayName = UA_LOCALIZEDTEXT("es-ES", "Robot");
    UA_Server_addObjectNode(server, UA_NODEID_NUMERIC(1, 1000), UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
                            UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
                            UA_QUALIFIEDNAME(1, "Robot"), UA_NODEID_NUMERIC(0, UA_NS0ID_BASEOBJECTTYPE),
                            robotAttr, NULL, &robotNodeId);

    UA_NodeId brazoNodeId;
    UA_ObjectAttributes brazoAttr = UA_ObjectAttributes_default;
    brazoAttr.displayName = UA_LOCALIZEDTEXT("es-ES", "Brazo");
    UA_Server_addObjectNode(server, UA_NODEID_NULL, robotNodeId, UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
                            UA_QUALIFIEDNAME(1, "Brazo"), UA_NODEID_NUMERIC(0, UA_NS0ID_BASEOBJECTTYPE),
                            brazoAttr, NULL, &brazoNodeId);

    UA_VariableAttributes posicionAttr = UA_VariableAttributes_default;
    posicionAttr.displayName = UA_LOCALIZEDTEXT("es-ES", "Posicion");
    int32_t posicionValue = 5;
    UA_Variant_setScalar(&posicionAttr.value, &posicionValue, &UA_TYPES[UA_TYPES_INT32]);
    
    UA_Server_addVariableNode(server, UA_NODEID_NULL, brazoNodeId, UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
                              UA_QUALIFIEDNAME(1, "Posicion"), UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
                              posicionAttr, NULL, &posicionNodeId);

    UA_NodeId presionNodeId;
    UA_VariableAttributes presionAttr = UA_VariableAttributes_default;
    presionAttr.displayName = UA_LOCALIZEDTEXT("es-ES", "Presion");
    presionAttr.accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;
    presionAttr.userAccessLevel = UA_ACCESSLEVELMASK_READ;
    int32_t presionValue = 0;
    UA_Variant_setScalar(&presionAttr.value, &presionValue, &UA_TYPES[UA_TYPES_INT32]);
    UA_Server_addVariableNode(server, UA_NODEID_NULL, brazoNodeId, UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
                              UA_QUALIFIEDNAME(1, "Presion"), UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
                              presionAttr, NULL, &presionNodeId);

    UA_Server_addRepeatedCallback(server, (UA_ServerCallback)updateRandomPressure, &presionNodeId, 1000, NULL);

    UA_Argument inputArgument;
    UA_Argument_init(&inputArgument);
    inputArgument.description = UA_LOCALIZEDTEXT("es-ES", "Nuevo valor de Posicion");
    inputArgument.name = UA_STRING("posicion");
    inputArgument.dataType = UA_TYPES[UA_TYPES_INT32].typeId;
    inputArgument.valueRank = UA_VALUERANK_SCALAR;

    UA_MethodAttributes setPosicionAttr = UA_MethodAttributes_default;
    setPosicionAttr.displayName = UA_LOCALIZEDTEXT("es-ES", "Set Posicion");
    setPosicionAttr.description = UA_LOCALIZEDTEXT("es-ES", "Establece el valor de posicion");
    setPosicionAttr.executable = true;
    setPosicionAttr.userExecutable = true;

    UA_Server_addMethodNode(server, UA_NODEID_NUMERIC(1, 1001), robotNodeId,
                            UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
                            UA_QUALIFIEDNAME(1, "setPosicion"), setPosicionAttr,
                            &setPosicionMethodCallback,
                            1, &inputArgument, 0, NULL, NULL, NULL);

    UA_Argument outputArgument;
    UA_Argument_init(&outputArgument);
    outputArgument.description = UA_LOCALIZEDTEXT("es-ES", "Valor actual de la Posicion");
    outputArgument.name = UA_STRING("posicion");
    outputArgument.dataType = UA_TYPES[UA_TYPES_INT32].typeId;
    outputArgument.valueRank = UA_VALUERANK_SCALAR;

    UA_MethodAttributes getPosicionAttr = UA_MethodAttributes_default;
    getPosicionAttr.displayName = UA_LOCALIZEDTEXT("es-ES", "Get Posicion");
    getPosicionAttr.description = UA_LOCALIZEDTEXT("es-ES", "Obtiene el valor de posicion");
    getPosicionAttr.executable = true;
    getPosicionAttr.userExecutable = true;

    UA_Server_addMethodNode(server, UA_NODEID_NUMERIC(1, 1002), robotNodeId,
                            UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
                            UA_QUALIFIEDNAME(1, "getPosicion"), getPosicionAttr,
                            &getPosicionMethodCallback,
                            0, NULL, 1, &outputArgument, NULL, NULL);

    UA_Argument multiplyInputArgument;
    UA_Argument_init(&multiplyInputArgument);
    multiplyInputArgument.description = UA_LOCALIZEDTEXT("es-ES", "Multiplicador para posicion");
    multiplyInputArgument.name = UA_STRING("multiplicador");
    multiplyInputArgument.dataType = UA_TYPES[UA_TYPES_INT32].typeId;
    multiplyInputArgument.valueRank = UA_VALUERANK_SCALAR;

    UA_Argument multiplyOutputArgument;
    UA_Argument_init(&multiplyOutputArgument);
    multiplyOutputArgument.description = UA_LOCALIZEDTEXT("es-ES", "Nuevo valor de la Posicion");
    multiplyOutputArgument.name = UA_STRING("posicion");
    multiplyOutputArgument.dataType = UA_TYPES[UA_TYPES_INT32].typeId;
    multiplyOutputArgument.valueRank = UA_VALUERANK_SCALAR;

    UA_MethodAttributes multiplyPosicionAttr = UA_MethodAttributes_default;
    multiplyPosicionAttr.displayName = UA_LOCALIZEDTEXT("es-ES", "Multiply Posicion");
    multiplyPosicionAttr.description = UA_LOCALIZEDTEXT("es-ES", "Multiplica el valor de posicion por el multiplicador dado");
    multiplyPosicionAttr.executable = true;
    multiplyPosicionAttr.userExecutable = true;

    UA_Server_addMethodNode(server, UA_NODEID_NUMERIC(1, 1003), robotNodeId,
                            UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
                            UA_QUALIFIEDNAME(1, "multiplyPosicion"), multiplyPosicionAttr,
                            &multiplyPosicionMethodCallback,
                            1, &multiplyInputArgument, 1, &multiplyOutputArgument, NULL, NULL);

    signal(SIGINT, stopHandler);
    signal(SIGTERM, stopHandler);

    UA_StatusCode retval = UA_Server_run(server, &running);
    UA_Server_delete(server);

    return retval == UA_STATUSCODE_GOOD ? EXIT_SUCCESS : EXIT_FAILURE;
}
