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

#ifndef ACCEPT_H
#define ACCEPT_H

#include <arpa/inet.h>

#include "rdma_messenger/ThreadWrapper.h"
#include "rdma_messenger/Callback.h"
#include "rdma_messenger/RDMAStack.h"

class AcceptThread : public ThreadWrapper {
	public:
	AcceptThread(RDMAStack* rdma_stack) : rdma_stack(rdma_stack)
	{}
	virtual ~AcceptThread()
	{}

	virtual void entry() override;
	virtual void abort() override;

	private:
	RDMAStack *rdma_stack;
};

class Acceptor {
	public:
	Acceptor(struct sockaddr* addr) :
		server_addr(addr)
	{}
	~Acceptor();

	void listen();
	void join();
	void shutdown();
	void set_network_stack(RDMAStack *rdma_stack);
	void set_accept_callback(Callback *accept_callback);

	private:
	struct sockaddr* server_addr;
	AcceptThread *accept_thread = nullptr;
	Callback *accept_callback = nullptr;
	RDMAStack *rdma_stack;
};

#endif
