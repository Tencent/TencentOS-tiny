#include "cmsis_os.h"
#include <stdio.h>

extern void application_entry(void *arg);

int main(void)
{
   printf("hello world\r\n");
   printf("***I am task\r\n");
   osKernelInitialize(); //TOS Tiny kernel initialize
   application_entry(NULL);
   osKernelStart(); //Start TOS Tiny

   while (1)
   {
   }
}