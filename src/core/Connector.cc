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

#include "rdma_messenger/Connector.h"

void ConnectThread::entry() {
	rdma_stack->cm_event_handler();
}

void ConnectThread::abort() {
	perror("connector error.");
	exit(1);
}

Connector::~Connector() {
	delete connect_thread;
	connect_thread = nullptr;
}

void Connector::connect() {
	assert(rdma_stack);
	connect_thread = new ConnectThread(rdma_stack);
	connect_thread->start(true);
	rdma_stack->connect(connector_addr);
}

void Connector::join() {
	connect_thread->join();
}

void Connector::shutdown() {
	rdma_stack->shutdown();
	join();
}

void Connector::set_network_stack(RDMAStack* rdma_stack) {
	this->rdma_stack = rdma_stack;
}

void Connector::set_connect_callback(Callback* connect_callback) {
	this->connect_callback = connect_callback;
	rdma_stack->set_connect_callback(this->connect_callback);
}
