/*==================[inclusions]=============================================*/

#include "board.h"
#include "os.h"

#include <string.h>

/*==================[macros and definitions]=================================*/

/** valor de retorno de excepción a cargar en el LR */
#define EXC_RETURN 0xFFFFFFF9

/** id de tarea inválida */
#define INVALID_TASK ((uint32_t)-1)

#define STACK_IDLE_SIZE 256

/** estructura interna de control de tareas */
typedef struct taskControlBlock {
	uint32_t sp;
	const taskDefinition * tdef;
	taskState state;
	uint32_t waiting_time;
}taskControlBlock;

/*==================[internal data declaration]==============================*/

/*==================[internal functions declaration]=========================*/

__attribute__ ((weak)) void * idle_hook(void * p);

/*==================[internal data definition]===============================*/

/** indice a la tarea actual */
static uint32_t current_task = INVALID_TASK;

/** estructura interna de control de tareas */
static taskControlBlock task_control_list[TASK_COUNT];

/** contexto de la tarea idle */
uint8_t stack_idle[STACK_IDLE_SIZE];
static taskDefinition idle_tdef = {
		stack_idle, STACK_IDLE_SIZE, idle_hook, 0
};
static taskControlBlock idle_task_control;

/*==================[external data definition]===============================*/

/*==================[internal functions definition]==========================*/

/* si una tarea ejecuta return, vengo acá */
static void return_hook(void * returnValue)
{
	while(1);
}

/* task_create sirve para crear un contexto inicial */
static void task_create(
		uint8_t * stack_frame, /* vector de pila (frame) */
		uint32_t stack_frame_size, /* el tamaño expresado en bytes */
		uint32_t * stack_pointer, /* donde guardar el puntero de pila */
		entry_point_t entry_point, /* punto de entrada de la tarea */
		void * parameter, /* parametro de la tarea */
		taskState * state)
{
	uint32_t * stack = (uint32_t *)stack_frame;

	/* inicializo el frame en cero */
	bzero(stack, stack_frame_size);

	/* último elemento del contexto inicial: xPSR
	 * necesita el bit 24 (T, modo Thumb) en 1
	 */
	stack[stack_frame_size/4 - 1] = 1<<24;

	/* anteúltimo elemento: PC (entry point) */
	stack[stack_frame_size/4 - 2] = (uint32_t)entry_point;

	/* penúltimo elemento: LR (return hook) */
	stack[stack_frame_size/4 - 3] = (uint32_t)return_hook;

	/* elemento -8: R0 (parámetro) */
	stack[stack_frame_size/4 - 8] = (uint32_t)parameter;

	stack[stack_frame_size/4 - 9] = EXC_RETURN;

	/* inicializo stack pointer inicial */
	*stack_pointer = (uint32_t)&(stack[stack_frame_size/4 - 17]);

	/* seteo estado inicial READY */
	*state = TASK_STATE_READY;
}

void task_delay_update(void)
{
	uint32_t i;
	for (i=0; i<TASK_COUNT; i++) {
		if ( (task_control_list[i].state == TASK_STATE_WAITING) &&
				(task_control_list[i].waiting_time > 0)) {
			task_control_list[i].waiting_time--;
			if (task_control_list[i].waiting_time == 0) {
				task_control_list[i].state = TASK_STATE_READY;
			}
		}
	}
}

/*==================[external functions definition]==========================*/

void delay(uint32_t milliseconds)
{
	if (current_task != INVALID_TASK) {
		task_control_list[current_task].state = TASK_STATE_WAITING;
		task_control_list[current_task].waiting_time = milliseconds;
		schedule();
	}
}

/* rutina de selección de próximo contexto a ejecutarse
 * acá definimos la política de scheduling de nuestro os
 */
