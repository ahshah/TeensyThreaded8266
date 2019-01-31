#ifndef ESP8266_PRIVATE
#define ESP8266_PRIVATE

typedef enum {
    NEW_CMD = 0,
    STATUS,
    IPD_STATUS,
    IPD_MUX,
    IPD_LENGTH,
    IPD_FRAME
} recv_state_t;

typedef enum {
    READY,
    WAIT_FOR_TRANSMISSION,
    WAIT_FOR_TRANSMISSION_RESULT,
    TRANSMISSION_COMPLETE,
    TRANSMIT
} tx_state_t;

typedef struct {
    char buf[1024];
    uint16_t iter;
    uint16_t ipd_length = 0;
    uint8_t ipd_mux    = 0;
    uint8_t  ipd_mux_term_index = 0;
    uint8_t  ipd_header_length = 0;
    recv_state_t state;
    recv_msg_t msg;
} recv_ctx_t;
recv_ctx_t ctx;

typedef struct {
    tx_state_t state;
    uint16_t requested_tx_len;
    uint16_t wrote;
    uint8_t mux_id;
    bool failed;
    Threads::Mutex lock;
    char reason[32];
} tx_ctx_t;

tx_ctx_t ctx_tx;
#endif
