#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/msg.h>
#include "reservas.h"

int shm_id;
Reserva *reservas;

// Semáforo para controlar el acceso a la lista de reservas
sem_t semaforo_reservas;

// Función para manejar la solicitud de reserva
void manejar_solicitud_reserva(int msgid)
{
    mensaje msg;
    if (msgrcv(msgid, &msg, sizeof(msg.mtext), 1, 0) == -1)
    {
        perror("Error al recibir mensaje");
        exit(1);
    }
    printf("Proceso hijo con PID: %d recibió el mensaje: %s\n", getpid(), msg.mtext);
    printf("Proceso hijo con PID: %d está manejando una solicitud de reserva.\n", getpid());
}

// Función para crear una nueva reserva
Nodo *crear_reserva(int id, const char *cliente, char *fecha_inicio_str, char *fecha_fin_str, int habitacion)
{
    Nodo *nueva_reserva = (Nodo *)malloc(sizeof(Nodo));
    if (nueva_reserva == NULL)
    {
        printf("Error al asignar memoria para la nueva reserva.\n");
        exit(1);
    }

    nueva_reserva->reserva.id = id;
    strncpy(nueva_reserva->reserva.cliente, cliente, 50);
    nueva_reserva->reserva.cliente[49] = '\0';

    strncpy(nueva_reserva->reserva.fecha_inicio, fecha_inicio_str, 11);
    nueva_reserva->reserva.fecha_inicio[10] = '\0';

    strncpy(nueva_reserva->reserva.fecha_fin, fecha_fin_str, 11);
    nueva_reserva->reserva.fecha_fin[10] = '\0';

    nueva_reserva->reserva.habitacion = habitacion;
    nueva_reserva->siguiente = NULL;

    return nueva_reserva;
}

// Función para agregar una nueva reserva a la lista
void agregar_reserva(Nodo **head, Nodo *nueva_reserva)
{
    if (nueva_reserva == NULL)
    {
        printf("Error: No se puede agregar una reserva nula.\n");
        return;
    }
    nueva_reserva->siguiente = *head;
    *head = nueva_reserva;
}

// Función para modificar una reserva existente
void modificar_reserva(Nodo *head, int id, const char *nuevo_cliente)
{
    if (head == NULL)
    {
        printf("Error: La lista de reservas está vacía.\n");
        return;
    }
    Nodo *actual = head;
    while (actual != NULL)
    {
        if (actual->reserva.id == id)
        {
            strncpy(actual->reserva.cliente, nuevo_cliente, 50);
            return;
        }
        actual = actual->siguiente;
    }
    printf("Reserva con ID %d no encontrada.\n", id);
}

// Función para eliminar una reserva
void eliminar_reserva(Nodo **head, int id)
{
    if (*head == NULL)
    {
        printf("Error: La lista de reservas está vacía.\n");
        return;
    }
    Nodo *actual = *head;
    Nodo *anterior = NULL;

    while (actual != NULL && actual->reserva.id != id)
    {
        anterior = actual;
        actual = actual->siguiente;
    }
    if (actual == NULL)
    {
        printf("Reserva con ID %d no encontrada.\n", id);
        return;
    }
    if (anterior == NULL)
    {
        *head = actual->siguiente;
    }
    else
    {
        anterior->siguiente = actual->siguiente;
    }
    free(actual);
}

// Función para guardar las reservas en un fichero
void guardar_reservas(Nodo *head, const char *filename)
{
    FILE *file = fopen(filename, "w");
    if (file == NULL)
    {
        perror("Error al abrir el archivo para guardar las reservas");
        exit(EXIT_FAILURE);
    }

    Nodo *actual = head;
    while (actual != NULL)
    {
        fprintf(file, "%d;%s;%s;%s;%d\n",
                actual->reserva.id,
                actual->reserva.cliente,
                actual->reserva.fecha_inicio,
                actual->reserva.fecha_fin,
                actual->reserva.habitacion);
        actual = actual->siguiente;
    }

    fclose(file);
}

