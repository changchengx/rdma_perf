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

#include <assert.h>

#include <iostream>

#include "rdma_messenger/RDMAStack.h"

RDMAConMgr::RDMAConMgr(RDMAStack* rdma_stack) : rdma_stack(rdma_stack)
{
	cq_threads.reserve(IO_WORKER_NUMS);
}

RDMAConMgr::~RDMAConMgr()
{
	for (auto m : qp_con_map) {
		delete m.second;
	}
}

RDMAConnection* RDMAConMgr::get_connection(uint32_t qp_num)
{
	if (qp_con_map.count(qp_num) != 0) {
		return qp_con_map[qp_num];
	} else {
		return nullptr;
	}
}

RDMAConnection* RDMAConMgr::new_connection(struct rdma_cm_id* cm_id)
{
	struct ibv_pd* pd = ibv_alloc_pd(cm_id->verbs);
	struct ibv_cq* cq = nullptr;
	uint64_t con_id = con_number;
	if (con_id < IO_WORKER_NUMS) {
		struct ibv_comp_channel *cq_channel = ibv_create_comp_channel(cm_id->verbs);
		cq = ibv_create_cq(cm_id->verbs, CQE_PER_CQ * 2, nullptr, cq_channel, 0);
		ibv_req_notify_cq(cq, 0);

		CQThread *cq_thread = new CQThread(rdma_stack, cq_channel, cq);
		cq_thread->start();
		cq_thread->set_affinity(con_id + 20);
		cq_threads.push_back(cq_thread);
	} else {
		cq = cq_map[con_id % IO_WORKER_NUMS];
	}
	RDMAConnection *new_con = new RDMAConnection(pd, cq, cm_id, con_id);
	add(new_con);
	new_con->post_recv_buffers();
	return new_con;
}

void RDMAConMgr::add(RDMAConnection* new_con)
{
	uint64_t con_id = con_number;
	uint32_t qp_num = new_con->get_qp()->qp_num;
	struct ibv_cq* cq = new_con->get_cq();
	qp_con_map.insert(std::pair<uint32_t, RDMAConnection*>(qp_num, new_con));
	con_map.insert(std::pair<uint64_t, RDMAConnection*>(con_id, new_con));
	cq_map.insert(std::pair<uint64_t, struct ibv_cq*>(con_id, cq));
	con_number++;
}

void RDMAConMgr::del(uint64_t con_id, uint32_t qp_num) {
	con_map.erase(con_id);
	qp_con_map.erase(qp_num);
	if (con_id >= IO_WORKER_NUMS)
		cq_map.erase(con_id);
}

void RDMAStack::init()
{
	sem_init(&sem, 0, 0);
	cm_channel = rdma_create_event_channel();
	rdma_create_id(cm_channel, &cm_id, NULL, RDMA_PS_TCP);
}

void RDMAStack::listen(struct sockaddr* addr)
{
  rdma_bind_addr(cm_id, addr);
  rdma_listen(cm_id, 1);
}

void RDMAStack::accept(struct rdma_cm_id* new_cm_id)
{
  struct rdma_event_channel* event_channel = nullptr;
  event_channel = rdma_create_event_channel();
  rdma_migrate_id(new_cm_id, event_channel);
  RDMAConnection *con = connection_establish(new_cm_id);
  rdma_accept(new_cm_id, NULL);
  if (accept_callback) {
    accept_callback->callback_entry(con);
  }
}

void RDMAStack::connect(struct sockaddr *addr)
{
	rdma_resolve_addr(cm_id, NULL, addr, 5000);
	sem_wait(&sem);

	RDMAConnection *con = connection_establish(cm_id);

	struct rdma_conn_param cm_params = {};
	cm_params.responder_resources = 1;
	cm_params.retry_count = 7;
	rdma_connect(cm_id, &cm_params);
	sem_wait(&sem);

	if (connect_callback) {
		connect_callback->callback_entry(con);
	}
}

void RDMAStack::shutdown()
{
	rdma_disconnect(cm_id);
	assert(this);
}

void RDMAStack::handle_recv(struct ibv_wc* wc)
{
	RDMAConnection* con = con_mgr->get_connection(wc->qp_num);
	if (wc->wr_id == 0) {
		if (con) {
			con_mgr->del(con->get_con_id(), wc->qp_num);
			con->close();
			con = nullptr;
		}
	} else {
		Chunk* ck = reinterpret_cast<Chunk*>(wc->wr_id);
		ck->chk_size = wc->byte_len;

		if (!con || !con->is_recv_buffer(ck->chk_buf))
			return;

		if (con->read_callback) {
			con->read_callback->callback_entry(con, ck);
		}
		con->post_recv_buffer(ck);
	}
}

