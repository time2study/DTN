/**
 * \addtogroup dtn
 * @{
 */
/**
 * \file
 *         the DTN routing protocol
 * \author
 *         Minmin Zhang <amy120728@gmail.com>
 */

#include "contiki.h" 
#include "dtn.h"
#include "dev/leds.h" 
#include "stdio.h" 
#include "lib/sensors.h" 
#include "net/rime.h" 
#include "stddef.h"
#include "net/packetqueue.h"
#include "string.h"

#define DTN_CSVLOG 1

#define FLASH_LED(l) {leds_on(l); clock_delay_msec(50); leds_off(l); clock_delay_msec(50);}

#define printf2ADDR(addr) printf("%02X:%02X", (addr)->u8[1], (addr)->u8[0])

#if DTN_CSVLOG
#define CSVLOG_PACKETBUF(type) csvlog_packetbuf(type)
#endif

/*-MESSAGE QUEUE-------------------------------------------------------------*/
PACKETQUEUE(msg_queue, DTN_QUEUE_MAX);
/*-PRIMITIVE FUNCTIONS-------------------------------------------------------*/
static void recv_spray(struct broadcast_conn *br,const rimeaddr_t *from);
static void recv_request_reply(struct unicast_conn *uni, const rimeaddr_t *from);
static void recv_handoff(struct runicast_conn *runi, const rimeaddr_t *from, uint8_t seqno);
/*-LOCAL CALLBACKS-----------------------------------------------------------*/
const static struct broadcast_callbacks spray_cb={recv_spray};
const static struct unicast_callbacks request_cb = {recv_request_reply};
const static struct runicast_callbacks handoff_cb = {recv_handoff};
/*-LOCAL FUNCTIONS-----------------------------------------------------------*/
void period_queue(struct dtn_conn * dtn_c);
static void send_spray_queue(void *c);
/*---------------------------------------------------------------------------*/
int verify_dtn_packet(struct msg_header * hd){
   return (hd !=NULL && hd->version==DTN_VERSION && hd->magic[0]=='s' && hd->magic[1]=='w');
}
/*---------------------------------------------------------------------------*/
void csvlog_packetbuf(char * type){
  struct msg_header *hd=(struct msg_header *)packetbuf_dataptr();
  printf("%s, ", type);
  printf2ADDR(packetbuf_addr(PACKETBUF_ADDR_SENDER));
  printf(", ");
  printf2ADDR(packetbuf_addr(PACKETBUF_ADDR_RECEIVER));

  if (verify_dtn_packet(hd)) {
    printf(", ");
    printf2ADDR(&hd->esender);
    printf(", ");
    printf2ADDR(&hd->ereceiver);
    printf(", %d, %d\n",hd->epacketid, hd->num_copies);
  } 
  else {
    printf(", X, X, X, X\n");
  }
}
/*---------------------------------------------------------------------------*/
void send_delay(void){
  clock_delay_usec(random_rand() % 10000 + 10000);
}
/*---------------------------------------------------------------------------*/
void print_message(struct msg_header *hd, char *type){
  printf("%s, ",type);
  printf2ADDR(packetbuf_addr(PACKETBUF_ADDR_SENDER));
  printf(", ");
  printf2ADDR(packetbuf_addr(PACKETBUF_ADDR_RECEIVER));
  if(verify_dtn_packet(hd)){
    printf("\n");
    printf("{message: ver:'%d',id:'%d', L:'%d', src:", hd->version,hd->epacketid, hd->num_copies);
    printf2ADDR(&hd->esender);
    printf(", dst:");
    printf2ADDR(&hd->ereceiver); 
    printf(", data: %s}\n", (void*)hd + sizeof(struct msg_header));
  }
  else{
    printf(" NOT ELIGIBLE PACKET!\n");
  }
}
/*---------------------------------------------------------------------------*/
void period_queue(struct dtn_conn * dtn_c){
  static struct ctimer ct;
  ctimer_set(&ct, DTN_SPRAY_DELAY, send_spray_queue, dtn_c);
}
/*---------------------------------------------------------------------------*/
struct packetqueue_item * find_item(struct msg_header *hd2){
  struct packetqueue_item *item=NULL;
	struct queuebuf *qb;
  struct msg_header *hd3;
  for(item=packetqueue_first(&msg_queue);item!=NULL;item=item->next){
    qb=packetqueue_queuebuf(item);
    hd3=(struct msg_header *)queuebuf_dataptr(qb);
    if((hd3->epacketid==hd2->epacketid)&&(rimeaddr_cmp(&(hd2->esender), &(hd3->esender)))){
      break;
    }
  }
  return item;
}
/*-SEND QUEUED SPRAY---------------------------------------------------------*/
static void send_spray_queue(void *c){ 
  struct dtn_conn *dtn_c = c;
  struct packetqueue_item *item=NULL;
  struct queuebuf *qb;
  struct msg_header *hd; 
  printf("===SPRAY THE QUEUED===\n");
  printf("Queue length: %d\n", packetqueue_len(&msg_queue));
  if(packetqueue_len(&msg_queue)==0){
     printf("Queue is empty, broadcast nothing!\n");
  }
  else{
    for(item=packetqueue_first(&msg_queue);item!=NULL;item=item->next){
      qb=packetqueue_queuebuf(item);
	    hd=(struct msg_header *)queuebuf_dataptr(qb);
      if(hd->num_copies>=1){
        packetbuf_clear();   
        queuebuf_to_packetbuf(qb);
        print_message(hd,"SPRAY");
        send_delay();
        CSVLOG_PACKETBUF("spray");
        broadcast_send(&dtn_c->spray_c);
      }
      else{
        printf("Message with copy num 0 doesn't have the authorities to send: ");
        print_message(hd,"SPARY");
      }
    }
  }
  period_queue(dtn_c);
}
/*-OPEN DTN--------------------------------------------------------------------*/
void dtn_open(struct dtn_conn *dtn_c,uint16_t cno,const struct dtn_callbacks *dtn_cb){
  broadcast_open(&dtn_c->spray_c, cno, &spray_cb);
  unicast_open(&dtn_c->request_c, cno+1, &request_cb);
  runicast_open(&dtn_c->handoff_c, cno+1, &handoff_cb);
  dtn_c->dtn_cb = dtn_cb;
  dtn_c->seqno = 0;
  packetqueue_init(&msg_queue);
  printf("DTN open!!\n");
  period_queue(dtn_c);
}
/*-RECEIVE SPRAY---------------------------------------------------------------*/
static void recv_spray(struct broadcast_conn *br, const rimeaddr_t *from){
  struct dtn_conn *c=(struct dtn_conn *)((char *)br-offsetof(struct dtn_conn, spray_c));
  struct msg_header *hd2 = (struct msg_header *) packetbuf_dataptr();
  struct msg_header *hd3;
  struct packetqueue_item *item;
  struct queuebuf *qb;
  rimeaddr_t sender;
  rimeaddr_copy(&sender, packetbuf_addr(PACKETBUF_ADDR_SENDER));
  if(verify_dtn_packet(hd2)){
    item=find_item(hd2);
    if (rimeaddr_cmp(&(hd2->ereceiver), &rimeaddr_node_addr)) {
      if((item==NULL)&&(hd2->num_copies!=0)){
        c->dtn_cb->recv(c, from);
        hd2->num_copies=0;
        if(packetqueue_enqueue_packetbuf(&msg_queue,DTN_MAX_LIFETIME,c)) {
           packetqueue_dequeue(&msg_queue);
           packetqueue_enqueue_packetbuf(&msg_queue,DTN_MAX_LIFETIME,c);
        }
        send_delay();
        CSVLOG_PACKETBUF("request");
        unicast_send(&c->request_c, &sender);
      }
    }
    else if (!rimeaddr_cmp(&(hd2->esender),&rimeaddr_node_addr)){
      if ((item== NULL)) {
        printf("===RECEIVE SPRAY===\n");
        printf("L num of the sender's message: %d\n", hd2->num_copies);
	      hd2->num_copies = 0;
        print_message(hd2,"REQUEST");
        if(packetqueue_len(&msg_queue)<5){
          packetqueue_enqueue_packetbuf(&msg_queue,DTN_MAX_LIFETIME,c);
          printf("SPRAY QUEUED\n");
          if(hd2->num_copies>1){
            send_delay();
            CSVLOG_PACKETBUF("request");
            unicast_send(&c->request_c, &sender);
            printf("SEND OUT REQUEST\n");
          }
        }
        else
         printf("SPRAY QUEUED FAILED\n");
      }
    } 
  }
  else
    printf("Incorrect version and magic of received spray!\n");
  FLASH_LED(LEDS_BLUE);
}
/*-DTN SEND--------------------------------------------------------------------*/
void dtn_send(struct dtn_conn *dtn_c, const rimeaddr_t *dst){
  struct msg_header msg_hd;
  struct queuebuf *qb;
  struct packetqueue_item *item=NULL;
  struct packetqueue_item *item1=NULL;
  msg_hd.version = DTN_VERSION;
  msg_hd.magic[0]='S';
  msg_hd.magic[1]='W';
  msg_hd.num_copies = DTN_L_COPIES;
  msg_hd.epacketid = dtn_c->seqno;
  rimeaddr_copy(&msg_hd.ereceiver, dst);
  rimeaddr_copy(&msg_hd.esender, &rimeaddr_node_addr);
  packetbuf_hdralloc(sizeof(struct msg_header));
  memcpy(packetbuf_hdrptr(),&msg_hd,sizeof(struct msg_header));
  uint8_t success = packetqueue_enqueue_packetbuf(&msg_queue,DTN_MAX_LIFETIME,dtn_c);
  if(!success) {
    packetqueue_dequeue(&msg_queue);
    packetqueue_enqueue_packetbuf(&msg_queue,DTN_MAX_LIFETIME,dtn_c);
  }
  for(item=packetqueue_first(&msg_queue);item!=NULL;item=item->next){
    item1=item;
  }                                                                                                       
  packetbuf_clear();
  qb = packetqueue_queuebuf(item1);
  queuebuf_to_packetbuf(qb);
  printf("++++++++++++++++++++++++++++++\n");
  printf("Generate && broadcast my own message!\n");
  print_message(packetbuf_dataptr(),"SPRAY");
  send_delay();
  CSVLOG_PACKETBUF("spray");
  broadcast_send(&(dtn_c->spray_c));
  dtn_c->seqno++;
}
/*-RECEIVE REQUEST/REPLY-------------------------------------------------------*/
static void recv_request_reply(struct unicast_conn *uni, const rimeaddr_t *from){
  struct msg_header *hd2=(struct msg_header *)packetbuf_dataptr();
  struct msg_header *hd3;
  struct queuebuf *qb;
  rimeaddr_t sender;
  rimeaddr_copy(&sender, packetbuf_addr(PACKETBUF_ADDR_SENDER));
  struct dtn_conn *c = (struct dtn_conn *) ((char *) uni - offsetof(struct dtn_conn, request_c));
  struct packetqueue_item *item;
  item=find_item(hd2);
  if (item!= NULL) {
    qb=packetqueue_queuebuf(item);
    hd3=(struct msg_header *)queuebuf_dataptr(qb);
    if(rimeaddr_cmp(&(hd2->ereceiver), &sender)){
      printf("Arrive destination: ");
      printf2ADDR(&hd2->ereceiver);
      printf(", last hop is: ");
      printf2ADDR(&sender);
      printf("\n");
      hd3->num_copies=0;
      queuebuf_to_packetbuf(qb);
      print_message(hd3,"REPLY");
    }
    else if(hd3->num_copies>1){
      printf("Handoff start, send copy num to: ");
      printf2ADDR(&sender);
      printf("\n");
      hd3->num_copies=hd3->num_copies/2;
      queuebuf_to_packetbuf(qb);
      hd3->num_copies=hd3->num_copies*2;
      send_delay();
      CSVLOG_PACKETBUF("handoff");
      if(runicast_send(&c->handoff_c,&sender,DTN_RTX) != 0) {
         hd3->num_copies=hd3->num_copies/2;
         printf("Handoff successes, changed message: \n");
         print_message(hd3,"HANDOFF");  
      }
      else
        printf("HANDOFF FAILED!\n");
   }
 }
}
/*-RECEIVE HANDOFF-----------------------------------------------------------*/
static void recv_handoff(struct runicast_conn *runi, const rimeaddr_t *from, uint8_t seqno){
  struct dtn_conn *c = (struct dtn_conn *) ((char *) runi - offsetof(struct dtn_conn, handoff_c));
  struct msg_header *hd2=(struct msg_header *)packetbuf_dataptr();
  struct msg_header *hd3;
  struct packetqueue_item *item;
  struct queuebuf *pb;
  rimeaddr_t sender;
  rimeaddr_copy(&sender, packetbuf_addr(PACKETBUF_ADDR_SENDER));
  item=find_item(hd2);
  if(item!=NULL){    
    pb=packetqueue_queuebuf(item);
    hd3=(struct msg_header *)queuebuf_dataptr(pb);
    hd3->num_copies+=hd2->num_copies;
    if(hd3->num_copies>8){
       hd3->num_copies=8;
    }
    printf("Get handoff from:");
    printf2ADDR(&sender);
    printf("\nReceived the copy num! Handoff successes!\n");
    CSVLOG_PACKETBUF("handoff");
    print_message(hd3,"HANDOFF");
  }
  printf("===END HANDOFF===\n");
}
/*-DTN CLOSE-----------------------------------------------------------------*/
void dtn_close(struct dtn_conn *dtn_c){
  broadcast_close(&dtn_c->spray_c);
  unicast_close(&dtn_c->request_c);
  runicast_close(&dtn_c->handoff_c);
}
/*---------------------------------------------------------------------------*/
/** @} */
