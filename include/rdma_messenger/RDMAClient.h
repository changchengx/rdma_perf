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

#ifndef RDMACLIENT_H
#define RDMACLIENT_H

#include <iostream>

#include "rdma_messenger/Connector.h"

class RDMAClient {
	public:
	RDMAClient(struct sockaddr *addr, uint32_t con_nums_) : client_addr(addr), con_nums(con_nums_)
	{
		stack = new RDMAStack(false);
		connector = new Connector(addr);
		connector->set_network_stack(stack);
	}

	~RDMAClient()
	{
		delete stack;
		delete connector;
		stack = nullptr;
		connector = nullptr;
	}

	void connect(Callback *connect_callback)
	{
		assert(stack);
		connector->set_connect_callback(connect_callback);
		for (uint32_t i = 0; i < con_nums; ++i) {
			stack->init();
			std::cout << "connecting..." << std::endl;
			connector->connect();
		}
	}

	void wait()
	{
		std::unique_lock<std::mutex> lk(finish_mtx);
		while (!finished) {
			cv.wait(lk);
		}
    }

	void close()
	{
		connector->shutdown();
		std::unique_lock<std::mutex> lk(finish_mtx);
		finished = true;
		// who notify wait
	}

	private:
	RDMAStack *stack = nullptr;
	Connector *connector = nullptr;
	struct sockaddr *client_addr = nullptr;
	bool finished = false;
	uint32_t con_nums = 0;
	std::mutex finish_mtx;
	std::condition_variable cv;
};

#endif
