/*
 * ScheduledFunctions.cpp
 *
 *  Created on: 27 apr. 2018
 *      Author: Herman
 */
#include "ScheduledFunctions.h"

std::list<ScheduledFunctions::ScheduledElement> ScheduledFunctions::scheduledFunctions;

ScheduledFunctions::ScheduledFunctions()
:ScheduledFunctions(UINT_MAX)
{
}

ScheduledFunctions::ScheduledFunctions(unsigned int reqMax)
{
	maxElements = reqMax;
}

ScheduledFunctions::~ScheduledFunctions() {
}

ScheduledRegistration ScheduledFunctions::insertElement(ScheduledElement se, bool front)
{
	if (countElements >= maxElements)
	{
		return nullptr;
	}
	else
	{
		countElements++;
		if (front)
		 {
			scheduledFunctions.push_front(se);
			return scheduledFunctions.begin()->registration;
		 }
		 else
		 {
			 scheduledFunctions.push_back(se);
			 return scheduledFunctions.rbegin()->registration;
		 }
	}
}

std::list<ScheduledFunctions::ScheduledElement>::iterator ScheduledFunctions::eraseElement(std::list<ScheduledFunctions::ScheduledElement>::iterator it)
{
	countElements--;
	return scheduledFunctions.erase(it);
}

bool ScheduledFunctions::scheduleFunction(ScheduledFunction sf, bool continuous, bool front)
{
	return (insertElement({this,continuous,nullptr,sf}, front) == nullptr);
}

bool ScheduledFunctions::scheduleFunction(ScheduledFunction sf)
{
	return scheduleFunction(sf, false, false);
}

ScheduledRegistration ScheduledFunctions::scheduleFunctionReg (ScheduledFunction sf, bool continuous, bool front)
{
	return insertElement({this,continuous,std::make_shared<int>(1),sf},front);
}

void ScheduledFunctions::runScheduledFunctions()
{
	auto lastElement = scheduledFunctions.end(); // do not execute elements added during runScheduledFunctions
	auto it = scheduledFunctions.begin();
	while (it != lastElement)
	{
		bool erase = false;
		if (it->registration == nullptr)
		{
			it->function();
		}
		else
		{
			if (it->registration.use_count() > 1)
			{
				it->function();
			}
			else
			{
				erase = true;
			}
		}
		if ((!it->continuous) || (erase))
		{
			it = it->_this->eraseElement(it);
		}
		else
		{
			it++;
		}
	}
}

void ScheduledFunctions::removeFunction(ScheduledRegistration sr)
{
	auto it = scheduledFunctions.begin();
	bool removed = false;
	while ((!removed) && (it != scheduledFunctions.end()))
	{
		if (it->registration == sr)
		{
			it = eraseElement(it);
			removed = true;
		}
		else
		{
			it++;
		}
	}
}

