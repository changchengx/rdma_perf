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

#ifndef RDMASTACK_H
#define RDMASTACK_H

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <semaphore.h>
#include <stdio.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>

#include <rdma/rdma_cma.h>

#include <set>
#include <unordered_map>
#include <atomic>

#include "rdma_messenger/Callback.h"
#include "rdma_messenger/rdma_config.h"
#include "rdma_messenger/ThreadWrapper.h"
#include "rdma_messenger/RDMAConnection.h"

enum cm_event_state {
	IDLE = 1,
	CONNECT_REQUEST,
	ADDR_RESOLVED,
	ROUTE_RESOLVED,
	CONNECTED,
	RDMA_READ_ADV,
	RDMA_READ_COMPLETE,
	RDMA_WRITE_ADV,
	RDMA_WRITE_COMPLETE,
	DISCONNECTED,
	ERROR,
};

class RDMAStack;
class CQThread;

class RDMAConMgr {
	public:
	RDMAConMgr(RDMAStack* rdma_stack);
	~RDMAConMgr();

	RDMAConnection* get_connection(uint32_t qp_num);
	RDMAConnection* new_connection(struct rdma_cm_id *cm_id);
	void add(RDMAConnection* new_con);
	void del(uint64_t con_id, uint32_t qp_num);

	private:
	uint64_t con_number = 0;
	RDMAStack* rdma_stack;
	std::unordered_map<uint32_t, RDMAConnection*> qp_con_map;
	std::unordered_map<uint64_t, RDMAConnection*> con_map;
	std::unordered_map<uint64_t, struct ibv_cq*> cq_map;
	std::vector<CQThread*> cq_threads;
};

class RDMAStack {
	public:
	RDMAStack(bool is_server = false) : is_server(is_server), stop(false), con_mgr(new RDMAConMgr(this))
	{}
	~RDMAStack()
	{
		stop.store(true);
		delete con_mgr;
		con_mgr = nullptr;
	}

	private:
	RDMAConnection* connection_establish(struct rdma_cm_id* cm_id);

	public:
	void init();

	void listen(struct sockaddr* addr);
	void accept(struct rdma_cm_id* new_cm_id);
	void connect(struct sockaddr* addr);
	void accept_abort();
	void connection_abort();
	void shutdown();

	void handle_recv(struct ibv_wc* wc);
	void handle_send(struct ibv_wc* wc);
	void handle_err(struct ibv_wc* wc);
	void cq_event_handler(struct ibv_comp_channel* cq_channel, struct ibv_cq* poll_cq);
	void cm_event_handler();

	void set_accept_callback(Callback* accept_callback);
	void set_connect_callback(Callback* connect_callback);

	private:
	sem_t sem;
	bool is_server;
	std::atomic<bool> stop;
	RDMAConMgr *con_mgr;

	cm_event_state state;
	struct rdma_event_channel* cm_channel = nullptr;
	struct rdma_cm_id* cm_id = nullptr;
	Callback* accept_callback = nullptr;
	Callback* connect_callback = nullptr;
};

class CQThread : public ThreadWrapper {
	public:
	CQThread(RDMAStack *rdma_stack, struct ibv_comp_channel *cq_channel, struct ibv_cq *cq) : rdma_stack(rdma_stack), cq_channel(cq_channel), cq(cq)
	{}
	virtual ~CQThread()
	{}

	virtual void entry() override
	{
		rdma_stack->cq_event_handler(cq_channel, cq);
	}
	virtual void abort() override
	{ }

	private:
	RDMAStack* rdma_stack;
	struct ibv_comp_channel* cq_channel;
	struct ibv_cq* cq;
};

#endif
