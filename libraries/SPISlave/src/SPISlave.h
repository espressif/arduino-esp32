#ifndef SLAVE_SPI_CLASS
#define SLAVE_SPI_CLASS
#include "Arduino.h"
#include "driver/spi_slave.h"
#include <functional>
#define SPI_BUFFER_LENGTH 34
#define SPI_BUFFER_PACKET_SIZE 32

typedef std::function<void(uint8_t *data, size_t len)> SpiSlaveDataHandler;
typedef std::function<void(uint32_t status)> SpiSlaveStatusHandler;
typedef std::function<void(void)> SpiSlaveSentHandler;

void setupIntr(spi_slave_transaction_t * trans);
void transIntr(spi_slave_transaction_t * trans);
class SlaveSPIClass
{
	static SlaveSPIClass** SlaveSPIVector;
	static int size;
	friend void setupIntr(spi_slave_transaction_t * trans);
	friend void transIntr(spi_slave_transaction_t * trans);
	String buff;//used to save incoming data
	String transBuffer;//used to buffer outgoing data !not tested!
	spi_slave_transaction_t * driver;
	String perfectPrintByteHex(uint8_t b);
	SpiSlaveDataHandler _data_cb;
  SpiSlaveStatusHandler _status_cb;
  SpiSlaveSentHandler _data_sent_cb;
	SpiSlaveSentHandler _status_sent_cb;
	boolean dataWaiting;
	boolean statusWaiting;
	public:
	SlaveSPIClass();
		// : _data_cb(NULL)
    // , _status_cb(NULL)
    // , _data_sent_cb(NULL)
    // , _status_sent_cb(NULL)
	// {}
	// ~SPISlave() {}
	uint8_t bufferRx[SPI_BUFFER_LENGTH];
	uint8_t bufferTx[SPI_BUFFER_LENGTH];
	void setup_intr(spi_slave_transaction_t *trans);//called when the trans is set in the queue
	void trans_intr(spi_slave_transaction_t *trans);//called when the trans has finished
	void setStatus(uint8_t status);
	void setData(uint8_t *buf, int len);
	void setData(const char * data)	{
		setData((uint8_t *)data, strlen(data));
	}
	void onData(SpiSlaveDataHandler cb);
	void onDataSent(SpiSlaveSentHandler cb);
	void onStatus(SpiSlaveStatusHandler cb);
	void onStatusSent(SpiSlaveSentHandler cb);
	void _data_rx(uint8_t * data, size_t len);
	void _status_rx(uint32_t data);
	void _data_tx(void);
	void _status_tx(void);
	
	void begin(gpio_num_t so,gpio_num_t si,gpio_num_t sclk,gpio_num_t ss);
	void trans_queue(String& transmission);//used to queue data to transmit 
	void trans_queue(uint8_t *buf, int len);
	inline char* operator[](int i){return (&buff[i]);}
	inline void flush(){buff = "";}
	inline bool match(spi_slave_transaction_t * trans);
	void setDriver();
	inline String* getBuff(){return &buff;}

};
#if !defined(NO_GLOBAL_INSTANCES) && !defined(NO_GLOBAL_SPISLAVE)
extern SPISlaveClass SPISlave;
#endif

#endif