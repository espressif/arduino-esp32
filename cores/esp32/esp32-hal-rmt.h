#ifdef __cplusplus
extern "C" {
#endif

// notification flags
#define RMT_FLAG_TX_DONE     (1)
#define RMT_FLAG_RX_DONE     (2)
#define RMT_FLAG_ERROR       (4)
#define RMT_FLAGS_ALL        (RMT_FLAG_TX_DONE | RMT_FLAG_RX_DONE | RMT_FLAG_ERROR)

struct rmt_obj_s;

typedef enum {
    e_reserve_64_items =  1,
    e_reserve_128_items = 2,
    e_reserve_192_items = 3,
    e_reserve_256_items = 4,
    e_reserve_320_items = 5,
    e_reserve_384_items = 6,
    e_reserve_448_items = 7,
    e_reserve_512_items = 8,
} rmt_reserve_memsize_t;

typedef struct rmt_obj_s rmt_obj_t;

/**
*    Initialize the object
*
*/
rmt_obj_t* rmtInit(int pin, bool tx_not_rx, rmt_reserve_memsize_t memsize);

/**
*    Sets the clock/divider of timebase the nearest tick to the supplied value in nanoseconds
*    return the real actual tick value in ns
*/
float rmtSetTick(rmt_obj_t* rmt, float tick);

/**
*    Sending data in one-go mode or continual mode
*     (more data being send while updating buffers in interrupts)
*
*/
bool rmtSend(rmt_obj_t* rmt, uint32_t* data, size_t size);

/**
*    Initiates async receive, event flag indicates data received
*
*/
bool rmtReceive(rmt_obj_t* rmt, uint32_t* data, size_t size, void* eventFlag, bool waitForData, uint32_t timeout);


/*  Additional interface */

/**
*    Start reception
*
*/
bool rmtBeginReceive(rmt_obj_t* rmt);

/**
*    Checks if reception completes
*
*/
bool rmtReceiveCompleted(rmt_obj_t* rmt);

/**
*    Reads the data for particular channel
*
*/
bool rmtGetData(rmt_obj_t* rmt, uint32_t* data, size_t size);

/**
 * Setting threshold for Rx completed
 */
bool rmtSetRxThreshold(rmt_obj_t* rmt, uint32_t value);

bool rmtSetCarrier(rmt_obj_t* rmt, bool carrier_en, bool carrier_level, uint32_t low, uint32_t high);

// TODO:
//  * uninstall interrupt when all channels are deinit
//  * send only-conti mode with circular-buffer
//  * put sanity checks to some macro or inlines
//  * doxy comments
//  * error reporting

#ifdef __cplusplus
}
#endif