void RDMAStack::handle_send(struct ibv_wc* wc)
{
	RDMAConnection* con = con_mgr->get_connection(wc->qp_num);
	if (wc->wr_id == 0) {
		if (con) {
			con_mgr->del(con->get_con_id(), wc->qp_num);
			con->close();
			con = nullptr;
		}
	} else {
		Chunk* ck = reinterpret_cast<Chunk*>(wc->wr_id);

		if (!con || !con->is_send_buffer(ck->chk_buf))
			return;
		con->reap_chunk(&ck);
	}
}

void RDMAStack::handle_err(struct ibv_wc* wc)
{
	RDMAConnection* con = con_mgr->get_connection(wc->qp_num);
	if (con) {
		con_mgr->del(con->get_con_id(), wc->qp_num);
		con->close();
	}
}

void RDMAStack::cq_event_handler(struct ibv_comp_channel* cq_channel, struct ibv_cq* poll_cq)
{
	struct ibv_cq* cq_triggered = nullptr;
	void* cq_ctx = nullptr;
	while (true) {
		if (cq_channel = nullptr) {
			continue;
		}
		ibv_get_cq_event(cq_channel, &cq_triggered, &cq_ctx);
		ibv_ack_cq_events(cq_triggered, 1);
		ibv_req_notify_cq(cq_triggered, 0);

		assert(poll_cq == cq_triggered);

		struct ibv_wc wc;
		while (ibv_poll_cq(poll_cq, 1, &wc) == 1) {
			if (wc.status) {
				std::cerr << "connection error: " << ibv_wc_status_str(wc.status)
					<< std::endl;
				handle_err(&wc);
				continue;
			}

			switch (wc.opcode) {
			case IBV_WC_SEND:
				handle_send(&wc);
				break;
			case IBV_WC_RECV:
				handle_recv(&wc);
				break;
			default:
				assert(0 == "bug");
			}
		}
	}
	return;
}

void RDMAStack::cm_event_handler()
{
	pthread_testcancel();
	int rst = 0;
	struct rdma_cm_event *cm_event = nullptr;
	while (rdma_get_cm_event(cm_channel, &cm_event) == 0) {
		std::cout << "got cm event: " << rdma_event_str(cm_event->event) << std::endl;
		switch (cm_event->event) {
		case RDMA_CM_EVENT_ADDR_RESOLVED: {
			rst = rdma_resolve_route(cm_id, 2000);
			if (rst) {
				sem_post(&sem);
			}
			break;
		}

		case RDMA_CM_EVENT_ROUTE_RESOLVED:
			sem_post(&sem);
			break;

		case RDMA_CM_EVENT_CONNECT_REQUEST: {
			struct rdma_cm_id* event_cm_id = cm_event->id;
			accept(event_cm_id);
			break;
		}

		case RDMA_CM_EVENT_ESTABLISHED:
			sem_post(&sem);
			if (!is_server)
				return;
			break;
		case RDMA_CM_EVENT_ADDR_ERROR:
		case RDMA_CM_EVENT_ROUTE_ERROR:
		case RDMA_CM_EVENT_CONNECT_ERROR:
		case RDMA_CM_EVENT_UNREACHABLE:
		case RDMA_CM_EVENT_REJECTED: {
			std::cerr << "cma event: " << rdma_event_str(cm_event->event)
				<< "error: " << cm_event->status << std::endl;
			sem_post(&sem);
			rst = -1;
			break;
		}

		case RDMA_CM_EVENT_DISCONNECTED:
			sem_post(&sem);
			break;

		case RDMA_CM_EVENT_DEVICE_REMOVAL: {
			std::cout << "cma detect device remove" << std::endl;
			sem_post(&sem);
			rst = -1;
			break;
		}

		default:
			std::cerr << "ignore unknown event: " << rdma_event_str(cm_event->event) << std::endl;
			break;
		}
		rdma_ack_cm_event(cm_event);
		cm_event = nullptr;
	}
}

RDMAConnection* RDMAStack::connection_establish(struct rdma_cm_id* cm_id)
{
  return con_mgr->new_connection(cm_id);
}

void RDMAStack::set_accept_callback(Callback *accept_callback)
{
	this->accept_callback = accept_callback;
}

void RDMAStack::set_connect_callback(Callback *connect_callback_)
{
	this->connect_callback = connect_callback;
}
