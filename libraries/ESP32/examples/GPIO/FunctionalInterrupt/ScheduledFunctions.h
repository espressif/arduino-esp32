/*
 * ScheduledFunctions.h
 *
 *  Created on: 27 apr. 2018
 *      Author: Herman
 */
#include "Arduino.h"
#include "Schedule.h"

#include <functional>
#include <memory>
#include <list>
#include <climits>

#ifndef SCHEDULEDFUNCTIONS_H_
#define SCHEDULEDFUNCTIONS_H_

typedef std::function<void(void)> ScheduledFunction;
typedef std::shared_ptr<void> ScheduledRegistration;

class ScheduledFunctions {

public:
	ScheduledFunctions();
	ScheduledFunctions(unsigned int reqMax);
	virtual ~ScheduledFunctions();

	struct ScheduledElement
	{
		ScheduledFunctions* _this;
		bool continuous;
		ScheduledRegistration registration;
		ScheduledFunction function;
	};

	ScheduledRegistration insertElement(ScheduledElement se, bool front);
	std::list<ScheduledElement>::iterator eraseElement(std::list<ScheduledElement>::iterator);
	bool scheduleFunction(ScheduledFunction sf, bool continuous, bool front);
	bool scheduleFunction(ScheduledFunction sf);
	ScheduledRegistration scheduleFunctionReg (ScheduledFunction sf, bool continuous, bool front);
	static void runScheduledFunctions();
	void removeFunction(ScheduledRegistration sr);


	static std::list<ScheduledElement> scheduledFunctions;
	unsigned int maxElements;
	unsigned int countElements = 0;

};

#endif /* SCHEDULEDFUNCTIONS_H_ */
