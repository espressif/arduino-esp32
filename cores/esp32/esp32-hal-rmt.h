
#ifdef __cplusplus
extern "C" {
#endif

struct rmt_obj_s;

typedef struct rmt_obj_s rmt_obj_t;

/**
*    Initialize the object
*
*/
rmt_obj_t* rmtInit(int pin, bool tx_not_rx, int entries, int period);

float rmtSetTick(rmt_obj_t* rmt, float tick);

bool rmtSend(rmt_obj_t* rmt, uint32_t* data, size_t size);

bool rmtSendQueued(rmt_obj_t* rmt, uint32_t* data, size_t size);

bool rmtReceive(rmt_obj_t* rmt, size_t idle_thres);

bool rmtWaitForData(rmt_obj_t* rmt, uint32_t* data, size_t size);

#ifdef __cplusplus
}
#endif