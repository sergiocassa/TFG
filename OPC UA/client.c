#include "open62541.h"
#include <stdio.h>
#include <stdlib.h>

void callSetPosicion(UA_Client *client, UA_NodeId robotNodeId) {
    int32_t nuevaPosicion;
    printf("Introduce el nuevo valor para posicion: ");
    scanf("%d", &nuevaPosicion);

    UA_Variant input;
    UA_Variant_init(&input);
    UA_Variant_setScalar(&input, &nuevaPosicion, &UA_TYPES[UA_TYPES_INT32]);

    UA_StatusCode status = UA_Client_call(client, robotNodeId, UA_NODEID_NUMERIC(1, 1001), 1, &input, NULL, NULL);
    if(status == UA_STATUSCODE_GOOD)
        printf("Método setPosicion invocado correctamente\n");
    else
        printf("Error al invocar setPosicion: %s\n", UA_StatusCode_name(status));
}

void callGetPosicion(UA_Client *client, UA_NodeId robotNodeId) {
    UA_Variant *output = NULL;
    size_t outputSize = 1;
    UA_StatusCode status = UA_Client_call(client, robotNodeId, UA_NODEID_NUMERIC(1, 1002), 0, NULL, &outputSize, &output);
    if(status == UA_STATUSCODE_GOOD && outputSize > 0) {
        int32_t posicionActual = *(int32_t*)output[0].data;
        printf("Posición actual: %d\n", posicionActual);
        UA_Variant_delete(output);  // Libera la memoria del output
    } else {
        printf("Error al invocar getPosicion: %s\n", UA_StatusCode_name(status));
    }
}

void callMultiplyPosicion(UA_Client *client, UA_NodeId robotNodeId) {
    int32_t multiplicador;
    printf("Introduce el valor multiplicador: ");
    scanf("%d", &multiplicador);

    UA_Variant inputMulti;
    UA_Variant_init(&inputMulti);
    UA_Variant_setScalar(&inputMulti, &multiplicador, &UA_TYPES[UA_TYPES_INT32]);

    UA_Variant *outputMulti = NULL;
    size_t outputMultiSize = 1;
    UA_StatusCode status = UA_Client_call(client, robotNodeId, UA_NODEID_NUMERIC(1, 1003), 1, &inputMulti, &outputMultiSize, &outputMulti);
    if(status == UA_STATUSCODE_GOOD && outputMultiSize > 0) {
        int32_t nuevaPosicion = *(int32_t*)outputMulti[0].data;
        printf("Posición tras multiplicación: %d\n", nuevaPosicion);
        UA_Variant_delete(outputMulti);  // Libera la memoria del outputMulti
    } else {
        printf("Error al invocar multiplyPosicion: %s\n", UA_StatusCode_name(status));
    }
}

int main(void) {
    UA_Client *client = UA_Client_new();
    UA_ClientConfig_setDefault(UA_Client_getConfig(client));

    // Conexión al servidor OPC UA
    UA_StatusCode status = UA_Client_connect(client, "opc.tcp://localhost:4840");
    if(status != UA_STATUSCODE_GOOD) {
        UA_Client_delete(client);
        printf("No se pudo conectar al servidor OPC UA\n");
        return status;
    }
    printf("Conectado al servidor OPC UA\n");

    // NodeId del objeto "Robot"
    UA_NodeId robotNodeId = UA_NODEID_NUMERIC(1, 1000);  // Ajustar según el servidor

    int opcion;
    while(1) {
        printf("\nSeleccione una opción:\n");
        printf("1. Establecer nueva posición (setPosicion)\n");
        printf("2. Obtener posición actual (getPosicion)\n");
        printf("3. Multiplicar posición (multiplyPosicion)\n");
        printf("0. Salir\n");
        printf("Opción: ");
        scanf("%d", &opcion);

        switch(opcion) {
            case 1:
                callSetPosicion(client, robotNodeId);
                break;
            case 2:
                callGetPosicion(client, robotNodeId);
                break;
            case 3:
                callMultiplyPosicion(client, robotNodeId);
                break;
            case 0:
                goto cleanup;
            default:
                printf("Opción no válida. Intente nuevamente.\n");
                break;
        }
    }

cleanup:
    UA_Client_disconnect(client);
    UA_Client_delete(client);
    printf("Desconectado del servidor OPC UA\n");

    return EXIT_SUCCESS;
}
