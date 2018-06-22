#ifdef __cplusplus
extern "C" {
#endif

// notification flags
#define RMT_FLAG_TX_DONE     (1)
#define RMT_FLAG_RX_DONE     (2)
#define RMT_FLAG_ERROR       (4)
#define RMT_FLAGS_ALL        (RMT_FLAG_TX_DONE | RMT_FLAG_RX_DONE | RMT_FLAG_ERROR)

struct rmt_obj_s;

typedef struct rmt_obj_s rmt_obj_t;

/**
*    Initialize the object
*
*/
rmt_obj_t* rmtInit(int pin, bool tx_not_rx, int entries, int period);

/**
*    Sets the clock/divider of timebase the nearest tick to the supplied value in nanoseconds
*    return the real actual tick value in ns
*/
float rmtSetTick(rmt_obj_t* rmt, float tick);

/**
*    Sending data in one-go mode (more data must span over multiple channels)
*
*/
bool rmtSend(rmt_obj_t* rmt, uint32_t* data, size_t size);

/**
*    Sending data in one-go mode (more data being send while updating buffers in interrupts)
*
*/
bool rmtSendQueued(rmt_obj_t* rmt, uint32_t* data, size_t size);

/**
*    Initiates simple data receive without interrupts
*
*/
bool rmtReceive(rmt_obj_t* rmt, size_t idle_thres);

/**
*    Wait method for synchronous receive
*
*/
bool rmtWaitForData(rmt_obj_t* rmt, uint32_t* data, size_t size);

/**
*    Initiates async receive, event flag indicates data received
*
*/
bool rmtReceiveAsync(rmt_obj_t* rmt, size_t idle_thres, uint32_t* data, size_t size, void* event_flags);



// TODO:
//  * carrier interface
//  * mutexes
//  * uninstall interrupt when all channels are deinit
//  * send only-conti mode with circular-buffer
//  * put sanity checks to some macro or inlines
//  * doxy comments

#ifdef __cplusplus
}
#endif