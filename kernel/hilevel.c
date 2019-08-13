/* Copyright (C) 2017 Daniel Page <csdsp@bristol.ac.uk>
 *
 * Use of this source code is restricted per the CC BY-NC-ND license, a copy of 
 * which can be found via http://creativecommons.org (and should be included as 
 * LICENSE.txt within the associated archive or repository).
 */

#include "hilevel.h"

pcb_t pcb[ 3]; pcb_t* current = NULL;


int maxPriority(){
    int maximum = 0;
    int temp;
    current->changingPriority = current->initialPriority;

    for(int i = 0; i<3;i++){
         if(pcb[i].pid != current->pid){
           pcb[i].changingPriority++;
       }
        if(pcb[i].changingPriority > maximum){
            maximum = pcb[i].changingPriority;
            temp = i;
        }
    } 
    return temp;
}

void print( char* str ) {
    for ( int i = 0; i < strlen( str ); i++ ) {
        PL011_putc( UART0, str[i], true ); 
    }
}

void dispatch( ctx_t* ctx, pcb_t* prev, pcb_t* next ) {
  char prev_pid = '?', next_pid = '?';

  if( NULL != prev ) {
    memcpy( &prev->ctx, ctx, sizeof( ctx_t ) ); // preserve execution context of P_{prev}
    prev_pid = '0' + prev->pid;
  }
  if( NULL != next ) {
    memcpy( ctx, &next->ctx, sizeof( ctx_t ) ); // restore  execution context of P_{next}
    next_pid = '0' + next->pid;
  }

    PL011_putc( UART0, '[',      true );
    PL011_putc( UART0, prev_pid, true );
    PL011_putc( UART0, '-',      true );
    PL011_putc( UART0, '>',      true );
    PL011_putc( UART0, next_pid, true );
    PL011_putc( UART0, ']',      true );

    current = next;                             // update   executing index   to P_{next}

  return;
}

void schedule( ctx_t* ctx ) {
   //next is the max priority , have current pid
    
    dispatch(ctx, current, &pcb[maxPriority()] );
    
    current->status = STATUS_READY;            
    pcb[maxPriority()].status = STATUS_EXECUTING;

  return;
}

extern void     main_P3(); 
extern uint32_t tos_P3;
extern void     main_P4(); 
extern uint32_t tos_P4;
extern void     main_P5(); 
extern uint32_t tos_P5;

void hilevel_handler_rst(ctx_t* ctx) {
  memset( &pcb[ 0 ], 0, sizeof( pcb_t ) );     // initialise 0-th PCB = P_1
  pcb[ 0 ].pid      = 0;
  pcb[ 0 ].status   = STATUS_CREATED;
  pcb[0].initialPriority = 8;
  pcb[0].changingPriority = pcb[0].initialPriority;
  pcb[ 0 ].ctx.cpsr = 0x50;
  pcb[ 0 ].ctx.pc   = ( uint32_t )( &main_P3 );
  pcb[ 0 ].ctx.sp   = ( uint32_t )( &tos_P3  );

  memset( &pcb[ 1 ], 0, sizeof( pcb_t ) );     // initialise 1-st PCB = P_2
  pcb[ 1 ].pid      = 1;
  pcb[ 1 ].status   = STATUS_CREATED;
  pcb[1].initialPriority = 3;
  pcb[1].changingPriority = pcb[1].initialPriority;
  pcb[ 1 ].ctx.cpsr = 0x50;
  pcb[ 1 ].ctx.pc   = ( uint32_t )( &main_P4 );
  pcb[ 1 ].ctx.sp   = ( uint32_t )( &tos_P4 );
    
  memset( &pcb[ 2 ], 0, sizeof( pcb_t ) );     // initialise 1-st PCB = P_2
  pcb[ 2 ].pid      =2;
  pcb[ 2 ].status   = STATUS_CREATED;
  pcb[2].initialPriority = 10;
  pcb[2].changingPriority = pcb[2].initialPriority;
  pcb[ 2 ].ctx.cpsr = 0x50;
  pcb[ 2 ].ctx.pc   = ( uint32_t )( &main_P5 );
  pcb[ 2 ].ctx.sp   = ( uint32_t )( &tos_P5 );
    
  dispatch( ctx, NULL, &pcb[ maxPriority()] );
  
    
  TIMER0->Timer1Load  = 0x00100000; // select period = 2^20 ticks ~= 1 sec
  TIMER0->Timer1Ctrl  = 0x00000002; // select 32-bit   timer
  TIMER0->Timer1Ctrl |= 0x00000040; // select periodic timer
  TIMER0->Timer1Ctrl |= 0x00000020; // enable          timer interrupt
  TIMER0->Timer1Ctrl |= 0x00000080; // enable          timer

  GICC0->PMR          = 0x000000F0; // unmask all            interrupts
  GICD0->ISENABLER1  |= 0x00000010; // enable timer          interrupt
  GICC0->CTLR         = 0x00000001; // enable GIC interface
  GICD0->CTLR         = 0x00000001; // enable GIC distributor

  int_enable_irq();

  return;
}

void hilevel_handler_irq(ctx_t* ctx) {
  uint32_t id = GICC0->IAR;
	
  if( id == GIC_SOURCE_TIMER0 ) {
    PL011_putc( UART0, 'T', true ); 
    TIMER0->Timer1IntClr = 0x01;
    schedule(ctx);
  }

  GICC0->EOIR = id;

  return;
}

void hilevel_handler_svc(ctx_t* ctx, uint32_t id) {
  switch( id ) {
    case 0x00 : { // 0x00 => yield()
      schedule( ctx );

      break;
    }

    case 0x01 : { // 0x01 => write( fd, x, n )
      int   fd = ( int   )( ctx->gpr[ 0 ] );  
      char*  x = ( char* )( ctx->gpr[ 1 ] );  
      int    n = ( int   )( ctx->gpr[ 2 ] ); 

      for( int i = 0; i < n; i++ ) {
        PL011_putc( UART0, *x++, true );
      }
      
      ctx->gpr[ 0 ] = n;

      break;
    }

    default   : { // 0x?? => unknown/unsupported
      break;
    }
  }

  return;
}
