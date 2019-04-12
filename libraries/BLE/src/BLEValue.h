/*
 * BLEValue.h
 *
 *  Created on: Jul 17, 2017
 *      Author: kolban
 */

#ifndef COMPONENTS_CPP_UTILS_BLEVALUE_H_
#define COMPONENTS_CPP_UTILS_BLEVALUE_H_
#include "sdkconfig.h"
#if defined(CONFIG_BT_ENABLED)
#include <string>

/**
 * @brief The model of a %BLE value.
 */
class BLEValue {
public:
	BLEValue();
	void		addPart(std::string part);
	void		addPart(uint8_t* pData, size_t length);
	void		cancel();
	void		commit();
	uint8_t*	getData();
	size_t	  getLength();
	uint16_t	getReadOffset();
	std::string getValue();
	void        setReadOffset(uint16_t readOffset);
	void        setValue(std::string value);
	void        setValue(uint8_t* pData, size_t length);

private:
	std::string m_accumulation;
	uint16_t    m_readOffset;
	std::string m_value;

};
#endif // CONFIG_BT_ENABLED
#endif /* COMPONENTS_CPP_UTILS_BLEVALUE_H_ */
