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

#include "rdma_messenger/Acceptor.h"

void AcceptThread::entry() {
	assert(rdma_stack);
	rdma_stack->cm_event_handler();
}

void AcceptThread::abort() {
	perror("acceptor error.");
	exit(1);
}

Acceptor::~Acceptor() {
	delete accept_thread;
	accept_thread = nullptr;
}

void Acceptor::listen() {
	assert(rdma_stack);
	accept_thread = new AcceptThread(rdma_stack);
	accept_thread->start(true);
	rdma_stack->listen(server_addr);
}

void Acceptor::join() {
	accept_thread->join();
}

void Acceptor::shutdown() {
	rdma_stack->shutdown();
	join();
}

void Acceptor::set_network_stack(RDMAStack *rdma_stack) {
	this->rdma_stack = rdma_stack;
}

void Acceptor::set_accept_callback(Callback *accept_callback) {
	this->accept_callback = accept_callback;
	rdma_stack->set_accept_callback(this->accept_callback);
}
