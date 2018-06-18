
#ifdef __cplusplus
extern "C" {
#endif


typedef  
    struct {
        uint32_t div_cnt:        8;             /*This register is used to configure the  frequency divider's factor in channel0-7.*/
        uint32_t idle_thres:    16;             /*In receive mode when no edge is detected on the input signal for longer than reg_idle_thres_ch0 then the receive process is done.*/
        uint32_t mem_size:       4;             /*This register is used to configure the the amount of memory blocks allocated to channel0-7.*/
        uint32_t carrier_en:     1;             /*This is the carrier modulation enable control bit for channel0-7.*/
        uint32_t carrier_out_lv: 1;             /*This bit is used to configure the way carrier wave is modulated for  channel0-7.1'b1:transmit on low output level  1'b0:transmit  on high output level.*/
        uint32_t mem_pd:         1;             /*This bit is used to reduce power consumed by memory. 1:memory is in low power state.*/
        uint32_t clk_en:         1;             /*This bit  is used  to control clock.when software configure RMT internal registers  it controls the register clock.*/

        uint32_t tx_start:        1;            /*Set this bit to start sending data for channel0-7.*/
        uint32_t rx_en:           1;            /*Set this bit to enable receiving data for channel0-7.*/
        uint32_t mem_wr_rst:      1;            /*Set this bit to reset write ram address for channel0-7 by receiver access.*/
        uint32_t mem_rd_rst:      1;            /*Set this bit to reset read ram address for channel0-7 by transmitter access.*/
        uint32_t apb_mem_rst:     1;            /*Set this bit to reset W/R ram address for channel0-7 by apb fifo access*/
        uint32_t mem_owner:       1;            /*This is the mark of channel0-7's ram usage right.1'b1：receiver uses the ram  0：transmitter uses the ram*/
        uint32_t tx_conti_mode:   1;            /*Set this bit to continue sending  from the first data to the last data in channel0-7 again and again.*/
        uint32_t rx_filter_en:    1;            /*This is the receive filter enable bit for channel0-7.*/
        uint32_t rx_filter_thres: 8;            /*in receive mode  channel0-7 ignore input pulse when the pulse width is smaller then this value.*/
        uint32_t ref_cnt_rst:     1;            /*This bit is used to reset divider in channel0-7.*/
        uint32_t ref_always_on:   1;            /*This bit is used to select base clock. 1'b1:clk_apb  1'b0:clk_ref*/
        uint32_t idle_out_lv:     1;            /*This bit configures the output signal's level for channel0-7 in IDLE state.*/
        uint32_t idle_out_en:     1;            /*This is the output enable control bit for channel0-7 in IDLE state.*/
        uint32_t reserved20:     12;
    } rmt_driver_config_t;


struct rmt_obj_s;

typedef struct rmt_obj_s rmt_obj_t;



void initPin(int pin, int channel, rmt_driver_config_t* config);

void initMemory(int offset, uint32_t* data, size_t size);

void startTx(int channel);


int setpin(int pin);
int run();

///


rmt_obj_t* rmtInit(int pin, bool tx_not_rx, int entries, int period);

float rmtSetTick(rmt_obj_t* rmt, float tick);


bool rmtSend(uint32_t* data, size_t size, rmt_obj_t* rmt);


#ifdef __cplusplus
}
#endif