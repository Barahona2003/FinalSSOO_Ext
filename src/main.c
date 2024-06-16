#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include "reservas.h"

#define NUM_SOLICITUDES 5
#define NUM_HILOS 2

int main()
{
    // Inicializar la memoria compartida.
    inicializar_memoria_compartida();

    // Inicializar el semáforo
    sem_init(&semaforo_reservas, 0, 1);

    // Cargar reservas desde el archivo
    Nodo *lista_reservas = cargar_reservas("reservas.txt");
    if (lista_reservas == NULL)
    {
        printf("No se encontraron reservas en el archivo.\n");
    }

    // Crear la cola de mensajes
    key_t key = ftok("reservas", 65);
    int msgid = msgget(key, 0666 | IPC_CREAT);
    if (msgid == -1)
    {
        perror("Error al crear la cola de mensajes\n");
        exit(1);
    }

    // Crear procesos para manejar solicitudes de reserva
    printf("Creando procesos e hilos para manejar solicitudes de reserva...\n\n");
    for (int i = 0; i < NUM_SOLICITUDES; i++)
    {
        pid_t pid = fork();
        if (pid == 0)
        {
            // Código del proceso hijo
            manejar_solicitud_reserva(msgid);
            exit(0);
        }
        else if (pid < 0)
        {
            // Error al crear el proceso
            perror("Error al crear el proceso");
            exit(1);
        }
    }

    // Crear hilos para consultar y actualizar reservas
    pthread_t hilos[NUM_HILOS];
    printf("Consultando reservas...\n");

    pthread_create(&hilos[0], NULL, consultar_reservas, (void *)lista_reservas);
    pthread_join(hilos[0], NULL); // Esperar a que el hilo 0 termine

    printf("\n");
    pthread_create(&hilos[1], NULL, actualizar_reserva, (void *)lista_reservas);

    // Enviar mensajes a los procesos hijos
    for (int i = 0; i < NUM_SOLICITUDES; i++)
    {
        mensaje msg;
        msg.mtype = 1;
        sprintf(msg.mtext, "Solicitud de reserva %d", i + 1);
        if (msgsnd(msgid, &msg, sizeof(msg.mtext), 0) == -1)
        {
            perror("Error al enviar mensaje");
            exit(1);
        }
    }

    // Esperar a que todos los procesos hijos terminen
    for (int i = 0; i < NUM_SOLICITUDES; i++)
    {
        wait(NULL);
    }

    // Esperar a que el hilo 1 termine
    pthread_join(hilos[1], NULL);

    // Variable para controlar el bucle de entrada del usuario
    bool continuar = true;
    while (continuar)
    {
        // Menú de opciones para el usuario
        printf("\n¿Qué acción desea realizar?\n");
        printf("1. Modificar una reserva\n");
        printf("2. Eliminar una reserva\n");
        printf("3. Añadir una reserva\n");
        printf("4. Listar reservas\n");
        printf("5. Salir\n");

        // Variable para almacenar la opción seleccionada por el usuario
        int opcion;
        printf("\nIngrese el número de la opción (1-5): ");
        while (scanf("%d", &opcion) != 1 || getchar() != '\n' || opcion < 1 || opcion > 5)
        {
            printf("Opción inválida. Por favor, ingrese un número válido (1-5): ");
            while (getchar() != '\n')
                ;
        }

        // Procesar la opción seleccionada por el usuario
        switch (opcion)
        {
        case 1: // Modificar una reserva
        {
            int id_modificar;
            bool id_valido = false;
            while (!id_valido)
            {
                printf("Ingrese el ID de la reserva que desea modificar: ");
                if (scanf("%d", &id_modificar) == 1 && getchar() == '\n')
                {
                    id_valido = true;
                }
                else
                {
                    printf("ID inválido. Por favor, ingrese un número válido.\n");
                    while (getchar() != '\n')
                        ;
                }
            }

            // Solicitar al usuario el nuevo nombre del cliente
            char nuevo_cliente[50];
            bool nombre_valido = false;
            while (!nombre_valido)
            {
                printf("Ingrese el nuevo nombre del cliente (mínimo 3 caracteres): ");
                if (scanf("%s", nuevo_cliente) == 1 && getchar() == '\n' && strlen(nuevo_cliente) >= 3 && strspn(nuevo_cliente, " \n") == 0)
                {
                    nombre_valido = true;
                }
                else
                {
                    printf("Nombre inválido. Por favor, ingrese un nombre válido (mínimo 3 caracteres y sin espacios en blanco ni saltos de línea).\n");
                    while (getchar() != '\n')
                        ;
                }
            }

            // Modificar la reserva
            modificar_reserva(lista_reservas, id_modificar, nuevo_cliente);
            printf("Reserva modificada correctamente.\n");
            break;
        }
        case 2: // Eliminar una reserva
        {
            int id_eliminar;
            bool id_valido = false;
            while (!id_valido)
            {
                printf("Ingrese el ID de la reserva que desea eliminar: ");
                if (scanf("%d", &id_eliminar) == 1 && getchar() == '\n')
                {
                    id_valido = true;
                }
                else
                {
                    printf("ID inválido. Por favor, ingrese un número válido.\n");
                    while (getchar() != '\n')
                        ;
                }
            }

            // Eliminar la reserva
            eliminar_reserva(&lista_reservas, id_eliminar);
            printf("Reserva eliminada correctamente.\n");
            break;
        }
        case 3: // Crear una nueva reserva
        {
            int id_nueva = 0;
            Nodo *temp = lista_reservas;
            while (temp != NULL)
            {
                if (temp->reserva.id > id_nueva)
                {
                    id_nueva = temp->reserva.id;
                }
                temp = temp->siguiente;
            }
            id_nueva++; // Incrementar el id en 1

            // Solicitar al usuario el nombre del cliente
            char cliente_nuevo[50];
            bool nombre_valido = false;
            while (!nombre_valido)
            {
                printf("Ingrese el nombre del cliente: ");
                if (scanf("%s", cliente_nuevo) == 1 && getchar() == '\n')
                {
                    nombre_valido = true;
                }
                else
                {
                    printf("Nombre inválido. Por favor, ingrese un nombre válido.\n");
                    while (getchar() != '\n')
                        ;
                }
            }

            // Solicitar al usuario las fechas de inicio y fin de la reserva
            char fecha_inicio_str[11];
            char fecha_fin_str[11];
            bool fecha_valida = false;
            while (!fecha_valida)
            {
                printf("Ingrese la fecha de inicio de la reserva (formato: AAAA-MM-DD): ");
                if (scanf("%s", fecha_inicio_str) == 1 && getchar() == '\n')
                {
                    // Validar la fecha si es necesario
                    fecha_valida = true;
                }
                else
                {
                    printf("Fecha inválida. Por favor, ingrese una fecha válida.\n");
                    while (getchar() != '\n')
                        ;
                }
            }

            fecha_valida = false;
            while (!fecha_valida)
            {
                printf("Ingrese la fecha de fin de la reserva (formato: AAAA-MM-DD): ");
                if (scanf("%s", fecha_fin_str) == 1 && getchar() == '\n')
                {
                    // Validar la fecha si es necesario
                    fecha_valida = true;
                }
                else
                {
                    printf("Fecha inválida. Por favor, ingrese una fecha válida.\n");
                    while (getchar() != '\n')
                        ;
                }
            }

            // Solicitar al usuario el número de habitación
            int habitacion_nueva;
            while (true)
            {
                printf("Ingrese el número de habitación: ");
                if (scanf("%d", &habitacion_nueva) == 1 && getchar() == '\n')
                {
                    break;
                }
                else
                {
                    printf("Número de habitación inválido. Por favor, ingrese un número válido.\n");
                    while (getchar() != '\n')
                        ;
                }
            }

            // Crear la nueva reserva
            Nodo *nueva_reserva = crear_reserva(id_nueva, cliente_nuevo, fecha_inicio_str, fecha_fin_str, habitacion_nueva);

            // Agregar la nueva reserva a la lista
            agregar_reserva(&lista_reservas, nueva_reserva);
            printf("Reserva añadida correctamente.\n");
            break;
        }
        case 4: // Leer las reservas
        {
            printf("Lista de reservas:\n");
            listar_reservas(lista_reservas);
            break;
        }
        case 5: // Guardar y salir.
        {
            FILE *archivo_reservas = fopen("reservas.txt", "w");
            if (archivo_reservas == NULL)
            {
                perror("Error al abrir o crear el archivo de reservas.txt");
                exit(1);
            }
            guardar_reservas(lista_reservas, "reservas.txt");
            fclose(archivo_reservas);

            printf("¡Hasta luego!\n");
            continuar = false;
            break;
        }
        default:
            break;
        }
    }

    // Liberar el semáforo
    sem_destroy(&semaforo_reservas);

    // Eliminar la cola de mensajes
    msgctl(msgid, IPC_RMID, NULL);

    // Liberar la memoria de la lista de reservas
    Nodo *temp;
    while (lista_reservas != NULL)
    {
        temp = lista_reservas;
        lista_reservas = lista_reservas->siguiente;
        free(temp);
    }

    return 0;
}
