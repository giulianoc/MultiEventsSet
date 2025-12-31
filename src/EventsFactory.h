/*
 Copyright (C) Giuliano Catrambone (giuliano.catrambone@catrasoftware.it)

 This program is free software; you can redistribute it and/or 
 modify it under the terms of the GNU General Public License 
 as published by the Free Software Foundation; either 
 version 2 of the License, or (at your option) any later 
 version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

 Commercial use other than under the terms of the GNU General Public
 License is allowed only after express negotiation of conditions
 with the authors.
*/

#pragma once

#include <deque>
#include <map>
#include <unordered_map>
#include <mutex>
#include <iostream>
#include <memory>
#include "Event2.h"

class EventsFactory
{
private:
    using UsedEventsMap = std::map<std::pair<long,long>, std::shared_ptr<Event2>>;

    struct EventTypeInfo
    {
        long                _eventTypeIdentifier;
        long                _blockEventsNumber;
        long                _maxEventsNumberToBeAllocated;

        long                _currentEventsNumber;
        std::deque<std::shared_ptr<Event2>>    _freeEvents;
        UsedEventsMap       _usedEvents;
    };

    using EventsTypeHashMap = std::unordered_map<long, std::shared_ptr<EventTypeInfo>>;

    std::recursive_mutex     _mtEvents;
    EventsTypeHashMap   _ethmEvents;

    const long          _defaultBlockEventsNumber = 50;
    const long          _defaultMaxEventsNumberToBeAllocated = 10000;

public:
    EventsFactory (void);

    ~EventsFactory (void);

    EventsFactory (const EventsFactory& eventsFactory);

    EventsFactory (EventsFactory&& eventsFactory);
    
    friend std::ostream& operator << (std::ostream& os, const EventsFactory& eventsFactory);

    void addEventType (long eventTypeIdentifier, long blockEventsNumber, 
        long maxEventsNumberToBeAllocated);

    template<typename T>
    std::shared_ptr<T> getFreeEvent (long eventTypeIdentifier)
    {
        std::lock_guard<std::recursive_mutex> locker(_mtEvents);

        EventsTypeHashMap::const_iterator itEventType = _ethmEvents.find(eventTypeIdentifier);
        if (itEventType == _ethmEvents.end())
        {
            addEventType(eventTypeIdentifier, _defaultBlockEventsNumber, _defaultMaxEventsNumberToBeAllocated);

            itEventType = _ethmEvents.find(eventTypeIdentifier);
            if (itEventType == _ethmEvents.end())
            {
                throw std::runtime_error(std::string("Error adding the Event Type identifier")
                        + ", eventTypeIdentifier: " + std::to_string(eventTypeIdentifier));
            }
        }

        const std::shared_ptr<EventTypeInfo>& eventTypeInfo = itEventType->second;
        std::deque<std::shared_ptr<Event2>>& freeEvents = eventTypeInfo->_freeEvents;

        if (freeEvents.size() == 0)
        {
            if (eventTypeInfo->_currentEventsNumber + eventTypeInfo->_blockEventsNumber >= eventTypeInfo->_maxEventsNumberToBeAllocated)
            {
                throw std::runtime_error(std::string("No more events to be allocated, reached the max number for the event type")
                        + ", _currentEventsNumber: " + std::to_string(eventTypeInfo->_currentEventsNumber)
                        + ", _blockEventsNumber: " + std::to_string(eventTypeInfo->_blockEventsNumber)
                        + ", _maxEventsNumberToBeAllocated: " + std::to_string(eventTypeInfo->_maxEventsNumberToBeAllocated)
                        );
            }

            for (int eventIndex = 0; eventIndex < eventTypeInfo->_blockEventsNumber; eventIndex++)
            {
                std::shared_ptr<Event2>   event   = std::make_shared<T>();

                event->setEventKey(std::make_pair(eventTypeInfo->_eventTypeIdentifier, eventTypeInfo->_currentEventsNumber));

                freeEvents.push_back(event);

                eventTypeInfo->_currentEventsNumber += 1;
            }                    
        }

        {
            UsedEventsMap   &usedEvents  = eventTypeInfo->_usedEvents;

            std::shared_ptr<T> event	= dynamic_pointer_cast<T>(freeEvents.front());

            event->setStartProcessingTime(std::chrono::system_clock::now());

            freeEvents.pop_front();
            usedEvents.insert(std::make_pair(std::make_pair(event->getEventKey().first,event->getEventKey().second), event));

            
            return event;
        }
    }

    template<typename T>
    void releaseEvent (std::shared_ptr<T>& event)
    {
        std::lock_guard<std::recursive_mutex> locker(_mtEvents);

        EventsTypeHashMap::const_iterator itEventType = _ethmEvents.find(event->getEventKey().first);
        if (itEventType == _ethmEvents.end())
        {
            throw std::runtime_error(std::string("Event Type was not found")
                    + ", eventTypeIdentifier: " + std::to_string(event->getEventKey().first));
        }

        const std::shared_ptr<EventTypeInfo>& eventTypeInfo = itEventType->second;

        std::deque<std::shared_ptr<Event2>> &freeEvents = eventTypeInfo->_freeEvents;
        UsedEventsMap   &usedEvents  = eventTypeInfo->_usedEvents;

        UsedEventsMap::const_iterator itEvent = usedEvents.find(std::make_pair(event->getEventKey().first, event->getEventKey().second));
        if (itEvent == usedEvents.end())
        {
            throw std::runtime_error(std::string("Event was not found")
                    + ", event->getEventKey().first: " + std::to_string(event->getEventKey().first)
                    + ", event->getEventKey().second: " + std::to_string(event->getEventKey().second)
                    );
        }

        freeEvents.push_back(dynamic_pointer_cast<Event2>(event));
        usedEvents.erase(itEvent);
    }
} ;
