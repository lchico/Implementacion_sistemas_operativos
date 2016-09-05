/*==================[inclusions]=============================================*/

#include "os.h"
#include "board.h"

/*==================[macros and definitions]=================================*/

/** tama��o de pila para los threads */
#define STACK_SIZE 512

/*==================[internal data declaration]==============================*/

/*==================[internal functions declaration]=========================*/

static void * tarea1(void * param);
static void * tarea2(void * param);

/*==================[internal data definition]===============================*/

/*==================[external data definition]===============================*/

/* pilas de cada tarea */
uint8_t stack1[STACK_SIZE];
uint8_t stack2[STACK_SIZE];

const taskDefinition task_list[TASK_COUNT] = {
		{stack1, STACK_SIZE, tarea1, (void *)0xAAAAAAAA},
		{stack2, STACK_SIZE, tarea2, (void *)0xBBBBBBBB}
};

/*==================[internal functions definition]==========================*/

static void * tarea1(void * param)
{
	uint32_t i=0,j;
	while (1) {
		Board_LED_Toggle(0);
		for(i=0;i<1000000;i++) j=i;
		//delay(500);
	}
	return (void *)0; /* a d��nde va? */
}

static void * tarea2(void * param)
{
	uint32_t i=0,j;
	while (1) {
		Board_LED_Toggle(3);
		for(i=0;i<1000000;i++) j=i;
		delay(1000);
	}
	return (void *)4; /* a d��nde va? */
}

/*==================[external functions definition]==========================*/

int main(void)
{
	/* Inicializaci��n del MCU */
	Board_Init();

	/* Inicio OS */
	start_os();

	/* no deber��amos volver ac�� */
	while(1);
}

/*==================[end of file]============================================*/