int32_t getNextContext(int32_t current_context)
{
	//Guardamos el estado anterior (current_task es una variable global)
	uint32_t previous_task = current_task;


	//Verificamos si está en el IDLE (o inicio), guardamos su contexto y chequeamos la 1era tarea
	if (current_task == INVALID_TASK) {
		idle_task_control.sp = current_context;
		current_task = 0;
	}
	//Si no, guardamos el contexto de la tarea y apuntamos a la próxima tarea (circular)
	else {
		task_control_list[current_task].sp = current_context;

		current_task++;
		if (current_task == TASK_COUNT) {
			current_task = 0;
		}

	}

	uint32_t Task_Count = TASK_COUNT;
	//A partir de la proxima, chequeamos todas mientras no estén READY
	while((task_control_list[current_task].state != TASK_STATE_READY)
			           && (Task_Count--) > 0 ) {
		//Cuando estaba en IDLE y no había tareas en READY, no volvía a IDLE sino
		//Que se quedaba en el loop. Aseguraré que sólo chequee el número de tareas
		//y salga.
		current_task++;
		if (current_task == TASK_COUNT) {
			current_task = 0;
		}

	}

	if (current_task == previous_task) {
			//Si volvió a la tarea anterior y no estaba running, no hay tareas para lanzar
			//así que se debe ir a Idle. En cualquiera de los casos, la tarea debe mantener
			//su estado, si estaba RUNNING, seguirá RUNNING y si no, no debe ser modificado
			if (task_control_list[current_task].state != TASK_STATE_RUNNING) {
				current_task = INVALID_TASK;
				idle_task_control.state = TASK_STATE_RUNNING;
				return idle_task_control.sp;
			}
		}

		else {

			//Si no volvió a la tarea anterior, hay que evaluar si viene de IDLE
			//y si se encontró una tarea para lanzar. Si no, se debe volver a IDLE
			if ((previous_task == INVALID_TASK)) {
				if (0 == Task_Count){
					current_task = INVALID_TASK;
					idle_task_control.state = TASK_STATE_RUNNING;
					return idle_task_control.sp;
				} else {
					idle_task_control.state = TASK_STATE_READY;
				}
			}
			//Si no viene de INVALID se debe cambiar el estado de la tarea anterior a READY
			//en caso de que estuviera RUNNING
			else {
				if (task_control_list[previous_task].state == TASK_STATE_RUNNING) {
					task_control_list[previous_task].state = TASK_STATE_READY;
				}
			}
			//En cualquiera de los casos, se debe colocar la nueva tarea en RUNNING
			//(Si la nueva tarea es IDLE, ya se le colocó RUNNING y el return no lo
			//deja llegar hasta acá)
			task_control_list[current_task].state = TASK_STATE_RUNNING;

		}

	//Si llegó hasta acá, no cayó en IDLE, así que se devuelve el contexto de la
	//próxima tarea a ejecutar.
	return task_control_list[current_task].sp;
}

void schedule(void)
{
	/* activo PendSV para llevar a cabo el cambio de contexto */
	SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;

	/* Instruction Synchronization Barrier: aseguramos que se
	 * ejecuten todas las instrucciones en  el pipeline
	 */
	__ISB();
	/* Data Synchronization Barrier: aseguramos que se
	 * completen todos los accesos a memoria
	 */
	__DSB();
}

void SysTick_Handler(void)
{
	task_delay_update();
	schedule();
}

int start_os(void)
{
	uint32_t i;

	/* actualizo SystemCoreClock (CMSIS) */
	SystemCoreClockUpdate();

	/* inicializo contexto idle */
	idle_task_control.tdef = &idle_tdef;
	task_create(idle_task_control.tdef->stack,
			idle_task_control.tdef->stack_size,
			&(idle_task_control.sp),
			idle_task_control.tdef->entry_point,
			idle_task_control.tdef->parameter,
			&(idle_task_control.state));

	/* inicializo contextos iniciales de cada tarea */
	for (i=0; i<TASK_COUNT; i++) {
		task_control_list[i].tdef = task_list+i;

		task_create(task_control_list[i].tdef->stack,
				task_control_list[i].tdef->stack_size,
				&(task_control_list[i].sp),
				task_control_list[i].tdef->entry_point,
				task_control_list[i].tdef->parameter,
				&(task_control_list[i].state));
	}

	/* configuro PendSV con la prioridad más baja */
	NVIC_SetPriority(PendSV_IRQn, 255);
	SysTick_Config(SystemCoreClock / 1000);

	/* llamo al scheduler */
	schedule();

	idle_hook(NULL);

	return -1;
}

void * idle_hook(void * p)
{
	while (1) {
		__WFI();
	}
}

/*==================[end of file]============================================*/
