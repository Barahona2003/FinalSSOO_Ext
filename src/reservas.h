#ifndef RESERVAS_H
#define RESERVAS_H

typedef struct
{
    int id;
    char cliente[50];
    char fecha_inicio[11];
    char fecha_fin[11];
    int habitacion;
} Reserva;

typedef struct Nodo
{
    Reserva reserva;
    struct Nodo *siguiente;
} Nodo;

// Estructura para mensajes
typedef struct
{
    long mtype;
    char mtext[100];
} mensaje;

// Semáforo para la sincronización de acceso a la lista de reservas
extern sem_t semaforo_reservas;

// Memoria compartida para la lista de reservas
extern Reserva *reservas;

// Funciones pensadas para la gestion de reservas
Nodo *crearNodo(int id, const char *cliente, char *fecha_inicio, char *fecha_fin, int habitacion);
Nodo *crear_reserva(int id, const char *cliente, char *fecha_inicio, char *fecha_fin, int habitacion);
void agregar_reserva(Nodo **head, Nodo *nueva_reserva);
void modificar_reserva(Nodo *head, int id, const char *nuevo_cliente);
void eliminar_reserva(Nodo **head, int id);

// Guardar y cargar reservas desde un fichero
void guardar_reservas(Nodo *head, const char *filename);
Nodo *cargar_reservas(const char *filename);

// Listar todas las reservas y luego permitir buscar reservas por cualquiera de los campos.
void listar_reservas(Nodo *head);
void manejar_solicitud_reserva(int msgid);

// Funciones para la gestión de reservas
void *consultar_reservas(void *arg);
void *actualizar_reserva(void *arg);

void inicializar_memoria_compartida();

#endif // RESERVAS_H