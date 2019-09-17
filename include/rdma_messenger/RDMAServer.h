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

#ifndef RDMASERVER_H
#define RDMASERVER_H

#include <string>
#include <mutex>
#include <thread>
#include <condition_variable>

#include "rdma_messenger/Acceptor.h"

class RDMAServer {
	public:
	RDMAServer(struct sockaddr *addr) : server_addr(addr)
	{
		stack =  new RDMAStack(true);
		acceptor = new Acceptor(server_addr);
		acceptor->set_network_stack(stack);
	}
	~RDMAServer()
	{
		delete stack;
		delete acceptor;
		stack = nullptr;
		acceptor = nullptr;
	}

	void start(Callback *accept_callback)
	{
		acceptor->set_accept_callback(accept_callback);
		acceptor->listen();
    }

	void wait()
	{
		std::unique_lock<std::mutex> lk(finish_mtx);
		while (finished == false) {
			cv.wait(lk);
		}
	}

	void close()
	{
		acceptor->shutdown();
		std::unique_lock<std::mutex> lk(finish_mtx);
		finished = true;
		// who notify wait
	}

	private:
	RDMAStack *stack = nullptr;
	Acceptor *acceptor = nullptr;
	struct sockaddr *server_addr = nullptr;
	bool finished = false;
	std::mutex finish_mtx;
	std::condition_variable cv;
};

#endif
