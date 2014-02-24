/**
 * \file
 * \brief
 *     An test of using the Delay-Tolerant Networking module
 * \author
 *     Minmin Zhang <amy120728@gmail.com>
 */
#include "stdio.h"
#include "contiki.h"
#include "button-sensors.h"
#include "dev/leds.h"
#include "random.h"
#include "dtn.h"
#include "net/rime.h"
#include "string.h"

#define printf2ADDR(addr) printf("%02X:%02X", (addr)->u8[1], (addr)->u8[0])

static void dtn_recv(struct dtn_conn *c, const rimeaddr_t *from){ 
  struct msg_header *hd=(struct msg_header *)packetbuf_dataptr(); 
  rimeaddr_t sender;
  rimeaddr_copy(&sender, packetbuf_addr(PACKETBUF_ADDR_SENDER));
  printf("===RECEIVE MESSAGE FOR ME VIA BROADCAST===\n");
  printf("my address: ");
  printf2ADDR(&rimeaddr_node_addr);
  printf("the last hop: ");
  printf2ADDR(&sender);
  printf("\n");
  printf("{received message: ver:'%d', id:'%d', L:'%d', src:", hd->version,hd->epacketid, hd->num_copies);
  printf2ADDR(&hd->esender);
  printf(", dst:");
  printf2ADDR(&hd->ereceiver);
  printf(", data: %s}\n", (void*)hd + sizeof(struct msg_header));
}

static const struct dtn_callbacks call = {dtn_recv}; 
static struct dtn_conn dtn_c; 

PROCESS(test_dtn_process, "Test DTN");
AUTOSTART_PROCESSES(&test_dtn_process);

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(test_dtn_process, ev, data) 
{ 
  PROCESS_EXITHANDLER(dtn_close(&dtn_c);) 
  PROCESS_BEGIN(); 

  static rimeaddr_t myaddr, dst;
  rimeaddr_copy(&myaddr, &rimeaddr_null);
  myaddr.u8[0] = 18;
  rimeaddr_set_node_addr(&myaddr);
  rimeaddr_copy(&dst, &rimeaddr_null);
  dst.u8[0] = 15;

  static char msg[200];
  static int count = 0;
  SENSORS_ACTIVATE(button_sensor); 
  leds_off(LEDS_BLUE | LEDS_GREEN); 
  PROCESS_WAIT_EVENT_UNTIL(ev == sensors_event && data == &button_sensor); 
  set_power(0x1);  
  dtn_open(&dtn_c,DTN_SPRAY_CHANNEL,&call);
  
  while(1) { 
    PROCESS_WAIT_EVENT_UNTIL(ev == sensors_event && data ==   &button_sensor); 
    sprintf(msg, "Amy Cheung-%d", count);
    packetbuf_copyfrom(msg,strlen(msg)+1); 
    dtn_send(&dtn_c,&dst);
    count++;
    leds_on(LEDS_BLUE | LEDS_GREEN); 
  }
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
