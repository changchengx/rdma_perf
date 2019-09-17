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

#ifndef CONNECT_H
#define CONNECT_H

#include <arpa/inet.h>

#include "rdma_messenger/ThreadWrapper.h"
#include "rdma_messenger/Callback.h"
#include "rdma_messenger/RDMAStack.h"

class ConnectThread : public ThreadWrapper {
	public:
	ConnectThread(RDMAStack* rdma_stack) : rdma_stack(rdma_stack)
	{}
	virtual ~ConnectThread()
	{}

	virtual void entry() override;
	virtual void abort() override;
	private:
	RDMAStack* rdma_stack;
};

class Connector {
	public:
	Connector(struct sockaddr* addr) :
		connector_addr(addr)
	{}
	~Connector();

	void connect();
	void join();
	void shutdown();
	void set_network_stack(RDMAStack* rdma_stack_);
	void set_connect_callback(Callback* callback);
	private:
	struct sockaddr* connector_addr;
	ConnectThread* connect_thread = nullptr;
	Callback* connect_callback = nullptr;
	RDMAStack* rdma_stack;
};

#endif
