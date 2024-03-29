/*
 * Copyright 2019 Liu Changcheng <changcheng.liu@aliyun.com>
 * Author: Liu Changcheng <changcheng.liu@aliyun.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef THREADWRAPPER_H
#define THREADWRAPPER_H
#include <assert.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <iostream>

class ThreadWrapper {
	public:
	ThreadWrapper() {}

	virtual ~ThreadWrapper() {
		std::cout << "destroy thread." << std::endl;
	}

	void join() {
		if (thread.joinable()) {
			thread.join();
		} else {
			std::unique_lock<std::mutex> l(join_mutex);
			while (done == false) {
				join_event.wait(l);
			}
		}
	}

	void start(bool background_thread = false) {
		thread = std::thread(&ThreadWrapper::thread_body, this);
		t_handler = thread.native_handle();
		if (background_thread) {
			thread.detach();
		}
	}

	void set_affinity(int cpu) {
		cpu_set_t cpuset;
		CPU_ZERO(&cpuset);
		CPU_SET(cpu, &cpuset);
		int res = pthread_setaffinity_np(thread.native_handle(), sizeof(cpu_set_t), &cpuset);
		if (res) {
			abort();
		}
	}

	void thread_body() {
		try {
			entry();
		} catch (ThreadAbortException&) {
			abort();
		} catch (std::exception& ex) {
			ExceptionCaught(ex);
		} catch (...) {
			UnknownExceptionCaught();
		}
	}

	void stop() {
		pthread_cancel(t_handler);
		std::cout << "finished." << std::endl;
	}

	private:
	class ThreadAbortException : std::exception {};

	protected:
	virtual void entry() = 0;
	virtual void abort() = 0;
	virtual void ExceptionCaught(std::exception& exception)
	{}
	virtual void UnknownExceptionCaught()
	{}

	private:
	bool done = false;
	std::thread thread;
	pthread_t t_handler;
	std::mutex join_mutex;
	std::condition_variable join_event;
};

#endif