// Función para listar todas las reservas
void listar_reservas(Nodo *head)
{
    if (head == NULL)
    {
        printf("La lista de reservas está vacía.\n");
        return;
    }
    Nodo *actual = head;

    while (actual != NULL)
    {
        printf("ID: %d, Cliente: %s, Fecha Inicio: %s, Fecha Fin: %s, Habitación: %d\n",
               actual->reserva.id, actual->reserva.cliente,
               actual->reserva.fecha_inicio, actual->reserva.fecha_fin,
               actual->reserva.habitacion);
        actual = actual->siguiente;
    }
}

// Función para inicializar la memoria compartida
void inicializar_memoria_compartida()
{
    shm_id = shmget(IPC_PRIVATE, sizeof(Reserva) * 100, IPC_CREAT | 0666);
    if (shm_id == -1)
    {
        perror("Error al crear la memoria compartida");
        exit(1);
    }
    reservas = shmat(shm_id, NULL, 0);
    if (reservas == (void *)-1)
    {
        perror("Error al adjuntar la memoria compartida");
        exit(1);
    }
}

// Función para consultar reservas (para hilos)
void *consultar_reservas(void *arg)
{
    Nodo *head = (Nodo *)arg;
    if (head == NULL)
    {
        printf("Error: La lista de reservas está vacía.\n");
        pthread_exit(NULL);
    }
    listar_reservas(head);
    pthread_exit(NULL);
}

// Función para actualizar reservas (para hilos)
void *actualizar_reserva(void *arg)
{
    Nodo *head = (Nodo *)arg;
    if (head == NULL)
    {
        printf("Error: La lista de reservas está vacía.\n");
        pthread_exit(NULL);
    }
    modificar_reserva(head, head->reserva.id, "Nuevo Cliente");
    printf("Reserva actualizada en hilo con ID: %ld\n", pthread_self());
    pthread_exit(NULL);
}

// Función para cargar las reservas desde un fichero
Nodo *cargar_reservas(const char *filename)
{
    FILE *file = fopen(filename, "r");
    if (file == NULL)
    {
        perror("Error al abrir el archivo para cargar las reservas");
        return NULL;
    }

    Nodo *head = NULL;
    char linea[128];
    while (fgets(linea, sizeof(linea), file))
    {
        Reserva reserva;

        // Leer los datos de la línea
        char *token = strtok(linea, ";");
        if (token == NULL)
        {
            printf("Error: No se pudo leer el ID de la reserva.\n");
            continue;
        }
        reserva.id = atoi(token);

        token = strtok(NULL, ";");
        if (token == NULL)
        {
            printf("Error: No se pudo leer el cliente de la reserva.\n");
            continue;
        }
        strncpy(reserva.cliente, token, sizeof(reserva.cliente) - 1);
        reserva.cliente[sizeof(reserva.cliente) - 1] = '\0';

        token = strtok(NULL, ";");
        if (token == NULL)
        {
            printf("Error: No se pudo leer la fecha de inicio de la reserva.\n");
            continue;
        }
        strncpy(reserva.fecha_inicio, token, sizeof(reserva.fecha_inicio) - 1);
        reserva.fecha_inicio[sizeof(reserva.fecha_inicio) - 1] = '\0';

        token = strtok(NULL, ";");
        if (token == NULL)
        {
            printf("Error: No se pudo leer la fecha de fin de la reserva.\n");
            continue;
        }
        strncpy(reserva.fecha_fin, token, sizeof(reserva.fecha_fin) - 1);
        reserva.fecha_fin[sizeof(reserva.fecha_fin) - 1] = '\0';

        token = strtok(NULL, ";");
        if (token == NULL)
        {
            printf("Error: No se pudo leer la habitación de la reserva.\n");
            continue;
        }
        reserva.habitacion = atoi(token);

        // Crear una nueva reserva y agregarla a la lista
        Nodo *nueva_reserva = crear_reserva(reserva.id, reserva.cliente, reserva.fecha_inicio, reserva.fecha_fin, reserva.habitacion);
        agregar_reserva(&head, nueva_reserva);
    }

    fclose(file);
    return head;
}