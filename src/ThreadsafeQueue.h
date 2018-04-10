/*
   Copyright (C) 2007, 2008, 2009, 2010, 2011 Her Majesty the Queen in
   Right of Canada (Communications Research Center Canada)

   Copyright (C) 2018
   Matthias P. Braendli, matthias.braendli@mpb.li

   An implementation for a threadsafe queue using std thread library

   When creating a ThreadsafeQueue, one can specify the minimal number
   of elements it must contain before it is possible to take one
   element out.
 */
/*
   This file is part of ODR-DabMod.

   ODR-DabMod is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the
   License, or (at your option) any later version.

   ODR-DabMod is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with ODR-DabMod.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <mutex>
#include <condition_variable>
#include <queue>
#include <utility>

/* This queue is meant to be used by two threads. One producer
 * that pushes elements into the queue, and one consumer that
 * retrieves the elements.
 *
 * The queue can make the consumer block until an element
 * is available.
 */

template<typename T>
class ThreadsafeQueue
{
public:
    /* Push one element into the queue, and notify another thread that
     * might be waiting.
     *
     * returns the new queue size.
     */
    size_t push(T const& val)
    {
        std::unique_lock<std::mutex> lock(the_mutex);
        the_queue.push(val);
        size_t queue_size = the_queue.size();
        lock.unlock();

        the_rx_notification.notify_one();

        return queue_size;
    }

    size_t push(T&& val)
    {
        std::unique_lock<std::mutex> lock(the_mutex);
        the_queue.emplace(std::move(val));
        size_t queue_size = the_queue.size();
        lock.unlock();

        the_rx_notification.notify_one();

        return queue_size;
    }

    /* Push one element into the queue, but wait until the
     * queue size goes below the threshold.
     *
     * Notify waiting thread.
     *
     * returns the new queue size.
     */
    size_t push_wait_if_full(T const& val, size_t threshold)
    {
        std::unique_lock<std::mutex> lock(the_mutex);
        while (the_queue.size() >= threshold) {
            the_tx_notification.wait(lock);
        }
        the_queue.push(val);
        size_t queue_size = the_queue.size();
        lock.unlock();

        the_rx_notification.notify_one();

        return queue_size;
    }

    /* Send a notification for the receiver thread */
    void notify(void)
    {
        the_rx_notification.notify_one();
    }

    bool empty() const
    {
        std::unique_lock<std::mutex> lock(the_mutex);
        return the_queue.empty();
    }

    size_t size() const
    {
        std::unique_lock<std::mutex> lock(the_mutex);
        return the_queue.size();
    }

    bool try_pop(T& popped_value)
    {
        std::unique_lock<std::mutex> lock(the_mutex);
        if (the_queue.empty()) {
            return false;
        }

        popped_value = the_queue.front();
        the_queue.pop();

        lock.unlock();
        the_tx_notification.notify_one();

        return true;
    }

    void wait_and_pop(T& popped_value, size_t prebuffering = 1)
    {
        std::unique_lock<std::mutex> lock(the_mutex);
        while (the_queue.size() < prebuffering) {
            the_rx_notification.wait(lock);
        }

        std::swap(popped_value, the_queue.front());
        the_queue.pop();

        lock.unlock();
        the_tx_notification.notify_one();
    }

private:
    std::queue<T> the_queue;
    mutable std::mutex the_mutex;
    std::condition_variable the_rx_notification;
    std::condition_variable the_tx_notification;
};

