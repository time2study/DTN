/**
 * \addtogroup dtn
 * @{
 */

 /**
 * \defgroup dtn Delay-Tolerant Networking
 * 
 * @{
 *
 * The dtn protocol sends packets via multiple hops to a specified
 * receiver somewhere in the network.
 *
 *
 * \section channels Channels
 *
 * The dtn protocol uses 3 channels: one for broadcasting packet to the
 * destination via multi-hops, one unicast channel to request for copy 
 * number before the packet copies can be broadcast, and one runicast
 * channel to send the copy number to the receiver sending out the
 * request.
 *
 */

/**
 * \file
 *         Header file for dtn protocol 
 * \author
 *         Minmin Zhang <amy120728@gmail.com>
 */

#ifndef __DTN_H__
#define __DTN_H__

#ifndef DTN_CONF_DEFAULT_L_COPIES 
#define DTN_L_COPIES 8
#else
#define DTN_L_COPIES DTN_CONF_DEFAULT_L_COPIES 
#endif

#include "math.h"
#include "broadcast.h"
#include "unicast.h"
#include "runicast.h"

#define DTN_QUEUE_MAX 5 
#define DTN_SPRAY_DELAY 5 * CLOCK_SECOND
#define DTN_MAX_LIFETIME DTN_SPRAY_DELAY*2*log(DTN_QUEUE_MAX)/log(2)+1
#define DTN_VERSION 1
#define DTN_RTX 3

#define PRINT2ADDR(addr) printf("%02X:%02X", addr->u8[1], addr->u8[0])

struct dtn_conn;

/** Callbacks structure for \ref dtn "DTN" */
struct dtn_callbacks { 
  /** Called when a packet destinated for me is received via broadcast channel. */
  void (* recv)(struct dtn_conn *c, const rimeaddr_t *from); 
};

/** Representation of a \ref dtn "DTN" connections */
struct dtn_conn{ 
  struct broadcast_conn spray_c;      /**< The broadcast connection for spray */
  struct unicast_conn request_c;      /**< The unicast connection for request */
  struct runicast_conn handoff_c;     /**< The runicast connection for handoff */
  const struct dtn_callbacks *dtn_cb; /**< Pointer to the callbacks structure */
  uint8_t seqno;                      /**< Current sequence number of the message */
};

/** Structure for \ref dtn "DTN" message header */
struct msg_header{ 
  uint8_t version;       /**< DTN protocol version */
  uint8_t magic[2];      /**< Magic defined in DTN protocol */
  uint16_t num_copies;   /**< Number of copies */
  rimeaddr_t esender;    /**< Origin's address */
  rimeaddr_t ereceiver;  /**< Destination's address */
  uint16_t epacketid;    /**< Identifier of message */
};

/**
 * \brief             Open a dtn connection
 * \param dtn_c       A pointer to a struct \ref dtn_conn
 * \param cno         Channels used for spray connetion, request connection
 *                    and handoff connection which are on cno, cno+1 and
 *                    cno+2 respectively.
 * \param dtn_cb      Pointer to callback structure \ref dtn_callbacks
 *
 *             This function sets up a dtn connection on three
 *             specified channels for broadcast, unicast as well  
 *             as runicast respectively. The caller must have 
 *             allocated the memory for thestruct \ref dtn_conn,
 *             usually by declaring it as a static variable.
 *
 *             The struct \ref dtn_callbacks pointer must point to a structure
 *             containing function pointers to function that will be called
 *             when a packet arrives on the broadcast channel.
 *
 */

void dtn_open(struct dtn_conn *dtn_c,uint16_t cno,const struct dtn_callbacks *dtn_cb);

/**
 * \brief          Send a dtn packet
 * \param dtn_c    A pointer to dtn connection \ref dtn_conn on which the packet 
 *                 should be sent
 * \param dst      A pointer to the address of the final destination of the packet
 *
 *             This function sends a DTN broadcast packet. The packet must be
 *             present in the packetbuf when this function is called.
 *
 *             The parameter dtn_c must point to an broadcast 
 *             connection used for broadcast packets via broadcast_open().
 *
 */

void dtn_send(struct dtn_conn *dtn_c, const rimeaddr_t *dst);

/**
 * \brief          Close an dtn connection
 * \param dtn_c    A pointer to a struct \ref dtn_conn
 *
 *             This function closes an dtn connection that has
 *             previously been opened with dtn_open().
 *
 *             This function typically is called as an exit handler.
 *
 */

void dtn_close(struct dtn_conn *dtn_c);


#endif /* __DTN_H__ */
/** @} */
/** @} */
